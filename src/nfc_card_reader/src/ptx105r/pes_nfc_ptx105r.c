/**
 * pes_nfc_ptx105r.c
 *
 * Implementation for the Renesas PTX105R NFC reader, calling the Renesas
 * PTX NFC SDK (ptxIoTRd_* / ptx_IOT_READER.h) directly instead of going
 * through the RM_NFC_READER_PTX FSP wrapper. Peripheral (SPI/GPIO/Timer)
 * bring-up still uses the ptxPLAT_* /ptxPERIPH_* platform-glue functions
 * that are also used internally by the FSP wrapper, pointed at the
 * FSP-generated peripheral instances in g_nfc_reader_ptx0_cfg.
 *
 * Each function is a direct, non-static entry point declared in
 * pes_nfc_ptx105r.h and called by name from the rest of the PES NFC Card
 * Reader module — no function-pointer vtable indirection.
 *
 * There is no local state-machine (open/idle/discovered/activated) here:
 * we rely on the PTX SDK's own error handling/return codes instead of
 * re-implementing the FSP wrapper's defensive state checks.
 */

#include "pes_nfc_ptx105r.h"
#include "pes_nfc_ptx105r_board.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "hal_data.h"
/* PTX NFC SDK — used directly instead of the RM_NFC_READER_PTX FSP API */
#include "ptx_IOT_READER.h"
#include "ptxPLAT_GPIO.h"
#include "ptxPLAT_SPI.h"
#include "ptxPLAT_TIMER.h"
#include "ptxPERIPH_APPTIMER.h"

/* ── Module-local constants ────────────────────────────────────────── */
#define PTX105R_ZERO                  (0)
#define PTX105R_SHUTDOWN_TEMP         (223U)
#define PTX105R_TIMEOUT_RAW           (200U) /* Application-timeout for raw-protocol exchanges */

/* Only perform temperature-sensor calibration once per power cycle */
static bool g_start_temp_calibration = true;

/* Tracks whether pes_nfc_ptx_open() succeeded (replaces FSP ctrl->open) */
static bool g_ptx_opened = false;

/* ── Interrupt-driven wait support ─────────────────────────────────── */

static volatile TaskHandle_t g_waiting_task = NULL;

static void ptx105r_irq_wake_cb(external_irq_callback_args_t *p_args)
{
    PES_COMMON_UNUSED(p_args);
    if (NULL != g_waiting_task)
    {
        BaseType_t higher_prio_task_woken = pdFALSE;
        vTaskNotifyGiveFromISR(g_waiting_task, &higher_prio_task_woken);
        portYIELD_FROM_ISR(higher_prio_task_woken);
    }
}

/* ── Internal helpers ──────────────────────────────────────────────── */

static pes_nfc_card_type_t map_card_type(ptxIoTRd_CardParams_t *card,
                                         ptxIoTRd_CardProtocol_t prot)
{
    if (NULL == card) { return PES_NFC_CARD_TYPE_UNKNOWN; }

    switch (card->TechType)
    {
        case Tech_TypeA:
            switch (prot)
            {
                case Prot_T2T:    return PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2;
                case Prot_ISODEP: return PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A;
                default:          return PES_NFC_CARD_TYPE_ISO14443A;
            }
        case Tech_TypeB:
            return (Prot_ISODEP == prot) ? PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B
                                         : PES_NFC_CARD_TYPE_ISO14443B;
        case Tech_TypeF:
            return (Prot_T3T == prot) ? PES_NFC_CARD_TYPE_NFC_TAG_TYPE_3
                                      : PES_NFC_CARD_TYPE_FELICA;
        case Tech_TypeV:
            return (Prot_T5T == prot) ? PES_NFC_CARD_TYPE_NFC_TAG_TYPE_5
                                      : PES_NFC_CARD_TYPE_ISO15693;
        default:
            return PES_NFC_CARD_TYPE_UNKNOWN;
    }
}

static pes_nfc_protocol_t map_protocol(ptxIoTRd_CardProtocol_t prot)
{
    switch (prot)
    {
        case Prot_T2T:       return PES_NFC_PROT_T2T;
        case Prot_T3T:       return PES_NFC_PROT_T3T;
        case Prot_ISODEP:    return PES_NFC_PROT_ISODEP;
        case Prot_NFCDEP:    return PES_NFC_PROT_NFCDEP;
        case Prot_T5T:       return PES_NFC_PROT_T5T;
        case Prot_Extension: return PES_NFC_PROT_EXTENSION;
        default:             return PES_NFC_PROT_UNDEFINED;
    }
}

static ptxIoTRd_CardProtocol_t choose_protocol(ptxIoTRd_CardParams_t *card)
{
    if (NULL == card) { return Prot_Undefined; }

    switch (card->TechType)
    {
        case Tech_TypeA:
            if (0u != (card->TechParams.CardAParams.SEL_RES & 0x40u))
                return Prot_NFCDEP;
            if (0u != (card->TechParams.CardAParams.SEL_RES & 0x20u))
                return Prot_ISODEP;
            return Prot_T2T;
        case Tech_TypeB:
            if (0u != (card->TechParams.CardBParams.SENSB_RES[10] & 0x01u))
                return Prot_ISODEP;
            return Prot_Undefined;
        case Tech_TypeF:
            if ((0x01u == card->TechParams.CardFParams.SENSF_RES[0]) &&
                (0xFEu == card->TechParams.CardFParams.SENSF_RES[1]))
                return Prot_NFCDEP;
            return Prot_T3T;
        case Tech_TypeV:
            return Prot_T5T;
        case Tech_TypeExtension:
            return Prot_Extension;
        default:
            return Prot_Undefined;
    }
}

static void extract_uid(ptxIoTRd_CardParams_t *card, uint8_t *uid, uint8_t *uid_len)
{
    *uid_len = 0;
    if (NULL == card) { return; }

    switch (card->TechType)
    {
        case Tech_TypeA:
        {
            uint8_t len = card->TechParams.CardAParams.NFCID1_LEN;
            if (len > PES_NFC_UID_MAX_BYTES) { len = PES_NFC_UID_MAX_BYTES; }
            (void)memcpy(uid, card->TechParams.CardAParams.NFCID1, len);
            *uid_len = len;
            break;
        }
        case Tech_TypeB:
            (void)memcpy(uid, &card->TechParams.CardBParams.SENSB_RES[1], 4u);
            *uid_len = 4u;
            break;
        case Tech_TypeF:
            (void)memcpy(uid, &card->TechParams.CardFParams.SENSF_RES[2], 8u);
            *uid_len = 8u;
            break;
        case Tech_TypeV:
            for (uint8_t i = 0; i < 8u; i++)
            {
                uid[i] = card->TechParams.CardVParams.UID[7u - i];
            }
            *uid_len = 8u;
            break;
        default:
            break;
    }
}

static pes_status_t ptx105r_discover_status(pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }

    uint8_t raw = 0;
    ptxStatus_t st = ptxIoTRd_Get_Status_Info(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                              StatusType_Discover, &raw);
    if (ptxStatus_Success != st) { return PES_ERR_INTERNAL; }

    switch (raw)
    {
        case RF_DISCOVER_STATUS_CARD_ACTIVE:     *out_status = PES_NFC_DISC_CARD_ACTIVE; break;
        case RF_DISCOVER_STATUS_DISCOVER_RUNNING:*out_status = PES_NFC_DISC_RUNNING;     break;
        case RF_DISCOVER_STATUS_DISCOVER_DONE:   *out_status = PES_NFC_DISC_DONE;        break;
        default:                                 *out_status = PES_NFC_DISC_NO_CARD;     break;
    }
    return PES_OK;
}

static pes_status_t ptx105r_system_check(void)
{
    uint8_t state = 0;
    ptxStatus_t st = ptxIoTRd_Get_Status_Info(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                              StatusType_System, &state);
    if (ptxStatus_Success != st) { return PES_ERR_INTERNAL; }
    return (PTX_SYSTEM_STATUS_OK == state) ? PES_OK : PES_ERR_INTERNAL;
}

static ptxIoTRd_CardRegistry_t *g_active_reg = NULL;

/* ── PTX SDK function implementations ──────────────────────────────── */

pes_status_t pes_nfc_ptx_open(pes_nfc_reader_device_t device)
{
    PES_COMMON_UNUSED(device);

    nfc_reader_ptx_cfg_t const * p_cfg = &g_nfc_reader_ptx0_cfg;

    /* Define IoT Reader parameters (mirrors former RM_NFC_READER_PTX_Open) */
    ptxIoTRd_InitPars_t            init_params;
    ptxIoTRd_TempSense_Params_t    temp_sensor;
    ptxIoTRd_ComInterface_Params_t com_interface;

    (void)memset(&init_params, PTX105R_ZERO, sizeof(init_params));
    (void)memset(&temp_sensor, PTX105R_ZERO, sizeof(temp_sensor));
    (void)memset(&com_interface, PTX105R_ZERO, sizeof(com_interface));

    /* Temperature-sensor calibration is only performed once per power cycle */
    if (true == g_start_temp_calibration)
    {
        temp_sensor.Calibrate = p_cfg->temp_sensor_calibrate;
        temp_sensor.Tambient  = p_cfg->temp_sensor_ambient;
        temp_sensor.Tshutdown = p_cfg->temp_sensor_shutdown;
        g_start_temp_calibration = false;
    }
    else
    {
        temp_sensor.Tshutdown = PTX105R_SHUTDOWN_TEMP;
    }

    init_params.TemperatureSensor = &temp_sensor;
    init_params.ComInterface      = &com_interface;

    /* Bring up the low-level peripherals (SPI, GPIO/IRQ, timers) required
     * by the PTX SDK. These are the same platform-glue calls the FSP
     * wrapper used to make internally. */
    if (ptxStatus_Success != ptxPLAT_GPIO_Open(p_cfg->p_gpio_context, p_cfg->p_irq_context, p_cfg->interrupt_pin))
    {
        return PES_ERR_INTERNAL;
    }
    if (ptxStatus_Success != ptxPLAT_TIMER_Open(p_cfg->p_timer_context))
    {
        return PES_ERR_INTERNAL;
    }
    if (ptxStatus_Success != ptxPERIPH_APPTIMER_Open(p_cfg->p_app_timer))
    {
        return PES_ERR_INTERNAL;
    }
    if (ptxStatus_Success != ptxPLAT_SPI_Open(p_cfg->p_comms_instance_ctrl, p_cfg->p_gpio_context))
    {
        return PES_ERR_INTERNAL;
    }

    /* Initiate the IoT-Reader System (PTX SDK) */
    ptxStatus_t st = ptxIoTRd_Init(p_cfg->iot_reader_context, &init_params);
    if (ptxStatus_Success != st)
    {
        /* Retry once after a Deinit, mirrors the FSP wrapper's recovery path */
        (void)ptxIoTRd_Deinit(p_cfg->iot_reader_context);
        st = ptxIoTRd_Init(p_cfg->iot_reader_context, &init_params);
    }

    if (ptxStatus_Success != st) { return PES_ERR_INTERNAL; }

    g_ptx_opened = true;
    return PES_OK;
}

pes_status_t pes_nfc_ptx_close(void)
{
    g_active_reg = NULL;
    ptxStatus_t st = ptxIoTRd_Deinit(g_nfc_reader_ptx0_cfg.iot_reader_context);
    g_ptx_opened = false;
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

bool pes_nfc_ptx_is_open(void)
{
    return g_ptx_opened;
}

pes_status_t pes_nfc_ptx_configure_polling(pes_nfc_tech_mask_t tech_mask)
{
    PES_COMMON_UNUSED(tech_mask);
    return PES_OK;
}

pes_status_t pes_nfc_ptx_start_polling(void)
{
    nfc_reader_ptx_cfg_t const * p_cfg = &g_nfc_reader_ptx0_cfg;

    ptxIoTRd_DiscConfig_t disc_config;
    (void)memset(&disc_config, PTX105R_ZERO, sizeof(disc_config));

    disc_config.PollTypeA            = p_cfg->poll_type_a;
    disc_config.PollTypeB            = p_cfg->poll_type_b;
    disc_config.PollTypeF212         = p_cfg->poll_type_f;
    disc_config.PollTypeV            = p_cfg->poll_type_v;
    disc_config.IdleTime             = p_cfg->idle_time_ms;
    disc_config.PollTypeADeviceLimit = p_cfg->device_limit;
    disc_config.PollTypeBDeviceLimit = p_cfg->device_limit;
    disc_config.PollTypeVDeviceLimit = p_cfg->device_limit;
    disc_config.PollTypeFDeviceLimit = p_cfg->device_limit;
    disc_config.Discover_Mode        = p_cfg->discover_mode;

    if (!(disc_config.PollTypeA || disc_config.PollTypeB ||
          disc_config.PollTypeF212 || disc_config.PollTypeV))
    {
        return PES_ERR_INVALID_CFG;
    }

    ptxStatus_t st = ptxIoTRd_Initiate_Discovery(p_cfg->iot_reader_context, &disc_config);
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_stop_polling(void)
{
    ptxStatus_t st = ptxIoTRd_Reader_Deactivation(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                                  PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_wait_for_card(uint32_t timeout_ms, pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }
    *out_status = PES_NFC_DISC_NO_CARD;

    pes_status_t sys = ptx105r_system_check();
    if (PES_OK != sys) { return sys; }

    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        R_BSP_SoftwareDelay(timeout_ms, BSP_DELAY_UNITS_MILLISECONDS);
        return ptx105r_discover_status(out_status);
    }

    TickType_t remaining_ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t start_tick      = xTaskGetTickCount();

    g_waiting_task = xTaskGetCurrentTaskHandle();

    (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, ptx105r_irq_wake_cb, NULL, NULL);

    (void)ulTaskNotifyTake(pdTRUE, 0);

    pes_status_t st = PES_OK;

    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, remaining_ticks);

        (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, ptxPLAT_GPIO_IsrCallback, NULL, NULL);

        st = ptx105r_discover_status(out_status);
        if (PES_OK != st) { break; }

        if (PES_NFC_DISC_NO_CARD != *out_status)
        {
            break;
        }

        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        if (elapsed >= remaining_ticks) { break; }
        remaining_ticks = pdMS_TO_TICKS(timeout_ms) - elapsed;

        g_waiting_task = xTaskGetCurrentTaskHandle();
        (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, ptx105r_irq_wake_cb, NULL, NULL);
    }

    g_waiting_task = NULL;
    return st;
}

pes_status_t pes_nfc_ptx_activate_card(pes_nfc_ptx_card_info_t *card_info)
{
    if (NULL == card_info) { return PES_ERR_INVALID_CFG; }
    (void)memset(card_info, 0, sizeof(*card_info));

    ptxIoTRd_t *iot_rd = g_nfc_reader_ptx0_cfg.iot_reader_context;

    ptxIoTRd_CardRegistry_t *reg = NULL;
    ptxStatus_t st = ptxIoTRd_Get_Card_Registry(iot_rd, &reg);
    if ((ptxStatus_Success != st) || (NULL == reg)) { return PES_ERR_INTERNAL; }

    g_active_reg = reg;

    if (NULL != reg->ActiveCard)
    {
        card_info->card_type = map_card_type(reg->ActiveCard, reg->ActiveCardProtType);
        card_info->protocol  = map_protocol(reg->ActiveCardProtType);
        extract_uid(reg->ActiveCard, card_info->uid, &card_info->uid_len);
        return PES_OK;
    }

    if (0u == reg->NrCards) { return PES_ERR_NOT_FOUND; }

    ptxIoTRd_CardProtocol_t prot = choose_protocol(&reg->Cards[0]);
    st = ptxIoTRd_Activate_Card(iot_rd, &reg->Cards[0], prot);
    if (ptxStatus_Success != st) { return PES_ERR_INTERNAL; }

    card_info->card_type = map_card_type(reg->ActiveCard, reg->ActiveCardProtType);
    card_info->protocol  = map_protocol(reg->ActiveCardProtType);
    extract_uid(reg->ActiveCard, card_info->uid, &card_info->uid_len);
    return PES_OK;
}

pes_status_t pes_nfc_ptx_get_card_type(pes_nfc_card_type_t *out_type)
{
    if (NULL == out_type) { return PES_ERR_INVALID_CFG; }

    if (NULL == g_active_reg || NULL == g_active_reg->ActiveCard)
    {
        *out_type = PES_NFC_CARD_TYPE_UNKNOWN;
        return PES_ERR_NOT_FOUND;
    }

    *out_type = map_card_type(g_active_reg->ActiveCard, g_active_reg->ActiveCardProtType);
    return PES_OK;
}

pes_status_t pes_nfc_ptx_get_uid(uint8_t *uid, uint8_t *uid_len)
{
    if ((NULL == uid) || (NULL == uid_len)) { return PES_ERR_INVALID_CFG; }

    if (NULL == g_active_reg || NULL == g_active_reg->ActiveCard)
    {
        *uid_len = 0;
        return PES_ERR_NOT_FOUND;
    }

    extract_uid(g_active_reg->ActiveCard, uid, uid_len);
    return PES_OK;
}

void pes_nfc_ptx_sleep(uint32_t ms)
{
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

pes_status_t pes_nfc_ptx_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                       uint8_t *rx, uint32_t *rx_len)
{
    if ((NULL == tx) || (NULL == rx) || (NULL == rx_len)) { return PES_ERR_INVALID_CFG; }

    ptxStatus_t st = ptxIoTRd_Data_Exchange(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                            (uint8_t *)(uintptr_t)tx, tx_len,
                                            rx, rx_len, PTX105R_TIMEOUT_RAW);

    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_deactivate(void)
{
    ptxStatus_t st = ptxIoTRd_Reader_Deactivation(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                                  PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER);
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_get_system_state(uint8_t *out_state)
{
    if (NULL == out_state) { return PES_ERR_INVALID_CFG; }
    ptxStatus_t st = ptxIoTRd_Get_Status_Info(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                              StatusType_System, out_state);
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_get_last_rf_error(uint8_t *out_err)
{
    if (NULL == out_err) { return PES_ERR_INVALID_CFG; }
    ptxStatus_t st = ptxIoTRd_Get_Status_Info(g_nfc_reader_ptx0_cfg.iot_reader_context,
                                              StatusType_LastRFError, out_err);
    return (ptxStatus_Success == st) ? PES_OK : PES_ERR_INTERNAL;
}

void pes_nfc_ptx_wake_waiting_task(void)
{
    TaskHandle_t task = g_waiting_task;
    if (NULL != task)
    {
        (void)xTaskNotifyGive(task);
    }
}

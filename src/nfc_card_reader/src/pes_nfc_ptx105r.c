/**
 * pes_nfc_ptx105r.c
 *
 * Implementation for the Renesas PTX105R NFC reader, using the
 * RM_NFC_READER_PTX FSP API surface exclusively.
 *
 * Each function is a direct, non-static entry point declared in
 * pes_nfc_ptx.h and called by name from the rest of the PES NFC Card
 * Reader module — no function-pointer vtable indirection.
 */

#include "pes_nfc_ptx.h"
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
/* g_nfc_reader_ptx0_ctrl/cfg, FSP types */
#include "hal_data.h"

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

/**
 * Poll discovery status (internal helper, folded into wait_for_card).
 */
static pes_status_t ptx105r_discover_status(pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }

    uint8_t raw = 0;
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_Discover, &raw);
    if (FSP_SUCCESS != err) { return PES_ERR_INTERNAL; }

    switch (raw)
    {
        case RF_DISCOVER_STATUS_CARD_ACTIVE:     *out_status = PES_NFC_DISC_CARD_ACTIVE; break;
        case RF_DISCOVER_STATUS_DISCOVER_RUNNING:*out_status = PES_NFC_DISC_RUNNING;     break;
        case RF_DISCOVER_STATUS_DISCOVER_DONE:   *out_status = PES_NFC_DISC_DONE;        break;
        default:                                 *out_status = PES_NFC_DISC_NO_CARD;     break;
    }
    return PES_OK;
}

/**
 * System-health check (internal helper, folded into wait_for_card).
 */
static pes_status_t ptx105r_system_check(void)
{
    uint8_t state = 0;
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_System, &state);
    if (FSP_SUCCESS != err) { return PES_ERR_INTERNAL; }
    return (PTX_SYSTEM_STATUS_OK == state) ? PES_OK : PES_ERR_INTERNAL;
}

/**
 * Cached card registry pointer — set by activate_card, used by
 * get_card_type / get_uid so they don't need to re-fetch the registry.
 */
static ptxIoTRd_CardRegistry_t *g_active_reg = NULL;

/* ── PTX function implementations ──────────────────────────────────── */

pes_status_t pes_nfc_ptx_open(pes_nfc_reader_device_t device)
{
    PES_COMMON_UNUSED(device);

    fsp_err_t err = RM_NFC_READER_PTX_Open(&g_nfc_reader_ptx0_ctrl, &g_nfc_reader_ptx0_cfg);

    /* Cold-boot recovery: first Open can fail on PTX105R. */
    if (FSP_ERR_INVALID_DATA == err)
    {
        (void)RM_NFC_READER_PTX_Close(&g_nfc_reader_ptx0_ctrl);
        err = RM_NFC_READER_PTX_Open(&g_nfc_reader_ptx0_ctrl, &g_nfc_reader_ptx0_cfg);
    }

    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_close(void)
{
    g_active_reg = NULL;
    fsp_err_t err = RM_NFC_READER_PTX_Close(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_configure_polling(pes_nfc_tech_mask_t tech_mask)
{
    /* Poll flags are configured in g_nfc_reader_ptx0_cfg at build time.
     * A future refinement could apply tech_mask dynamically here. */
    PES_COMMON_UNUSED(tech_mask);
    return PES_OK;
}

pes_status_t pes_nfc_ptx_start_polling(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_DiscoveryStart(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_stop_polling(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_ReaderDeactivation(&g_nfc_reader_ptx0_ctrl,
                                                          NFC_READER_PTX_RETURN_IDLE);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_wait_for_card(uint32_t timeout_ms,
                                       pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }
    *out_status = PES_NFC_DISC_NO_CARD;

    /* Folded system-health check (was separate pes_nfc_ptx_system_check). */
    pes_status_t sys = ptx105r_system_check();
    if (PES_OK != sys) { return sys; }

    /* Before the scheduler starts (e.g. cold-boot recovery paths) there is
     * no task context to notify — fall back to a short blocking status
     * read instead of installing the ISR wrapper. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        R_BSP_SoftwareDelay(timeout_ms, BSP_DELAY_UNITS_MILLISECONDS);
        return ptx105r_discover_status(out_status);
    }

    TickType_t remaining_ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t start_tick      = xTaskGetTickCount();

    g_waiting_task = xTaskGetCurrentTaskHandle();

    /* Install our lightweight wake-up ISR for the duration of the wait. */
    (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, ptx105r_irq_wake_cb, NULL, NULL);

    /* Clear any stale notification so we only react to fresh IRQs. */
    (void)ulTaskNotifyTake(pdTRUE, 0);

    pes_status_t st = PES_OK;

    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, remaining_ticks);

        /* Restore the SDK's normal ISR handler before touching the SPI/
         * status registers. */
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

    ptxIoTRd_CardRegistry_t *reg = NULL;
    fsp_err_t err = RM_NFC_READER_PTX_CardRegistryGet(&g_nfc_reader_ptx0_ctrl, &reg);
    if ((FSP_SUCCESS != err) || (NULL == reg)) { return PES_ERR_INTERNAL; }

    g_active_reg = reg;

    /* If a card is already active (single-card path), use it directly. */
    if (NULL != reg->ActiveCard)
    {
        card_info->card_type = map_card_type(reg->ActiveCard, reg->ActiveCardProtType);
        card_info->protocol  = map_protocol(reg->ActiveCardProtType);
        extract_uid(reg->ActiveCard, card_info->uid, &card_info->uid_len);
        return PES_OK;
    }

    /* Multi-card path: activate the first card. */
    if (0u == reg->NrCards) { return PES_ERR_NOT_FOUND; }

    ptxIoTRd_CardProtocol_t prot = choose_protocol(&reg->Cards[0]);
    err = RM_NFC_READER_PTX_CardActivate(&g_nfc_reader_ptx0_ctrl, &reg->Cards[0], prot);
    if (FSP_SUCCESS != err) { return PES_ERR_INTERNAL; }

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

    nfc_reader_ptx_data_info_t info;
    info.p_tx_buf  = (uint8_t *)(uintptr_t)tx;
    info.tx_length = tx_len;
    info.p_rx_buf  = rx;
    info.rx_length = *rx_len;

    fsp_err_t err = RM_NFC_READER_PTX_DataExchange(&g_nfc_reader_ptx0_ctrl, &info);
    *rx_len = info.rx_length;

    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_deactivate(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_ReaderDeactivation(&g_nfc_reader_ptx0_ctrl,
                                                          NFC_READER_PTX_RETURN_DISCOVER);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_get_system_state(uint8_t *out_state)
{
    if (NULL == out_state) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_System, out_state);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_ptx_get_last_rf_error(uint8_t *out_err)
{
    if (NULL == out_err) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_LastRFError, out_err);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

void pes_nfc_ptx_wake_waiting_task(void)
{
    TaskHandle_t task = g_waiting_task;
    if (NULL != task)
    {
        (void)xTaskNotifyGive(task);
    }
}

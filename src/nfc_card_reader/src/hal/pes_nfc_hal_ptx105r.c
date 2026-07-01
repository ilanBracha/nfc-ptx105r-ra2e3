/**
 * pes_nfc_hal_ptx105r.c
 *
 * HAL implementation for the Renesas PTX105R NFC reader, using the
 * RM_NFC_READER_PTX FSP API surface exclusively.
 */

#include "pes_nfc_hal.h"
#include "hal_data.h"          /* g_nfc_reader_ptx0_ctrl/cfg, FSP types */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

/* ── Interrupt-driven wait support ─────────────────────────────────── */

/* Task to notify from our temporary ISR wrapper (set while waiting). */
static volatile TaskHandle_t g_waiting_task = NULL;

/*
 * Lightweight ISR installed only for the duration of
 * pes_nfc_hal_wait_for_card(). Fires on the same ICU IRQ7 edge that the
 * PTX105R uses to signal pending host notifications (RF-discovery,
 * RF-error, etc.). Just wakes the waiting task; all actual SPI/status
 * processing happens afterwards from thread context.
 */
static void pes_nfc_hal_irq_wake_cb(external_irq_callback_args_t *p_args)
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

/**
 * Map PTX SDK tech type + protocol to PES card type.
 */
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

/**
 * Map PTX protocol enum to PES protocol enum.
 */
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

/**
 * Determine the activation protocol for the first card in the registry.
 * Mirrors the SEL_RES/SENSB/SENSF inference logic from the old AUC
 * ptxAPP_DemoState_SelectCard().
 */
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

/**
 * Extract UID bytes from the activated card.
 */
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
        {
            /* PUPI = bytes 1..4 of SENSB_RES */
            (void)memcpy(uid, &card->TechParams.CardBParams.SENSB_RES[1], 4u);
            *uid_len = 4u;
            break;
        }
        case Tech_TypeF:
        {
            /* NFCID2 = bytes 2..9 of SENSF_RES */
            (void)memcpy(uid, &card->TechParams.CardFParams.SENSF_RES[2], 8u);
            *uid_len = 8u;
            break;
        }
        case Tech_TypeV:
        {
            /* UID stored LSB-first in the SDK, copy reversed to MSB-first */
            for (uint8_t i = 0; i < 8u; i++)
            {
                uid[i] = card->TechParams.CardVParams.UID[7u - i];
            }
            *uid_len = 8u;
            break;
        }
        default:
            break;
    }
}

/* ── HAL API implementation ───────────────────────────────────────── */

pes_status_t pes_nfc_hal_open(pes_nfc_reader_device_t device)
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

pes_status_t pes_nfc_hal_discover_start(pes_nfc_tech_mask_t tech_mask)
{
    PES_COMMON_UNUSED(tech_mask);
    /* Poll flags are configured in g_nfc_reader_ptx0_cfg at build time.
     * A future refinement could apply tech_mask dynamically. */
    fsp_err_t err = RM_NFC_READER_PTX_DiscoveryStart(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_hal_discover_status(pes_nfc_disc_status_t *out_status)
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

pes_status_t pes_nfc_hal_wait_for_card(uint32_t timeout_ms, pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }
    *out_status = PES_NFC_DISC_NO_CARD;

    /* Before the scheduler starts (e.g. cold-boot recovery paths) there is
     * no task context to notify — fall back to a short blocking status
     * read instead of installing the ISR wrapper. */
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        R_BSP_SoftwareDelay(timeout_ms, BSP_DELAY_UNITS_MILLISECONDS);
        return pes_nfc_hal_discover_status(out_status);
    }

    TickType_t remaining_ticks = pdMS_TO_TICKS(timeout_ms);
    TickType_t start_tick      = xTaskGetTickCount();

    g_waiting_task = xTaskGetCurrentTaskHandle();

    /* Install our lightweight wake-up ISR for the duration of the wait. */
    (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, pes_nfc_hal_irq_wake_cb, NULL, NULL);

    /* Clear any stale notification so we only react to fresh IRQs. */
    (void)ulTaskNotifyTake(pdTRUE, 0);

    pes_status_t st = PES_OK;

    for (;;)
    {
        (void)ulTaskNotifyTake(pdTRUE, remaining_ticks);

        /* Restore the SDK's normal ISR handler before touching the SPI/
         * status registers so RM_NFC_READER_PTX_StatusGet's internal
         * ptxPLAT_TriggerRx()/notification-processing behaves exactly as
         * it would under the original (polling) call pattern. */
        (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, ptxPLAT_GPIO_IsrCallback, NULL, NULL);

        st = pes_nfc_hal_discover_status(out_status);
        if (PES_OK != st) { break; }

        if (PES_NFC_DISC_NO_CARD != *out_status)
        {
            break;  /* card found, discovery done, or multi-card state */
        }

        /* Spurious wake (unrelated IRQ) or plain timeout: check remaining
         * time and, if any is left, resume waiting. */
        TickType_t elapsed = xTaskGetTickCount() - start_tick;
        if (elapsed >= remaining_ticks) { break; }
        remaining_ticks = pdMS_TO_TICKS(timeout_ms) - elapsed;

        /* Re-arm our wake-up ISR for the next wait iteration. */
        g_waiting_task = xTaskGetCurrentTaskHandle();
        (void)g_ext_irq.p_api->callbackSet(g_ext_irq.p_ctrl, pes_nfc_hal_irq_wake_cb, NULL, NULL);
    }

    g_waiting_task = NULL;
    return st;
}


pes_status_t pes_nfc_hal_card_activate(pes_nfc_hal_card_info_t *card_info)
{
    if (NULL == card_info) { return PES_ERR_INVALID_CFG; }
    (void)memset(card_info, 0, sizeof(*card_info));

    ptxIoTRd_CardRegistry_t *reg = NULL;
    fsp_err_t err = RM_NFC_READER_PTX_CardRegistryGet(&g_nfc_reader_ptx0_ctrl, &reg);
    if ((FSP_SUCCESS != err) || (NULL == reg)) { return PES_ERR_INTERNAL; }

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

pes_status_t pes_nfc_hal_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                       uint8_t *rx, uint32_t *rx_len)
{
    if ((NULL == tx) || (NULL == rx) || (NULL == rx_len)) { return PES_ERR_INVALID_CFG; }

    nfc_reader_ptx_data_info_t info;
    info.p_tx_buf  = (uint8_t *)(uintptr_t)tx;   /* FSP API takes non-const */
    info.tx_length = tx_len;
    info.p_rx_buf  = rx;
    info.rx_length = *rx_len;

    fsp_err_t err = RM_NFC_READER_PTX_DataExchange(&g_nfc_reader_ptx0_ctrl, &info);
    *rx_len = info.rx_length;

    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_hal_deactivate(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_ReaderDeactivation(&g_nfc_reader_ptx0_ctrl,
                                                          NFC_READER_PTX_RETURN_DISCOVER);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_hal_system_check(void)
{
    uint8_t state = 0;
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_System, &state);
    if (FSP_SUCCESS != err) { return PES_ERR_INTERNAL; }
    return (PTX_SYSTEM_STATUS_OK == state) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_hal_get_system_state(uint8_t *out_state)
{
    if (NULL == out_state) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_System, out_state);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

pes_status_t pes_nfc_hal_get_last_rf_error(uint8_t *out_err)
{
    if (NULL == out_err) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_LastRFError, out_err);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

void pes_nfc_hal_wake_waiting_task(void)
{
    TaskHandle_t task = g_waiting_task;
    if (NULL != task)
    {
        (void)xTaskNotifyGive(task);
    }
}

pes_status_t pes_nfc_hal_close(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_Close(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

void pes_nfc_hal_sleep_ms(uint32_t ms)
{
    /* Yield the CPU to other FreeRTOS tasks instead of busy-waiting.
     * Falls back to BSP delay if called before the scheduler is running
     * (e.g. during pes_nfc_hal_open cold-boot recovery). */
    if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        R_BSP_SoftwareDelay(ms, BSP_DELAY_UNITS_MILLISECONDS);
    }
}

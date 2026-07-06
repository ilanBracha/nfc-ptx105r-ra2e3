/**
 * pes_nfc_hal_ptx105r.c
 *
 * HAL implementation for the Renesas PTX105R NFC reader, using the
 * RM_NFC_READER_PTX FSP API surface exclusively.
 *
 * Every function is static; the only exported symbol is the const
 * vtable instance g_pes_nfc_hal_ptx105r.
 */

#include "pes_nfc_hal.h"
#include "hal_data.h"          /* g_nfc_reader_ptx0_ctrl/cfg, FSP types */
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"

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

/* ── Internal helpers (not exposed through the vtable) ─────────────── */

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

/* ── vtable function implementations ──────────────────────────────── */

static pes_status_t ptx105r_open(pes_nfc_reader_device_t device)
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

static pes_status_t ptx105r_close(void)
{
    g_active_reg = NULL;
    fsp_err_t err = RM_NFC_READER_PTX_Close(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static pes_status_t ptx105r_configure_polling(pes_nfc_tech_mask_t tech_mask)
{
    /* Poll flags are configured in g_nfc_reader_ptx0_cfg at build time.
     * A future refinement could apply tech_mask dynamically here. */
    PES_COMMON_UNUSED(tech_mask);
    return PES_OK;
}

static pes_status_t ptx105r_start_polling(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_DiscoveryStart(&g_nfc_reader_ptx0_ctrl);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static pes_status_t ptx105r_stop_polling(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_ReaderDeactivation(&g_nfc_reader_ptx0_ctrl,
                                                          NFC_READER_PTX_RETURN_IDLE);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static pes_status_t ptx105r_wait_for_card(uint32_t timeout_ms,
                                           pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }
    *out_status = PES_NFC_DISC_NO_CARD;

    /* Folded system-health check (was separate pes_nfc_hal_system_check). */
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

static pes_status_t ptx105r_activate_card(pes_nfc_hal_card_info_t *card_info)
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

static pes_status_t ptx105r_get_card_type(pes_nfc_card_type_t *out_type)
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

static pes_status_t ptx105r_get_uid(uint8_t *uid, uint8_t *uid_len)
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

/* ── NDEF helpers (moved from pes_ndef_read.c) ─────────────────────── */

#define NDEF_RX_BUF  PES_NFC_HAL_RX_BUF_SIZE

static pes_status_t ptx105r_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                           uint8_t *rx, uint32_t *rx_len);

static pes_status_t t4t_exchange_ok(const uint8_t *cmd, uint32_t cmd_len,
                                     uint8_t *rx, uint32_t *rx_len)
{
    *rx_len = NDEF_RX_BUF;
    pes_status_t st = ptx105r_data_exchange(cmd, cmd_len, rx, rx_len);
    if (PES_OK != st)          { return st; }
    if (*rx_len < 2u)          { return PES_ERR_INTERNAL; }
    if ((0x90u != rx[*rx_len - 2u]) || (0x00u != rx[*rx_len - 1u]))
    {
        return PES_ERR_INTERNAL;
    }
    return PES_OK;
}

static pes_status_t ptx105r_ndef_probe(bool *out_supported)
{
    if (NULL == out_supported) { return PES_ERR_INVALID_CFG; }
    *out_supported = false;

    if (NULL == g_active_reg || NULL == g_active_reg->ActiveCard)
    {
        return PES_ERR_NOT_FOUND;
    }

    pes_nfc_card_type_t ct = map_card_type(g_active_reg->ActiveCard,
                                            g_active_reg->ActiveCardProtType);
    switch (ct)
    {
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2:
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
            *out_supported = true;
            break;
        default:
            break;
    }
    return PES_OK;
}

static pes_status_t ptx105r_ndef_read(uint8_t *ndef_data, uint16_t max_bytes,
                                       uint16_t *ndef_len)
{
    if ((NULL == ndef_data) || (NULL == ndef_len)) { return PES_ERR_INVALID_CFG; }
    *ndef_len = 0;

    if (NULL == g_active_reg || NULL == g_active_reg->ActiveCard)
    {
        return PES_ERR_NOT_FOUND;
    }

    pes_nfc_card_type_t ct = map_card_type(g_active_reg->ActiveCard,
                                            g_active_reg->ActiveCardProtType);

    /* ── Type 4 Tag NDEF read ──────────────────────────────────────── */
    if ((PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A == ct) ||
        (PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B == ct))
    {
        uint8_t rx[NDEF_RX_BUF];
        uint32_t rx_len;
        uint8_t cmd[16];

        /* SELECT NDEF Tag Application */
        static const uint8_t sel_app[] = {0x00,0xA4,0x04,0x00,0x07,
                                          0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00};
        if (PES_OK != t4t_exchange_ok(sel_app, sizeof(sel_app), rx, &rx_len))
            return PES_ERR_NOT_FOUND;

        /* SELECT Capability Container */
        static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
        if (PES_OK != t4t_exchange_ok(sel_cc, sizeof(sel_cc), rx, &rx_len))
            return PES_ERR_NOT_FOUND;

        /* READ CC */
        static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
        if (PES_OK != t4t_exchange_ok(read_cc, sizeof(read_cc), rx, &rx_len))
            return PES_ERR_NOT_FOUND;
        if (rx_len < 17u) return PES_ERR_NOT_FOUND;

        uint16_t mle    = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);
        uint8_t fid_hi  = rx[9];
        uint8_t fid_lo  = rx[10];

        /* SELECT NDEF file */
        cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C;
        cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
        if (PES_OK != t4t_exchange_ok(cmd, 7u, rx, &rx_len))
            return PES_ERR_NOT_FOUND;

        /* READ NLEN */
        cmd[0]=0x00; cmd[1]=0xB0; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
        if (PES_OK != t4t_exchange_ok(cmd, 5u, rx, &rx_len))
            return PES_ERR_NOT_FOUND;
        if (rx_len < 4u) return PES_ERR_NOT_FOUND;

        uint16_t nlen = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
        if (0u == nlen) { *ndef_len = 0; return PES_OK; }

        /* READ NDEF body in chunks */
        uint32_t chunk = ((0u == mle) || (mle > 0xFFu)) ? 0xFFu : (uint32_t)mle;
        uint32_t total = (nlen > max_bytes) ? max_bytes : (uint32_t)nlen;
        uint32_t got = 0;
        uint16_t offset = 2u;

        while (got < total)
        {
            uint32_t want = total - got;
            if (want > chunk) want = chunk;

            cmd[0]=0x00; cmd[1]=0xB0;
            cmd[2]=(uint8_t)(offset >> 8);
            cmd[3]=(uint8_t)(offset & 0xFFu);
            cmd[4]=(uint8_t)want;

            if (PES_OK != t4t_exchange_ok(cmd, 5u, rx, &rx_len)) break;

            uint32_t data = rx_len - 2u;
            if (data > want) data = want;
            if (0u == data) break;

            (void)memcpy(&ndef_data[got], rx, data);
            got    += data;
            offset  = (uint16_t)(offset + data);
        }

        *ndef_len = (uint16_t)got;
        return PES_OK;
    }

    /* ── Type 2 Tag NDEF read ──────────────────────────────────────── */
    if (PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2 == ct)
    {
        uint8_t rx[NDEF_RX_BUF];
        uint32_t rx_len;
        uint8_t cmd[2];
        uint8_t data_buf[PES_NFC_NDEF_MAX_BYTES];

        /* READ block 3 -> CC */
        cmd[0] = 0x30; cmd[1] = 0x03;
        rx_len = NDEF_RX_BUF;
        if (PES_OK != ptx105r_data_exchange(cmd, 2u, rx, &rx_len)) return PES_ERR_NOT_FOUND;
        if ((rx_len < 4u) || (0xE1u != rx[0])) return PES_ERR_NOT_FOUND;

        uint32_t data_area = (uint32_t)rx[2] * 8u;
        uint32_t cap = (data_area > PES_NFC_NDEF_MAX_BYTES) ? PES_NFC_NDEF_MAX_BYTES : data_area;
        if (0u == cap) cap = PES_NFC_NDEF_MAX_BYTES;

        /* Read data area starting at block 4 */
        uint32_t got = 0;
        uint8_t block = 4u;
        while (got < cap)
        {
            cmd[0] = 0x30; cmd[1] = block;
            rx_len = NDEF_RX_BUF;
            if (PES_OK != ptx105r_data_exchange(cmd, 2u, rx, &rx_len)) break;
            if (rx_len < 4u) break;

            uint32_t take = (rx_len < 16u) ? rx_len : 16u;
            if ((got + take) > cap) take = cap - got;
            (void)memcpy(&data_buf[got], rx, take);
            got += take;
            if ((uint32_t)block + 4u > 0xFFu) break;
            block = (uint8_t)(block + 4u);
        }

        /* Walk TLV area for NDEF Message TLV (0x03) */
        uint32_t p = 0;
        while (p < got)
        {
            uint8_t t = data_buf[p++];
            if (0x00u == t) continue;
            if (0xFEu == t) break;
            if (p >= got) break;

            uint32_t l = data_buf[p++];
            if (0xFFu == l)
            {
                if ((p + 2u) > got) break;
                l = ((uint32_t)data_buf[p] << 8) | data_buf[p + 1u];
                p += 2u;
            }

            if (0x03u == t)
            {
                if ((p + l) > got) l = got - p;
                uint16_t copy = (l > max_bytes) ? max_bytes : (uint16_t)l;
                (void)memcpy(ndef_data, &data_buf[p], copy);
                *ndef_len = copy;
                return PES_OK;
            }
            p += l;
        }

        /* NDEF-formatted but no NDEF TLV found */
        *ndef_len = 0;
        return PES_OK;
    }

    return PES_ERR_NOT_FOUND;
}

/* ── Remaining vtable members ─────────────────────────────────────── */

static void ptx105r_sleep(uint32_t ms)
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

static pes_status_t ptx105r_data_exchange(const uint8_t *tx, uint32_t tx_len,
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

static pes_status_t ptx105r_deactivate(void)
{
    fsp_err_t err = RM_NFC_READER_PTX_ReaderDeactivation(&g_nfc_reader_ptx0_ctrl,
                                                          NFC_READER_PTX_RETURN_DISCOVER);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static pes_status_t ptx105r_get_system_state(uint8_t *out_state)
{
    if (NULL == out_state) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_System, out_state);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static pes_status_t ptx105r_get_last_rf_error(uint8_t *out_err)
{
    if (NULL == out_err) { return PES_ERR_INVALID_CFG; }
    fsp_err_t err = RM_NFC_READER_PTX_StatusGet(&g_nfc_reader_ptx0_ctrl,
                                                 StatusType_LastRFError, out_err);
    return (FSP_SUCCESS == err) ? PES_OK : PES_ERR_INTERNAL;
}

static void ptx105r_wake_waiting_task(void)
{
    TaskHandle_t task = g_waiting_task;
    if (NULL != task)
    {
        (void)xTaskNotifyGive(task);
    }
}

/* ── Const vtable instance (static initialisation, no heap) ────────── */

const pes_nfc_hal_api_t g_pes_nfc_hal_ptx105r = {
    .open              = ptx105r_open,
    .close             = ptx105r_close,
    .configure_polling = ptx105r_configure_polling,
    .start_polling     = ptx105r_start_polling,
    .stop_polling      = ptx105r_stop_polling,
    .wait_for_card     = ptx105r_wait_for_card,
    .activate_card     = ptx105r_activate_card,
    .get_card_type     = ptx105r_get_card_type,
    .get_uid           = ptx105r_get_uid,
    .ndef_probe        = ptx105r_ndef_probe,
    .ndef_read         = ptx105r_ndef_read,
    .sleep             = ptx105r_sleep,
    .data_exchange     = ptx105r_data_exchange,
    .deactivate        = ptx105r_deactivate,
    .get_system_state  = ptx105r_get_system_state,
    .get_last_rf_error = ptx105r_get_last_rf_error,
    .wake_waiting_task = ptx105r_wake_waiting_task,
};

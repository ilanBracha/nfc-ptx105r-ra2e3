/**
 * pes_nfc_card_reader.c
 *
 * PES NFC Card Reader — main orchestrator.
 *
 * Owns the full FSP-driven discovery/activation/read state machine. The
 * application interacts exclusively through the PES public API:
 *
 *   - PES_NFCCardReader_Read()         : run the polling loop
 *   - PES_NFCCardReader_DataExchange() : raw exchange with the active card
 *
 * Two operating modes are selected by the caller via the cfg:
 *
 *   1. Event-loop mode (cfg->on_card_event != NULL):
 *        runs a continuous detect -> activate -> NDEF read -> notify ->
 *        deactivate -> restart-discovery loop for cfg->timeout_ms.
 *        timeout_ms == UINT32_MAX => loop forever (never returns).
 *
 *   2. Single-shot mode (cfg->on_card_event == NULL):
 *        legacy behavior: detect/activate one card, fill result_out, return.
 */

#include "pes_nfc_card_reader.h"
#include "pes_nfc_hal.h"
#include "pes_nfc_card_reader_deps.h"
#include "pes_nfc_internal.h"
#include <string.h>
#include <stdint.h>

/* ── Constants ─────────────────────────────────────────────────────── */
#define DEFAULT_TIMEOUT_MS       5000U
#define DEFAULT_RETRY_COUNT      0U
#define LOOP_TICK_MS             5U       /* discovery poll cadence */
#define SUMMARY_BUF_SIZE         128U

/* PTX system/RF status raw codes the orchestrator differentiates.
 * Mirrors values exposed by RM_NFC_READER_PTX_StatusGet(). */
#define PTX_SYS_STATUS_OK                       0x00u
#define PTX_RF_ERR_WARNING_PA_OVERCURRENT_LIMIT 0x06u

/* ── Per-call orchestrator state ──────────────────────────────────── */
typedef enum {
    LOOP_WAIT_FOR_ACTIVATION = 0,
    LOOP_DATA_EVENT,
    LOOP_DEACTIVATE,
    LOOP_SYSTEM_ERROR,
} loop_state_t;

/* ── Single-attempt implementation (used by retry wrapper & single-shot) ── */
pes_status_t pes_nfc_card_reader_try_once(const void *cfg_raw, void *result_raw)
{
    const pes_nfc_card_reader_cfg_t *cfg = (const pes_nfc_card_reader_cfg_t *)cfg_raw;
    pes_nfc_card_result_t           *res = (pes_nfc_card_result_t *)result_raw;
    pes_status_t st;

    uint32_t timeout = (NULL != cfg) ? cfg->timeout_ms : DEFAULT_TIMEOUT_MS;

    /* 1. Wait for a card */
    pes_nfc_disc_status_t disc = PES_NFC_DISC_NO_CARD;
    st = pes_nfc_detect_poll(timeout, &disc);
    if (PES_OK != st) { return st; }

    /* 2. Activate the card */
    pes_nfc_hal_card_info_t card_info;
    st = pes_nfc_hal_card_activate(&card_info);
    if (PES_OK != st) { return st; }

    /* 3. Fill basic result fields */
    if (NULL != res)
    {
        res->card_type = card_info.card_type;
        res->protocol  = card_info.protocol;
        (void)memcpy(res->uid, card_info.uid, card_info.uid_len);
        res->uid_len      = card_info.uid_len;
        res->ndef_present = false;
        res->ndef_len     = 0;
        res->rssi_dbm     = 0;
        res->read_time_ms = 0;

        /* 4. Optionally read NDEF */
        bool want_ndef = (NULL != cfg) ? cfg->read_ndef : true;
        if (want_ndef)
        {
            switch (card_info.card_type)
            {
                case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:
                case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
                    (void)pes_ndef_read_t4t(res);
                    break;

                case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2:
                    (void)pes_ndef_read_t2t(res);
                    break;

                default:
                    /* NDEF reading not supported for this card type yet */
                    break;
            }
        }
    }

    return PES_OK;
}

/* ── Event-loop mode ──────────────────────────────────────────────── */
static pes_status_t run_event_loop(const pes_nfc_card_reader_cfg_t *cfg,
                                   pes_nfc_card_result_t *result_out)
{
    pes_nfc_card_result_t   local_res;
    pes_nfc_card_result_t  *res = (NULL != result_out) ? result_out : &local_res;
    char                    summary[SUMMARY_BUF_SIZE];
    loop_state_t            state         = LOOP_WAIT_FOR_ACTIVATION;
    uint32_t                elapsed_ms    = 0u;
    const bool              loop_forever  = (UINT32_MAX == cfg->timeout_ms);

    while (loop_forever || (elapsed_ms < cfg->timeout_ms))
    {
        /* Critical system-error watchdog */
        uint8_t sys_state = PTX_SYS_STATUS_OK;
        if (PES_OK == pes_nfc_hal_get_system_state(&sys_state))
        {
            if (PTX_SYS_STATUS_OK != sys_state) { state = LOOP_SYSTEM_ERROR; }
        }

        /* PA overcurrent / other RF warning notifications (informational only) */
        uint8_t last_rf_err = 0u;
        (void)pes_nfc_hal_get_last_rf_error(&last_rf_err);
        if ((PTX_RF_ERR_WARNING_PA_OVERCURRENT_LIMIT == last_rf_err) &&
            (NULL != cfg->on_card_event))
        {
            cfg->on_card_event(PES_OK, NULL,
                               "WARN: PA overcurrent limiter activated",
                               cfg->p_card_event_context);
        }

        switch (state)
        {
            case LOOP_WAIT_FOR_ACTIVATION:
            {
                pes_nfc_disc_status_t disc = PES_NFC_DISC_NO_CARD;
                if (PES_OK != pes_nfc_hal_discover_status(&disc))
                {
                    state = LOOP_DEACTIVATE;
                    break;
                }
                if ((PES_NFC_DISC_CARD_ACTIVE == disc) ||
                    (PES_NFC_DISC_DONE        == disc))
                {
                    state = LOOP_DATA_EVENT;
                }
                break;
            }

            case LOOP_DATA_EVENT:
            {
                pes_nfc_hal_card_info_t info;
                pes_status_t st = pes_nfc_hal_card_activate(&info);
                if (PES_OK == st)
                {
                    (void)memset(res, 0, sizeof(*res));
                    res->card_type = info.card_type;
                    res->protocol  = info.protocol;
                    (void)memcpy(res->uid, info.uid, info.uid_len);
                    res->uid_len = info.uid_len;

                    /* Optionally read NDEF */
                    if (cfg->read_ndef)
                    {
                        switch (info.card_type)
                        {
                            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:
                            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
                                (void)pes_ndef_read_t4t(res);
                                break;
                            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2:
                                (void)pes_ndef_read_t2t(res);
                                break;
                            default:
                                break;
                        }
                    }

                    /* Build summary string and fire the per-card event */
                    (void)pes_card_summary_build(res, summary, sizeof(summary));
                    if (NULL != cfg->on_card_event)
                    {
                        cfg->on_card_event(PES_OK, res, summary,
                                           cfg->p_card_event_context);
                    }
                }
                else
                {
                    if (NULL != cfg->on_card_event)
                    {
                        cfg->on_card_event(st, NULL, "card activate failed",
                                           cfg->p_card_event_context);
                    }
                }
                state = LOOP_DEACTIVATE;
                break;
            }

            case LOOP_DEACTIVATE:
            {
                (void)pes_nfc_hal_deactivate();    /* restart discovery */
                state = LOOP_WAIT_FOR_ACTIVATION;
                break;
            }

            case LOOP_SYSTEM_ERROR:
            {
                if (NULL != cfg->on_card_event)
                {
                    cfg->on_card_event(PES_ERR_INTERNAL, NULL,
                       "ERROR: critical system-error (overcurrent/temperature)",
                                       cfg->p_card_event_context);
                }
                return PES_ERR_INTERNAL;
            }

            default:
                break;
        }

        pes_nfc_hal_sleep_ms(LOOP_TICK_MS);
        if (!loop_forever) { elapsed_ms += LOOP_TICK_MS; }
    }

    return PES_OK; /* timed out without a fatal error */
}

/* ── Public API ────────────────────────────────────────────────────── */
pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t *cfg,
                                    pes_nfc_card_result_t *result_out)
{
    pes_status_t st;

    if (NULL == cfg) { return PES_ERR_INVALID_CFG; }

    /* Non-blocking (operation-end callback) path not yet supported. */
    if (NULL != cfg->callback)
    {
        return PES_ERR_INVALID_CFG;
    }

    if (cfg->validate_dependencies)
    {
        st = pes_nfc_card_reader_validate_deps();
        if (PES_OK != st) { return st; }
    }

    if (NULL != result_out)
    {
        (void)memset(result_out, 0, sizeof(*result_out));
    }

    st = pes_nfc_hal_open(cfg->reader);
    if (PES_OK != st) { return st; }

    st = pes_nfc_hal_discover_start(cfg->tech_mask);
    if (PES_OK != st)
    {
        (void)pes_nfc_hal_close();
        return st;
    }

    /* Mode selection: event-loop if a per-card callback was supplied,
     * single-shot/retry otherwise. */
    if (NULL != cfg->on_card_event)
    {
        st = run_event_loop(cfg, result_out);
    }
    else if (cfg->retry_count > 0u)
    {
        st = pes_nfc_retry(cfg, result_out, cfg->retry_count);
    }
    else
    {
        st = pes_nfc_card_reader_try_once(cfg, result_out);
    }

    (void)pes_nfc_hal_deactivate();
    (void)pes_nfc_hal_close();
    return st;
}

pes_status_t PES_NFCCardReader_DataExchange(const uint8_t *tx, uint32_t tx_len,
                                            uint8_t *rx, uint32_t *rx_len)
{
    return pes_nfc_hal_data_exchange(tx, tx_len, rx, rx_len);
}

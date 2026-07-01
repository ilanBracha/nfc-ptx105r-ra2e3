/**
 * pes_nfc_card_reader.c
 *
 * PES NFC Card Reader — main orchestrator (blocking + non-blocking).
 *
 * Owns the full FSP-driven discovery/activation/read state machine. The
 * application interacts exclusively through the PES public API:
 *
 *   - PES_NFCCardReader_Read()         : run the interrupt-driven detect/read loop
 *   - PES_NFCCardReader_DataExchange() : raw exchange with the active card
 *   - PES_NFCCardReader_Stop()         : request graceful stop
 *
 * Operating modes (selected via cfg fields):
 *
 *   1. Blocking event-loop (callback==NULL, on_card_event!=NULL)
 *   2. Blocking single-shot (callback==NULL, on_card_event==NULL)
 *   3. Non-blocking (callback!=NULL): spawns a static FreeRTOS task that
 *      runs mode 1 or 2 internally. Read() returns PES_OK immediately.
 */

#include "pes_nfc_card_reader.h"
#include "pes_nfc_hal.h"
#include "pes_nfc_card_reader_deps.h"
#include "pes_nfc_internal.h"
#include <string.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* ── Constants ─────────────────────────────────────────────────────── */
#define DEFAULT_TIMEOUT_MS       5000U
#define DEFAULT_RETRY_COUNT      0U
#define SUMMARY_BUF_SIZE         128U

/* Upper bound on a single interrupt-wait inside run_event_loop().
 * After each wait we re-check system-error and RF-warning state.
 * Stop() wakes the task immediately via pes_nfc_hal_wake_waiting_task(),
 * so this only bounds how often non-IRQ health checks run. */
#define EVENT_LOOP_WAIT_CHUNK_MS 500U

/* Async worker task configuration (static allocation — no heap) */
#define ASYNC_TASK_STACK_WORDS   (4096U / sizeof(StackType_t))
#define ASYNC_TASK_PRIORITY      1U
#define ASYNC_TASK_NAME          "PES_NFC"

/* PTX system/RF status raw codes */
#define PTX_SYS_STATUS_OK                       0x00u
#define PTX_RF_ERR_WARNING_PA_OVERCURRENT_LIMIT 0x06u

/* ── Per-call orchestrator state ──────────────────────────────────── */
typedef enum {
    LOOP_WAIT_FOR_ACTIVATION = 0,
    LOOP_DATA_EVENT,
    LOOP_DEACTIVATE,
    LOOP_SYSTEM_ERROR,
} loop_state_t;

/* ── Stop-requested flag ──────────────────────────────────────────── */
static volatile bool g_stop_requested = false;

bool pes_nfc_card_reader_is_stop_requested(void)
{
    return g_stop_requested;
}

/* ── Non-blocking async context (static allocation) ───────────────── */
typedef struct {
    pes_nfc_card_reader_cfg_t  cfg;          /* deep copy of caller cfg  */
    pes_nfc_card_result_t     *p_result_out; /* caller's result pointer  */
    volatile bool              active;       /* re-entrancy guard        */
} pes_nfc_async_ctx_t;

static pes_nfc_async_ctx_t  g_async_ctx;
static StaticTask_t         g_async_task_tcb;
static StackType_t          g_async_task_stack[ASYNC_TASK_STACK_WORDS];
static TaskHandle_t         g_async_task_handle = NULL;

/* ── Forward declarations ─────────────────────────────────────────── */
static pes_status_t pes_nfc_read_blocking(const pes_nfc_card_reader_cfg_t *cfg,
                                           pes_nfc_card_result_t *result_out);
static pes_status_t run_event_loop(const pes_nfc_card_reader_cfg_t *cfg,
                                   pes_nfc_card_result_t *result_out);

/* ── Single-attempt (used by retry wrapper & single-shot) ─────────── */
pes_status_t pes_nfc_card_reader_try_once(const void *cfg_raw, void *result_raw)
{
    const pes_nfc_card_reader_cfg_t *cfg = (const pes_nfc_card_reader_cfg_t *)cfg_raw;
    pes_nfc_card_result_t           *res = (pes_nfc_card_result_t *)result_raw;
    pes_status_t st;

    uint32_t timeout = (NULL != cfg) ? cfg->timeout_ms : DEFAULT_TIMEOUT_MS;

    /* 1. Wait for a card */
    pes_nfc_disc_status_t disc = PES_NFC_DISC_NO_CARD;
    st = pes_nfc_detect_wait(timeout, &disc);
    if (PES_OK != st) { return st; }

    /* If stop was requested during the wait, detect_wait returns PES_OK
     * with NO_CARD — treat as a clean exit. */
    if (g_stop_requested || (PES_NFC_DISC_NO_CARD == disc))
    { 
        return PES_OK;
    }

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
        /* Stop requested? Exit cleanly. */
        if (g_stop_requested) { return PES_OK; }

        /* Critical system-error watchdog */
        uint8_t sys_state = PTX_SYS_STATUS_OK;
        if (PES_OK == pes_nfc_hal_get_system_state(&sys_state))
        {
            if (PTX_SYS_STATUS_OK != sys_state) { state = LOOP_SYSTEM_ERROR; }
        }

        /* PA overcurrent / other RF warning notifications */
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
                /* Interrupt-driven wait: blocks (zero CPU) until the
                 * reader's IRQ line signals a discovery event or this
                 * chunk's wait elapses, whichever comes first. Bounded to
                 * EVENT_LOOP_WAIT_CHUNK_MS so the Stop()/system-error/RF-
                 * warning checks above stay responsive. */
                uint32_t remaining = loop_forever ? EVENT_LOOP_WAIT_CHUNK_MS
                                                   : (cfg->timeout_ms - elapsed_ms);
                uint32_t chunk = (remaining < EVENT_LOOP_WAIT_CHUNK_MS)
                                 ? remaining : EVENT_LOOP_WAIT_CHUNK_MS;

                pes_nfc_disc_status_t disc = PES_NFC_DISC_NO_CARD;
                if (PES_OK != pes_nfc_hal_wait_for_card(chunk, &disc))
                {
                    state = LOOP_DEACTIVATE;
                    break;
                }
                if ((PES_NFC_DISC_CARD_ACTIVE == disc) ||
                    (PES_NFC_DISC_DONE        == disc))
                {
                    state = LOOP_DATA_EVENT;
                }
                if (!loop_forever) { elapsed_ms += chunk; }
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

                    /* Build summary string and fire per-card event */
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
    }

    return PES_OK; /* timed out without a fatal error */
}

/* ── Blocking core (open → discover → loop/shot → cleanup) ────────── */
static pes_status_t pes_nfc_read_blocking(const pes_nfc_card_reader_cfg_t *cfg,
                                           pes_nfc_card_result_t *result_out)
{
    pes_status_t st;

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

    /* Mode selection */
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

/* ── Async worker task function ───────────────────────────────────── */
static void pes_nfc_async_worker(void *pvParameters)
{
    (void)pvParameters;
    pes_nfc_async_ctx_t *ctx = &g_async_ctx;

    /* Run full blocking flow inside this dedicated task. */
    pes_status_t st = pes_nfc_read_blocking(&ctx->cfg, ctx->p_result_out);

    /* Fire the operation-end callback. */
    if (NULL != ctx->cfg.callback)
    {
        ctx->cfg.callback(st, ctx->cfg.p_context);
    }

    /* Mark context inactive and self-delete. */
    ctx->active = false;
    g_async_task_handle = NULL;
    vTaskDelete(NULL);
}

/* ── Public API ────────────────────────────────────────────────────── */

pes_status_t PES_NFCCardReader_Validate(const pes_nfc_card_reader_cfg_t *cfg)
{
    if (NULL == cfg) { return PES_ERR_INVALID_CFG; }

    if ((int)cfg->reader != (int)PES_NFC_READER_PTX105R)
    {
        return PES_ERR_INVALID_CFG;
    }

    if (0u == cfg->tech_mask) { return PES_ERR_INVALID_CFG; }
    if (0u == cfg->timeout_ms) { return PES_ERR_INVALID_CFG; }

    if (cfg->read_ndef)
    {
        if ((0u == cfg->max_ndef_bytes) ||
            (cfg->max_ndef_bytes > PES_NFC_NDEF_MAX_BYTES))
        {
            return PES_ERR_INVALID_CFG;
        }
    }

    /* Non-blocking callback is now accepted (no longer rejected). */

    if (cfg->validate_dependencies)
    {
        pes_status_t dep_st = pes_nfc_card_reader_validate_deps();
        if (PES_OK != dep_st) { return dep_st; }
    }

    return PES_OK;
}

pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t *cfg,
                                    pes_nfc_card_result_t *result_out)
{
    pes_status_t st;

    /* Validate configuration */
    st = PES_NFCCardReader_Validate(cfg);
    if (PES_OK != st) { return st; }

    /* ── Non-blocking path ─────────────────────────────────────────── */
    if (NULL != cfg->callback)
    {
        /* Re-entrancy guard: only one async Read() at a time. */
        if (g_async_ctx.active) { return PES_ERR_INTERNAL; }

        /* Deep-copy config into static context. */
        (void)memcpy(&g_async_ctx.cfg, cfg, sizeof(*cfg));
        g_async_ctx.p_result_out = result_out;
        g_async_ctx.active       = true;
        g_stop_requested         = false;

        /* Create async worker (static allocation — no heap). */
        g_async_task_handle = xTaskCreateStatic(
            pes_nfc_async_worker,
            ASYNC_TASK_NAME,
            ASYNC_TASK_STACK_WORDS,
            NULL,
            ASYNC_TASK_PRIORITY,
            g_async_task_stack,
            &g_async_task_tcb
        );

        if (NULL == g_async_task_handle)
        {
            g_async_ctx.active = false;
            return PES_ERR_INTERNAL;
        }

        return PES_OK;  /* returns immediately */
    }

    /* ── Blocking path ─────────────────────────────────────────────── */
    g_stop_requested = false;
    return pes_nfc_read_blocking(cfg, result_out);
}

pes_status_t PES_NFCCardReader_Stop(void)
{
    g_stop_requested = true;
    /* Wake any task blocked in pes_nfc_hal_wait_for_card() so it can
     * observe the stop flag immediately instead of sleeping until the
     * next timeout expiry or IRQ event. */
    pes_nfc_hal_wake_waiting_task();
    return PES_OK;
}

pes_status_t PES_NFCCardReader_DataExchange(const uint8_t *tx, uint32_t tx_len,
                                            uint8_t *rx, uint32_t *rx_len)
{
    return pes_nfc_hal_data_exchange(tx, tx_len, rx, rx_len);
}
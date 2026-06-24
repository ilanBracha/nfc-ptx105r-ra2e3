/**
 * pes_nfc_card_reader.c
 *
 * PES NFC Card Reader — main orchestrator.
 *
 * Implements PES_NFCCardReader_Read() by coordinating the HAL, detection,
 * NDEF reading and retry sub-modules. The caller gets a clean, self-contained
 * "detect → activate → read → deactivate → close" sequence behind a single
 * blocking call (non-blocking TBD).
 */

#include "pes_nfc_card_reader.h"
#include "pes_nfc_hal.h"
#include "pes_nfc_card_reader_deps.h"
#include "pes_nfc_internal.h"
#include <string.h>

/* ── Default configuration values ─────────────────────────────────── */
#define DEFAULT_TIMEOUT_MS   5000U
#define DEFAULT_RETRY_COUNT  0U

/* ── Single-attempt implementation (called by retry wrapper) ──────── */

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
        (void)memcpy(res->uid, card_info.uid, card_info.uid_len);
        res->uid_len      = card_info.uid_len;
        res->ndef_present  = false;
        res->ndef_len      = 0;
        res->rssi_dbm      = 0;
        res->read_time_ms  = 0;

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

/* ── Public API ────────────────────────────────────────────────────── */

pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t *cfg,
                                    pes_nfc_card_result_t *result_out)
{
    pes_status_t st;

    /* --- Validate cfg --- */
    if (NULL == cfg) { return PES_ERR_INVALID_CFG; }

    /* --- Non-blocking not yet supported --- */
    if (NULL != cfg->callback)
    {
        /* TODO: implement non-blocking path */
        return PES_ERR_INVALID_CFG;
    }

    /* --- Optional dependency validation --- */
    if (cfg->validate_dependencies)
    {
        st = pes_nfc_card_reader_validate_deps();
        if (PES_OK != st) { return st; }
    }

    /* --- Initialize result --- */
    if (NULL != result_out)
    {
        (void)memset(result_out, 0, sizeof(*result_out));
    }

    /* --- Open the NFC reader --- */
    st = pes_nfc_hal_open(cfg->reader);
    if (PES_OK != st) { return st; }

    /* --- Start discovery --- */
    st = pes_nfc_hal_discover_start(cfg->tech_mask);
    if (PES_OK != st)
    {
        (void)pes_nfc_hal_close();
        return st;
    }

    /* --- Detect + activate + read (with retries) --- */
    uint8_t retries = cfg->retry_count;
    if (retries > 0u)
    {
        st = pes_nfc_retry(cfg, result_out, retries);
    }
    else
    {
        st = pes_nfc_card_reader_try_once(cfg, result_out);
    }

    /* --- Cleanup --- */
    (void)pes_nfc_hal_deactivate();
    (void)pes_nfc_hal_close();

    return st;
}
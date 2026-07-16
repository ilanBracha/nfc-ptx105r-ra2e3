/**
 * rs_nfc_detect.c
 *
 * Card detection: wait for a card-discovery event, the timeout to expire,
 * or a stop request. Interrupt-driven — no fixed-cadence status polling.
 * Uses only the rs_nfc_ptx API.
 */

#include "rs_nfc_ptx105r.h"

rs_status_t rs_nfc_detect_wait(uint32_t timeout_ms, rs_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return RS_ERR_INVALID_CFG; }

    /* Early exit on stop request */
    if (rs_nfc_reader_is_stop_requested())
    {
        *out_status = RS_NFC_DISC_NO_CARD;
        return RS_OK;
    }

    /* System-health check is now performed internally by
     * rs_nfc_ptx_wait_for_card. */

    /* Block (zero-CPU) until the reader's IRQ line signals an event or
     * the timeout elapses. If rs_nfc_reader_Stop() is called while
     * we are blocked, it sends a task notification to wake us
     * immediately so we can observe the stop flag. */
    rs_status_t st = rs_nfc_ptx_wait_for_card(timeout_ms, out_status);
    if (RS_OK != st) { return st; }

    if (RS_NFC_DISC_NO_CARD != *out_status)
    {
        return RS_OK;  /* card found or discovery done */
    }

    /* Check stop again — we may have been woken by Stop(). */
    if (rs_nfc_reader_is_stop_requested())
    {
        *out_status = RS_NFC_DISC_NO_CARD;
        return RS_OK;
    }

    *out_status = RS_NFC_DISC_NO_CARD;
    return RS_ERR_TIMEOUT;
}

/**
 * pes_nfc_detect.c
 *
 * Card detection: wait for a card-discovery event, the timeout to expire,
 * or a stop request. Interrupt-driven — no fixed-cadence status polling.
 * Uses only the pes_nfc_ptx API.
 */

#include "pes_nfc_ptx105r.h"

pes_status_t pes_nfc_detect_wait(uint32_t timeout_ms, pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }

    /* Early exit on stop request */
    if (pes_nfc_card_reader_is_stop_requested())
    {
        *out_status = PES_NFC_DISC_NO_CARD;
        return PES_OK;
    }

    /* System-health check is now performed internally by
     * pes_nfc_ptx_wait_for_card. */

    /* Block (zero-CPU) until the reader's IRQ line signals an event or
     * the timeout elapses. If PES_NFCCardReader_Stop() is called while
     * we are blocked, it sends a task notification to wake us
     * immediately so we can observe the stop flag. */
    pes_status_t st = pes_nfc_ptx_wait_for_card(timeout_ms, out_status);
    if (PES_OK != st) { return st; }

    if (PES_NFC_DISC_NO_CARD != *out_status)
    {
        return PES_OK;  /* card found or discovery done */
    }

    /* Check stop again — we may have been woken by Stop(). */
    if (pes_nfc_card_reader_is_stop_requested())
    {
        *out_status = PES_NFC_DISC_NO_CARD;
        return PES_OK;
    }

    *out_status = PES_NFC_DISC_NO_CARD;
    return PES_ERR_TIMEOUT;
}

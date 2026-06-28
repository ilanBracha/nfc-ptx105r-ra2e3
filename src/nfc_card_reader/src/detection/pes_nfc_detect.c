/**
 * pes_nfc_detect.c
 *
 * Card detection: poll discovery status in a loop until a card is found,
 * the timeout expires, or a stop is requested. Uses only the pes_nfc_hal API.
 */

#include "pes_nfc_hal.h"
#include "pes_nfc_internal.h"

#define POLL_INTERVAL_MS  5U

pes_status_t pes_nfc_detect_poll(uint32_t timeout_ms, pes_nfc_disc_status_t *out_status)
{
    if (NULL == out_status) { return PES_ERR_INVALID_CFG; }

    uint32_t elapsed = 0;

    while (elapsed < timeout_ms)
    {
        /* Early exit on stop request (non-blocking Stop() support) */
        if (pes_nfc_card_reader_is_stop_requested())
        {
            *out_status = PES_NFC_DISC_NO_CARD;
            return PES_OK;
        }

        /* Check system health first */
        pes_status_t sys = pes_nfc_hal_system_check();
        if (PES_OK != sys) { return sys; }

        pes_status_t st = pes_nfc_hal_discover_status(out_status);
        if (PES_OK != st) { return st; }

        if (PES_NFC_DISC_NO_CARD != *out_status)
        {
            return PES_OK;  /* card found or discovery done */
        }

        pes_nfc_hal_sleep_ms(POLL_INTERVAL_MS);
        elapsed += POLL_INTERVAL_MS;
    }

    *out_status = PES_NFC_DISC_NO_CARD;
    return PES_ERR_TIMEOUT;
}

/**
 * pes_nfc_detect.c
 *
 * Card detection: wait for a card-discovery event, the timeout to expire,
 * or a stop request. Interrupt-driven — no fixed-cadence status polling.
 * Uses only the pes_nfc_hal API.
 */

#include "pes_nfc_hal.h"
#include "pes_nfc_internal.h"

/* Upper bound on a single interrupt-wait chunk. Bounding the wait (instead
 * of blocking for the full timeout in one call) keeps PES_NFCCardReader_
 * Stop() and the system-health check responsive without reintroducing
 * fine-grained SPI-status polling (this is a coarse safety net, not a
 * poll cadence — the IRQ still wakes us immediately on a real event). */
#define STOP_CHECK_INTERVAL_MS  200U

pes_status_t pes_nfc_detect_wait(uint32_t timeout_ms, pes_nfc_disc_status_t *out_status)
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

        /* Block (zero-CPU) until the reader's IRQ line signals an event,
         * this chunk's wait elapses, or the overall timeout is reached —
         * whichever comes first. */
        uint32_t remaining = timeout_ms - elapsed;
        uint32_t chunk = (remaining < STOP_CHECK_INTERVAL_MS) ? remaining : STOP_CHECK_INTERVAL_MS;

        pes_status_t st = pes_nfc_hal_wait_for_card(chunk, out_status);
        if (PES_OK != st) { return st; }

        if (PES_NFC_DISC_NO_CARD != *out_status)
        {
            return PES_OK;  /* card found or discovery done */
        }

        elapsed += chunk;
    }

    *out_status = PES_NFC_DISC_NO_CARD;
    return PES_ERR_TIMEOUT;
}

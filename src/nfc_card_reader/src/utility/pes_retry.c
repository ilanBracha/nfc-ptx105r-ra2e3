/**
 * pes_retry.c
 *
 * Retry wrapper for the PES NFC Card Reader.
 */

#include "pes_common.h"
#include "pes_nfc_ptx.h"
#include "pes_nfc_internal.h"

pes_status_t pes_nfc_retry(const void *cfg, void *result_out, uint8_t max_retries)
{
    pes_status_t st = PES_ERR_TIMEOUT;

    for (uint8_t attempt = 0; attempt <= max_retries; attempt++)
    {
        st = pes_nfc_card_reader_try_once(cfg, result_out);
        if (PES_OK == st) { break; }

        /* Deactivate + re-discover between retries */
        (void)pes_nfc_ptx_deactivate();
    }

    return st;
}

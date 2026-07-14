/**
 * rs_retry.c
 *
 * Retry wrapper for the RS NFC Reader.
 */

#include "rs_nfc_ptx105r.h"

rs_status_t rs_nfc_retry(const void *cfg, void *result_out, uint8_t max_retries)
{
    rs_status_t st = RS_ERR_TIMEOUT;

    for (uint8_t attempt = 0; attempt <= max_retries; attempt++)
    {
        st = rs_nfc_reader_try_once(cfg, result_out);
        if (RS_OK == st) { break; }

        /* Deactivate + re-discover between retries */
        (void)rs_nfc_ptx_deactivate();
    }

    return st;
}

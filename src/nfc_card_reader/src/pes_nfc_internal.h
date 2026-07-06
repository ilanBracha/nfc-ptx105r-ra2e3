/**
 * pes_nfc_internal.h
 *
 * Internal forward declarations shared between the PES NFC Card Reader
 * sub-modules.  NOT part of the public API.
 */

#ifndef PES_NFC_INTERNAL_H
#define PES_NFC_INTERNAL_H

#include "pes_nfc_hal.h"            /* pes_nfc_disc_status_t, etc. */
#include "pes_nfc_card_reader.h"    /* pes_nfc_card_result_t, etc. */

#ifdef __cplusplus
extern "C" {
#endif

/* detection/pes_nfc_detect.c */
pes_status_t pes_nfc_detect_wait(uint32_t timeout_ms,
                                 pes_nfc_disc_status_t *out_status);

/* utility/pes_retry.c */
pes_status_t pes_nfc_retry(const void *cfg, void *result_out,
                           uint8_t max_retries);

/* utility/pes_timeout.c */
void pes_timeout_sleep_ms(uint32_t ms);

/* pes_nfc_card_reader.c  (used by retry) */
pes_status_t pes_nfc_card_reader_try_once(const void *cfg, void *result_out);

/* pes_nfc_card_reader.c — stop flag (checked by detect_wait & event loop) */
bool pes_nfc_card_reader_is_stop_requested(void);

/* ndef/pes_card_summary.c — Builds a one-line human-readable summary of
 * the activated card into a caller-supplied buffer (no printing). Returns
 * the number of bytes written (excluding the trailing NUL). */
uint32_t pes_card_summary_build(const pes_nfc_card_result_t *res,
                                char *buf, uint32_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_INTERNAL_H */

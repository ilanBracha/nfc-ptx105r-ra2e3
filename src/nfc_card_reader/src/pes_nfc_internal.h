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
pes_status_t pes_nfc_detect_poll(uint32_t timeout_ms,
                                 pes_nfc_disc_status_t *out_status);

/* ndef/pes_ndef_read.c */
pes_status_t pes_ndef_read_t4t(pes_nfc_card_result_t *result_out);
pes_status_t pes_ndef_read_t2t(pes_nfc_card_result_t *result_out);

/* utility/pes_retry.c */
pes_status_t pes_nfc_retry(const void *cfg, void *result_out,
                           uint8_t max_retries);

/* utility/pes_timeout.c */
void pes_timeout_sleep_ms(uint32_t ms);

/* pes_nfc_card_reader.c  (used by retry) */
pes_status_t pes_nfc_card_reader_try_once(const void *cfg, void *result_out);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_INTERNAL_H */

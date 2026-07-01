/**
 * pes_ndef_read.c
 *
 * NDEF message reading — thin wrappers around the HAL vtable's
 * ndef_probe / ndef_read function pointers.  All protocol-level
 * APDU / TLV logic lives in the HAL implementation
 * (pes_nfc_hal_ptx105r.c).
 */

#include "pes_nfc_hal.h"
#include "pes_nfc_internal.h"

/**
 * Read NDEF from an NFC Forum Type 4 Tag.
 */
pes_status_t pes_ndef_read_t4t(pes_nfc_card_result_t *result_out)
{
    if (NULL == result_out) { return PES_ERR_INVALID_CFG; }

    pes_status_t st = g_pes_nfc_hal_ptx105r.ndef_read(
        result_out->ndef_data, PES_NFC_NDEF_MAX_BYTES, &result_out->ndef_len);
    result_out->ndef_present = (PES_OK == st);
    return st;
}

/**
 * Read NDEF from an NFC Forum Type 2 Tag.
 */
pes_status_t pes_ndef_read_t2t(pes_nfc_card_result_t *result_out)
{
    if (NULL == result_out) { return PES_ERR_INVALID_CFG; }

    pes_status_t st = g_pes_nfc_hal_ptx105r.ndef_read(
        result_out->ndef_data, PES_NFC_NDEF_MAX_BYTES, &result_out->ndef_len);
    result_out->ndef_present = (PES_OK == st);
    return st;
}

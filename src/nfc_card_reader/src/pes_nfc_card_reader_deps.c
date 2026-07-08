/**
 * pes_nfc_card_reader_deps.c
 *
 * Runtime dependency validation for the PES NFC Card Reader.
 * Checks that the PTX SDK backend has been opened successfully.
 */

#include "pes_nfc_card_reader_deps.h"
#include "pes_nfc_ptx105r.h"

pes_status_t pes_nfc_card_reader_validate_deps(void)
{
    /* pes_nfc_ptx_is_open() reflects whether pes_nfc_ptx_open() (which
     * initializes the PTX SDK IoT-Reader context directly) has succeeded. */
    if (!pes_nfc_ptx_is_open())
    {
        return PES_ERR_DEPENDENCY;
    }

    return PES_OK;
}

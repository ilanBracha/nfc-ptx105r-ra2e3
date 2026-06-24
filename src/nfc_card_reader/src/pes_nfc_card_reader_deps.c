/**
 * pes_nfc_card_reader_deps.c
 *
 * Runtime dependency validation for the PES NFC Card Reader.
 * Checks that the FSP NFC stack control block is in the expected state.
 */

#include "pes_nfc_card_reader_deps.h"
#include "hal_data.h"   /* g_nfc_reader_ptx0_ctrl */

pes_status_t pes_nfc_card_reader_validate_deps(void)
{
    /* The FSP ctrl block's `open` flag is non-zero after a successful
     * RM_NFC_READER_PTX_Open(). If it's 0, the stack isn't initialized. */
    if (0u == g_nfc_reader_ptx0_ctrl.open)
    {
        return PES_ERR_DEPENDENCY;
    }
    return PES_OK;
}

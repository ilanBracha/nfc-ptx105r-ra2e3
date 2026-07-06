/**
 * pes_nfc_card_reader_deps.c
 *
 * Runtime dependency validation for the PES NFC Card Reader.
 * Checks that the HAL vtable is populated and the FSP NFC stack
 * control block is in the expected state.
 */

#include "pes_nfc_card_reader_deps.h"
#include "pes_nfc_hal.h"
#include "hal_data.h"   /* g_nfc_reader_ptx0_ctrl */

pes_status_t pes_nfc_card_reader_validate_deps(void)
{
    /* Verify that the HAL backend vtable has critical function pointers. */
    if ((NULL == g_pes_nfc_hal_ptx105r.open)  ||
        (NULL == g_pes_nfc_hal_ptx105r.close) ||
        (NULL == g_pes_nfc_hal_ptx105r.start_polling) ||
        (NULL == g_pes_nfc_hal_ptx105r.wait_for_card) ||
        (NULL == g_pes_nfc_hal_ptx105r.activate_card))
    {
        return PES_ERR_DEPENDENCY;
    }

    /* The FSP ctrl block's `open` flag is non-zero after a successful
     * RM_NFC_READER_PTX_Open(). If it's 0, the stack isn't initialized. */
    if (0u == g_nfc_reader_ptx0_ctrl.open)
    {
        return PES_ERR_DEPENDENCY;
    }
    return PES_OK;
}

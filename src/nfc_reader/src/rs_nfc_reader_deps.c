/**
 * rs_nfc_reader_deps.c
 *
 * Runtime dependency validation for the RS NFC Reader.
 * Checks that the PTX SDK backend has been opened successfully.
 */

#include "rs_nfc_reader_deps.h"
#include "rs_nfc_ptx105r.h"

rs_status_t rs_nfc_reader_validate_deps(void)
{
    /* rs_nfc_ptx_is_open() reflects whether rs_nfc_ptx_open() (which
     * initializes the PTX SDK IoT-Reader context directly) has succeeded. */
    if (!rs_nfc_ptx_is_open())
    {
        return RS_ERR_DEPENDENCY;
    }

    return RS_OK;
}

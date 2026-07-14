/**
 * rs_nfc_reader_deps.h
 *
 * Internal header for dependency validation.
 */

#ifndef RS_NFC_READER_DEPS_H
#define RS_NFC_READER_DEPS_H

#include "rs_nfc_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate that the NFC reader hardware and FSP stack are ready.
 * Returns RS_OK if all dependencies are satisfied.
 */
rs_status_t rs_nfc_reader_validate_deps(void);

#ifdef __cplusplus
}
#endif

#endif /* RS_NFC_READER_DEPS_H */

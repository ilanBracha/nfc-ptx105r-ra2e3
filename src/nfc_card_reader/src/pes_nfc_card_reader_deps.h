/**
 * pes_nfc_card_reader_deps.h
 *
 * Internal header for dependency validation.
 */

#ifndef PES_NFC_CARD_READER_DEPS_H
#define PES_NFC_CARD_READER_DEPS_H

#include "pes_nfc_card_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Validate that the NFC reader hardware and FSP stack are ready.
 * Returns PES_OK if all dependencies are satisfied.
 */
pes_status_t pes_nfc_card_reader_validate_deps(void);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_CARD_READER_DEPS_H */

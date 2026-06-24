/**
 * pes_nfc_hal.h
 *
 * Hardware Abstraction Layer for the PES NFC Card Reader module.
 * Isolates PES business logic from the vendor-specific NFC stack
 * (currently: Renesas PTX105R via RM_NFC_READER_PTX FSP wrappers).
 */

#ifndef PES_NFC_HAL_H
#define PES_NFC_HAL_H

#include "pes_common.h"
#include "pes_nfc_card_reader.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Discovery status ──────────────────────────────────────────────── */
typedef enum {
    PES_NFC_DISC_NO_CARD = 0,
    PES_NFC_DISC_CARD_ACTIVE,
    PES_NFC_DISC_RUNNING,
    PES_NFC_DISC_DONE,
} pes_nfc_disc_status_t;

/* ── Activated card info (filled by HAL after activation) ──────────── */
#define PES_NFC_HAL_RX_BUF_SIZE   300U
#define PES_NFC_HAL_TX_BUF_SIZE   280U

typedef struct {
    pes_nfc_card_type_t card_type;
    uint8_t             uid[PES_NFC_UID_MAX_BYTES];
    uint8_t             uid_len;
} pes_nfc_hal_card_info_t;

/* ── HAL API ───────────────────────────────────────────────────────── */

/** Open / initialize the NFC reader (includes cold-boot recovery). */
pes_status_t pes_nfc_hal_open(pes_nfc_reader_device_t device);

/** Start RF discovery. tech_mask = bitmask of PES_NFC_TECH_* flags. */
pes_status_t pes_nfc_hal_discover_start(pes_nfc_tech_mask_t tech_mask);

/** Poll discovery status (non-blocking). */
pes_status_t pes_nfc_hal_discover_status(pes_nfc_disc_status_t *out_status);

/** Activate the first discovered card; fills card_info. */
pes_status_t pes_nfc_hal_card_activate(pes_nfc_hal_card_info_t *card_info);

/** Exchange raw data with the activated card. rx_len is in/out. */
pes_status_t pes_nfc_hal_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                       uint8_t *rx, uint32_t *rx_len);

/** Deactivate the current card and return to discovery. */
pes_status_t pes_nfc_hal_deactivate(void);

/** Check system health. Returns PES_ERR_INTERNAL on critical error. */
pes_status_t pes_nfc_hal_system_check(void);

/** Close / de-initialize the NFC reader. */
pes_status_t pes_nfc_hal_close(void);

/** Sleep for the given number of milliseconds. */
void pes_nfc_hal_sleep_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_HAL_H */

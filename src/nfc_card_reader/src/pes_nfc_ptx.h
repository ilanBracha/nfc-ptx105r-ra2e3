/**
 * pes_nfc_ptx.h
 *
 * Hardware Abstraction Layer for the PES NFC Card Reader module.
 * Isolates PES business logic from the vendor-specific NFC stack
 * (currently: Renesas PTX105R via RM_NFC_READER_PTX FSP wrappers).
 *
 * Each function below is implemented directly by the PTX105R backend
 * (pes_nfc_ptx105r.c) and called directly by name — no function-
 * pointer vtable indirection.
 */

#ifndef PES_NFC_PTX_H
#define PES_NFC_PTX_H

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

/* ── Activated card info (filled after activation) ─────────────────── */
#define PES_NFC_PTX_RX_BUF_SIZE   300U
#define PES_NFC_PTX_TX_BUF_SIZE   280U

typedef struct {
    pes_nfc_card_type_t card_type;
    pes_nfc_protocol_t  protocol;
    uint8_t             uid[PES_NFC_UID_MAX_BYTES];
    uint8_t             uid_len;
} pes_nfc_ptx_card_info_t;

/* ── PTX functions (PTX105R backend, pes_nfc_ptx105r.c) ─────────────── */

/** Open / initialize the NFC reader (includes cold-boot recovery). */
pes_status_t pes_nfc_ptx_open(pes_nfc_reader_device_t device);

/** Close / de-initialize the NFC reader. */
pes_status_t pes_nfc_ptx_close(void);

/** Apply tech_mask to the polling configuration. */
pes_status_t pes_nfc_ptx_configure_polling(pes_nfc_tech_mask_t tech_mask);

/** Enable RF field and start polling. */
pes_status_t pes_nfc_ptx_start_polling(void);

/** Stop polling / disable RF field. */
pes_status_t pes_nfc_ptx_stop_polling(void);

/**
 * Interrupt-driven wait for a card-discovery event (or timeout).
 * Includes an internal system-health check before waiting.
 *
 * @param[in]  timeout_ms  Max time to wait, in milliseconds.
 * @param[out] out_status  Discovery status after the wait.
 * @return PES_OK on a valid read; PES_ERR_INTERNAL on lower-level failure.
 */
pes_status_t pes_nfc_ptx_wait_for_card(uint32_t timeout_ms,
                                       pes_nfc_disc_status_t *out_status);

/** Activate the first discovered card; fills card_info. */
pes_status_t pes_nfc_ptx_activate_card(pes_nfc_ptx_card_info_t *card_info);

/** Return the card type of the currently active card. */
pes_status_t pes_nfc_ptx_get_card_type(pes_nfc_card_type_t *out_type);

/** Return the UID of the currently active card. */
pes_status_t pes_nfc_ptx_get_uid(uint8_t *uid, uint8_t *uid_len);

/** Sleep for the given number of milliseconds. */
void pes_nfc_ptx_sleep(uint32_t ms);

/** Exchange raw data with the activated card. rx_len is in/out. */
pes_status_t pes_nfc_ptx_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                       uint8_t *rx, uint32_t *rx_len);

/** Deactivate the current card and return to discovery. */
pes_status_t pes_nfc_ptx_deactivate(void);

/** Raw system-state byte. Caller inspects the byte to distinguish
 *  OK / overcurrent / temperature. */
pes_status_t pes_nfc_ptx_get_system_state(uint8_t *out_state);

/** Last RF error byte. 0 means no error. */
pes_status_t pes_nfc_ptx_get_last_rf_error(uint8_t *out_err);

/**
 * Wake the task currently blocked in pes_nfc_ptx_wait_for_card() (if any).
 * Called from PES_NFCCardReader_Stop().
 */
void pes_nfc_ptx_wake_waiting_task(void);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_PTX_H */

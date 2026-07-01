/**
 * pes_nfc_hal.h
 *
 * Hardware Abstraction Layer for the PES NFC Card Reader module.
 * Isolates PES business logic from the vendor-specific NFC stack
 * (currently: Renesas PTX105R via RM_NFC_READER_PTX FSP wrappers).
 *
 * The HAL is exposed as a const struct of function pointers
 * (g_pes_nfc_hal_ptx105r) so that engines call through an abstract
 * vtable.  This enables testability (mock HAL) and portability
 * (swap in a different NFC reader IC).
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
    pes_nfc_protocol_t  protocol;
    uint8_t             uid[PES_NFC_UID_MAX_BYTES];
    uint8_t             uid_len;
} pes_nfc_hal_card_info_t;

/* ── HAL function-pointer table (vtable) ───────────────────────────── */

/**
 * Each HAL backend (e.g. PTX105R) provides a const instance of this
 * struct.  Engines and the orchestrator call through the named global
 * (e.g. g_pes_nfc_hal_ptx105r.open(...)) — no pointer indirection.
 */
typedef struct {

    /* ── 12 spec-defined members (§3.15) ───────────────────────────── */

    /** Open / initialize the NFC reader (includes cold-boot recovery). */
    pes_status_t (*open)(pes_nfc_reader_device_t device);

    /** Close / de-initialize the NFC reader. */
    pes_status_t (*close)(void);

    /** Apply tech_mask to the polling configuration. */
    pes_status_t (*configure_polling)(pes_nfc_tech_mask_t tech_mask);

    /** Enable RF field and start polling. */
    pes_status_t (*start_polling)(void);

    /** Stop polling / disable RF field. */
    pes_status_t (*stop_polling)(void);

    /**
     * Interrupt-driven wait for a card-discovery event (or timeout).
     * Includes an internal system-health check before waiting.
     *
     * @param[in]  timeout_ms  Max time to wait, in milliseconds.
     * @param[out] out_status  Discovery status after the wait.
     * @return PES_OK on a valid read; PES_ERR_INTERNAL on lower-level failure.
     */
    pes_status_t (*wait_for_card)(uint32_t timeout_ms,
                                  pes_nfc_disc_status_t *out_status);

    /** Activate the first discovered card; fills card_info. */
    pes_status_t (*activate_card)(pes_nfc_hal_card_info_t *card_info);

    /** Return the card type of the currently active card. */
    pes_status_t (*get_card_type)(pes_nfc_card_type_t *out_type);

    /** Return the UID of the currently active card. */
    pes_status_t (*get_uid)(uint8_t *uid, uint8_t *uid_len);

    /** Probe whether the active card supports NDEF. */
    pes_status_t (*ndef_probe)(bool *out_supported);

    /**
     * Read NDEF payload from the active card.
     * @param[out] ndef_data   Destination buffer for NDEF message bytes.
     * @param[in]  max_bytes   Size of the destination buffer.
     * @param[out] ndef_len    Actual number of NDEF bytes read.
     * @return PES_OK on success, PES_ERR_NOT_FOUND if no NDEF message.
     */
    pes_status_t (*ndef_read)(uint8_t *ndef_data, uint16_t max_bytes,
                              uint16_t *ndef_len);

    /** Sleep for the given number of milliseconds. */
    void (*sleep)(uint32_t ms);

    /* ── Additional members (not in spec, cannot fold) ─────────────── */

    /** Exchange raw data with the activated card. rx_len is in/out. */
    pes_status_t (*data_exchange)(const uint8_t *tx, uint32_t tx_len,
                                  uint8_t *rx, uint32_t *rx_len);

    /** Deactivate the current card and return to discovery. */
    pes_status_t (*deactivate)(void);

    /** Raw system-state byte. Absorbs the old system_check — caller
     *  inspects the byte to distinguish OK / overcurrent / temperature. */
    pes_status_t (*get_system_state)(uint8_t *out_state);

    /** Last RF error byte. 0 means no error. */
    pes_status_t (*get_last_rf_error)(uint8_t *out_err);

    /**
     * Wake the task currently blocked in wait_for_card() (if any).
     * Called from PES_NFCCardReader_Stop().
     */
    void (*wake_waiting_task)(void);

} pes_nfc_hal_api_t;

/* ── PTX105R backend instance ─────────────────────────────────────── */
extern const pes_nfc_hal_api_t g_pes_nfc_hal_ptx105r;

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_HAL_H */

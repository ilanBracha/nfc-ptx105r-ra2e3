/**
 * pes_nfc_card_reader.h
 *
 * PES NFC Card Reader module public API.
 */

#ifndef PES_NFC_CARD_READER_H
#define PES_NFC_CARD_READER_H

#include "pes_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Device selection ──────────────────────────────────────────────── */
typedef enum {
    PES_NFC_READER_PTX105R = 0,
} pes_nfc_reader_device_t;

/* ── Technology mask ───────────────────────────────────────────────── */
typedef uint32_t pes_nfc_tech_mask_t;

#define PES_NFC_TECH_ISO14443A   (1UL << 0)
#define PES_NFC_TECH_ISO14443B   (1UL << 1)
#define PES_NFC_TECH_FELICA      (1UL << 2)
#define PES_NFC_TECH_ISO15693    (1UL << 3)
#define PES_NFC_TECH_NFC_FORUM   (1UL << 4)

#define PES_NFC_TECH_ALL  (PES_NFC_TECH_ISO14443A | \
                           PES_NFC_TECH_ISO14443B | \
                           PES_NFC_TECH_FELICA    | \
                           PES_NFC_TECH_ISO15693  | \
                           PES_NFC_TECH_NFC_FORUM)

/* ── Card type (must precede result struct) ────────────────────────── */
typedef enum {
    PES_NFC_CARD_TYPE_UNKNOWN = 0,
    PES_NFC_CARD_TYPE_ISO14443A,
    PES_NFC_CARD_TYPE_ISO14443B,
    PES_NFC_CARD_TYPE_FELICA,
    PES_NFC_CARD_TYPE_ISO15693,
    PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2,
    PES_NFC_CARD_TYPE_NFC_TAG_TYPE_3,
    PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A,
    PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B,
    PES_NFC_CARD_TYPE_NFC_TAG_TYPE_5,
} pes_nfc_card_type_t;

/* ── Active-card RF protocol (PES-owned mirror of PTX protocol enum) ─ */
typedef enum {
    PES_NFC_PROT_UNDEFINED = 0,
    PES_NFC_PROT_T2T,
    PES_NFC_PROT_T3T,
    PES_NFC_PROT_ISODEP,
    PES_NFC_PROT_NFCDEP,
    PES_NFC_PROT_T5T,
    PES_NFC_PROT_EXTENSION,
} pes_nfc_protocol_t;

/* ── Operation-end callback (legacy) ───────────────────────────────── */
typedef void (*pes_nfc_callback_t)(pes_status_t status, void *p_context);

/* ── Result (forward-declared so the per-card event cb can reference it) */
struct pes_nfc_card_result_s;
typedef struct pes_nfc_card_result_s pes_nfc_card_result_t;

/* ── Per-card event callback ───────────────────────────────────────── */
/*
 * Fired by PES_NFCCardReader_Read() each time a card is detected, activated
 * and (optionally) NDEF-read. The application MUST treat result/summary as
 * read-only and MUST NOT retain pointers past the call: both buffers are
 * reused on the next iteration of the read loop.
 */
typedef void (*pes_nfc_card_event_cb_t)(pes_status_t status,
                                        const pes_nfc_card_result_t *result,
                                        const char *summary,
                                        void *p_context);

/* ── Configuration ─────────────────────────────────────────────────── */
typedef struct {
    /* Reader device selection */
    pes_nfc_reader_device_t reader;

    /* Polling configuration */
    pes_nfc_tech_mask_t tech_mask;
    /* Run duration of PES_NFCCardReader_Read() in milliseconds.
     * Set to UINT32_MAX to loop forever (never return). */
    uint32_t timeout_ms;
    uint8_t retry_count;

    /* Read options */
    bool read_ndef;
    uint16_t max_ndef_bytes;

    /* Non-blocking support
     * callback = NULL  -> blocking opt-in
     * callback != NULL -> non-blocking, fires when complete
     */
    pes_nfc_callback_t callback;
    void *p_context;

    /* Per-card event (fires once per detected/activated card during Read()).
     * When set, the orchestrator runs in continuous-loop mode and emits one
     * event per card until timeout_ms elapses. */
    pes_nfc_card_event_cb_t on_card_event;
    void                   *p_card_event_context;

    /* Optional runtime dependency validation */
    bool validate_dependencies;
} pes_nfc_card_reader_cfg_t;

/* ── Result ────────────────────────────────────────────────────────── */
#define PES_NFC_UID_MAX_BYTES       10U
#define PES_NFC_NDEF_MAX_BYTES      512U

struct pes_nfc_card_result_s {
    pes_nfc_card_type_t card_type;
    pes_nfc_protocol_t  protocol;     /* active RF protocol */
    uint8_t uid[PES_NFC_UID_MAX_BYTES];
    uint8_t uid_len;
    bool ndef_present;
    uint8_t ndef_data[PES_NFC_NDEF_MAX_BYTES];
    uint16_t ndef_len;
    int8_t rssi_dbm; /* optional, HAL may return 0 if unsupported */
    uint32_t read_time_ms;
};

/* ── API ───────────────────────────────────────────────────────────── */
pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t * cfg, pes_nfc_card_result_t * result_out);

/* Raw data exchange with the currently-activated card. Use this from inside
 * an on_card_event callback to issue protocol-specific frames (T2T READ,
 * T3T CHECK, T5T READ_SINGLE_BLOCK, ISO-DEP APDUs, raw NDEF write, ...).
 *
 * tx       : pointer to the TX frame
 * tx_len   : TX frame length in bytes
 * rx       : caller-supplied RX buffer
 * rx_len   : IN  = capacity of rx buffer
 *            OUT = number of bytes received
 *
 * Returns PES_OK on success, PES_ERR_* on failure. */
pes_status_t PES_NFCCardReader_DataExchange(const uint8_t *tx, uint32_t tx_len,
                                            uint8_t *rx, uint32_t *rx_len);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_CARD_READER_H */

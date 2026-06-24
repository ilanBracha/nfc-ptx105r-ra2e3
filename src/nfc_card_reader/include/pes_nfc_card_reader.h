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

/* ── Callback (must precede cfg struct) ────────────────────────────── */
typedef void (*pes_nfc_callback_t)(pes_status_t status, void *p_context);

/* ── Configuration ─────────────────────────────────────────────────── */
typedef struct {
    /* Reader device selection */
    pes_nfc_reader_device_t reader;

    /* Polling configuration */
    pes_nfc_tech_mask_t tech_mask;
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

    /* Optional runtime dependency validation */
    bool validate_dependencies;
} pes_nfc_card_reader_cfg_t;

/* ── Result ────────────────────────────────────────────────────────── */
#define PES_NFC_UID_MAX_BYTES       10U
#define PES_NFC_NDEF_MAX_BYTES      512U

typedef struct {
    pes_nfc_card_type_t card_type;
    uint8_t uid[PES_NFC_UID_MAX_BYTES];
    uint8_t uid_len;
    bool ndef_present;
    uint8_t ndef_data[PES_NFC_NDEF_MAX_BYTES];
    uint16_t ndef_len;
    int8_t rssi_dbm; /* optional, HAL may return 0 if unsupported */
    uint32_t read_time_ms;
} pes_nfc_card_result_t;

/* ── API ───────────────────────────────────────────────────────────── */
pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t *cfg, pes_nfc_card_result_t *result_out);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_CARD_READER_H */
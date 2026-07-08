/**
 * pes_nfc_card_reader.h
 *
 * PES NFC Card Reader module public API.
 *
 * This header is the single public include for the module. It provides:
 *   - Shared types and status codes (formerly pes_common.h)
 *   - NDEF parsing/decoding types and API (formerly pes_ndef_util.h)
 *   - NFC Card Reader configuration, result, and API
 */

#ifndef PES_NFC_CARD_READER_H
#define PES_NFC_CARD_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************
 * Common types, status codes, and callback convention
 **********************************************************************************************************************/

#define PES_COMMON_UNUSED(x) (void)(x)

typedef enum {
    PES_OK                  =   0,
    PES_ERR_TIMEOUT         =  -1,
    PES_ERR_CRED_INVALID    =  -2,
    PES_ERR_CONN_FAIL       =  -3,
    PES_ERR_NO_IP           =  -4,
    PES_ERR_NO_CLOUD        =  -5,
    PES_ERR_DEPENDENCY      =  -6,
    PES_ERR_INVALID_CFG     =  -7,
    PES_ERR_NOT_FOUND       =  -8,
    PES_ERR_BUFFER_OVERFLOW =  -9,
    PES_ERR_INTERNAL        = -99,
} pes_status_t;

typedef void (*pes_callback_t)(pes_status_t status, void *p_context);

#ifndef PES_LOG
#define PES_LOG(fmt, ...)   /* default: silent */
#endif

/**********************************************************************************************************************
 * NDEF parsing and decoding utilities
 **********************************************************************************************************************/

#define PES_NDEF_MAX_RECORDS        8U
#define PES_NDEF_MAX_TYPE_LEN      32U
#define PES_NDEF_WIFI_SSID_MAX     32U
#define PES_NDEF_WIFI_PASS_MAX     64U
#define PES_NDEF_BT_NAME_MAX       48U

typedef struct {
    uint8_t  tnf;
    uint8_t  type[PES_NDEF_MAX_TYPE_LEN];
    uint8_t  type_len;
    const uint8_t *payload;
    uint32_t payload_len;
    uint8_t  flags;
} pes_ndef_record_t;

typedef struct {
    pes_ndef_record_t records[PES_NDEF_MAX_RECORDS];
    uint8_t           record_count;
    bool              truncated;
} pes_ndef_decoded_t;

typedef struct {
    char     ssid[PES_NDEF_WIFI_SSID_MAX + 1];
    uint8_t  ssid_len;
    uint16_t auth_type;
    uint16_t enc_type;
    char     password[PES_NDEF_WIFI_PASS_MAX + 1];
    uint8_t  password_len;
    uint8_t  mac_addr[6];
    bool     mac_present;
} pes_wifi_info_t;

typedef struct {
    uint8_t  bd_addr[6];
    bool     addr_present;
    char     local_name[PES_NDEF_BT_NAME_MAX + 1];
    uint8_t  name_len;
    bool     is_le;
} pes_bt_info_t;

pes_status_t PES_NDEF_DecodeMessage(const uint8_t *msg, uint32_t len,
                                    pes_ndef_decoded_t *out);

pes_status_t PES_NDEF_DecodeWifi(const uint8_t *payload, uint32_t len,
                                 pes_wifi_info_t *out);

pes_status_t PES_NDEF_DecodeBluetooth(const uint8_t *payload, uint32_t len,
                                      bool is_le, pes_bt_info_t *out);

bool PES_NDEF_TlvFind(const uint8_t *buf, uint32_t len, uint16_t tag,
                      const uint8_t **val, uint32_t *val_len);

bool PES_NDEF_TypeEquals(const uint8_t *type, uint8_t type_len,
                         const char *str);

bool PES_NDEF_StartsWith(const char *str, uint32_t str_len,
                         const char *prefix);

/**********************************************************************************************************************
 * Device selection
 **********************************************************************************************************************/
typedef enum {
    PES_NFC_READER_PTX105R = 0,
} pes_nfc_reader_device_t;

/**********************************************************************************************************************
 * Technology mask
 **********************************************************************************************************************/
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

/**********************************************************************************************************************
 * Card type (must precede result struct)
 **********************************************************************************************************************/
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

/**********************************************************************************************************************
 * Active-card RF protocol (PES-owned mirror of PTX protocol enum)
 **********************************************************************************************************************/
typedef enum {
    PES_NFC_PROT_UNDEFINED = 0,
    PES_NFC_PROT_T2T,
    PES_NFC_PROT_T3T,
    PES_NFC_PROT_ISODEP,
    PES_NFC_PROT_NFCDEP,
    PES_NFC_PROT_T5T,
    PES_NFC_PROT_EXTENSION,
} pes_nfc_protocol_t;

/**********************************************************************************************************************
 * Operation-end callback (non-blocking mode)
 **********************************************************************************************************************/
/**
 * Fired once when a non-blocking PES_NFCCardReader_Read() completes
 * (timeout, fatal error, or PES_NFCCardReader_Stop() was called).
 */
typedef void (*pes_nfc_callback_t)(pes_status_t status, void *p_context);

/**********************************************************************************************************************
 * Result (forward-declared so the per-card event cb can reference it)
 **********************************************************************************************************************/
struct pes_nfc_card_result_s;
typedef struct pes_nfc_card_result_s pes_nfc_card_result_t;

/**********************************************************************************************************************
 * Per-card event callback
 **********************************************************************************************************************/
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

/**********************************************************************************************************************
 * Configuration
 **********************************************************************************************************************/
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

    /* Non-blocking support:
     * callback == NULL -> blocking (Read blocks until done)
     * callback != NULL -> non-blocking (Read returns immediately,
     *                     spawns a static FreeRTOS task; callback fires
     *                     on completion). Only one non-blocking Read may
     *                     be active at a time.
     */
    pes_nfc_callback_t callback;
    void *p_context;

    /* Per-card event (fires once per detected/activated card during Read()).
     * When set, the orchestrator runs in continuous-loop mode and emits one
     * event per card until timeout_ms elapses. Works in both blocking and
     * non-blocking modes. */
    pes_nfc_card_event_cb_t on_card_event;
    void                   *p_card_event_context;

    /* Optional runtime dependency validation */
    bool validate_dependencies;
} pes_nfc_card_reader_cfg_t;

/**********************************************************************************************************************
 * Result
 **********************************************************************************************************************/
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

    /* Extended card-info fields (populated by PES_NFCCardReader_ReadCardInfo) */
    uint32_t    data_area_size;   /**< Tag capacity in bytes (from CC)       */
    bool        writeable;        /**< true if tag write-access is granted   */
    const char *tag_type_name;    /**< Human-readable tag type, e.g.
                                       "NFC Forum Type 2 Tag (T2T)".
                                       Points to a static string — do NOT free. */
};

/* Internal NDEF read helper function for Type 4 Tags (T4T) */
pes_status_t pes_ndef_read_t4t(pes_nfc_card_result_t *result_out);

/* Internal NDEF read helper function for Type 2 Tags (T2T) */
pes_status_t pes_ndef_read_t2t(pes_nfc_card_result_t *result_out);

/**********************************************************************************************************************
 * API
 **********************************************************************************************************************/

/**
 * Read NFC cards according to the supplied configuration.
 *
 * Blocking mode (cfg->callback == NULL):
 *   Blocks until timeout_ms elapses or a fatal error occurs.
 *
 * Non-blocking mode (cfg->callback != NULL):
 *   Spawns a dedicated static FreeRTOS task, returns PES_OK immediately.
 *   The callback fires once when the operation completes. Only one
 *   non-blocking Read() may be active at a time; a second call while
 *   a task is running returns PES_ERR_INTERNAL.
 *
 * In both modes, if on_card_event is set the orchestrator runs in
 * continuous-loop mode and fires one event per detected card.
 */
pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t * cfg, pes_nfc_card_result_t * result_out);

/**
 * Request graceful stop of a running Read (blocking or non-blocking).
 * The loop exits cleanly; the operation-end callback fires with PES_OK.
 * Safe to call even when no operation is in flight.
 * @return PES_OK always.
 */
pes_status_t PES_NFCCardReader_Stop(void);

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
pes_status_t PES_NFCCardReader_DataExchange(const uint8_t *tx, uint32_t tx_len, uint8_t *rx, uint32_t *rx_len);

/**
 * Validates NFC card reader configuration and optional dependency state.
 *
 * Does not start polling, activate RF field, or acquire card resources.
 * Safe to call multiple times.
 *
 * Checks performed always:
 *   - cfg is non-NULL
 *   - reader is a valid enum value
 *   - tech_mask is non-zero
 *   - timeout_ms > 0
 *   - if read_ndef = true: max_ndef_bytes > 0 and <= PES_NFC_NDEF_MAX_BYTES
 *
 * Additional checks when cfg->validate_dependencies = true:
 *   - PTX105R lower-level stack is initialized (FSP ctrl block open)
 *
 * @param cfg [in] Configuration to validate. Must not be NULL.
 *
 * @return PES_OK if all checks pass.
 *         PES_ERR_INVALID_CFG if configuration is incomplete or invalid.
 *         PES_ERR_DEPENDENCY if a runtime dependency check fails.
 */
pes_status_t PES_NFCCardReader_Validate(const pes_nfc_card_reader_cfg_t *cfg);

/**
 * Read structured card information (CC, NDEF message, tag type, size,
 * write-access) from the currently-activated card. Dispatches internally
 * to the appropriate T2T or T4T read sequence based on `protocol`.
 *
 * Populates result->ndef_data/ndef_len/ndef_present, data_area_size,
 * writeable, and tag_type_name. Call from inside an on_card_event callback
 * while the card is still activated.
 *
 * @param[in]     protocol   Active RF protocol (from result->protocol).
 * @param[in,out] result     Result struct to populate. Must not be NULL.
 * @return PES_OK on success; PES_ERR_NOT_FOUND if not NDEF formatted.
 */
pes_status_t PES_NFCCardReader_ReadCardInfo(pes_nfc_protocol_t protocol,
                                            pes_nfc_card_result_t *result);

/**
 * Write an NDEF message to the currently-activated card.
 * Dispatches to T2T or T4T write based on `protocol`.
 *
 * @param[in] protocol  Active RF protocol.
 * @param[in] ndef      NDEF message bytes (may be NULL when ndef_len==0).
 * @param[in] ndef_len  Length in bytes (max 248). 0 is equivalent to erase.
 * @return PES_OK on success.
 */
pes_status_t PES_NFCCardReader_WriteNDEF(pes_nfc_protocol_t protocol,
                                         const uint8_t *ndef,
                                         uint16_t ndef_len);

/**
 * Erase the NDEF message on the currently-activated card (set NLEN=0).
 * Convenience wrapper around PES_NFCCardReader_WriteNDEF with ndef_len==0.
 *
 * @param[in] protocol  Active RF protocol.
 * @return PES_OK on success.
 */
pes_status_t PES_NFCCardReader_EraseNDEF(pes_nfc_protocol_t protocol);

/**
 * Build a single NFC Forum well-known Text NDEF record (RTD-Text, "en",
 * UTF-8) into the caller-supplied buffer.
 *
 * @param[in]  text      UTF-8 text payload (not NUL-terminated).
 * @param[in]  text_len  Length of text in bytes (max 248).
 * @param[out] out       Destination buffer (must have room for text_len+7).
 * @param[out] out_len   Receives the total record length.
 * @return PES_OK on success; PES_ERR_INVALID_CFG on bad params.
 */
pes_status_t PES_NDEF_BuildTextRecord(const char *text, uint16_t text_len,
                                      uint8_t *out, uint16_t *out_len);

/**
 * Perform a protocol-specific raw demo exchange with the activated card.
 * Builds the correct frame (T2T READ, T3T CHECK, T5T READ_SINGLE_BLOCK,
 * NFC-DEP SYMM) and sends it via the HAL data_exchange.
 *
 * @param[in]  protocol  Active RF protocol.
 * @param[in]  uid       Card UID (needed for T3T NFCID2 and T5T addressing).
 * @param[in]  uid_len   UID length in bytes.
 * @param[out] tx        Caller buffer filled with the TX frame that was sent.
 * @param[out] tx_len    TX frame length.
 * @param[out] rx        Caller buffer filled with the RX response.
 * @param[out] rx_len    IN: capacity; OUT: received length.
 * @return PES_OK on success.
 */
pes_status_t PES_NFCCardReader_RawExchange(pes_nfc_protocol_t protocol,
                                           const uint8_t *uid, uint8_t uid_len,
                                           uint8_t *tx, uint32_t *tx_len,
                                           uint8_t *rx, uint32_t *rx_len);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_CARD_READER_H */

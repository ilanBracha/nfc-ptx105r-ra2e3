/**
 * rs_nfc_reader.h
 *
 * RS NFC Reader module public API.
 *
 * This header is the single public include for the module. It provides:
 *   - Shared types and status codes (formerly rs_common.h)
 *   - NDEF parsing/decoding types and API (formerly rs_ndef_util.h)
 *   - NFC Reader configuration, result, and API
 */

#ifndef RS_NFC_READER_H
#define RS_NFC_READER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************
 * Common types, status codes, and callback convention
 **********************************************************************************************************************/

#define RS_COMMON_UNUSED(x) (void)(x)

typedef enum {
    RS_OK                  =   0,
    RS_ERR_TIMEOUT         =  -1,
    RS_ERR_CRED_INVALID    =  -2,
    RS_ERR_CONN_FAIL       =  -3,
    RS_ERR_NO_IP           =  -4,
    RS_ERR_NO_CLOUD        =  -5,
    RS_ERR_DEPENDENCY      =  -6,
    RS_ERR_INVALID_CFG     =  -7,
    RS_ERR_NOT_FOUND       =  -8,
    RS_ERR_BUFFER_OVERFLOW =  -9,
    RS_ERR_INTERNAL        = -99,
} rs_status_t;

typedef void (*rs_callback_t)(rs_status_t status, void *p_context);

#ifndef RS_LOG
#define RS_LOG(fmt, ...)   /* default: silent */
#endif

/**********************************************************************************************************************
 * NDEF parsing and decoding utilities
 **********************************************************************************************************************/

#define RS_NDEF_MAX_RECORDS        8U
#define RS_NDEF_MAX_TYPE_LEN      32U
#define RS_NDEF_WIFI_SSID_MAX     32U
#define RS_NDEF_WIFI_PASS_MAX     64U
#define RS_NDEF_BT_NAME_MAX       48U

typedef struct {
    uint8_t  tnf;
    uint8_t  type[RS_NDEF_MAX_TYPE_LEN];
    uint8_t  type_len;
    const uint8_t *payload;
    uint32_t payload_len;
    uint8_t  flags;
} rs_ndef_record_t;

typedef struct {
    rs_ndef_record_t records[RS_NDEF_MAX_RECORDS];
    uint8_t           record_count;
    bool              truncated;
} rs_ndef_decoded_t;

typedef struct {
    char     ssid[RS_NDEF_WIFI_SSID_MAX + 1];
    uint8_t  ssid_len;
    uint16_t auth_type;
    uint16_t enc_type;
    char     password[RS_NDEF_WIFI_PASS_MAX + 1];
    uint8_t  password_len;
    uint8_t  mac_addr[6];
    bool     mac_present;
} rs_wifi_info_t;

typedef struct {
    uint8_t  bd_addr[6];
    bool     addr_present;
    char     local_name[RS_NDEF_BT_NAME_MAX + 1];
    uint8_t  name_len;
    bool     is_le;
} rs_bt_info_t;

rs_status_t rs_ndef_decode_message(const uint8_t *msg, uint32_t len,
                                    rs_ndef_decoded_t *out);

rs_status_t rs_ndef_decode_wifi(const uint8_t *payload, uint32_t len,
                                 rs_wifi_info_t *out);

rs_status_t rs_ndef_decode_bluetooth(const uint8_t *payload, uint32_t len,
                                      bool is_le, rs_bt_info_t *out);

bool rs_ndef_tlv_find(const uint8_t *buf, uint32_t len, uint16_t tag,
                      const uint8_t **val, uint32_t *val_len);

bool rs_ndef_type_equals(const uint8_t *type, uint8_t type_len,
                         const char *str);

bool rs_ndef_starts_with(const char *str, uint32_t str_len,
                         const char *prefix);

/**********************************************************************************************************************
 * Technology mask
 **********************************************************************************************************************/
typedef uint32_t rs_nfc_tech_mask_t;

#define RS_NFC_TECH_ISO14443A   (1UL << 0)
#define RS_NFC_TECH_ISO14443B   (1UL << 1)
#define RS_NFC_TECH_FELICA      (1UL << 2)
#define RS_NFC_TECH_ISO15693    (1UL << 3)
#define RS_NFC_TECH_NFC_FORUM   (1UL << 4)

#define RS_NFC_TECH_ALL  (RS_NFC_TECH_ISO14443A | \
                           RS_NFC_TECH_ISO14443B | \
                           RS_NFC_TECH_FELICA    | \
                           RS_NFC_TECH_ISO15693  | \
                           RS_NFC_TECH_NFC_FORUM)

/**********************************************************************************************************************
 * Card type (must precede result struct)
 **********************************************************************************************************************/
typedef enum {
    RS_NFC_CARD_TYPE_UNKNOWN = 0,
    RS_NFC_CARD_TYPE_ISO14443A,
    RS_NFC_CARD_TYPE_ISO14443B,
    RS_NFC_CARD_TYPE_FELICA,
    RS_NFC_CARD_TYPE_ISO15693,
    RS_NFC_CARD_TYPE_NFC_TAG_TYPE_2,
    RS_NFC_CARD_TYPE_NFC_TAG_TYPE_3,
    RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4A,
    RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4B,
    RS_NFC_CARD_TYPE_NFC_TAG_TYPE_5,
} rs_nfc_card_type_t;

/**********************************************************************************************************************
 * Active-card RF protocol (RS-owned mirror of PTX protocol enum)
 **********************************************************************************************************************/
typedef enum {
    RS_NFC_PROT_UNDEFINED = 0,
    RS_NFC_PROT_T2T,
    RS_NFC_PROT_T3T,
    RS_NFC_PROT_ISODEP,
    RS_NFC_PROT_NFCDEP,
    RS_NFC_PROT_T5T,
    RS_NFC_PROT_EXTENSION,
} rs_nfc_protocol_t;

/**********************************************************************************************************************
 * Operation-end callback (non-blocking mode)
 **********************************************************************************************************************/
/**
 * Fired once when a non-blocking rs_nfc_reader_Read() completes
 * (timeout, fatal error, or rs_nfc_reader_Stop() was called).
 */
typedef void (*rs_nfc_callback_t)(rs_status_t status, void *p_context);

/**********************************************************************************************************************
 * Result (forward-declared so the per-card event cb can reference it)
 **********************************************************************************************************************/
struct rs_nfc_card_result_s;
typedef struct rs_nfc_card_result_s rs_nfc_card_result_t;

/**********************************************************************************************************************
 * Per-card event callback
 **********************************************************************************************************************/
/*
 * Fired by rs_nfc_reader_Read() each time a card is detected, activated
 * and (optionally) NDEF-read. The application MUST treat result/summary as
 * read-only and MUST NOT retain pointers past the call: both buffers are
 * reused on the next iteration of the read loop.
 */
typedef void (*rs_nfc_card_event_cb_t)(rs_status_t status,
                                        const rs_nfc_card_result_t *result,
                                        const char *summary,
                                        void *p_context);

/**********************************************************************************************************************
 * Configuration
 **********************************************************************************************************************/
typedef struct {
    /* Polling configuration: Must be within [RS_NFC_POLLING_INTERVAL_MIN_MS, RS_NFC_POLLING_INTERVAL_MAX_MS]. */
    uint32_t polling_interval_ms;
    rs_nfc_tech_mask_t tech_mask;

    /* Run duration of rs_nfc_reader_Read() in milliseconds.
     * Set to UINT32_MAX to loop forever (never return). */
    uint32_t timeout_ms;
    uint8_t retry_count;

    /* Read options */
    bool read_ndef;
    uint16_t max_ndef_bytes;

    /* Opt-in demo: after activation, perform a protocol-appropriate raw
     * frame exchange (T2T READ / T3T CHECK / T5T READ_SINGLE_BLOCK / NFC-DEP
     * SYMM). The TX/RX frames are exposed to the on_card_event callback via
     * the result->raw_exchange fields. No effect for ISO-DEP or when
     * on_card_event is NULL. Default: false. */
    bool run_raw_exchange;

    /* Non-blocking support:
     * callback == NULL -> blocking (Read blocks until done)
     * callback != NULL -> non-blocking (Read returns immediately,
     *                     spawns a static FreeRTOS task; callback fires
     *                     on completion). Only one non-blocking Read may
     *                     be active at a time.
     */
    rs_nfc_callback_t callback;
    void *p_context;

    /* Per-card event (fires once per detected/activated card during Read()).
     * When set, the orchestrator runs in continuous-loop mode and emits one
     * event per card until timeout_ms elapses. Works in both blocking and
     * non-blocking modes. */
    rs_nfc_card_event_cb_t on_card_event;
    void                   *p_card_event_context;

    /* Optional runtime dependency validation */
    bool validate_dependencies;
} rs_nfc_reader_cfg_t;


/**********************************************************************************************************************
 * Result
 **********************************************************************************************************************/
#define RS_NFC_UID_MAX_BYTES       10U
#define RS_NFC_NDEF_MAX_BYTES      512U

/**
 * Snapshot of the last raw protocol exchange performed on an activated card.
 * Populated only when cfg->run_raw_exchange = true and the active protocol
 * supports it (i.e. NOT ISO-DEP / UNDEFINED). The tx / rx pointers reference
 * RS-internal static buffers and MUST NOT be retained past the
 * on_card_event callback (same contract as `summary`). Consumers must check
 * `valid` before using any other field.
 */
typedef struct {
    bool           valid;
    rs_status_t    status;
    const uint8_t *tx;
    uint32_t       tx_len;
    const uint8_t *rx;
    uint32_t       rx_len;
} rs_nfc_raw_exchange_t;

struct rs_nfc_card_result_s {
    rs_nfc_card_type_t card_type;
    rs_nfc_protocol_t  protocol;     /* active RF protocol */
    uint8_t uid[RS_NFC_UID_MAX_BYTES];
    uint8_t uid_len;
    bool ndef_present;
    uint8_t ndef_data[RS_NFC_NDEF_MAX_BYTES];
    uint16_t ndef_len;
    int8_t rssi_dbm; /* optional, HAL may return 0 if unsupported */
    uint32_t read_time_ms;

    /* Extended card-info fields (populated by rs_nfc_reader_ReadCardInfo) */
    uint32_t    data_area_size;   /**< Tag capacity in bytes (from CC)       */
    bool        writeable;        /**< true if tag write-access is granted   */
    const char *tag_type_name;    /**< Human-readable tag type, e.g.
                                       "NFC Forum Type 2 Tag (T2T)".
                                       Points to a static string — do NOT free. */

    /* Last raw exchange (see rs_nfc_raw_exchange_t doc). Check
     * raw_exchange.valid before use. */
    rs_nfc_raw_exchange_t raw_exchange;
};

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
 *   Spawns a dedicated static FreeRTOS task, returns RS_OK immediately.
 *   The callback fires once when the operation completes. Only one
 *   non-blocking Read() may be active at a time; a second call while
 *   a task is running returns RS_ERR_INTERNAL.
 *
 * In both modes, if on_card_event is set the orchestrator runs in
 * continuous-loop mode and fires one event per detected card.
 */
rs_status_t rs_nfc_reader_Read(const rs_nfc_reader_cfg_t * cfg, rs_nfc_card_result_t * result_out);

/**
 * Request graceful stop of a running Read (blocking or non-blocking).
 * The loop exits cleanly; the operation-end callback fires with RS_OK.
 * Safe to call even when no operation is in flight.
 * @return RS_OK always.
 */
rs_status_t rs_nfc_reader_Stop(void);

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
 *   - if read_ndef = true: max_ndef_bytes > 0 and <= RS_NFC_NDEF_MAX_BYTES
 *
 * Additional checks when cfg->validate_dependencies = true:
 *   - PTX105R lower-level stack is initialized (FSP ctrl block open)
 *
 * @param cfg [in] Configuration to validate. Must not be NULL.
 *
 * @return RS_OK if all checks pass.
 *         RS_ERR_INVALID_CFG if configuration is incomplete or invalid.
 *         RS_ERR_DEPENDENCY if a runtime dependency check fails.
 */
rs_status_t rs_nfc_reader_Validate(const rs_nfc_reader_cfg_t *cfg);

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
 * @return RS_OK on success; RS_ERR_NOT_FOUND if not NDEF formatted.
 */
rs_status_t rs_nfc_reader_ReadCardInfo(rs_nfc_protocol_t protocol,
                                      rs_nfc_card_result_t *result);

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
 * @return RS_OK on success.
 */
rs_status_t rs_nfc_reader_RawExchange(rs_nfc_protocol_t protocol,
                                     const uint8_t *uid, uint8_t uid_len,
                                     uint8_t *tx, uint32_t *tx_len,
                                     uint8_t *rx, uint32_t *rx_len);

#ifdef __cplusplus
}
#endif

#endif /* RS_NFC_READER_H */
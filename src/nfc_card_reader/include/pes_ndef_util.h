/**
 * pes_ndef_util.h
 *
 * NDEF parsing and decoding utilities for the PES NFC Card Reader module.
 *
 * All functions are pure data transforms: they read input buffers and
 * populate caller-supplied structs.  NO printing or logging — the AUC
 * layer is responsible for rendering the decoded data to the user.
 */

#ifndef PES_NDEF_UTIL_H
#define PES_NDEF_UTIL_H

#include "pes_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits ────────────────────────────────────────────────────────── */
#define PES_NDEF_MAX_RECORDS        8U   /**< Max records decoded per msg  */
#define PES_NDEF_MAX_TYPE_LEN      32U   /**< Max record type length       */
#define PES_NDEF_WIFI_SSID_MAX     32U
#define PES_NDEF_WIFI_PASS_MAX     64U
#define PES_NDEF_BT_NAME_MAX       48U

/* ── NDEF Record (decoded) ─────────────────────────────────────────── */
typedef struct {
    uint8_t  tnf;                           /**< Type Name Format (0–7) */
    uint8_t  type[PES_NDEF_MAX_TYPE_LEN];   /**< Record type field      */
    uint8_t  type_len;
    const uint8_t *payload;                 /**< Points into the original
                                                 NDEF message buffer    */
    uint32_t payload_len;
    uint8_t  flags;                         /**< Raw header byte (MB/ME/CF/SR/IL) */
} pes_ndef_record_t;

/* ── Decoded NDEF message ──────────────────────────────────────────── */
typedef struct {
    pes_ndef_record_t records[PES_NDEF_MAX_RECORDS];
    uint8_t           record_count;         /**< Number of decoded records */
    bool              truncated;            /**< true if > PES_NDEF_MAX_RECORDS */
} pes_ndef_decoded_t;

/* ── Wi-Fi Simple Config info ──────────────────────────────────────── */
typedef struct {
    char     ssid[PES_NDEF_WIFI_SSID_MAX + 1]; /**< NUL-terminated         */
    uint8_t  ssid_len;
    uint16_t auth_type;                         /**< WSC Auth Type value    */
    uint16_t enc_type;                          /**< WSC Encryption Type    */
    char     password[PES_NDEF_WIFI_PASS_MAX + 1]; /**< NUL-terminated      */
    uint8_t  password_len;
    uint8_t  mac_addr[6];
    bool     mac_present;
} pes_wifi_info_t;

/* ── Bluetooth OOB info ────────────────────────────────────────────── */
typedef struct {
    uint8_t  bd_addr[6];                     /**< BD_ADDR (MSB-first)     */
    bool     addr_present;
    char     local_name[PES_NDEF_BT_NAME_MAX + 1]; /**< NUL-terminated   */
    uint8_t  name_len;
    bool     is_le;                          /**< true = BLE, false = BR/EDR */
} pes_bt_info_t;

/* ── API ───────────────────────────────────────────────────────────── */

/**
 * Parse an NDEF message into individual records.
 *
 * @param[in]  msg     Raw NDEF message bytes.
 * @param[in]  len     Length of msg in bytes.
 * @param[out] out     Decoded output structure. Must not be NULL.
 * @return PES_OK on success; PES_ERR_NOT_FOUND if msg is empty/NULL.
 */
pes_status_t PES_NDEF_DecodeMessage(const uint8_t *msg, uint32_t len,
                                    pes_ndef_decoded_t *out);

/**
 * Decode a Wi-Fi Simple Config (WSC / vnd.wfa.wsc) MIME payload.
 *
 * @param[in]  payload   WSC TLV data.
 * @param[in]  len       Payload length.
 * @param[out] out       Decoded Wi-Fi info. Must not be NULL.
 * @return PES_OK on success; PES_ERR_NOT_FOUND if no SSID found.
 */
pes_status_t PES_NDEF_DecodeWifi(const uint8_t *payload, uint32_t len,
                                 pes_wifi_info_t *out);

/**
 * Decode a Bluetooth OOB (BR/EDR or LE) MIME payload.
 *
 * @param[in]  payload   OOB data.
 * @param[in]  len       Payload length.
 * @param[in]  is_le     true for BLE, false for BR/EDR.
 * @param[out] out       Decoded Bluetooth info. Must not be NULL.
 * @return PES_OK on success.
 */
pes_status_t PES_NDEF_DecodeBluetooth(const uint8_t *payload, uint32_t len,
                                      bool is_le, pes_bt_info_t *out);

/**
 * Lightweight BER-TLV search (1- or 2-byte tags, multi-byte length,
 * recursive into constructed TLVs).
 *
 * @param[in]  buf     TLV data buffer.
 * @param[in]  len     Buffer length.
 * @param[in]  tag     Tag to search for (1 or 2 bytes, MSB-aligned).
 * @param[out] val     Receives pointer to the value field.
 * @param[out] val_len Receives value length.
 * @return true if found, false otherwise.
 */
bool PES_NDEF_TlvFind(const uint8_t *buf, uint32_t len, uint16_t tag,
                      const uint8_t **val, uint32_t *val_len);

/**
 * Compare an NDEF type field against a NUL-terminated C string.
 * @return true if equal.
 */
bool PES_NDEF_TypeEquals(const uint8_t *type, uint8_t type_len,
                         const char *str);

/**
 * Check if a (non-NUL-terminated) string starts with a prefix.
 * @return true if str starts with prefix.
 */
bool PES_NDEF_StartsWith(const char *str, uint32_t str_len,
                         const char *prefix);

#ifdef __cplusplus
}
#endif

#endif /* PES_NDEF_UTIL_H */

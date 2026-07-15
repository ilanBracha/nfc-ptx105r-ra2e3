/**
 * rs_ndef_read.c
 *
 * NDEF message reading for NFC Forum Type 2 Tags (hand-rolled, small
 * footprint) and Type 3/Type 4/Type 5 Tags (via the PTX SDK's lean
 * ptxNDEF_T3TOP / ptxNDEF_T4TOP / ptxNDEF_T5TOP components). We use
 * the individual tag-type NDEF-OP components instead of the generic
 * ptxNDEF dispatcher (ptxNDEF.c) which unconditionally links all four
 * tag-type operation components (~13 KB flash).
 *
 * Also contains the NDEF message-level decoders (Wi-Fi, Bluetooth,
 * record parser) which have no SDK equivalent.
 *
 * NO printing — all results go into the caller's rs_nfc_card_result_t.
 */

#include "rs_nfc_reader.h"
#include "rs_nfc_ptx105r.h"
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T3TOP.h"
#include "ptxNDEF_T4TOP.h"
#include "ptxNDEF_T5TOP.h"
#include <string.h>

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/
#define RX_BUF_SIZE   RS_NFC_PTX_RX_BUF_SIZE
#define TX_BUF_SIZE   RS_NFC_PTX_TX_BUF_SIZE

/***********************************************************************************************************************
 * Type 4 Tag NDEF Read — via PTX SDK ptxNDEF_T4TOP component
 **********************************************************************************************************************/

static rs_status_t read_t4t_ndef(rs_nfc_card_result_t *res)
{
    res->tag_type_name = "ISO-DEP (Type 4 Tag / ISO 14443-4)";

    rs_status_t st = rs_nfc_ptx_ndef_open();
    if (RS_OK != st) { return st; }

    struct ptxNDEF_T4TOP *t4t = rs_nfc_ptx_get_ndef_comp();

    ptxStatus_t ptx_st = ptxNDEF_T4TOpCheckMessage(t4t);
    if (ptxStatus_Success != ptx_st)
    {
        rs_nfc_ptx_ndef_close();
        res->ndef_present   = false;
        res->ndef_len       = 0;
        return RS_ERR_NOT_FOUND;
    }

    uint32_t msg_len = RS_NFC_NDEF_MAX_BYTES;
    ptx_st = ptxNDEF_T4TOpReadMessage(t4t, res->ndef_data, &msg_len);

    if (ptxStatus_Success == ptx_st)
    {
        res->ndef_len     = (uint16_t)msg_len;
        res->ndef_present = (msg_len > 0u);
    }
    else
    {
        res->ndef_present = false;
        res->ndef_len     = 0;
    }

    /* Extract CC metadata from the SDK T4TOP component */
    res->data_area_size = t4t->CCParams.NDEFFileSize;
    res->writeable      = (0x00u == t4t->CCParams.NDEFAccessWrite);

    rs_nfc_ptx_ndef_close();
    return RS_OK;
}

/***********************************************************************************************************************
 * Type 5 Tag NDEF Read — via PTX SDK ptxNDEF_T5TOP component
 **********************************************************************************************************************/

static rs_status_t read_t5t_ndef(rs_nfc_card_result_t *res)
{
    res->tag_type_name = "NFC Forum Type 5 Tag (T5T/ISO 15693)";

    rs_status_t st = rs_nfc_ptx_ndef_t5t_open();
    if (RS_OK != st) { return st; }

    struct ptxNDEF_T5TOP *t5t = rs_nfc_ptx_get_ndef_t5t_comp();

    ptxStatus_t ptx_st = ptxNDEF_T5TOpCheckMessage(t5t);
    if (ptxStatus_Success != ptx_st)
    {
        rs_nfc_ptx_ndef_t5t_close();
        res->ndef_present   = false;
        res->ndef_len       = 0;
        return RS_ERR_NOT_FOUND;
    }

    uint32_t msg_len = RS_NFC_NDEF_MAX_BYTES;
    ptx_st = ptxNDEF_T5TOpReadMessage(t5t, res->ndef_data, &msg_len);

    if (ptxStatus_Success == ptx_st)
    {
        res->ndef_len     = (uint16_t)msg_len;
        res->ndef_present = (msg_len > 0u);
    }
    else
    {
        res->ndef_present = false;
        res->ndef_len     = 0;
    }

    /* Extract CC metadata from the SDK T5TOP component */
    res->data_area_size = (uint32_t)t5t->CCParams.MLEN;
    res->writeable      = (0x00u == t5t->CCParams.WriteAccess);

    rs_nfc_ptx_ndef_t5t_close();
    return RS_OK;
}

/***********************************************************************************************************************
 * Type 3 Tag NDEF Read — via PTX SDK ptxNDEF_T3TOP component
 **********************************************************************************************************************/

static rs_status_t read_t3t_ndef (rs_nfc_card_result_t *res)
{
    res->tag_type_name = "NFC Forum Type 3 Tag (T3T/FeliCa)";

    rs_status_t st = rs_nfc_ptx_ndef_t3t_open();
    if (RS_OK != st) { return st; }

    struct ptxNDEF_T3TOP *t3t = rs_nfc_ptx_get_ndef_t3t_comp();

    ptxStatus_t ptx_st = ptxNDEF_T3TOpCheckMessage(t3t);
    if (ptxStatus_Success != ptx_st)
    {
        rs_nfc_ptx_ndef_t3t_close();
        res->ndef_present   = false;
        res->ndef_len       = 0;
        return RS_ERR_NOT_FOUND;
    }

    uint32_t msg_len = RS_NFC_NDEF_MAX_BYTES;
    ptx_st = ptxNDEF_T3TOpReadMessage(t3t, res->ndef_data, &msg_len);

    if (ptxStatus_Success == ptx_st)
    {
        res->ndef_len     = (uint16_t)msg_len;
        res->ndef_present = (msg_len > 0u);
    }
    else
    {
        res->ndef_present = false;
        res->ndef_len     = 0;
    }

    /* Extract CC metadata from the SDK T3TOP component.
     * NmaxB is the maximum number of 16-byte blocks available for NDEF data.
     * RWFlag: 0x00 = read-only, non-zero = read/write. */
    res->data_area_size = (uint32_t)t3t->CCParams.NmaxB * PTX_T3T_BLOCK_SIZE;
    res->writeable      = (0x00u != t3t->CCParams.RWFlag);

    rs_nfc_ptx_ndef_t3t_close();

    return RS_OK;
}

/***********************************************************************************************************************
 * Type 2 Tag NDEF Read — hand-rolled (small footprint, proven)
 **********************************************************************************************************************/
static rs_status_t read_t2t_ndef(rs_nfc_card_result_t *res)
{
    uint8_t rx[RX_BUF_SIZE];
    uint8_t cmd[2];
    uint32_t rx_len;
    uint8_t  data_buf[TX_BUF_SIZE];   /* accumulate TLV area here */

    /* READ block 3 -> CC (response = blocks 3..6, 16 bytes) */
    cmd[0] = 0x30; cmd[1] = 0x03;
    rx_len = RX_BUF_SIZE;
    rs_status_t st = rs_nfc_ptx_data_exchange(cmd, 2u, rx, &rx_len);
    if ((RS_OK != st) || (rx_len < 4u))
    {
        return RS_ERR_NOT_FOUND;
    }

    /* CC: [0]=magic(0xE1) [1]=version [2]=size(x8) [3]=access */
    if (0xE1u != rx[0])
    {
        return RS_ERR_NOT_FOUND;  /* not NDEF formatted */
    }

    uint32_t data_area  = (uint32_t)rx[2] * 8u;
    uint8_t  wa_nibble  = (uint8_t)(rx[3] & 0x0Fu);

    res->data_area_size = data_area;
    res->writeable      = (0x00u == wa_nibble);
    res->tag_type_name  = "NFC Forum Type 2 Tag (T2T)";

    /* Read the data area (starting at block 4) */
    uint32_t cap = (data_area > (uint32_t)TX_BUF_SIZE)
                   ? (uint32_t)TX_BUF_SIZE : data_area;
    if (0u == cap) { cap = (uint32_t)TX_BUF_SIZE; }
    uint32_t got   = 0;
    uint8_t  block = 4u;

    while (got < cap)
    {
        cmd[0] = 0x30; cmd[1] = block;
        rx_len = RX_BUF_SIZE;
        st = rs_nfc_ptx_data_exchange(cmd, 2u, rx, &rx_len);
        if ((RS_OK != st) || (rx_len < 4u))
        {
            break;
        }

        uint32_t take = (rx_len < 16u) ? rx_len : 16u;
        if ((got + take) > cap) { take = cap - got; }
        (void)memcpy(&data_buf[got], rx, take);
        got += take;

        if ((uint32_t)block + 4u > 0xFFu) { break; }
        block = (uint8_t)(block + 4u);
    }

    /* Walk TLV area, locate NDEF Message TLV (tag 0x03) */
    uint32_t p = 0;
    while (p < got)
    {
        uint8_t t = data_buf[p++];
        if (0x00u == t) { continue; }     /* NULL TLV       */
        if (0xFEu == t) { break;    }     /* Terminator TLV */
        if (p >= got)   { break; }

        uint32_t l = data_buf[p++];
        if (0xFFu == l)  /* 3-byte length form */
        {
            if ((p + 2u) > got) { break; }
            l = ((uint32_t)data_buf[p] << 8) | data_buf[p + 1u];
            p += 2u;
        }

        if (0x03u == t)  /* NDEF Message TLV */
        {
            if ((p + l) > got) { l = got - p; }
            uint32_t copy = (l > RS_NFC_NDEF_MAX_BYTES)
                            ? RS_NFC_NDEF_MAX_BYTES : l;
            (void)memcpy(res->ndef_data, &data_buf[p], copy);
            res->ndef_len     = (uint16_t)copy;
            res->ndef_present = (copy > 0u);
            return RS_OK;
        }

        p += l;  /* skip Lock/Memory/other TLVs */
    }

    /* No NDEF TLV found */
    res->ndef_present = false;
    res->ndef_len     = 0;
    return RS_OK;
}

/***********************************************************************************************************************
 * Public API
 **********************************************************************************************************************/

rs_status_t RS_NFCReader_ReadCardInfo(rs_nfc_protocol_t protocol,
                                            rs_nfc_card_result_t *result)
{
    if (NULL == result) { return RS_ERR_INVALID_CFG; }

    /* Clear the extended fields */
    result->ndef_present   = false;
    result->ndef_len       = 0;
    result->data_area_size = 0;
    result->writeable      = false;
    result->tag_type_name  = NULL;

    switch (protocol)
    {
        case RS_NFC_PROT_ISODEP:
            return read_t4t_ndef(result);

        case RS_NFC_PROT_T2T:
            return read_t2t_ndef(result);

        case RS_NFC_PROT_T5T:
            return read_t5t_ndef(result);

        case RS_NFC_PROT_T3T:
            return read_t3t_ndef(result);

        case RS_NFC_PROT_NFCDEP:
            result->tag_type_name = "NFC-DEP (Peer-to-Peer)";
            return RS_OK;

        default:
            result->tag_type_name = "Unknown";
            return RS_OK;
    }
}

/***********************************************************************************************************************
 * NDEF parsing and decoding utilities
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * BER-TLV search
 **********************************************************************************************************************/

bool RS_NDEF_TlvFind(const uint8_t *buf, uint32_t len, uint16_t tag,
                      const uint8_t **val, uint32_t *val_len)
{
    uint32_t i = 0;
    while (i < len)
    {
        if ((0x00u == buf[i]) || (0xFFu == buf[i])) { i++; continue; }

        uint16_t ct = (uint16_t)buf[i];
        uint8_t  constr = (uint8_t)(buf[i] & 0x20u);
        i++;
        if (((ct & 0x1Fu) == 0x1Fu) && (i < len))
        {
            ct = (uint16_t)((ct << 8) | buf[i]);
            i++;
        }
        if (i >= len) { break; }

        uint32_t cl = (uint32_t)buf[i];
        i++;
        if (0u != (cl & 0x80u))
        {
            uint8_t n = (uint8_t)(cl & 0x7Fu);
            cl = 0;
            while (n-- && (i < len))
            {
                cl = (cl << 8) | buf[i];
                i++;
            }
        }
        if ((i + cl) > len) { break; }

        if (ct == tag)
        {
            *val     = &buf[i];
            *val_len = cl;
            return true;
        }
        if (constr && RS_NDEF_TlvFind(&buf[i], cl, tag, val, val_len))
        {
            return true;
        }
        i += cl;
    }
    return false;
}

/***********************************************************************************************************************
 * String helpers
 **********************************************************************************************************************/

bool RS_NDEF_TypeEquals(const uint8_t *type, uint8_t type_len,
                         const char *str)
{
    uint32_t n = 0;
    while (str[n] != '\0') { n++; }
    if (n != (uint32_t)type_len) { return false; }
    for (uint32_t k = 0; k < n; k++)
    {
        if (type[k] != (uint8_t)str[k]) { return false; }
    }
    return true;
}

bool RS_NDEF_StartsWith(const char *str, uint32_t str_len,
                         const char *prefix)
{
    uint32_t n = 0;
    while (prefix[n] != '\0')
    {
        if ((n >= str_len) || (str[n] != prefix[n])) { return false; }
        n++;
    }
    return true;
}

/***********************************************************************************************************************
 * NDEF message decoder
 **********************************************************************************************************************/

rs_status_t RS_NDEF_DecodeMessage(const uint8_t *msg, uint32_t len,
                                    rs_ndef_decoded_t *out)
{
    if ((NULL == out)) { return RS_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));

    if ((NULL == msg) || (0u == len))
    {
        return RS_ERR_NOT_FOUND;
    }

    uint32_t pos = 0;

    while (pos < len)
    {
        if (out->record_count >= RS_NDEF_MAX_RECORDS)
        {
            out->truncated = true;
            break;
        }

        rs_ndef_record_t *rec = &out->records[out->record_count];
        uint8_t hdr = msg[pos++];
        rec->flags = hdr;
        rec->tnf   = (uint8_t)(hdr & 0x07u);

        bool mb = (0u != (hdr & 0x80u));
        bool me = (0u != (hdr & 0x40u));
        bool sr = (0u != (hdr & 0x10u));
        bool il = (0u != (hdr & 0x08u));
        (void)mb;

        if (pos >= len) { break; }
        uint8_t type_len = msg[pos++];

        uint32_t payload_len;
        if (sr)
        {
            if (pos >= len) { break; }
            payload_len = (uint32_t)msg[pos++];
        }
        else
        {
            if ((pos + 4u) > len) { break; }
            payload_len = ((uint32_t)msg[pos] << 24) |
                          ((uint32_t)msg[pos + 1u] << 16) |
                          ((uint32_t)msg[pos + 2u] << 8) |
                          ((uint32_t)msg[pos + 3u]);
            pos += 4u;
        }

        uint8_t id_len = 0;
        if (il)
        {
            if (pos >= len) { break; }
            id_len = msg[pos++];
        }

        if ((pos + type_len) > len) { break; }
        uint8_t copy_type = (type_len <= RS_NDEF_MAX_TYPE_LEN)
                            ? type_len : RS_NDEF_MAX_TYPE_LEN;
        (void)memcpy(rec->type, &msg[pos], copy_type);
        rec->type_len = type_len;
        pos += type_len;

        if ((pos + id_len) > len) { break; }
        pos += id_len;

        if ((pos + payload_len) > len) { break; }
        rec->payload     = &msg[pos];
        rec->payload_len = payload_len;
        pos += payload_len;

        out->record_count++;

        if (me) { break; }
    }

    return RS_OK;
}

/***********************************************************************************************************************
 * Wi-Fi WSC attribute search
 **********************************************************************************************************************/

static bool wsc_find(const uint8_t *buf, uint32_t len, uint16_t want,
                     const uint8_t **val, uint16_t *vlen)
{
    uint32_t i = 0;
    while ((i + 4u) <= len)
    {
        uint16_t t = (uint16_t)(((uint16_t)buf[i] << 8) | buf[i + 1u]);
        uint16_t l = (uint16_t)(((uint16_t)buf[i + 2u] << 8) | buf[i + 3u]);
        i += 4u;
        if (((uint32_t)i + l) > len) { break; }
        if (t == want) { *val = &buf[i]; *vlen = l; return true; }
        if (0x100Eu == t)
        {
            if (wsc_find(&buf[i], l, want, val, vlen)) { return true; }
        }
        i += l;
    }
    return false;
}

rs_status_t RS_NDEF_DecodeWifi(const uint8_t *payload, uint32_t len,
                                 rs_wifi_info_t *out)
{
    if (NULL == out) { return RS_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));

    const uint8_t *v;
    uint16_t vl;

    if (!wsc_find(payload, len, 0x1045u, &v, &vl))
    {
        return RS_ERR_NOT_FOUND;
    }
    uint8_t copy = (vl <= RS_NDEF_WIFI_SSID_MAX)
                   ? (uint8_t)vl : RS_NDEF_WIFI_SSID_MAX;
    (void)memcpy(out->ssid, v, copy);
    out->ssid[copy]  = '\0';
    out->ssid_len    = copy;

    if (wsc_find(payload, len, 0x1003u, &v, &vl) && (vl >= 2u))
    {
        out->auth_type = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    }

    if (wsc_find(payload, len, 0x100Fu, &v, &vl) && (vl >= 2u))
    {
        out->enc_type = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    }

    if (wsc_find(payload, len, 0x1027u, &v, &vl))
    {
        uint8_t pc = (vl <= RS_NDEF_WIFI_PASS_MAX)
                     ? (uint8_t)vl : RS_NDEF_WIFI_PASS_MAX;
        (void)memcpy(out->password, v, pc);
        out->password[pc] = '\0';
        out->password_len = pc;
    }

    if (wsc_find(payload, len, 0x1020u, &v, &vl) && (vl >= 6u))
    {
        (void)memcpy(out->mac_addr, v, 6u);
        out->mac_present = true;
    }

    return RS_OK;
}

/***********************************************************************************************************************
 * Bluetooth OOB decoder
 **********************************************************************************************************************/

rs_status_t RS_NDEF_DecodeBluetooth(const uint8_t *payload, uint32_t len,
                                      bool is_le, rs_bt_info_t *out)
{
    if (NULL == out) { return RS_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));
    out->is_le = is_le;

    uint32_t eir = is_le ? 0u : 8u;
    if ((!is_le) && (len >= 8u))
    {
        for (uint8_t k = 0; k < 6u; k++)
        {
            out->bd_addr[k] = payload[2u + (5u - k)];
        }
        out->addr_present = true;
    }

    uint32_t i = eir;
    while ((i + 1u) < len)
    {
        uint8_t l = payload[i];
        if (0u == l) { break; }
        if (((uint32_t)i + 1u + l) > len) { break; }
        uint8_t adt = payload[i + 1u];
        if ((0x09u == adt) || (0x08u == adt))
        {
            uint8_t nc = (uint8_t)(l - 1u);
            if (nc > RS_NDEF_BT_NAME_MAX) { nc = RS_NDEF_BT_NAME_MAX; }
            (void)memcpy(out->local_name, &payload[i + 2u], nc);
            out->local_name[nc] = '\0';
            out->name_len = nc;
        }
        i += (uint32_t)l + 1u;
    }

    return RS_OK;
}

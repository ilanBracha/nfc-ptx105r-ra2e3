/**
 * pes_ndef_util.c
 *
 * NDEF parsing and decoding utilities.
 * Pure data transforms — NO printing or logging.
 */

#include "pes_ndef_util.h"
#include <string.h>

/* ── BER-TLV search ────────────────────────────────────────────────── */

bool PES_NDEF_TlvFind(const uint8_t *buf, uint32_t len, uint16_t tag,
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
        if (constr && PES_NDEF_TlvFind(&buf[i], cl, tag, val, val_len))
        {
            return true;
        }
        i += cl;
    }
    return false;
}

/* ── String helpers ────────────────────────────────────────────────── */

bool PES_NDEF_TypeEquals(const uint8_t *type, uint8_t type_len,
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

bool PES_NDEF_StartsWith(const char *str, uint32_t str_len,
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

/* ── NDEF message decoder ──────────────────────────────────────────── */

pes_status_t PES_NDEF_DecodeMessage(const uint8_t *msg, uint32_t len,
                                    pes_ndef_decoded_t *out)
{
    if ((NULL == out)) { return PES_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));

    if ((NULL == msg) || (0u == len))
    {
        return PES_ERR_NOT_FOUND;
    }

    uint32_t pos = 0;

    while (pos < len)
    {
        if (out->record_count >= PES_NDEF_MAX_RECORDS)
        {
            out->truncated = true;
            break;
        }

        pes_ndef_record_t *rec = &out->records[out->record_count];
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

        /* Payload length */
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

        /* ID length (optional) */
        uint8_t id_len = 0;
        if (il)
        {
            if (pos >= len) { break; }
            id_len = msg[pos++];
        }

        /* Type */
        if ((pos + type_len) > len) { break; }
        uint8_t copy_type = (type_len <= PES_NDEF_MAX_TYPE_LEN)
                            ? type_len : PES_NDEF_MAX_TYPE_LEN;
        (void)memcpy(rec->type, &msg[pos], copy_type);
        rec->type_len = type_len;
        pos += type_len;

        /* ID (skip) */
        if ((pos + id_len) > len) { break; }
        pos += id_len;

        /* Payload */
        if ((pos + payload_len) > len) { break; }
        rec->payload     = &msg[pos];
        rec->payload_len = payload_len;
        pos += payload_len;

        out->record_count++;

        if (me) { break; }
    }

    return PES_OK;
}

/* ── Wi-Fi WSC attribute search ────────────────────────────────────── */

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
        if (0x100Eu == t)  /* Credential -> nested */
        {
            if (wsc_find(&buf[i], l, want, val, vlen)) { return true; }
        }
        i += l;
    }
    return false;
}

pes_status_t PES_NDEF_DecodeWifi(const uint8_t *payload, uint32_t len,
                                 pes_wifi_info_t *out)
{
    if (NULL == out) { return PES_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));

    const uint8_t *v;
    uint16_t vl;

    /* SSID (0x1045) */
    if (!wsc_find(payload, len, 0x1045u, &v, &vl))
    {
        return PES_ERR_NOT_FOUND;
    }
    uint8_t copy = (vl <= PES_NDEF_WIFI_SSID_MAX)
                   ? (uint8_t)vl : PES_NDEF_WIFI_SSID_MAX;
    (void)memcpy(out->ssid, v, copy);
    out->ssid[copy]  = '\0';
    out->ssid_len    = copy;

    /* Auth type (0x1003) */
    if (wsc_find(payload, len, 0x1003u, &v, &vl) && (vl >= 2u))
    {
        out->auth_type = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    }

    /* Encryption type (0x100F) */
    if (wsc_find(payload, len, 0x100Fu, &v, &vl) && (vl >= 2u))
    {
        out->enc_type = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
    }

    /* Network Key (0x1027) */
    if (wsc_find(payload, len, 0x1027u, &v, &vl))
    {
        uint8_t pc = (vl <= PES_NDEF_WIFI_PASS_MAX)
                     ? (uint8_t)vl : PES_NDEF_WIFI_PASS_MAX;
        (void)memcpy(out->password, v, pc);
        out->password[pc] = '\0';
        out->password_len = pc;
    }

    /* MAC address (0x1020) */
    if (wsc_find(payload, len, 0x1020u, &v, &vl) && (vl >= 6u))
    {
        (void)memcpy(out->mac_addr, v, 6u);
        out->mac_present = true;
    }

    return PES_OK;
}

/* ── Bluetooth OOB decoder ─────────────────────────────────────────── */

pes_status_t PES_NDEF_DecodeBluetooth(const uint8_t *payload, uint32_t len,
                                      bool is_le, pes_bt_info_t *out)
{
    if (NULL == out) { return PES_ERR_INVALID_CFG; }
    (void)memset(out, 0, sizeof(*out));
    out->is_le = is_le;

    /* BR/EDR OOB: [0..1]=total len (LE16), [2..7]=BD_ADDR (LE) then EIR */
    uint32_t eir = is_le ? 0u : 8u;
    if ((!is_le) && (len >= 8u))
    {
        /* BD_ADDR is stored LSB-first; reverse to MSB-first */
        for (uint8_t k = 0; k < 6u; k++)
        {
            out->bd_addr[k] = payload[2u + (5u - k)];
        }
        out->addr_present = true;
    }

    /* Scan EIR/AD structures for a local name */
    uint32_t i = eir;
    while ((i + 1u) < len)
    {
        uint8_t l = payload[i];
        if (0u == l) { break; }
        if (((uint32_t)i + 1u + l) > len) { break; }
        uint8_t adt = payload[i + 1u];
        if ((0x09u == adt) || (0x08u == adt))  /* complete / shortened */
        {
            uint8_t nc = (uint8_t)(l - 1u);
            if (nc > PES_NDEF_BT_NAME_MAX) { nc = PES_NDEF_BT_NAME_MAX; }
            (void)memcpy(out->local_name, &payload[i + 2u], nc);
            out->local_name[nc] = '\0';
            out->name_len = nc;
        }
        i += (uint32_t)l + 1u;
    }

    return PES_OK;
}

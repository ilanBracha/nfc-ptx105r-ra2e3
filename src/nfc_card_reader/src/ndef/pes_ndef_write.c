/**
 * pes_ndef_write.c
 *
 * NDEF message writing / erasing for NFC Forum Type 2 and Type 4 Tags
 * using raw protocol commands via the HAL data_exchange vtable.
 * Also provides PES_NDEF_BuildTextRecord() for constructing RTD-Text records.
 *
 * NO printing — returns pes_status_t only.
 */

#include "pes_nfc_card_reader.h"
#include "pes_nfc_ptx105r.h"
#include <string.h>

/* ── Constants ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE   PES_NFC_PTX_RX_BUF_SIZE

/* ── Internal helper: T4T APDU exchange with SW=9000 check ─────────── */
static bool t4t_exchange(uint8_t *cmd, uint32_t cmd_len,
                         uint8_t *rx, uint32_t *rx_len)
{
    *rx_len = RX_BUF_SIZE;
    pes_status_t st = pes_nfc_ptx_data_exchange(cmd, cmd_len,
                                                rx, rx_len);
    if ((PES_OK != st) || (*rx_len < 2u) ||
        (0x90u != rx[*rx_len - 2u]) || (0x00u != rx[*rx_len - 1u]))
    {
        return false;
    }
    return true;
}

/* ── Type 4 Tag NDEF Write/Erase ───────────────────────────────────── */

static pes_status_t write_t4t_ndef(const uint8_t *ndef, uint16_t ndef_len)
{
    uint8_t rx[RX_BUF_SIZE];
    uint32_t rx_len;
    uint8_t cmd[5u + 248u];

    if (ndef_len > 248u) { return PES_ERR_INVALID_CFG; }

    /* SELECT NDEF Tag Application */
    static const uint8_t sel_app[] = {
        0x00,0xA4,0x04,0x00,0x07,
        0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00
    };
    if (!t4t_exchange((uint8_t *)sel_app, (uint32_t)sizeof(sel_app),
                      rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* SELECT CC */
    static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
    if (!t4t_exchange((uint8_t *)sel_cc, (uint32_t)sizeof(sel_cc),
                      rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* READ CC -> learn NDEF file id and check write access */
    static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
    if (!t4t_exchange((uint8_t *)read_cc, (uint32_t)sizeof(read_cc),
                      rx, &rx_len) || (rx_len < 17u))
    {
        return PES_ERR_NOT_FOUND;
    }
    uint8_t fid_hi = rx[9];
    uint8_t fid_lo = rx[10];
    if (0x00u != rx[14])
    {
        return PES_ERR_INVALID_CFG;  /* tag is read-only */
    }

    /* SELECT NDEF file */
    cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C;
    cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
    if (!t4t_exchange(cmd, 7u, rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* UPDATE BINARY @0: NLEN = 0 (erase / start of partial write) */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x00;
    cmd[4]=0x02; cmd[5]=0x00; cmd[6]=0x00;
    if (!t4t_exchange(cmd, 7u, rx, &rx_len))
    {
        return PES_ERR_INTERNAL;
    }

    if (0u == ndef_len) { return PES_OK; }  /* erase complete */

    /* UPDATE BINARY @2: NDEF body */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x02;
    cmd[4]=(uint8_t)ndef_len;
    (void)memcpy(&cmd[5], ndef, ndef_len);
    if (!t4t_exchange(cmd, (uint32_t)(5u + ndef_len), rx, &rx_len))
    {
        return PES_ERR_INTERNAL;
    }

    /* UPDATE BINARY @0: NLEN = ndef_len (commit, big-endian) */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
    cmd[5]=(uint8_t)(ndef_len >> 8);
    cmd[6]=(uint8_t)(ndef_len & 0xFFu);
    if (!t4t_exchange(cmd, 7u, rx, &rx_len))
    {
        return PES_ERR_INTERNAL;
    }

    return PES_OK;
}

/* ── Type 2 Tag NDEF Write/Erase ───────────────────────────────────── */

static pes_status_t write_t2t_ndef(const uint8_t *ndef, uint16_t ndef_len)
{
    uint8_t  tlv[3u + 248u + 1u];
    uint16_t tlv_len = 0u;
    uint32_t rx_len;
    uint8_t  cmd[6];
    uint8_t  rx[RX_BUF_SIZE];

    if (ndef_len > 248u) { return PES_ERR_INVALID_CFG; }

    /* Build the NDEF Message TLV: 0x03 <len> <msg...> 0xFE */
    tlv[tlv_len++] = 0x03u;
    tlv[tlv_len++] = (uint8_t)ndef_len;
    if ((ndef_len > 0u) && (NULL != ndef))
    {
        (void)memcpy(&tlv[tlv_len], ndef, ndef_len);
        tlv_len = (uint16_t)(tlv_len + ndef_len);
    }
    tlv[tlv_len++] = 0xFEu;  /* Terminator TLV */

    /* Write 4-byte pages starting at block 4 */
    uint16_t offset = 0u;
    uint8_t  block  = 4u;
    while (offset < tlv_len)
    {
        uint8_t chunk = ((uint16_t)(tlv_len - offset) >= 4u)
                        ? 4u : (uint8_t)(tlv_len - offset);

        cmd[0] = 0xA2u;          /* T2T WRITE */
        cmd[1] = block;
        (void)memset(&cmd[2], 0, 4u);
        (void)memcpy(&cmd[2], &tlv[offset], chunk);

        rx_len = RX_BUF_SIZE;
        pes_status_t st = pes_nfc_ptx_data_exchange(
            cmd, 6u, rx, &rx_len);
        if (PES_OK != st)
        {
            return PES_ERR_INTERNAL;
        }

        offset = (uint16_t)(offset + chunk);
        if ((uint16_t)block + 1u > 0xFFu) { break; }
        block = (uint8_t)(block + 1u);
    }

    return PES_OK;
}

/* ── Public API: Write NDEF ────────────────────────────────────────── */

pes_status_t PES_NFCCardReader_WriteNDEF(pes_nfc_protocol_t protocol,
                                         const uint8_t *ndef,
                                         uint16_t ndef_len)
{
    switch (protocol)
    {
        case PES_NFC_PROT_T2T:
            return write_t2t_ndef(ndef, ndef_len);

        case PES_NFC_PROT_ISODEP:
            return write_t4t_ndef(ndef, ndef_len);

        default:
            return PES_ERR_INVALID_CFG;  /* protocol not supported */
    }
}

/* ── Public API: Erase NDEF ────────────────────────────────────────── */

pes_status_t PES_NFCCardReader_EraseNDEF(pes_nfc_protocol_t protocol)
{
    /* Erasing is writing with length 0 */
    return PES_NFCCardReader_WriteNDEF(protocol, NULL, 0u);
}

/* ── Public API: Build RTD-Text Record ─────────────────────────────── */

pes_status_t PES_NDEF_BuildTextRecord(const char *text, uint16_t text_len,
                                      uint8_t *out, uint16_t *out_len)
{
    if ((NULL == text) || (NULL == out) || (NULL == out_len))
    {
        return PES_ERR_INVALID_CFG;
    }
    if ((0u == text_len) || (text_len > 248u))
    {
        return PES_ERR_INVALID_CFG;
    }

    /*
     * Layout (Short-Record form, MB=ME=1, TNF=0x01 Well-known, type 'T'):
     *   [0] 0xD1           header
     *   [1] 0x01           type length
     *   [2] payload_len    (1 status + 2 lang + text_len)
     *   [3] 0x54           type 'T'
     *   [4] 0x02           status: UTF-8, lang_len=2
     *   [5..6] 'e','n'     language code
     *   [7..]  text        UTF-8 payload
     */
    const uint8_t payload_len = (uint8_t)(1u + 2u + text_len);

    out[0] = 0xD1u;
    out[1] = 0x01u;
    out[2] = payload_len;
    out[3] = 0x54u;
    out[4] = 0x02u;
    out[5] = 0x65u;  /* 'e' */
    out[6] = 0x6Eu;  /* 'n' */
    (void)memcpy(&out[7], text, text_len);

    *out_len = (uint16_t)(7u + text_len);
    return PES_OK;
}

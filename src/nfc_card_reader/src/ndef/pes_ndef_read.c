/**
 * pes_ndef_read.c
 *
 * NDEF message reading for Type 2 and Type 4 Tags via raw data exchange.
 * Uses only the pes_nfc_hal API — no direct FSP or PTX SDK calls.
 */

#include "pes_nfc_hal.h"
#include "pes_nfc_internal.h"
#include <string.h>

#define NDEF_RX_BUF  PES_NFC_HAL_RX_BUF_SIZE

/* ── Type 4 Tag helpers ───────────────────────────────────────────── */

static pes_status_t t4t_exchange_ok(const uint8_t *cmd, uint32_t cmd_len,
                                    uint8_t *rx, uint32_t *rx_len)
{
    *rx_len = NDEF_RX_BUF;
    pes_status_t st = pes_nfc_hal_data_exchange(cmd, cmd_len, rx, rx_len);
    if (PES_OK != st)          { return st; }
    if (*rx_len < 2u)          { return PES_ERR_INTERNAL; }
    if ((0x90u != rx[*rx_len - 2u]) || (0x00u != rx[*rx_len - 1u]))
    {
        return PES_ERR_INTERNAL;
    }
    return PES_OK;
}

/**
 * Read NDEF from an NFC Forum Type 4 Tag.
 * Populates result_out->ndef_data/ndef_len on success.
 */
pes_status_t pes_ndef_read_t4t(pes_nfc_card_result_t *result_out)
{
    uint8_t rx[NDEF_RX_BUF];
    uint32_t rx_len;
    uint8_t cmd[16];

    /* 1. SELECT NDEF Tag Application (AID D276000085010100) */
    static const uint8_t sel_app[] = {0x00,0xA4,0x04,0x00,0x07,
                                      0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00};
    if (PES_OK != t4t_exchange_ok(sel_app, sizeof(sel_app), rx, &rx_len))
        return PES_ERR_NOT_FOUND;

    /* 2. SELECT Capability Container (EF E103) */
    static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
    if (PES_OK != t4t_exchange_ok(sel_cc, sizeof(sel_cc), rx, &rx_len))
        return PES_ERR_NOT_FOUND;

    /* 3. READ CC (15 bytes) */
    static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
    if (PES_OK != t4t_exchange_ok(read_cc, sizeof(read_cc), rx, &rx_len))
        return PES_ERR_NOT_FOUND;
    if (rx_len < 17u) return PES_ERR_NOT_FOUND;

    uint16_t mle    = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);
    uint8_t fid_hi  = rx[9];
    uint8_t fid_lo  = rx[10];

    /* 4. SELECT NDEF file */
    cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C;
    cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
    if (PES_OK != t4t_exchange_ok(cmd, 7u, rx, &rx_len))
        return PES_ERR_NOT_FOUND;

    /* 5. READ NLEN (first 2 bytes) */
    cmd[0]=0x00; cmd[1]=0xB0; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
    if (PES_OK != t4t_exchange_ok(cmd, 5u, rx, &rx_len))
        return PES_ERR_NOT_FOUND;
    if (rx_len < 4u) return PES_ERR_NOT_FOUND;

    uint16_t nlen = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    if (0u == nlen) { result_out->ndef_present = true; result_out->ndef_len = 0; return PES_OK; }

    /* 6. READ NDEF message body in chunks */
    uint32_t chunk = ((0u == mle) || (mle > 0xFFu)) ? 0xFFu : (uint32_t)mle;
    uint32_t total = (nlen > PES_NFC_NDEF_MAX_BYTES) ? PES_NFC_NDEF_MAX_BYTES : (uint32_t)nlen;
    uint32_t got = 0;
    uint16_t offset = 2u;

    while (got < total)
    {
        uint32_t want = total - got;
        if (want > chunk) want = chunk;

        cmd[0]=0x00; cmd[1]=0xB0;
        cmd[2]=(uint8_t)(offset >> 8);
        cmd[3]=(uint8_t)(offset & 0xFFu);
        cmd[4]=(uint8_t)want;

        if (PES_OK != t4t_exchange_ok(cmd, 5u, rx, &rx_len)) break;

        uint32_t data = rx_len - 2u;
        if (data > want) data = want;
        if (0u == data) break;

        (void)memcpy(&result_out->ndef_data[got], rx, data);
        got    += data;
        offset  = (uint16_t)(offset + data);
    }

    result_out->ndef_present = true;
    result_out->ndef_len     = (uint16_t)got;
    return PES_OK;
}

/* ── Type 2 Tag ───────────────────────────────────────────────────── */

/**
 * Read NDEF from an NFC Forum Type 2 Tag.
 */
pes_status_t pes_ndef_read_t2t(pes_nfc_card_result_t *result_out)
{
    uint8_t rx[NDEF_RX_BUF];
    uint32_t rx_len;
    uint8_t cmd[2];
    uint8_t data_buf[PES_NFC_NDEF_MAX_BYTES];

    /* READ block 3 -> CC (response = blocks 3..6, ≥16 bytes) */
    cmd[0] = 0x30; cmd[1] = 0x03;
    rx_len = NDEF_RX_BUF;
    if (PES_OK != pes_nfc_hal_data_exchange(cmd, 2u, rx, &rx_len)) return PES_ERR_NOT_FOUND;
    if ((rx_len < 4u) || (0xE1u != rx[0])) return PES_ERR_NOT_FOUND;

    uint32_t data_area = (uint32_t)rx[2] * 8u;
    uint32_t cap = (data_area > PES_NFC_NDEF_MAX_BYTES) ? PES_NFC_NDEF_MAX_BYTES : data_area;
    if (0u == cap) cap = PES_NFC_NDEF_MAX_BYTES;

    /* Read data area starting at block 4 */
    uint32_t got = 0;
    uint8_t block = 4u;
    while (got < cap)
    {
        cmd[0] = 0x30; cmd[1] = block;
        rx_len = NDEF_RX_BUF;
        if (PES_OK != pes_nfc_hal_data_exchange(cmd, 2u, rx, &rx_len)) break;
        if (rx_len < 4u) break;

        uint32_t take = (rx_len < 16u) ? rx_len : 16u;
        if ((got + take) > cap) take = cap - got;
        (void)memcpy(&data_buf[got], rx, take);
        got += take;
        if ((uint32_t)block + 4u > 0xFFu) break;
        block = (uint8_t)(block + 4u);
    }

    /* Walk TLV area for NDEF Message TLV (0x03) */
    uint32_t p = 0;
    while (p < got)
    {
        uint8_t t = data_buf[p++];
        if (0x00u == t) continue;
        if (0xFEu == t) break;
        if (p >= got) break;

        uint32_t l = data_buf[p++];
        if (0xFFu == l)
        {
            if ((p + 2u) > got) break;
            l = ((uint32_t)data_buf[p] << 8) | data_buf[p + 1u];
            p += 2u;
        }

        if (0x03u == t)
        {
            if ((p + l) > got) l = got - p;
            uint16_t copy = (l > PES_NFC_NDEF_MAX_BYTES) ? PES_NFC_NDEF_MAX_BYTES : (uint16_t)l;
            (void)memcpy(result_out->ndef_data, &data_buf[p], copy);
            result_out->ndef_len     = copy;
            result_out->ndef_present = true;
            return PES_OK;
        }
        p += l;
    }

    /* Tag is NDEF-formatted but no NDEF TLV found */
    result_out->ndef_present = true;
    result_out->ndef_len     = 0;
    return PES_OK;
}

/**
 * pes_ndef_read.c
 *
 * NDEF message reading from NFC Forum Type 2 and Type 4 Tags using raw
 * protocol commands via the HAL data_exchange vtable.
 *
 * NO printing — all results go into the caller's pes_nfc_card_result_t.
 */

#include "pes_nfc_hal.h"
#include "pes_nfc_internal.h"
#include "pes_nfc_card_reader.h"
#include <string.h>

/* ── Constants ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE   PES_NFC_HAL_RX_BUF_SIZE
#define TX_BUF_SIZE   PES_NFC_HAL_TX_BUF_SIZE

/* ── Internal helper: T4T APDU exchange with SW=9000 check ─────────── */
static bool t4t_exchange(uint8_t *cmd, uint32_t cmd_len,
                         uint8_t *rx, uint32_t *rx_len)
{
    *rx_len = RX_BUF_SIZE;
    pes_status_t st = pes_nfc_hal_data_exchange(cmd, cmd_len,
                                                rx, rx_len);
    if ((PES_OK != st) || (*rx_len < 2u) ||
        (0x90u != rx[*rx_len - 2u]) || (0x00u != rx[*rx_len - 1u]))
    {
        return false;
    }
    return true;
}

/* ── Type 4 Tag NDEF Read ──────────────────────────────────────────── */

static pes_status_t read_t4t_ndef(pes_nfc_card_result_t *res)
{
    uint8_t rx[RX_BUF_SIZE];
    uint8_t cmd[16];
    uint32_t rx_len;

    /* 1. SELECT NDEF Tag Application (AID D2760000850101) */
    static const uint8_t sel_app[] = {
        0x00,0xA4,0x04,0x00,0x07,
        0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00
    };
    if (!t4t_exchange((uint8_t *)sel_app, (uint32_t)sizeof(sel_app),
                      rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* 2. SELECT Capability Container (EF E103) */
    static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
    if (!t4t_exchange((uint8_t *)sel_cc, (uint32_t)sizeof(sel_cc),
                      rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* 3. READ CC (15 bytes) */
    static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
    if (!t4t_exchange((uint8_t *)read_cc, (uint32_t)sizeof(read_cc),
                      rx, &rx_len) || (rx_len < 17u))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* Parse CC */
    uint16_t mle     = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);
    uint8_t  fid_hi  = rx[9];
    uint8_t  fid_lo  = rx[10];
    uint16_t maxfile = (uint16_t)(((uint16_t)rx[11] << 8) | rx[12]);
    uint8_t  wa      = rx[14];

    res->data_area_size = (uint32_t)maxfile;
    res->writeable      = (0x00u == wa);
    res->tag_type_name  = "ISO-DEP (Type 4 Tag / ISO 14443-4)";

    /* 4. SELECT NDEF file */
    cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C;
    cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
    if (!t4t_exchange(cmd, 7u, rx, &rx_len))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* 5. READ NLEN (first 2 bytes) */
    cmd[0]=0x00; cmd[1]=0xB0; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
    if (!t4t_exchange(cmd, 5u, rx, &rx_len) || (rx_len < 4u))
    {
        return PES_ERR_NOT_FOUND;
    }
    uint16_t nlen = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);

    if (0u == nlen)
    {
        res->ndef_present = false;
        res->ndef_len     = 0;
        return PES_OK;
    }

    /* 6. READ the NDEF message body in chunks */
    uint32_t chunk = ((0u == mle) || (mle > 0xFFu)) ? 0xFFu : (uint32_t)mle;
    uint32_t total = ((uint32_t)nlen > PES_NFC_NDEF_MAX_BYTES)
                     ? PES_NFC_NDEF_MAX_BYTES : (uint32_t)nlen;
    uint32_t got    = 0;
    uint16_t offset = 2u;

    while (got < total)
    {
        uint32_t want = total - got;
        if (want > chunk) { want = chunk; }

        cmd[0]=0x00; cmd[1]=0xB0;
        cmd[2]=(uint8_t)(offset >> 8);
        cmd[3]=(uint8_t)(offset & 0xFFu);
        cmd[4]=(uint8_t)want;
        if (!t4t_exchange(cmd, 5u, rx, &rx_len) || (rx_len < 2u))
        {
            break;
        }

        uint32_t data = rx_len - 2u;
        if (data > want) { data = want; }
        if (0u == data)  { break; }

        (void)memcpy(&res->ndef_data[got], rx, data);
        got    += data;
        offset  = (uint16_t)(offset + data);
    }

    res->ndef_len     = (uint16_t)got;
    res->ndef_present = (got > 0u);
    return PES_OK;
}

/* ── Type 2 Tag NDEF Read ──────────────────────────────────────────── */

static pes_status_t read_t2t_ndef(pes_nfc_card_result_t *res)
{
    uint8_t rx[RX_BUF_SIZE];
    uint8_t cmd[2];
    uint32_t rx_len;
    uint8_t  data_buf[TX_BUF_SIZE];   /* accumulate TLV area here */

    /* READ block 3 -> CC (response = blocks 3..6, 16 bytes) */
    cmd[0] = 0x30; cmd[1] = 0x03;
    rx_len = RX_BUF_SIZE;
    pes_status_t st = pes_nfc_hal_data_exchange(cmd, 2u, rx, &rx_len);
    if ((PES_OK != st) || (rx_len < 4u))
    {
        return PES_ERR_NOT_FOUND;
    }

    /* CC: [0]=magic(0xE1) [1]=version [2]=size(x8) [3]=access */
    if (0xE1u != rx[0])
    {
        return PES_ERR_NOT_FOUND;  /* not NDEF formatted */
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
        st = pes_nfc_hal_data_exchange(cmd, 2u, rx, &rx_len);
        if ((PES_OK != st) || (rx_len < 4u))
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
            uint32_t copy = (l > PES_NFC_NDEF_MAX_BYTES)
                            ? PES_NFC_NDEF_MAX_BYTES : l;
            (void)memcpy(res->ndef_data, &data_buf[p], copy);
            res->ndef_len     = (uint16_t)copy;
            res->ndef_present = (copy > 0u);
            return PES_OK;
        }

        p += l;  /* skip Lock/Memory/other TLVs */
    }

    /* No NDEF TLV found */
    res->ndef_present = false;
    res->ndef_len     = 0;
    return PES_OK;
}

/* ── Public API ────────────────────────────────────────────────────── */

pes_status_t PES_NFCCardReader_ReadCardInfo(pes_nfc_protocol_t protocol,
                                            pes_nfc_card_result_t *result)
{
    if (NULL == result) { return PES_ERR_INVALID_CFG; }

    /* Clear the extended fields */
    result->ndef_present   = false;
    result->ndef_len       = 0;
    result->data_area_size = 0;
    result->writeable      = false;
    result->tag_type_name  = NULL;

    switch (protocol)
    {
        case PES_NFC_PROT_ISODEP:
            return read_t4t_ndef(result);

        case PES_NFC_PROT_T2T:
            return read_t2t_ndef(result);

        case PES_NFC_PROT_T5T:
            result->tag_type_name = "NFC Forum Type 5 Tag (T5T/ISO 15693)";
            return PES_OK;

        case PES_NFC_PROT_T3T:
            result->tag_type_name = "NFC Forum Type 3 Tag (T3T/FeliCa)";
            return PES_OK;

        case PES_NFC_PROT_NFCDEP:
            result->tag_type_name = "NFC-DEP (Peer-to-Peer)";
            return PES_OK;

        default:
            result->tag_type_name = "Unknown";
            return PES_OK;
    }
}

/* ── Legacy thin wrappers (kept for internal PES use) ──────────────── */

pes_status_t pes_ndef_read_t4t(pes_nfc_card_result_t *result_out)
{
    if (NULL == result_out) { return PES_ERR_INVALID_CFG; }
    return read_t4t_ndef(result_out);
}

pes_status_t pes_ndef_read_t2t(pes_nfc_card_result_t *result_out)
{
    if (NULL == result_out) { return PES_ERR_INVALID_CFG; }
    return read_t2t_ndef(result_out);
}

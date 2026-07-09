/**
 * pes_ndef_write.c
 *
 * NDEF message writing / erasing for NFC Forum Type 2 Tags (hand-rolled,
 * small footprint) and Type 4/Type 5 Tags (via the PTX SDK's lean
 * ptxNDEF_T4TOP / ptxNDEF_T5TOP components). T3T NDEF write is not
 * supported — see pes_ndef_read.c for the flash-budget rationale.
 * Also provides PES_NDEF_BuildTextRecord() for constructing RTD-Text records.
 *
 * NO printing — returns pes_status_t only.
 */

#include "pes_nfc_card_reader.h"
#include "pes_nfc_ptx105r.h"
#include "ptx_IOT_READER.h"
#include "ptxNDEF_T4TOP.h"
#include "ptxNDEF_T5TOP.h"
#include <string.h>

/***********************************************************************************************************************
 * Constants
 **********************************************************************************************************************/
#define RX_BUF_SIZE   PES_NFC_PTX_RX_BUF_SIZE

/***********************************************************************************************************************
 * Type 4 Tag NDEF Write/Erase — via PTX SDK ptxNDEF_T4TOP component
 **********************************************************************************************************************/

static pes_status_t write_t4t_ndef(const uint8_t *ndef, uint16_t ndef_len)
{
    pes_status_t st = pes_nfc_ptx_ndef_open();
    if (PES_OK != st) { return st; }

    struct ptxNDEF_T4TOP *t4t = pes_nfc_ptx_get_ndef_comp();

    /* CheckMessage first — the SDK needs CC info before writing */
    ptxStatus_t ptx_st = ptxNDEF_T4TOpCheckMessage(t4t);
    if (ptxStatus_Success != ptx_st)
    {
        pes_nfc_ptx_ndef_close();
        return PES_ERR_NOT_FOUND;
    }

    uint32_t wlen = ((NULL != ndef) && (ndef_len > 0u)) ? (uint32_t)ndef_len : 0u;
    ptx_st = ptxNDEF_T4TOpWriteMessage(t4t, (uint8_t *)(uintptr_t)ndef, wlen);

    pes_nfc_ptx_ndef_close();
    return (ptxStatus_Success == ptx_st) ? PES_OK : PES_ERR_INTERNAL;
}

/***********************************************************************************************************************
 * Type 5 Tag NDEF Write/Erase — via PTX SDK ptxNDEF_T5TOP component
 **********************************************************************************************************************/

static pes_status_t write_t5t_ndef(const uint8_t *ndef, uint16_t ndef_len)
{
    pes_status_t st = pes_nfc_ptx_ndef_t5t_open();
    if (PES_OK != st) { return st; }

    struct ptxNDEF_T5TOP *t5t = pes_nfc_ptx_get_ndef_t5t_comp();

    /* CheckMessage first — the SDK needs CC info before writing */
    ptxStatus_t ptx_st = ptxNDEF_T5TOpCheckMessage(t5t);
    if (ptxStatus_Success != ptx_st)
    {
        pes_nfc_ptx_ndef_t5t_close();
        return PES_ERR_NOT_FOUND;
    }

    uint32_t wlen = ((NULL != ndef) && (ndef_len > 0u)) ? (uint32_t)ndef_len : 0u;
    ptx_st = ptxNDEF_T5TOpWriteMessage(t5t, (uint8_t *)(uintptr_t)ndef, wlen);

    pes_nfc_ptx_ndef_t5t_close();
    return (ptxStatus_Success == ptx_st) ? PES_OK : PES_ERR_INTERNAL;
}

/***********************************************************************************************************************
 * Type 2 Tag NDEF Write/Erase — hand-rolled (small footprint, proven)
 **********************************************************************************************************************/

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

/***********************************************************************************************************************
 * Public API: Write NDEF
 **********************************************************************************************************************/

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

        case PES_NFC_PROT_T5T:
            return write_t5t_ndef(ndef, ndef_len);

        default:
            return PES_ERR_INVALID_CFG;  /* protocol not supported */
    }
}

/***********************************************************************************************************************
 * Public API: Erase NDEF
 **********************************************************************************************************************/

pes_status_t PES_NFCCardReader_EraseNDEF(pes_nfc_protocol_t protocol)
{
    /* Erasing is writing with length 0 */
    return PES_NFCCardReader_WriteNDEF(protocol, NULL, 0u);
}

/***********************************************************************************************************************
 * Public API: Build RTD-Text Record
 **********************************************************************************************************************/

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

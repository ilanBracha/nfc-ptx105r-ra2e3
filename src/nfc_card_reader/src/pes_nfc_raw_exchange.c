/**
 * pes_nfc_raw_exchange.c
 *
 * Protocol-specific raw demo exchange: builds the correct frame for T2T,
 * T3T, T5T, or NFC-DEP and sends it through the HAL data_exchange.
 *
 * NO printing — returns TX/RX data in caller-supplied buffers.
 */

#include "pes_nfc_ptx.h"
#include "pes_nfc_card_reader.h"
#include <string.h>

/* ── Constants ─────────────────────────────────────────────────────── */
#define RX_BUF_SIZE   PES_NFC_PTX_RX_BUF_SIZE

pes_status_t PES_NFCCardReader_RawExchange(pes_nfc_protocol_t protocol,
                                           const uint8_t *uid, uint8_t uid_len,
                                           uint8_t *tx, uint32_t *tx_len,
                                           uint8_t *rx, uint32_t *rx_len)
{
    if ((NULL == tx) || (NULL == tx_len) || (NULL == rx) || (NULL == rx_len))
    {
        return PES_ERR_INVALID_CFG;
    }

    uint32_t frame_len = 0;

    switch (protocol)
    {
        /* ── T2T: READ BLOCK 0 ──────────────────────────────────────── */
        case PES_NFC_PROT_T2T:
        {
            tx[0] = 0x30u;  /* READ */
            tx[1] = 0x00u;  /* block 0 */
            frame_len = 2u;
            break;
        }

        /* ── T3T: CHECK BLOCK 0 (NFCID2 inserted) ──────────────────── */
        case PES_NFC_PROT_T3T:
        {
            /*  Byte layout (15 bytes total, LEN managed internally):
             *    [0]     = 0x06 (CHECK command code)
             *    [1..8]  = NFCID2 (8 bytes from uid)
             *    [9]     = 0x01 (Number of Services)
             *    [10..11]= 0x0B,0x00 (Service Code List)
             *    [12]    = 0x01 (Number of Blocks)
             *    [13..14]= 0x80,0x00 (Block List)
             */
            static const uint8_t t3t_template[] = {
                0x06, /* followed by 8-byte NFCID2 */
            };
            static const uint8_t t3t_tail[] = {
                0x01, 0x0B, 0x00, 0x01, 0x80, 0x00
            };

            tx[0] = t3t_template[0];
            if ((NULL != uid) && (uid_len >= 8u))
            {
                (void)memcpy(&tx[1], uid, 8u);
            }
            else
            {
                (void)memset(&tx[1], 0, 8u);
            }
            (void)memcpy(&tx[9], t3t_tail, sizeof(t3t_tail));
            frame_len = 1u + 8u + (uint32_t)sizeof(t3t_tail);  /* 15 */
            break;
        }

        /* ── T5T: READ_SINGLE_BLOCK 0 (addressed mode, UID reversed) ── */
        case PES_NFC_PROT_T5T:
        {
            /*  [0] = 0x22  flags: high data-rate + addressed
             *  [1] = 0x20  READ_SINGLE_BLOCK command
             *  [2..9]      UID (LSB-first; PES stores MSB-first)
             *  [10]= 0x00  block number 0
             */
            tx[0] = 0x22u;
            tx[1] = 0x20u;
            if ((NULL != uid) && (uid_len >= 8u))
            {
                for (uint8_t i = 0; i < 8u; i++)
                {
                    tx[2u + i] = uid[7u - i];
                }
            }
            else
            {
                (void)memset(&tx[2], 0, 8u);
            }
            tx[10] = 0x00u;
            frame_len = 11u;
            break;
        }

        /* ── NFC-DEP: LLCP SYMM (0x0000) ───────────────────────────── */
        case PES_NFC_PROT_NFCDEP:
        {
            tx[0] = 0x00u;
            tx[1] = 0x00u;
            frame_len = 2u;
            break;
        }

        default:
            return PES_ERR_INVALID_CFG;
    }

    *tx_len = frame_len;

    /* Perform the exchange */
    return pes_nfc_ptx_data_exchange(tx, frame_len, rx, rx_len);
}

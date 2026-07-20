/**
 * rs_nfc_uid.c
 *
 * UID formatting utilities for the RS NFC Reader module.
 * Pure data transforms — NO printing or logging.
 */

#include "rs_nfc_ptx105r.h"
#include <stdint.h>

static const char HEX_DIGITS[] = "0123456789ABCDEF";

uint32_t rs_nfc_uid_to_hex (const uint8_t * uid,
                            uint8_t         uid_len,
                            char          * out,
                            uint32_t        out_size)
{
    if ((NULL == uid) || (NULL == out) || (0u == out_size))
    {
        return 0u;
    }

    uint32_t pos = 0u;

    for (uint8_t i = 0; i < uid_len; i++)
    {
        if (pos + 2u >= out_size)
        {
            break;
        }

        out[pos++] = HEX_DIGITS[(uid[i] >> 4) & 0x0Fu];
        out[pos++] = HEX_DIGITS[uid[i] & 0x0Fu];
    }

    if (pos < out_size)
    {
        out[pos] = '\0';
    }
    else
    {
        out[out_size - 1u] = '\0';
    }

    return pos;
}

uint32_t rs_nfc_uid_to_hex_colon (const uint8_t * uid,
                                  uint8_t         uid_len,
                                  char          * out,
                                  uint32_t        out_size)
{
    if ((NULL == uid) || (NULL == out) || (0u == out_size))
    {
        return 0u;
    }

    uint32_t pos = 0u;

    for (uint8_t i = 0; i < uid_len; i++)
    {
        if ((i > 0u) && (pos + 1u < out_size))
        {
            out[pos++] = ':';
        }

        if (pos + 2u >= out_size)
        {
            break;
        }

        out[pos++] = HEX_DIGITS[(uid[i] >> 4) & 0x0Fu];
        out[pos++] = HEX_DIGITS[uid[i] & 0x0Fu];
    }

    if (pos < out_size)
    {
        out[pos] = '\0';
    }
    else
    {
        out[out_size - 1u] = '\0';
    }

    return pos;
}

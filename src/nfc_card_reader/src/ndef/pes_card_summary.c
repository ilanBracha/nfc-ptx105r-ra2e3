/**
 * pes_card_summary.c
 *
 * Builds a one-line, human-readable summary of an activated NFC card into a
 * caller-supplied buffer. No printing — the PES module never owns an output
 * sink; the application's on_card_event callback is responsible for routing
 * the string to RTT/UART/etc.
 */

#include "pes_nfc_internal.h"
#include "pes_nfc_card_reader.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Local hex helper (avoids pulling in stdio's snprintf for a few bytes). */
static const char HEX_DIGITS[] = "0123456789ABCDEF";

static uint32_t copy_str(char *dst, uint32_t dst_size, uint32_t off, const char *s)
{
    while ((NULL != s) && ('\0' != *s) && (off + 1u < dst_size))
    {
        dst[off++] = *s++;
    }
    return off;
}

static uint32_t copy_hex_byte(char *dst, uint32_t dst_size, uint32_t off, uint8_t b)
{
    if (off + 2u >= dst_size) { return off; }
    dst[off++] = HEX_DIGITS[(b >> 4) & 0x0Fu];
    dst[off++] = HEX_DIGITS[b & 0x0Fu];
    return off;
}

static uint32_t copy_uint(char *dst, uint32_t dst_size, uint32_t off, uint32_t v)
{
    char tmp[11];
    uint32_t n = 0;
    if (0u == v)
    {
        if (off + 1u < dst_size) { dst[off++] = '0'; }
        return off;
    }
    while (v > 0u && n < sizeof(tmp))
    {
        tmp[n++] = (char)('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0u && off + 1u < dst_size)
    {
        dst[off++] = tmp[--n];
    }
    return off;
}

static const char * card_type_name(pes_nfc_card_type_t t)
{
    switch (t)
    {
        case PES_NFC_CARD_TYPE_ISO14443A:        return "ISO14443A";
        case PES_NFC_CARD_TYPE_ISO14443B:        return "ISO14443B";
        case PES_NFC_CARD_TYPE_FELICA:           return "FeliCa";
        case PES_NFC_CARD_TYPE_ISO15693:         return "ISO15693";
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_2:   return "NFC-T2T";
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_3:   return "NFC-T3T";
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:  return "NFC-T4A";
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:  return "NFC-T4B";
        case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_5:   return "NFC-T5T";
        default:                                 return "Unknown";
    }
}

uint32_t pes_card_summary_build(const pes_nfc_card_result_t *res,
                                char *buf, uint32_t buf_size)
{
    if ((NULL == buf) || (0u == buf_size)) { return 0u; }
    if (NULL == res)
    {
        buf[0] = '\0';
        return 0u;
    }

    uint32_t off = 0u;

    off = copy_str(buf, buf_size, off, "CARD DETECTED! type=");
    off = copy_str(buf, buf_size, off, card_type_name(res->card_type));
    off = copy_str(buf, buf_size, off, " UID=");
    for (uint8_t i = 0; i < res->uid_len; i++)
    {
        off = copy_hex_byte(buf, buf_size, off, res->uid[i]);
    }
    if (res->ndef_present)
    {
        off = copy_str(buf, buf_size, off, " NDEF=");
        off = copy_uint(buf, buf_size, off, (uint32_t)res->ndef_len);
        off = copy_str(buf, buf_size, off, "B");
    }

    if (off < buf_size) { buf[off] = '\0'; }
    else                { buf[buf_size - 1u] = '\0'; off = buf_size - 1u; }
    return off;
}

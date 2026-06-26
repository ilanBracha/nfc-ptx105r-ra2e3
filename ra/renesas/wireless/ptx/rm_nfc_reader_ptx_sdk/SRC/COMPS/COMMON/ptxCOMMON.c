/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of
       conditions and the following disclaimer in the documentation and/or other
       materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



    THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX1K
    Module      : COMMON API
    File        : ptxCOMMON.c

    Description : Common API for common functions.
*/


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxCOMMON.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
/*
 * Printf output is routed to both SEGGER RTT (channel 0) and the debug UART
 * (UserUartLog). Uses SEGGER_RTT_vprintf (lightweight, already linked) for RTT
 * and a minimal va-formatter for UART to avoid pulling in the heavy newlib
 * vsnprintf (~3 KB flash).
 */
#include <stdarg.h>
#include "SEGGER_RTT.h"
#include "auc_nfc_card_reader_log.h"

/* Minimal format-to-buffer: supports %s %c %d %u %x %X %02X %04X %02d %04d %p %% and width/zero-pad for integers */
static int ptxCommon_mini_vsnprintf(char *buf, unsigned max, const char *fmt, va_list ap)
{
    unsigned pos = 0u;
#define PUT(c) do { if (pos < (max - 1u)) { buf[pos] = (c); } pos++; } while(0)

    while (*fmt)
    {
        if (*fmt != '%') { PUT(*fmt); fmt++; continue; }
        fmt++; /* skip '%' */

        /* flags / width */
        char pad = ' ';
        if (*fmt == '0') { pad = '0'; fmt++; }
        unsigned width = 0u;
        while (*fmt >= '0' && *fmt <= '9') { width = width * 10u + (unsigned)(*fmt - '0'); fmt++; }

        /* length modifier (ignored, treat as int/unsigned) */
        if (*fmt == 'l') { fmt++; }

        char tmp[12]; /* enough for 32-bit in decimal */
        unsigned tlen = 0u;

        switch (*fmt)
        {
            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s) { PUT(*s); s++; }
                break;
            }
            case 'c':
            {
                char c = (char)va_arg(ap, int);
                PUT(c);
                break;
            }
            case 'd':
            case 'i':
            {
                int v = va_arg(ap, int);
                unsigned uv;
                if (v < 0) { PUT('-'); uv = (unsigned)(-(v+1)) + 1u; } else { uv = (unsigned)v; }
                /* convert to string (reversed) */
                if (0u == uv) { tmp[tlen++] = '0'; }
                else { while (uv) { tmp[tlen++] = (char)('0' + (uv % 10u)); uv /= 10u; } }
                while (tlen < width) { tmp[tlen++] = pad; }
                for (unsigned k = tlen; k > 0u; k--) { PUT(tmp[k-1u]); }
                break;
            }
            case 'u':
            {
                unsigned uv = va_arg(ap, unsigned);
                if (0u == uv) { tmp[tlen++] = '0'; }
                else { while (uv) { tmp[tlen++] = (char)('0' + (uv % 10u)); uv /= 10u; } }
                while (tlen < width) { tmp[tlen++] = pad; }
                for (unsigned k = tlen; k > 0u; k--) { PUT(tmp[k-1u]); }
                break;
            }
            case 'x':
            case 'X':
            {
                const char *hex = (*fmt == 'X') ? "0123456789ABCDEF" : "0123456789abcdef";
                unsigned uv = va_arg(ap, unsigned);
                if (0u == uv) { tmp[tlen++] = '0'; }
                else { while (uv) { tmp[tlen++] = hex[uv & 0xFu]; uv >>= 4; } }
                while (tlen < width) { tmp[tlen++] = pad; }
                for (unsigned k = tlen; k > 0u; k--) { PUT(tmp[k-1u]); }
                break;
            }
            case 'p':
            {
                unsigned uv = (unsigned)(uintptr_t)va_arg(ap, void *);
                PUT('0'); PUT('x');
                if (0u == uv) { tmp[tlen++] = '0'; }
                else { while (uv) { tmp[tlen++] = "0123456789abcdef"[uv & 0xFu]; uv >>= 4; } }
                while (tlen < 8u) { tmp[tlen++] = '0'; }
                for (unsigned k = tlen; k > 0u; k--) { PUT(tmp[k-1u]); }
                break;
            }
            case '%':
                PUT('%');
                break;
            case '\0':
                goto done;
            default:
                PUT('%'); PUT(*fmt);
                break;
        }
        fmt++;
    }
done:
    buf[(pos < max) ? pos : (max - 1u)] = '\0';
    return (int)pos;
#undef PUT
}

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

void ptxCommon_PrintF(const char *format, ...)
{
    va_list ap1, ap2;
    va_start(ap1, format);
    va_copy(ap2, ap1);

    /* RTT: use SEGGER's lightweight formatter (already compiled in) */
    (void)SEGGER_RTT_vprintf(0, format, &ap1);

    /* UART: format into stack buffer and send */
    char buf[256];
    int len = ptxCommon_mini_vsnprintf(buf, sizeof(buf), format, ap2);
    if (len > 0)
    {
        UserUartLog_Write((const uint8_t *)buf, (unsigned)len > sizeof(buf)-1u ? sizeof(buf)-1u : (unsigned)len);
    }

    va_end(ap2);
    va_end(ap1);
}

void ptxCommon_Print_Buffer(uint8_t *buffer, uint32_t bufferOffset, uint32_t bufferLength, uint8_t addNewLine, uint8_t printASCII)
{
    uint32_t i;
    uint32_t lineIdx = 0;
    uint8_t character_to_print;

    if (NULL != buffer)
    {
        if (0 != bufferLength)
        {
            for (i = 0; (i < bufferLength) && (i < (uint32_t)TX_BUFFER_SIZE); i++)
            {
                if ((i > 0) && ((i % (LINE_LENGTH - 5) == 0)))
                {
                    lineIdx++;
                    ptxCommon_PrintF("\n     ");
                }

                if (0 == printASCII)
                {
                    ptxCommon_PrintF("%02X", (uint8_t)buffer[i + bufferOffset]);
                } else
                {
                    character_to_print = (uint8_t)buffer[i + bufferOffset];
                    /* avoid unintentional interpretation of ascii-commands */
                    if (character_to_print < 0x20)
                    {
                        ptxCommon_PrintF(".");
                    } else
                    {
                        ptxCommon_PrintF("%c", character_to_print);
                    }
                }
            }

            if (0 != addNewLine)
            {
                ptxCommon_PrintF("\n");
            }
        }
    }
}

void ptxCommon_PrintStatusMessage(const char *message, ptxStatus_t st)
{
    if (NULL != message)
    {
        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("%s ... OK\n", message);
        } else
        {
            ptxCommon_PrintF("%s ... ERROR (Status-Code = %04X)\n", message, st);
        }
    }
}


/*
 * user_uart_log.c
 *
 * Implementation of the UART debug-log sink. See user_uart_log.h.
 *
 * Notes:
 *  - We register our own callback via R_SCI_UART_CallbackSet(), so we do not
 *    care what the FSP pin/UART tab set as the callback (typically the
 *    generated "NULL" stub). That keeps the helper self-contained and survives
 *    project regeneration.
 *  - TX is fully non-blocking: app_nfc_reader_log_write() copies into an internal
 *    ring buffer and returns immediately. The SCI TX_COMPLETE ISR chain-loads
 *    the next contiguous chunk until the ring drains. If the ring fills up
 *    (very long burst at <115200 baud) the excess bytes are dropped so the
 *    debug log can never throttle the main loop or RTT.
 *  - RX bytes are collected in a separate ring buffer from the RX_CHAR event
 *    and consumed by user_cli via app_nfc_reader_log_rx_get().
 *  - The pin mux for P1_09 (TXD9) / P1_10 (RXD9) on the RA2E3 FPB is forced
 *    here via R_IOPORT_PinCfg, mirroring the SPI workaround in ptxPLAT_SPI.c
 *    -- the generated g_bsp_pin_cfg does not include those pins.
 */
#include <stdio.h>
#include "app_nfc_reader_log.h"
#include "hal_data.h"
#include "r_ioport.h"
#include <string.h>
#include <stdarg.h>
#include "ptxCOMMON.h"

/*
 * SCI9 routes to P1_09 (TXD9) / P1_10 (RXD9) on the RA2E3 FPB.
 * SCI1/3/5/7/9 use PSEL = 0x05 -> IOPORT_PERIPHERAL_SCI1_3_5_7_9.
 *
 * The generated g_bsp_pin_cfg in ra_gen/pin_data.c does NOT mux these pins
 * to the peripheral (TXD/RXD are missing from the FSP Pins tab), so the SCI
 * peripheral is opened but its signals never reach the package pins. We force
 * the mux here, mirroring the SPI workaround in ptxPLAT_SPI.c.
 */
#ifndef app_nfc_reader_log_TXD_PIN
#define app_nfc_reader_log_TXD_PIN  BSP_IO_PORT_01_PIN_09
#endif
#ifndef app_nfc_reader_log_RXD_PIN
#define app_nfc_reader_log_RXD_PIN  BSP_IO_PORT_01_PIN_10
#endif

/*
 * ####################################################################################################################
 * INTERNAL STATE
 * ####################################################################################################################
 */
static volatile uint8_t s_uart_initialized = 0u;

/* Optional per-byte RX callback (registered by CLI layer). */
static app_nfc_reader_log_rx_callback_t s_rx_callback = NULL;

/* RX ring buffer (ISR producer / main consumer). Size MUST be a power of 2. */
#define app_nfc_reader_log_RX_BUF_SIZE   128u
#define app_nfc_reader_log_RX_BUF_MASK   (app_nfc_reader_log_RX_BUF_SIZE - 1u)

static volatile uint8_t  s_rx_buf[app_nfc_reader_log_RX_BUF_SIZE];
static volatile uint16_t s_rx_head; /* written by ISR (producer) */
static volatile uint16_t s_rx_tail; /* written by main (consumer) */

/* TX ring buffer (main producer / ISR consumer). Size MUST be a power of 2.
 * Big enough to hold a full NDEF hex dump (512 bytes * 3 chars + headers)
 * so logging never blocks the main loop. Increase if you see drops. */
#define app_nfc_reader_log_TX_BUF_SIZE   2048u
#define app_nfc_reader_log_TX_BUF_MASK   (app_nfc_reader_log_TX_BUF_SIZE - 1u)

static volatile uint8_t  s_tx_buf[app_nfc_reader_log_TX_BUF_SIZE];
static volatile uint16_t s_tx_head;     /* next write index (main)  */
static volatile uint16_t s_tx_tail;     /* next byte to be sent     */
static volatile uint16_t s_tx_chunk;    /* bytes in current FSP write */
static volatile uint8_t  s_tx_busy;     /* 1 while FSP write in flight */

/* Forward decl: kick the next contiguous chunk if idle. May be called from
 * either main (after enqueue) or ISR (on TX_COMPLETE). Caller must guarantee
 * mutual exclusion (we disable IRQs around the main-side call). */
static void app_nfc_reader_log_tx_locked(void);
void app_nfc_reader_log_write(const uint8_t *buf, size_t len);

/*
 * ####################################################################################################################
 * CALLBACK
 * ####################################################################################################################
 */
static void app_nfc_reader_log_uart_cb (uart_callback_args_t *p_args)
{
    if (NULL == p_args)
    {
        return;
    }

    switch (p_args->event)
    {
        case UART_EVENT_TX_COMPLETE:
        {
            /* Previous chunk fully shifted out. Advance tail and start the
             * next contiguous chunk if any data is still pending. */
            uint16_t tail = (uint16_t)((s_tx_tail + s_tx_chunk) & app_nfc_reader_log_TX_BUF_MASK);
            s_tx_tail  = tail;
            s_tx_chunk = 0u;
            s_tx_busy  = 0u;
            app_nfc_reader_log_tx_locked();
            break;
        }

        case UART_EVENT_RX_CHAR:
        {
            uint8_t rxb = (uint8_t)p_args->data;

            /* Forward to registered callback (e.g. CLI) if present. */
            if (NULL != s_rx_callback)
            {
                s_rx_callback(rxb);
            }

            /* Always store in ring buffer for app_nfc_reader_log_rx_get() consumers. */
            uint16_t next = (uint16_t)((s_rx_head + 1u) & app_nfc_reader_log_RX_BUF_MASK);
            if (next != s_rx_tail)
            {
                s_rx_buf[s_rx_head] = rxb;
                s_rx_head = next;
            }
            /* If full, byte is dropped silently (CLI lines are short). */
            break;
        }

        case UART_EVENT_ERR_PARITY:
        case UART_EVENT_ERR_FRAMING:
        case UART_EVENT_ERR_OVERFLOW:
        case UART_EVENT_BREAK_DETECT:
            /* Errors on RX are ignored for a pure debug-output port. */
            break;

        default:
            break;
    }
}

/*
 * ####################################################################################################################
 * API
 * ####################################################################################################################
 */
int app_nfc_reader_log_init(void)
{
    if (0u != s_uart_initialized)
    {
        return 0;
    }

    fsp_err_t err;

    /* Force the SCI9 pin mux for TXD9 (P1_09) and RXD9 (P1_10). The IOPORT
     * driver was already opened from R_BSP_WarmStart() POST_C, so this just
     * overrides the two PFS registers we need. */
    (void)R_IOPORT_PinCfg(&g_ioport_ctrl,
                          app_nfc_reader_log_TXD_PIN,
                          ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI1_3_5_7_9));
    (void)R_IOPORT_PinCfg(&g_ioport_ctrl,
                          app_nfc_reader_log_RXD_PIN,
                          ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI1_3_5_7_9));

    err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
    if (FSP_SUCCESS != err)
    {
        return (int)err;
    }

    /* Hook our own callback so we observe UART_EVENT_TX_COMPLETE. */
    err = g_uart0.p_api->callbackSet(g_uart0.p_ctrl, app_nfc_reader_log_uart_cb, NULL, NULL);
    if (FSP_SUCCESS != err)
    {
        (void)g_uart0.p_api->close(g_uart0.p_ctrl);
        return (int)err;
    }

    s_tx_head          = 0u;
    s_tx_tail          = 0u;
    s_tx_chunk         = 0u;
    s_tx_busy          = 0u;
    s_uart_initialized = 1u;
    return 0;
}

/* Dispatch the next contiguous chunk (tail .. min(head, end-of-ring)) to the
 * FSP UART driver. Safe to call when interrupts are masked OR from the ISR
 * itself (where they're effectively masked at this priority anyway). */
static void app_nfc_reader_log_tx_locked (void)
{
    if (s_tx_busy)
    {
        return;
    }
    uint16_t head = s_tx_head;
    uint16_t tail = s_tx_tail;
    if (head == tail)
    {
        return; /* nothing to send */
    }

    /* Contiguous span from tail up to either head or the end of the ring. */
    uint16_t end = (head > tail) ? head : (uint16_t)app_nfc_reader_log_TX_BUF_SIZE;
    uint16_t len = (uint16_t)(end - tail);

    s_tx_chunk = len;
    s_tx_busy  = 1u;

    fsp_err_t err = g_uart0.p_api->write(g_uart0.p_ctrl,
                                         (uint8_t const *)&s_tx_buf[tail],
                                         (uint32_t)len);
    if (FSP_SUCCESS != err)
    {
        /* Drop this chunk and try to recover. */
        s_tx_busy  = 0u;
        s_tx_chunk = 0u;
        s_tx_tail  = head; /* discard everything pending */
    }
}

void app_nfc_reader_log_write(const uint8_t *buf, size_t len)
{
    if ((0u == s_uart_initialized) || (NULL == buf) || (0u == len))
    {
        return;
    }

    /* Producer side: copy bytes into the TX ring under a brief IRQ lock so
     * head/tail are not racing the SCI9 TX_COMPLETE ISR. Dropped bytes (ring
     * full) are silently discarded -- the debug log must never throttle the
     * main loop. dispatch() is called once at the end so the FSP gets a
     * single large write to chew on (not 1 byte at a time). */
    __disable_irq();
    uint16_t head = s_tx_head;
    uint16_t tail = s_tx_tail;
    for (size_t i = 0u; i < len; i++)
    {
        uint16_t next = (uint16_t)((head + 1u) & app_nfc_reader_log_TX_BUF_MASK);
        if (next == tail)
        {
            break; /* ring full -- drop remainder */
        }
        s_tx_buf[head] = buf[i];
        head = next;
    }
    s_tx_head = head;
    app_nfc_reader_log_tx_locked();
    __enable_irq();
}

void app_nfc_reader_log_puts(const char *s)
{
    if (NULL == s)
    {
        return;
    }
    app_nfc_reader_log_write((const uint8_t *)s, strlen(s));
}

size_t app_nfc_reader_log_rx_available(void)
{
    if (0u == s_uart_initialized)
    {
        return 0u;
    }
    uint16_t head = s_rx_head;
    uint16_t tail = s_rx_tail;
    return (size_t)((head - tail) & app_nfc_reader_log_RX_BUF_MASK);
}

int app_nfc_reader_log_rx_get(uint8_t *out)
{
    if ((0u == s_uart_initialized) || (NULL == out))
    {
        return 0;
    }
    uint16_t tail = s_rx_tail;
    if (tail == s_rx_head)
    {
        return 0;
    }
    *out = s_rx_buf[tail];
    s_rx_tail = (uint16_t)((tail + 1u) & app_nfc_reader_log_RX_BUF_MASK);
    return 1;
}

void app_nfc_reader_log_rx_callback(app_nfc_reader_log_rx_callback_t cb)
{
    s_rx_callback = cb;
}

/*
 * ####################################################################################################################
 * ptxCOMMON API IMPLEMENTATION
 * ####################################################################################################################
 *
 * The SDK declares ptxCommon_PrintF / Print_Buffer / PrintStatusMessage in
 * ptxCOMMON.h (ra/renesas/wireless/...).  We provide the implementation HERE
 * so the vendor ptxCOMMON.c can remain unmodified (excluded from build).
 * Output is routed to both SEGGER RTT (channel 0) and the debug UART.
 */

/* Minimal format-to-buffer: supports %s %c %d %u %x %X %02X %04X %02d %04d %p %% and width/zero-pad for integers */
static int app_nfc_reader_log_vsnprintf (char *buf, unsigned max, const char *fmt, va_list ap)
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

void ptxCommon_PrintF(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    /* UART only (RTT sink removed to reclaim flash): format into stack
     * buffer and send. */
    char buf[256];
    int len = app_nfc_reader_log_vsnprintf(buf, sizeof(buf), format, ap);

    if (len > 0)
    {
        app_nfc_reader_log_write((const uint8_t *)buf, (unsigned)len > sizeof(buf)-1u ? sizeof(buf)-1u : (unsigned)len);
    }

    va_end(ap);
}

void ptxCommon_Print_Buffer (uint8_t *buffer, uint32_t bufferOffset, uint32_t bufferLength, uint8_t addNewLine, uint8_t printASCII)
{
    uint32_t i;
    uint8_t character_to_print;

    if (NULL != buffer)
    {
        if (0 != bufferLength)
        {
            for (i = 0; (i < bufferLength) && (i < (uint32_t)TX_BUFFER_SIZE); i++)
            {
                if ((i > 0) && ((i % (LINE_LENGTH - 5) == 0)))
                {
                    ptxCommon_PrintF("\n     ");
                }

                if (0 == printASCII)
                {
                    ptxCommon_PrintF("%02X", (uint8_t)buffer[i + bufferOffset]);
                } else
                {
                    character_to_print = (uint8_t)buffer[i + bufferOffset];
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

/*
 * ####################################################################################################################
 * APPLICATION-LEVEL CARD-INFO PRINTER
 * ####################################################################################################################
 *
 * Reads fields from rs_nfc_card_result_t and formats a human-readable block
 * to both RTT and UART.  Pure I/O â€” no LED or board interaction; the caller
 * is responsible for any visual feedback (blink, etc.).
 */
void app_nfc_reader_log_print_card_info(const rs_nfc_card_result_t * result)
{
    char rf_tech[16];

    if (NULL == result)
    {
        return;
    }

    switch (result->card_type)
    {
        case RS_NFC_CARD_TYPE_ISO14443A:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_2:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE A");
            break;

        case RS_NFC_CARD_TYPE_ISO14443B:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE B");
            break;

        case RS_NFC_CARD_TYPE_FELICA:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_3:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE F");
            break;

        case RS_NFC_CARD_TYPE_ISO15693:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_5:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE V");
            break;

        default:
            snprintf(rf_tech, sizeof(rf_tech), "UNKNOWN");
            break;
    }

    ptxCommon_PrintF("RF Technology  : "APP_NFC_READER_LOG_COL_BRIGHT_CYAN "%s\n" APP_NFC_READER_LOG_COL_RESET, rf_tech);

    /* Tag Type */
    ptxCommon_PrintF("Tag Type       : %s\n",
                     (NULL != result->tag_type_name)
                         ? result->tag_type_name : "Unknown");

    /* Serial Number */
    ptxCommon_PrintF("Serial Number  : ");

    if (0u == result->uid_len)
    {
        ptxCommon_PrintF("N/A");
    }
    else
    {
        for (uint8_t i = 0; i < result->uid_len; i++)
        {
            if (i) { ptxCommon_PrintF(":"); }
            ptxCommon_PrintF("%02X", result->uid[i]);
        }
    }

    ptxCommon_PrintF("\n");

    /* Size / Writeable */
    if (result->data_area_size > 0u)
    {
        ptxCommon_PrintF("Size           : %u bytes\n",
                         (unsigned)result->data_area_size);
        ptxCommon_PrintF("Writeable      : %s\n",
                         result->writeable ? "Yes" : "No");
    }
    else
    {
        ptxCommon_PrintF("Size           : N/A\n");
        ptxCommon_PrintF("Writeable      : N/A\n");
    }

    /* NDEF records */
    if (result->ndef_present && (result->ndef_len > 0u))
    {
        ptxCommon_PrintF("NDEF           : %u bytes\n",
                         (unsigned)result->ndef_len);
        ptxCommon_PrintF("  NDEF raw (%u bytes):", (unsigned)result->ndef_len);
        for (uint32_t k = 0u; k < result->ndef_len; k++)
        {
            if ((k > 0u) && (0u == (k % 16u)))
            {
                ptxCommon_PrintF("\n                       ");
            }
            ptxCommon_PrintF(" %02X", result->ndef_data[k]);
        }
        ptxCommon_PrintF("\n");
    }
    else
    {
        ptxCommon_PrintF("Records        : (none)\n");
    }
}

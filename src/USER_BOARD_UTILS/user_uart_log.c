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
 *  - TX is fully non-blocking: UserUartLog_Write() copies into an internal
 *    ring buffer and returns immediately. The SCI TX_COMPLETE ISR chain-loads
 *    the next contiguous chunk until the ring drains. If the ring fills up
 *    (very long burst at <115200 baud) the excess bytes are dropped so the
 *    debug log can never throttle the main loop or RTT.
 *  - RX bytes are collected in a separate ring buffer from the RX_CHAR event
 *    and consumed by user_cli via UserUartLog_RxGet().
 *  - The pin mux for P1_09 (TXD9) / P1_10 (RXD9) on the RA2E3 FPB is forced
 *    here via R_IOPORT_PinCfg, mirroring the SPI workaround in ptxPLAT_SPI.c
 *    -- the generated g_bsp_pin_cfg does not include those pins.
 */

#include "user_uart_log.h"
#include "hal_data.h"
#include "r_ioport.h"
#include "r_sci_uart.h"
#include "SEGGER_RTT.h"
#include <string.h>

/* The FSP-generated baud setting struct lives in ra_gen/hal_data.c and is
 * NOT const, so we can rewrite it in place before opening g_uart0. */
extern baud_setting_t g_uart0_baud_setting;

/*
 * SCI9 routes to P1_09 (TXD9) / P1_10 (RXD9) on the RA2E3 FPB.
 * SCI1/3/5/7/9 use PSEL = 0x05 -> IOPORT_PERIPHERAL_SCI1_3_5_7_9.
 *
 * The generated g_bsp_pin_cfg in ra_gen/pin_data.c does NOT mux these pins
 * to the peripheral (TXD/RXD are missing from the FSP Pins tab), so the SCI
 * peripheral is opened but its signals never reach the package pins. We force
 * the mux here, mirroring the SPI workaround in ptxPLAT_SPI.c.
 */
#ifndef USER_UART_LOG_TXD_PIN
#define USER_UART_LOG_TXD_PIN  BSP_IO_PORT_01_PIN_09
#endif
#ifndef USER_UART_LOG_RXD_PIN
#define USER_UART_LOG_RXD_PIN  BSP_IO_PORT_01_PIN_10
#endif

/* Target baud rate. The FSP-generated g_uart0_baud_setting in ra_gen/hal_data.c
 * is for 115200; we override it at runtime so this stays decoupled from the
 * Configurator. J-Link OB VCOM on the FPB-RA2E3 supports up to ~1 Mbps.
 * Common values: 115200, 230400, 460800, 921600. */
#ifndef USER_UART_LOG_BAUD
#define USER_UART_LOG_BAUD     460800u
#endif

/* Max baud-rate error tolerated by BaudCalculate, in 1/1000 of a percent.
 * 5000 == 5%. The FSP example uses 5000 and it picks the best divider. */
#ifndef USER_UART_LOG_BAUD_ERR_X1000
#define USER_UART_LOG_BAUD_ERR_X1000  5000u
#endif

/*
 * ####################################################################################################################
 * INTERNAL STATE
 * ####################################################################################################################
 */
static volatile uint8_t s_uart_initialized = 0u;

/* Optional per-byte RX callback (registered by CLI layer). */
static UserUartLog_RxCallback_t s_rx_callback = NULL;

/* RX ring buffer (ISR producer / main consumer). Size MUST be a power of 2. */
#define USER_UART_RX_BUF_SIZE   128u
#define USER_UART_RX_BUF_MASK   (USER_UART_RX_BUF_SIZE - 1u)
static volatile uint8_t  s_rx_buf[USER_UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head; /* written by ISR (producer) */
static volatile uint16_t s_rx_tail; /* written by main (consumer) */

/* TX ring buffer (main producer / ISR consumer). Size MUST be a power of 2.
 * Big enough to hold a full ptxCommon_PrintF line (~256 B) plus some CLI echo
 * so logging never blocks the main loop. Increase if you see drops. */
#define USER_UART_TX_BUF_SIZE   1024u
#define USER_UART_TX_BUF_MASK   (USER_UART_TX_BUF_SIZE - 1u)
static volatile uint8_t  s_tx_buf[USER_UART_TX_BUF_SIZE];
static volatile uint16_t s_tx_head;     /* next write index (main)  */
static volatile uint16_t s_tx_tail;     /* next byte to be sent     */
static volatile uint16_t s_tx_chunk;    /* bytes in current FSP write */
static volatile uint8_t  s_tx_busy;     /* 1 while FSP write in flight */

/* Forward decl: kick the next contiguous chunk if idle. May be called from
 * either main (after enqueue) or ISR (on TX_COMPLETE). Caller must guarantee
 * mutual exclusion (we disable IRQs around the main-side call). */
static void user_uart_tx_dispatch_locked(void);
void UserUartLog_Write(const uint8_t *buf, size_t len);
/*
 * ####################################################################################################################
 * CALLBACK
 * ####################################################################################################################
 */
static void user_uart_cb(uart_callback_args_t *p_args)
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
            uint16_t tail = (uint16_t)((s_tx_tail + s_tx_chunk) & USER_UART_TX_BUF_MASK);
            s_tx_tail  = tail;
            s_tx_chunk = 0u;
            s_tx_busy  = 0u;
            user_uart_tx_dispatch_locked();
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

            /* Always store in ring buffer for UserUartLog_RxGet() consumers. */
            uint16_t next = (uint16_t)((s_rx_head + 1u) & USER_UART_RX_BUF_MASK);
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
int UserUartLog_Init(void)
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
                          USER_UART_LOG_TXD_PIN,
                          ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI1_3_5_7_9));
    (void)R_IOPORT_PinCfg(&g_ioport_ctrl,
                          USER_UART_LOG_RXD_PIN,
                          ((uint32_t)IOPORT_CFG_PERIPHERAL_PIN | (uint32_t)IOPORT_PERIPHERAL_SCI1_3_5_7_9));

    /* Override the FSP-generated baud rate (typically 115200) with our own,
     * BEFORE open() so the FSP applies our divider directly. Try the requested
     * rate first; if it cannot be achieved within tolerance, walk down a
     * fallback ladder so we *always* boot with a known, predictable rate
     * (no silent stay-at-115200). Bit-rate modulation is enabled for max
     * flexibility on RA2E3 (small SCI source clock). */
    static const uint32_t k_baud_ladder[] =
    {
        (uint32_t)USER_UART_LOG_BAUD,
        921600u,
        460800u,
        230400u,
        115200u,
    };
    uint32_t chosen_baud = 0u;
    for (size_t i = 0u; i < (sizeof(k_baud_ladder) / sizeof(k_baud_ladder[0])); i++)
    {
        baud_setting_t bs;
        if (FSP_SUCCESS == R_SCI_UART_BaudCalculate(k_baud_ladder[i],
                                                    true, /* bitrate modulation */
                                                    (uint32_t)USER_UART_LOG_BAUD_ERR_X1000,
                                                    &bs))
        {
            g_uart0_baud_setting = bs;
            chosen_baud          = k_baud_ladder[i];
            break;
        }
    }
    /* Tell RTT what we actually programmed so we can confirm without a scope. */
    SEGGER_RTT_printf(0, "[user_uart_log] UART baud = %u (requested %u)\r\n",
                      (unsigned)chosen_baud, (unsigned)USER_UART_LOG_BAUD);

    err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
    if (FSP_SUCCESS != err)
    {
        return (int)err;
    }

    /* Hook our own callback so we observe UART_EVENT_TX_COMPLETE. */
    err = g_uart0.p_api->callbackSet(g_uart0.p_ctrl, user_uart_cb, NULL, NULL);
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
static void user_uart_tx_dispatch_locked(void)
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
    uint16_t end = (head > tail) ? head : (uint16_t)USER_UART_TX_BUF_SIZE;
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

void UserUartLog_Write(const uint8_t *buf, size_t len)
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
        uint16_t next = (uint16_t)((head + 1u) & USER_UART_TX_BUF_MASK);
        if (next == tail)
        {
            break; /* ring full -- drop remainder */
        }
        s_tx_buf[head] = buf[i];
        head = next;
    }
    s_tx_head = head;
    user_uart_tx_dispatch_locked();
    __enable_irq();
}

void UserUartLog_Puts(const char *s)
{
    if (NULL == s)
    {
        return;
    }
    UserUartLog_Write((const uint8_t *)s, strlen(s));
}

size_t UserUartLog_RxAvailable(void)
{
    if (0u == s_uart_initialized)
    {
        return 0u;
    }
    uint16_t head = s_rx_head;
    uint16_t tail = s_rx_tail;
    return (size_t)((head - tail) & USER_UART_RX_BUF_MASK);
}

int UserUartLog_RxGet(uint8_t *out)
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
    s_rx_tail = (uint16_t)((tail + 1u) & USER_UART_RX_BUF_MASK);
    return 1;
}

void UserUartLog_RegisterRxCallback(UserUartLog_RxCallback_t cb)
{
    s_rx_callback = cb;
}

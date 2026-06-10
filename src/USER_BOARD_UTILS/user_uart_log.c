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
 *  - Writes are blocking and serialized: a TX-in-progress flag is polled with
 *    a timeout safety so a missing TX_COMPLETE event can never wedge the app.
 *  - The pin mux for P1_09 (TXD0) / P1_10 (RXD0) is taken from the generated
 *    BSP pin config. If it is missing, add it to g_bsp_pin_cfg in the FSP
 *    Pins tab (or force it here with R_IOPORT_PinCfg, mirroring the SPI
 *    workaround in ptxPLAT_SPI.c).
 */

#include "user_uart_log.h"
#include "hal_data.h"
#include "r_ioport.h"

#include <string.h>

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

/*
 * ####################################################################################################################
 * INTERNAL STATE
 * ####################################################################################################################
 */
static volatile uint8_t s_uart_initialized = 0u;
static volatile uint8_t s_tx_complete      = 1u;   /* 1 = idle / ready to write */

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
            s_tx_complete = 1u;
            break;

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

    s_tx_complete      = 1u;
    s_uart_initialized = 1u;
    return 0;
}

void UserUartLog_Write(const uint8_t *buf, size_t len)
{
    if ((0u == s_uart_initialized) || (NULL == buf) || (0u == len))
    {
        return;
    }

    /* Wait for any previous transfer to finish (with a generous safety bound). */
    uint32_t safety = 1000000u;
    while ((0u == s_tx_complete) && (0u != safety))
    {
        safety--;
    }

    s_tx_complete = 0u;
    fsp_err_t err = g_uart0.p_api->write(g_uart0.p_ctrl, buf, len);
    if (FSP_SUCCESS != err)
    {
        /* Mark idle again so a subsequent call is not blocked. */
        s_tx_complete = 1u;
        return;
    }

    /* Block until TX_COMPLETE fires (last byte fully shifted out of TSR). */
    safety = 1000000u;
    while ((0u == s_tx_complete) && (0u != safety))
    {
        safety--;
    }
}

void UserUartLog_Puts(const char *s)
{
    if (NULL == s)
    {
        return;
    }
    UserUartLog_Write((const uint8_t *)s, strlen(s));
}

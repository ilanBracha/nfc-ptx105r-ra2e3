/*
 * user_uart_log.h
 *
 * Tiny helper that pipes debug strings out via the FSP-generated `g_uart0`
 * (r_sci_uart) instance.  Used as an additional sink for ptxCommon_PrintF
 * so that the boot/initialization log is visible on a USB-UART adapter
 * connected to the PMOD1/Arduino TXD pin (P1_09) in addition to SEGGER RTT.
 */

#ifndef USER_BOARD_UTILS_USER_UART_LOG_H_
#define USER_BOARD_UTILS_USER_UART_LOG_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Open g_uart0 and register the internal TX/RX callback.
 * Safe to call multiple times: the second and later calls are no-ops.
 * Returns 0 on success, non-zero on FSP error.
 */
int  UserUartLog_Init(void);

/**
 * Blocking write of `len` bytes through g_uart0.
 * Returns when the SCI peripheral has finished shifting out the last byte.
 * Silently drops data if Init() has not been called or failed.
 */
void UserUartLog_Write(const uint8_t *buf, size_t len);

/**
 * Convenience wrapper: blocking write of a NUL-terminated string.
 */
void UserUartLog_Puts(const char *s);

#ifdef __cplusplus
}
#endif

#endif /* USER_BOARD_UTILS_USER_UART_LOG_H_ */

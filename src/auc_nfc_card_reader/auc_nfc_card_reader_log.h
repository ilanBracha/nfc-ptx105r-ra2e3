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
#include "pes_nfc_card_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Signature of an optional RX byte callback. When registered, the UART ISR
 * forwards each received byte to this function instead of (in addition to)
 * the internal RX ring buffer. The callback runs in ISR context.
 */
typedef void (*UserUartLog_RxCallback_t)(uint8_t byte);

/**
 * Open g_uart0 and register the internal TX/RX callback.
 * Safe to call multiple times: the second and later calls are no-ops.
 * Returns 0 on success, non-zero on FSP error.
 */
int  UserUartLog_Init(void);

/**
 * Register a callback that will be invoked from the UART RX ISR for every
 * received byte. Pass NULL to unregister. Only one callback is supported.
 * The callback runs in interrupt context — keep it short.
 */
void UserUartLog_RegisterRxCallback(UserUartLog_RxCallback_t cb);

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

/**
 * Non-blocking: returns the number of received bytes currently waiting in the
 * internal RX ring buffer. Safe to call from main context.
 */
size_t UserUartLog_RxAvailable(void);

/**
 * Non-blocking: pop one byte from the RX ring buffer.
 * Returns 1 if a byte was written to *out, 0 if the buffer is empty.
 */
int UserUartLog_RxGet(uint8_t *out);

/**
 * Print formatted card-info block (tag type, UID, size, NDEF) to both
 * RTT and UART.  Pure I/O — no LED or board interaction.
 */
void ptxAPP_PrintCardInfo(const pes_nfc_card_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* USER_BOARD_UTILS_USER_UART_LOG_H_ */

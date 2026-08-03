/*
 * app_nfc_reader_log.h
 *
 * Application log/RX helper. Debug output (printf) is emitted via the
 * pes-console-io stdio layer; this module no longer owns a UART instance.
 * Its remaining jobs are: (1) force the SCI9 pin mux, (2) bridge received
 * UART bytes to a registered RX callback (the CLI), and (3) provide the
 * card-info / buffer print helpers.
 */

#ifndef APP_NFC_READER_LOG_H_
#define APP_NFC_READER_LOG_H_

#include <stddef.h>
#include <stdint.h>
#include "rs_nfc_reader.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_NFC_READER_LOG_COL_RESET        "\x1B[0m"
#define APP_NFC_READER_LOG_COL_BRIGHT_GREEN "\x1B[1;32m"
#define APP_NFC_READER_LOG_COL_BRIGHT_CYAN  "\x1B[1;36m"

/**
 * Signature of an optional RX byte callback. When registered, every received
 * UART byte is forwarded to this function via app_nfc_reader_log_rx_dispatch().
 * The callback runs in the RX ISR context.
 */
typedef void (* app_nfc_reader_log_rx_callback_t)(uint8_t byte);


/**
 * Register a callback that will be invoked from the UART RX ISR for every
 * received byte. Pass NULL to unregister. Only one callback is supported.
 * The callback runs in interrupt context — keep it short.
 */
void app_nfc_reader_log_rx_callback(app_nfc_reader_log_rx_callback_t cb);

/**
 * Forward a single received UART byte to the registered RX callback (if any).
 * Intended to be called from the UART RX ISR for every received byte so
 * consumers such as the CLI see every keystroke. Runs in the caller's (ISR)
 * context — keep it short.
 */
void app_nfc_reader_log_rx_dispatch(uint8_t byte);

/**
 * Convenience wrapper: blocking write of a NUL-terminated string.
 * Install the application UART RX bridge on g_uart_jlob_vcom. This overrides
 * the stdio callback owned by the pes-console-io submodule with a wrapper that
 * forwards every received byte to app_nfc_reader_log_rx_dispatch() (and thus
 * the CLI) before chaining to the original stdio callback so getchar() keeps
 * working. MUST be called after the stdio UART has been opened (i.e. after the
 * first printf/getchar).
 */
void app_nfc_reader_log_attach_rx(void);

/**
 * Print formatted card-info block (tag type, UID, size, NDEF) to both
 * RTT and UART.  Pure I/O — no LED or board interaction.
 */
void app_nfc_reader_log_print_card_info(const rs_nfc_card_result_t * result);

/**
 * Hex/ASCII dump of `bufferLength` bytes from `buffer` (starting at
 * `bufferOffset`) via printf.
 *   addNewLine != 0 : append a trailing newline.
 *   printASCII != 0 : print printable ASCII (non-printables as '.'),
 *                     otherwise print two-digit hex.
 */
void app_nfc_reader_log_print_buffer(uint8_t  * buffer,
                                     uint32_t   bufferOffset,
                                     uint32_t   bufferLength,
                                     uint8_t    addNewLine,
                                     uint8_t    printASCII);

#ifdef __cplusplus
}
#endif

#endif /* APP_NFC_READER_LOG_H_ */

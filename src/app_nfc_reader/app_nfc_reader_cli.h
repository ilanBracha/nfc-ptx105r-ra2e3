/*
 * app_nfc_reader_cli.h
 *
 * Tiny line-based command line interface over the pes-console-io stdio UART.
 *   - The UART RX ISR (registered via app_nfc_reader_log_rx_callback) only
 *     enqueues raw bytes into an ISR-to-main FIFO.
 *   - app_nfc_reader_cli_process(), called from the main loop, drains that
 *     FIFO and performs echo, backspace/line editing and command dispatch
 *     in main context (blocking stdio TX is safe there).
 *
 * Because both logging (printf) and CLI output happen from the same main
 * context, log lines are never interleaved mid-byte with CLI output and the
 * user-visible behavior is: "menu on boot, prompt waiting, async logs scroll
 * past, prompt is re-printed after each command".
 */

#ifndef APP_NFC_READER_CLI_H_
#define APP_NFC_READER_CLI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print the welcome banner + menu, register the UART RX ISR callback, and
 * arm the line buffer.
 */
void app_nfc_reader_cli_init(void);

/**
 * Drain bytes captured by the UART RX ISR and process them (echo, line
 * editing, command dispatch) in main context. Call regularly from the
 * main loop / a task.
 */
void app_nfc_reader_cli_process(void);

/**
 * Print the menu again (useful from anywhere, e.g. after a long log burst).
 */
void app_nfc_reader_cli_print_menu(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_NFC_READER_CLI_H_ */

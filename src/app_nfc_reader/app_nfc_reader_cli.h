/*
 * app_nfc_reader_cli.h
 *
 * Tiny line-based command line interface running on top of g_uart0
 * (see user_uart_log). It is fully interrupt-driven:
 *   - RX bytes are forwarded by the UART ISR to an internal callback
 *     (registered via app_nfc_reader_log_rx_callback) which handles echo,
 *     backspace, and line buffering in ISR context.
 *   - When a full line (CR/LF) is received, the ISR parses and executes
 *     the command immediately — no main-loop polling is needed.
 *
 * Because logging (ptxCommon_PrintF -> UART) happens from the same main
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
 * Maximum number of characters in a single CLI input line (not counting the
 * terminating NUL). Lines longer than this are truncated.
 */
void app_nfc_reader_cli_prompt (void);

/**
 * Print the welcome banner + menu, register the UART RX ISR callback, and
 * arm the line buffer. Requires app_nfc_reader_log_init() to have been called first.
 */
void app_nfc_reader_cli_init(void);

/**
 * Dispatch any pending command that was completed by the UART RX ISR.
 * Since v2 the dispatch happens directly in the RX ISR, so this function
 * is a no-op. Kept for backward compatibility so existing call sites compile.
 */
void app_nfc_reader_cli_process(void);

/**
 * Legacy API — equivalent to app_nfc_reader_cli_process(). Kept for backward
 * compatibility so existing call sites continue to compile.
 */
void app_nfc_reader_cli_poll(void);

/**
 * Print the menu again (useful from anywhere, e.g. after a long log burst).
 */
void app_nfc_reader_cli_print_menu(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_NFC_READER_CLI_H_ */
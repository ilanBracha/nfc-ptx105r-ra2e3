/*
 * app_main_cli.h
 *
 * Tiny line-based command line interface running on top of g_uart0
 * (see user_uart_log). It is fully interrupt-driven:
 *   - RX bytes are forwarded by the UART ISR to an internal callback
 *     (registered via app_main_log_rx_callback) which handles echo,
 *     backspace, and line buffering in ISR context.
 *   - When a full line (CR/LF) is received, the ISR parses and executes
 *     the command immediately — no main-loop polling is needed.
 *
 * Because logging (ptxCommon_PrintF -> UART) happens from the same main
 * context, log lines are never interleaved mid-byte with CLI output and the
 * user-visible behavior is: "menu on boot, prompt waiting, async logs scroll
 * past, prompt is re-printed after each command".
 */

#ifndef APP_MAIN_CLI_H_
#define APP_MAIN_CLI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define APP_MAIN_CLI_WRITE_TEXT_MAX 96u

/**
 * Maximum number of characters in a single CLI input line (not counting the
 * terminating NUL). Lines longer than this are truncated.
 */
void app_main_cli_prompt (void);

/**
 * Print the welcome banner + menu, register the UART RX ISR callback, and
 * arm the line buffer. Requires app_main_log_init() to have been called first.
 */
void app_main_cli_init(void);

/**
 * Dispatch any pending command that was completed by the UART RX ISR.
 * Since v2 the dispatch happens directly in the RX ISR, so this function
 * is a no-op. Kept for backward compatibility so existing call sites compile.
 */
void app_main_cli_process(void);

/**
 * Legacy API — equivalent to app_main_cli_process(). Kept for backward
 * compatibility so existing call sites continue to compile.
 */
void app_main_cli_poll(void);

/**
 * Print the menu again (useful from anywhere, e.g. after a long log burst).
 */
void app_main_cli_print_menu(void);

/**
 * Arm a one-shot "erase the next tag" request from the CLI side. The NFC
 * read loop should poll app_main_cli_is_erase_armed() once a tag has been activated
 * and, if armed, perform the erase and then call app_main_cli_clr_erase_armed().
 */
void    app_main_cli_arm_erase_next (void);
uint8_t app_main_cli_is_erase_armed (void);
void    app_main_cli_clr_erase_armed(void);

/**
 * One-shot "write a Text record to the next tag" request. app_main_cli_arm_write_next
 * stores a copy of `text` (max USER_CLI_WRITE_TEXT_MAX bytes) and arms the flag.
 * The NFC loop polls app_main_cli_is_write_armed(), retrieves the payload via
 * app_main_cli_get_write_text(), performs the write, and calls app_main_cli_clr_write_armed().
 */
void         app_main_cli_arm_write_next (const char * text, uint16_t text_len);
uint8_t      app_main_cli_is_write_armed (void);
const char * app_main_cli_get_write_text (uint16_t * out_len);
void         app_main_cli_clr_write_armed(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_CLI_H_ */

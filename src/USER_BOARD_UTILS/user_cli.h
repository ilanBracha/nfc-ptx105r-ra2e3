/*
 * user_cli.h
 *
 * Tiny line-based command line interface running on top of g_uart0
 * (see user_uart_log). It is fully interrupt-driven:
 *   - RX bytes are forwarded by the UART ISR to an internal callback
 *     (registered via UserUartLog_RegisterRxCallback) which handles echo,
 *     backspace, and line buffering in ISR context.
 *   - When a full line (CR/LF) is received, the ISR parses and executes
 *     the command immediately — no main-loop polling is needed.
 *
 * Because logging (ptxCommon_PrintF -> UART) happens from the same main
 * context, log lines are never interleaved mid-byte with CLI output and the
 * user-visible behavior is: "menu on boot, prompt waiting, async logs scroll
 * past, prompt is re-printed after each command".
 */

#ifndef USER_BOARD_UTILS_USER_CLI_H_
#define USER_BOARD_UTILS_USER_CLI_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Print the welcome banner + menu, register the UART RX ISR callback, and
 * arm the line buffer. Requires UserUartLog_Init() to have been called first.
 */
void UserCli_Init(void);

/**
 * Dispatch any pending command that was completed by the UART RX ISR.
 * Since v2 the dispatch happens directly in the RX ISR, so this function
 * is a no-op. Kept for backward compatibility so existing call sites compile.
 */
void UserCli_Process(void);

/**
 * Legacy API — equivalent to UserCli_Process(). Kept for backward
 * compatibility so existing call sites continue to compile.
 */
void UserCli_Poll(void);

/**
 * Print the menu again (useful from anywhere, e.g. after a long log burst).
 */
void UserCli_PrintMenu(void);

/**
 * Arm a one-shot "erase the next tag" request from the CLI side. The NFC
 * read loop should poll UserCli_IsEraseArmed() once a tag has been activated
 * and, if armed, perform the erase and then call UserCli_ClearEraseArmed().
 */
void    UserCli_ArmEraseNext   (void);
uint8_t UserCli_IsEraseArmed   (void);
void    UserCli_ClearEraseArmed(void);

/**
 * One-shot "write a Text record to the next tag" request. UserCli_ArmWriteNext
 * stores a copy of `text` (max USER_CLI_WRITE_TEXT_MAX bytes) and arms the flag.
 * The NFC loop polls UserCli_IsWriteArmed(), retrieves the payload via
 * UserCli_GetWriteText(), performs the write, and calls UserCli_ClearWriteArmed().
 */
#define USER_CLI_WRITE_TEXT_MAX 96u

void        UserCli_ArmWriteNext   (const char *text, uint16_t text_len);
uint8_t     UserCli_IsWriteArmed   (void);
const char *UserCli_GetWriteText   (uint16_t *out_len);
void        UserCli_ClearWriteArmed(void);

#ifdef __cplusplus
}
#endif

#endif /* USER_BOARD_UTILS_USER_CLI_H_ */

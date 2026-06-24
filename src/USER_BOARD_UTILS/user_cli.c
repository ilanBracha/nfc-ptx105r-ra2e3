/*
 * user_cli.c
 *
 * Tiny interrupt-driven line-based CLI on g_uart0. See user_cli.h.
 *
 * Character reception (echo, backspace, line editing) AND command dispatch are
 * handled entirely inside the UART RX ISR via a callback registered with
 * UserUartLog_RegisterRxCallback(). When a full line is received (CR/LF),
 * the ISR parses and executes the command immediately — no main-loop polling
 * is required. Command handlers are kept simple (set flags, print text) so
 * they are safe to run at ISR priority.
 *
 * Adding a new command:
 *   1. Implement a `static void cmd_xxx(const char *args)` handler.
 *   2. Add an entry to s_cmds[] below (name, handler, one-line help).
 * Commands receive whatever non-whitespace text followed the command name on
 * the same input line (NUL-terminated). They may print using
 * UserUartLog_Puts/Write or ptxCommon_PrintF.
 */

#include "user_cli.h"
#include "user_uart_log.h"
#include "user_board_utils.h"
#include "hal_data.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Optional - only used by some commands. Pulling these in is harmless if
 * the symbol exists at link time; otherwise the command just won't be useful. */
extern void ptxCommon_PrintF(const char *format, ...);

/*
 * ####################################################################################################################
 * CONFIG
 * ####################################################################################################################
 */
#define USER_CLI_COLOR_KNRM  "\x1B[0m"
#define USER_CLI_COLOR_KRED  "\x1B[31m"
#define USER_CLI_COLOR_KGRN  "\x1B[32m"
#define USER_CLI_COLOR_KYEL  "\x1B[33m"
#define USER_CLI_COLOR_KBLU  "\x1B[34m"
#define USER_CLI_COLOR_KMAG  "\x1B[35m"
#define USER_CLI_COLOR_KCYN  "\x1B[36m"
#define USER_CLI_COLOR_KWHT  "\x1B[37m"

#define USER_CLI_LINE_MAX    120u
#define USER_CLI_PROMPT      "$ "
#define USER_CLI_NEWLINE     "\r\n"

/*
 * ####################################################################################################################
 * STATE
 * ####################################################################################################################
 */
typedef void (*cli_cmd_fn_t)(const char *args);

typedef struct
{
    const char   *name;
    cli_cmd_fn_t  handler;
    const char   *help;
} cli_cmd_t;

/* Line buffer filled by the ISR callback (echo + line editing in ISR). */
static volatile char     s_line[USER_CLI_LINE_MAX + 1u];
static volatile uint16_t s_line_len;
static volatile uint8_t  s_prev_was_cr = 0u; /* swallow LF that follows CR (CRLF) */

/* Deferred dispatch: when ISR sees CR/LF it copies the completed line here
 * and sets s_line_ready. Main-context UserCli_Process() picks it up. */
static char              s_pending_line[USER_CLI_LINE_MAX + 1u];
static volatile uint8_t  s_line_ready = 0u;

static uint8_t  s_initialized = 0u;

/* One-shot "erase the next activated tag" request. Set by the `erase` CLI
 * command; consumed by the NFC main loop once it has an active tag. */
static volatile uint8_t s_erase_armed = 0u;

/* One-shot "write a Text record to the next activated tag" request. Set by the
 * `write "..."` CLI command; consumed by the NFC main loop. */
static volatile uint8_t s_write_armed    = 0u;
static char             s_write_text[USER_CLI_WRITE_TEXT_MAX + 1u];
static uint16_t         s_write_text_len = 0u;

void cli_prompt(void);

/*
 * ####################################################################################################################
 * SMALL HELPERS
 * ####################################################################################################################
 */
static void cli_write (const char *s)
{
    UserUartLog_Puts(s);
}

static void cli_write_byte (uint8_t b)
{
    UserUartLog_Write(&b, 1u);
}

void cli_prompt (void)
{
    cli_write(USER_CLI_NEWLINE USER_CLI_PROMPT);
}

static int cli_streq_ci(const char *a, const char *b)
{
    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;
        if ((ca >= 'A') && (ca <= 'Z')) { ca = (char)(ca + ('a' - 'A')); }
        if ((cb >= 'A') && (cb <= 'Z')) { cb = (char)(cb + ('a' - 'A')); }
        if (ca != cb) { return 0; }
        a++; b++;
    }
    return (*a == '\0') && (*b == '\0');
}

/*
 * ####################################################################################################################
 * COMMAND HANDLERS
 * ####################################################################################################################
 */
static void cmd_help (const char *args);

static void cmd_version (const char *args)
{
    (void)args;
    cli_write("PTX IoT Reader (RA2E3 FPB) - CLI v1.0" USER_CLI_NEWLINE);
}

static void cmd_write (const char *args)
{
    /* Expect: write "text to write"
     * The opening quote is mandatory so users can include spaces; the closing
     * quote terminates the payload (no escape sequences). */
    const char *p = args;
    while ((*p == ' ') || (*p == '\t')) { p++; }
    if (*p != '"')
    {
        cli_write("usage: write \"text to write\"" USER_CLI_NEWLINE);
        return;
    }
    p++;
    const char *start = p;
    while ((*p != '\0') && (*p != '"')) { p++; }
    if (*p != '"')
    {
        cli_write("write: missing closing '\"'" USER_CLI_NEWLINE);
        return;
    }
    size_t len = (size_t)(p - start);
    if (0u == len)
    {
        UserCli_ClearWriteArmed();
        cli_write("write: disarmed" USER_CLI_NEWLINE);
        return;
    }
    if (len > USER_CLI_WRITE_TEXT_MAX)
    {
        cli_write("write: text too long (max 96 bytes)" USER_CLI_NEWLINE);
        return;
    }

    UserCli_ArmWriteNext(start, (uint16_t)len);
    /* Arming write supersedes any pending erase. */
    s_erase_armed = 0u;

    cli_write("write: armed - present a TAG to write the NDEF Text record" USER_CLI_NEWLINE);
    cli_write("       (type 'write \"\"' to cancel)" USER_CLI_NEWLINE);
}

static void cmd_erase (const char *args)
{
    (void)args;
    /* Toggle: a second `erase` cancels a pending arm. Arming erase also clears
     * any pending write since the unified NFC hook treats write as the
     * dominant op. */
    if ((0u != s_erase_armed) || (0u != s_write_armed))
    {
        s_erase_armed    = 0u;
        s_write_armed    = 0u;
        s_write_text_len = 0u;
        cli_write("erase: disarmed (no tag will be erased)" USER_CLI_NEWLINE);
    }
    else
    {
        s_erase_armed = 1u;
        cli_write("erase: armed - present a TAG to erase its NDEF content" USER_CLI_NEWLINE);
        cli_write("       (type 'erase' again to cancel)" USER_CLI_NEWLINE);
    }
}

#if (USER_BOARD_LED_FUNC_EN == 1)
static void cmd_ledon (const char *args)
{
    (void)args;
    UserBoardUtils_SetStatusLed(LED_ACTIVE);
    cli_write("status LED: ON" USER_CLI_NEWLINE);
}

static void cmd_ledoff (const char *args)
{
    (void)args;
    UserBoardUtils_SetStatusLed(LED_INACTIVE);
    cli_write("status LED: OFF" USER_CLI_NEWLINE);
}

static void cmd_blink (const char *args)
{
    (void)args;
    UserBoardUtils_BlinkAllLeds();
    cli_write("blink: done" USER_CLI_NEWLINE);
}
#endif

static void cmd_menu (const char *args)
{
    (void)args;
    UserCli_PrintMenu();
}

static void cmd_reboot (const char *args)
{
    (void)args;
    cli_write("rebooting..." USER_CLI_NEWLINE);
    /* Drain TX by waiting a moment, then issue an AIRCR system reset. */
    for (volatile uint32_t i = 0; i < 200000u; i++) { __asm volatile ("nop"); }
    NVIC_SystemReset();
}

static const cli_cmd_t s_cmds[] =
{
    { "help",    cmd_help,    "show this menu"                 },
    { "?",       cmd_help,    "alias of 'help'"                },
    { "menu",    cmd_menu,    "reprint the menu"               },
    { "version", cmd_version, "firmware identification"        },
    { "write",   cmd_write,   "write \"text\" to next tag"     },
    { "erase",   cmd_erase,   "arm: erase NDEF of the next tag"},
#if (USER_BOARD_LED_FUNC_EN == 1)
    { "lon",     cmd_ledon,   "turn the status LED on"         },
    { "loff",    cmd_ledoff,  "turn the status LED off"        },
    { "blink",   cmd_blink,   "blink all board LEDs once"      },
#endif
    { "reboot",  cmd_reboot,  "soft-reset the MCU"             },
};
#define CLI_CMD_COUNT (sizeof(s_cmds) / sizeof(s_cmds[0]))

static void cmd_help(const char *args)
{
    (void)args;
    UserCli_PrintMenu();
}

/*
 * ####################################################################################################################
 * INTERNAL: PARSING / DISPATCH
 * ####################################################################################################################
 */
static void cli_dispatch(char *line)
{
    /* Skip leading whitespace. */
    while ((*line == ' ') || (*line == '\t')) { line++; }
    if (*line == '\0') { return; }

    /* Split into command + args at the first whitespace. */
    char *p = line;
    while ((*p != '\0') && (*p != ' ') && (*p != '\t')) { p++; }
    char *args = p;
    if (*p != '\0')
    {
        *p   = '\0';
        args = p + 1;
        while ((*args == ' ') || (*args == '\t')) { args++; }
    }

    for (size_t i = 0u; i < CLI_CMD_COUNT; i++)
    {
        if (cli_streq_ci(line, s_cmds[i].name))
        {
            s_cmds[i].handler(args);
            return;
        }
    }

    cli_write("unknown command: '");
    cli_write(line);
    cli_write("'  (type 'help')" USER_CLI_NEWLINE);
}

static void cli_handle_byte(uint8_t b)
{
    /* CRLF handling: treat \r as end-of-line, swallow a following \n. */
    if (s_prev_was_cr && (b == '\n'))
    {
        s_prev_was_cr = 0u;
        return;
    }
    s_prev_was_cr = (b == '\r') ? 1u : 0u;

    if ((b == '\r') || (b == '\n'))
    {
        cli_write(USER_CLI_NEWLINE);
        s_line[s_line_len] = '\0';
        if (s_line_len > 0u)
        {
            /* Dispatch the command directly in ISR context. All command handlers
             * only set flags or call UserUartLog_Write (which is ISR-safe), so
             * no deferral to the main loop is required. */
            (void)memcpy(s_pending_line, (const char *)s_line, s_line_len + 1u);
            cli_dispatch(s_pending_line);
            cli_write(USER_CLI_COLOR_KGRN USER_CLI_PROMPT);
        }
        else
        {
            /* Empty line — just reprint the prompt. */
            cli_write(USER_CLI_COLOR_KGRN USER_CLI_PROMPT);
        }
        s_line_len = 0u;
        return;
    }

    /* Backspace / DEL */
    if ((b == 0x08u) || (b == 0x7Fu))
    {
        if (s_line_len > 0u)
        {
            s_line_len--;
            /* Erase the last character on the terminal. */
            cli_write("\b \b");
        }
        return;
    }

    /* Ignore other control chars. */
    if (b < 0x20u)
    {
        return;
    }

    if (s_line_len < USER_CLI_LINE_MAX)
    {
        s_line[s_line_len++] = (char)b;
        cli_write_byte(b); /* local echo */
    }
    else
    {
        /* Line full: beep. */
        cli_write_byte(0x07u);
    }
}

/*
 * ####################################################################################################################
 * ISR CALLBACK (registered with UserUartLog_RegisterRxCallback)
 * ####################################################################################################################
 */
static void cli_rx_isr_callback(uint8_t byte)
{
    if (0u == s_initialized) { return; }
    cli_handle_byte(byte);
}

/*
 * ####################################################################################################################
 * PUBLIC API
 * ####################################################################################################################
 */
void UserCli_PrintMenu(void)
{
    cli_write(USER_CLI_COLOR_KCYN);
    cli_write(USER_CLI_NEWLINE);
    cli_write("=== PTX IoT Reader CLI ===" USER_CLI_NEWLINE);
    for (size_t i = 0u; i < CLI_CMD_COUNT; i++)
    {
        /* Format: "  name        - help" with a simple fixed-width pad. */
        cli_write("  ");
        cli_write(s_cmds[i].name);
        size_t n = strlen(s_cmds[i].name);
        while (n < 10u) { cli_write_byte((uint8_t)' '); n++; }
        cli_write(" - ");
        cli_write(s_cmds[i].help);
        cli_write(USER_CLI_NEWLINE);
    }

    cli_write(USER_CLI_COLOR_KNRM);
}

void UserCli_Init(void)
{
    s_line_len    = 0u;
    s_prev_was_cr = 0u;
    s_line_ready  = 0u;
    s_initialized = 1u;

    /* Register our byte handler so the UART RX ISR feeds us directly. */
    UserUartLog_RegisterRxCallback(cli_rx_isr_callback);

    UserCli_PrintMenu();
}

void UserCli_Process(void)
{
    /* Command dispatch now happens directly in the UART RX ISR, so this
     * function is a no-op. Kept for backward compatibility. */
    (void)0;
}

void UserCli_Poll(void)
{
    /* Legacy API kept for backward compatibility. Equivalent to Process(). */
    UserCli_Process();
}

void UserCli_ArmEraseNext(void)
{
    s_erase_armed = 1u;
}

uint8_t UserCli_IsEraseArmed(void)
{
    return s_erase_armed;
}

void UserCli_ClearEraseArmed(void)
{
    s_erase_armed = 0u;
}

void UserCli_ArmWriteNext(const char *text, uint16_t text_len)
{
    if ((NULL == text) || (0u == text_len) || (text_len > USER_CLI_WRITE_TEXT_MAX))
    {
        s_write_armed    = 0u;
        s_write_text_len = 0u;
        return;
    }
    (void)memcpy(s_write_text, text, text_len);
    s_write_text[text_len] = '\0';
    s_write_text_len       = text_len;
    s_write_armed          = 1u;
}

uint8_t UserCli_IsWriteArmed(void)
{
    return s_write_armed;
}

const char *UserCli_GetWriteText(uint16_t *out_len)
{
    if (NULL != out_len) { *out_len = s_write_text_len; }
    return s_write_text;
}

void UserCli_ClearWriteArmed(void)
{
    s_write_armed    = 0u;
    s_write_text_len = 0u;
}

/*
 * user_cli.c
 *
 * Tiny cooperative line-based CLI on g_uart0. See user_cli.h.
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
#define CLI_LINE_MAX        120u
#define CLI_PROMPT          "$ "
#define CLI_NEWLINE         "\r\n"

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

static char     s_line[CLI_LINE_MAX + 1u];
static uint16_t s_line_len;
static uint8_t  s_initialized = 0u;
static uint8_t  s_prev_was_cr = 0u; /* swallow LF that follows CR (CRLF) */

/* One-shot "erase the next activated tag" request. Set by the `erase` CLI
 * command; consumed by the NFC main loop once it has an active tag. */
static volatile uint8_t s_erase_armed = 0u;

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
    cli_write(CLI_NEWLINE CLI_PROMPT);
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
    cli_write("PTX IoT Reader (RA2E3 FPB) - CLI v1.0" CLI_NEWLINE);
}

static void cmd_erase (const char *args)
{
    (void)args;
    /* Toggle: a second `erase` cancels a pending arm. */
    if (0u != s_erase_armed)
    {
        s_erase_armed = 0u;
        cli_write("erase: disarmed (no tag will be erased)" CLI_NEWLINE);
    }
    else
    {
        s_erase_armed = 1u;
        cli_write("erase: armed - present a TAG to erase its NDEF content" CLI_NEWLINE);
        cli_write("       (type 'erase' again to cancel)" CLI_NEWLINE);
    }
}

static void cmd_ledon (const char *args)
{
    (void)args;
    UserBoardUtils_SetStatusLed(LED_ACTIVE);
    cli_write("status LED: ON" CLI_NEWLINE);
}

static void cmd_ledoff (const char *args)
{
    (void)args;
    UserBoardUtils_SetStatusLed(LED_INACTIVE);
    cli_write("status LED: OFF" CLI_NEWLINE);
}

static void cmd_blink (const char *args)
{
    (void)args;
    UserBoardUtils_BlinkAllLeds();
    cli_write("blink: done" CLI_NEWLINE);
}

static void cmd_menu (const char *args)
{
    (void)args;
    UserCli_PrintMenu();
}

static void cmd_reboot (const char *args)
{
    (void)args;
    cli_write("rebooting..." CLI_NEWLINE);
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
    { "erase",   cmd_erase,   "arm: erase NDEF of the next tag"},
    { "lon",     cmd_ledon,   "turn the status LED on"         },
    { "loff",    cmd_ledoff,  "turn the status LED off"        },
    { "blink",   cmd_blink,   "blink all board LEDs once"      },
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
    cli_write("'  (type 'help')" CLI_NEWLINE);
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
        cli_write(CLI_NEWLINE);
        s_line[s_line_len] = '\0';
        if (s_line_len > 0u)
        {
            cli_dispatch(s_line);
        }
        s_line_len = 0u;
        cli_write(CLI_PROMPT);
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

    if (s_line_len < CLI_LINE_MAX)
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
 * PUBLIC API
 * ####################################################################################################################
 */
void UserCli_PrintMenu(void)
{
    cli_write(CLI_NEWLINE);
    cli_write("=== PTX IoT Reader CLI ===" CLI_NEWLINE);
    for (size_t i = 0u; i < CLI_CMD_COUNT; i++)
    {
        /* Format: "  name        - help" with a simple fixed-width pad. */
        cli_write("  ");
        cli_write(s_cmds[i].name);
        size_t n = strlen(s_cmds[i].name);
        while (n < 10u) { cli_write_byte((uint8_t)' '); n++; }
        cli_write(" - ");
        cli_write(s_cmds[i].help);
        cli_write(CLI_NEWLINE);
    }
    cli_write(CLI_PROMPT);
}

void UserCli_Init(void)
{
    s_line_len    = 0u;
    s_prev_was_cr = 0u;
    s_initialized = 1u;
    UserCli_PrintMenu();
}

void UserCli_Poll(void)
{
    if (0u == s_initialized) { return; }

    uint8_t b;
    /* Drain whatever is available; CLI output uses the same TX path so this
     * naturally yields between bytes. Bound the burst to avoid starving the
     * caller if someone pastes a huge blob. */
    uint16_t budget = 64u;
    while ((budget-- > 0u) && (0 != UserUartLog_RxGet(&b)))
    {
        cli_handle_byte(b);
    }
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

/*
 * app_nfc_reader_cli.c
 *
 * Tiny interrupt-driven line-based CLI on g_uart0. See app_nfc_reader_cli.h.
 *
 * Character reception (echo, backspace, line editing) AND command dispatch are
 * handled entirely inside the UART RX ISR via a callback registered with
 * app_nfc_reader_log_rx_callback(). When a full line is received (CR/LF),
 * the ISR parses and executes the command immediately — no main-loop polling
 * is required. Command handlers are kept simple (set flags, print text) so
 * they are safe to run at ISR priority.
 *
 * Adding a new command:
 *   1. Implement a `static void cmd_xxx(const char *args)` handler.
 *   2. Add an entry to s_cmds[] below (name, handler, one-line help).
 * Commands receive whatever non-whitespace text followed the command name on
 * the same input line (NUL-terminated). They may print using
 * app_nfc_reader_log_puts/Write or ptxCommon_PrintF.
 */

#include "app_nfc_reader_cli.h"
#include "app_nfc_reader_log.h"
#include "hal_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * ####################################################################################################################
 * CONFIG
 * ####################################################################################################################
 */
#define APP_NFC_READER_CLI_COLOR_KNRM  "\x1B[0m"
#define APP_NFC_READER_CLI_COLOR_KRED  "\x1B[31m"
#define APP_NFC_READER_CLI_COLOR_KGRN  "\x1B[32m"
#define APP_NFC_READER_CLI_COLOR_KCYN  "\x1B[36m"
#define APP_NFC_READER_CLI_LINE_MAX    120u
#define APP_NFC_READER_CLI_PROMPT      "$ "
#define APP_NFC_READER_CLI_NEWLINE     "\r\n"
#define APP_NFC_READER_CLI_CMD_COUNT   (sizeof(s_cmds) / sizeof(s_cmds[0]))

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
static volatile char     s_line[APP_NFC_READER_CLI_LINE_MAX + 1u];
static volatile uint16_t s_line_len;
static volatile uint8_t  s_prev_was_cr = 0u; /* swallow LF that follows CR (CRLF) */

/* Deferred dispatch: when ISR sees CR/LF it copies the completed line here
 * and sets s_line_ready. Main-context app_nfc_reader_cli_process() picks it up. */
static char              s_pending_line[APP_NFC_READER_CLI_LINE_MAX + 1u];
static volatile uint8_t  s_line_ready = 0u;

static uint8_t           s_initialized = 0u;

/*
 * ####################################################################################################################
 * SMALL HELPERS
 * ####################################################################################################################
 */
static void app_nfc_reader_cli_write (const char *s)
{
    app_nfc_reader_log_puts(s);
}

static void app_nfc_reader_cli_write_byte (uint8_t b)
{
    app_nfc_reader_log_write(&b, 1u);
}

void app_nfc_reader_cli_prompt (void)
{
    app_nfc_reader_cli_write(APP_NFC_READER_CLI_NEWLINE APP_NFC_READER_CLI_PROMPT);
}

static int app_nfc_reader_cli_streq_ci (const char *a, const char *b)
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

static void app_nfc_reader_cli_cmd_version (const char *args)
{
    (void)args;
    app_nfc_reader_cli_write("PTX IoT Reader (RA2E3 FPB) - CLI v1.0" APP_NFC_READER_CLI_NEWLINE);
}

static void app_nfc_reader_cli_cmd_help (const char *args)
{
    (void)args;
    app_nfc_reader_cli_print_menu();
}

static void app_nfc_reader_cli_cmd_menu (const char *args)
{
    (void)args;
    app_nfc_reader_cli_print_menu();
}

static void app_nfc_reader_cli_cmd_reboot (const char *args)
{
    (void)args;
    app_nfc_reader_cli_write("rebooting..." APP_NFC_READER_CLI_NEWLINE);
    /* Drain TX by waiting a moment, then issue an AIRCR system reset. */
    for (volatile uint32_t i = 0; i < 200000u; i++) { __asm volatile ("nop"); }
    NVIC_SystemReset();
}

static const cli_cmd_t s_cmds[] =
{
    { "help",    app_nfc_reader_cli_cmd_help,    "show this menu"                 },
    { "?",       app_nfc_reader_cli_cmd_help,    "alias of 'help'"                },
    { "menu",    app_nfc_reader_cli_cmd_menu,    "reprint the menu"               },
    { "version", app_nfc_reader_cli_cmd_version, "firmware identification"        },
    { "reboot",  app_nfc_reader_cli_cmd_reboot,  "soft-reset the MCU"             },
};

/*
 * ####################################################################################################################
 * INTERNAL: PARSING / DISPATCH
 * ####################################################################################################################
 */
static void cli_dispatch (char *line)
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

    for (size_t i = 0u; i < APP_NFC_READER_CLI_CMD_COUNT; i++)
    {
        if (app_nfc_reader_cli_streq_ci(line, s_cmds[i].name))
        {
            s_cmds[i].handler(args);
            return;
        }
    }

    app_nfc_reader_cli_write("unknown command: '");
    app_nfc_reader_cli_write(line);
    app_nfc_reader_cli_write("'  (type 'help')" APP_NFC_READER_CLI_NEWLINE);
}

static void app_nfc_reader_cli_handle_byte (uint8_t b)
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
        app_nfc_reader_cli_write(APP_NFC_READER_CLI_NEWLINE);
        s_line[s_line_len] = '\0';
        if (s_line_len > 0u)
        {
            (void)memcpy(s_pending_line, (const char *)s_line, s_line_len + 1u);
            cli_dispatch(s_pending_line);
            app_nfc_reader_cli_write(APP_NFC_READER_CLI_COLOR_KGRN APP_NFC_READER_CLI_PROMPT);
        }
        else
        {
            /* Empty line — just reprint the prompt. */
            app_nfc_reader_cli_write(APP_NFC_READER_CLI_COLOR_KGRN APP_NFC_READER_CLI_PROMPT);
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
            app_nfc_reader_cli_write("\b \b");
        }
        return;
    }

    /* Ignore other control chars. */
    if (b < 0x20u)
    {
        return;
    }

    if (s_line_len < APP_NFC_READER_CLI_LINE_MAX)
    {
        s_line[s_line_len++] = (char)b;
        app_nfc_reader_cli_write_byte(b); /* local echo */
    }
    else
    {
        /* Line full: beep. */
        app_nfc_reader_cli_write_byte(0x07u);
    }
}

/*
 * ####################################################################################################################
 * ISR CALLBACK (registered with app_nfc_reader_log_rx_callback)
 * ####################################################################################################################
 */
static void app_nfc_reader_cli_rx_isr_callback (uint8_t byte)
{
    if (0u == s_initialized) { return; }
    app_nfc_reader_cli_handle_byte(byte);
}

/*
 * ####################################################################################################################
 * PUBLIC API
 * ####################################################################################################################
 */
void app_nfc_reader_cli_print_menu (void)
{
    app_nfc_reader_cli_write(APP_NFC_READER_CLI_COLOR_KCYN);
    app_nfc_reader_cli_write(APP_NFC_READER_CLI_NEWLINE);
    app_nfc_reader_cli_write("=== PTX IoT Reader CLI ===" APP_NFC_READER_CLI_NEWLINE);

    for (size_t i = 0u; i < APP_NFC_READER_CLI_CMD_COUNT; i++)
    {
        /* Format: "  name        - help" with a simple fixed-width pad. */
        app_nfc_reader_cli_write("  ");
        app_nfc_reader_cli_write(s_cmds[i].name);
        size_t n = strlen(s_cmds[i].name);
        while (n < 10u) { app_nfc_reader_cli_write_byte((uint8_t)' '); n++; }
        app_nfc_reader_cli_write(" - ");
        app_nfc_reader_cli_write(s_cmds[i].help);
        app_nfc_reader_cli_write(APP_NFC_READER_CLI_NEWLINE);
    }

    app_nfc_reader_cli_write(APP_NFC_READER_CLI_COLOR_KNRM);
}

void app_nfc_reader_cli_init (void)
{
    s_line_len    = 0u;
    s_prev_was_cr = 0u;
    s_line_ready  = 0u;
    s_initialized = 1u;

    /* Register our byte handler so the UART RX ISR feeds us directly. */
    app_nfc_reader_log_rx_callback(app_nfc_reader_cli_rx_isr_callback);

    app_nfc_reader_cli_print_menu();
}

void app_nfc_reader_cli_process (void)
{
    /* Command dispatch now happens directly in the UART RX ISR, so this
     * function is a no-op. Kept for backward compatibility. */
    (void)0;
}

void app_nfc_reader_cli_poll (void)
{
    /* Legacy API kept for backward compatibility. Equivalent to Process(). */
    app_nfc_reader_cli_process();
}

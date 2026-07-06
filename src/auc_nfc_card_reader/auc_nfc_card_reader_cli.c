/*
 * user_cli.c
 *
 * Tiny interrupt-driven line-based CLI on g_uart0. See user_cli.h.
 *
 * Character reception (echo, backspace, line editing) AND command dispatch are
 * handled entirely inside the UART RX ISR via a callback registered with
 * auc_nfc_card_reader_log_rx_callback(). When a full line is received (CR/LF),
 * the ISR parses and executes the command immediately — no main-loop polling
 * is required. Command handlers are kept simple (set flags, print text) so
 * they are safe to run at ISR priority.
 *
 * Adding a new command:
 *   1. Implement a `static void cmd_xxx(const char *args)` handler.
 *   2. Add an entry to s_cmds[] below (name, handler, one-line help).
 * Commands receive whatever non-whitespace text followed the command name on
 * the same input line (NUL-terminated). They may print using
 * auc_nfc_card_reader_log_puts/Write or ptxCommon_PrintF.
 */

#include "auc_nfc_card_reader_cli.h"
#include "auc_nfc_card_reader_log.h"
#include "auc_nfc_card_reader_utils.h"
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
#define AUC_NFC_CARD_READER_CLI_COLOR_KNRM  "\x1B[0m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KRED  "\x1B[31m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KGRN  "\x1B[32m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KYEL  "\x1B[33m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KBLU  "\x1B[34m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KMAG  "\x1B[35m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KCYN  "\x1B[36m"
#define AUC_NFC_CARD_READER_CLI_COLOR_KWHT  "\x1B[37m"
#define AUC_NFC_CARD_READER_CLI_LINE_MAX    120u
#define AUC_NFC_CARD_READER_CLI_PROMPT      "$ "
#define AUC_NFC_CARD_READER_CLI_NEWLINE     "\r\n"
#define AUC_NFC_CARD_READER_CLI_CMD_COUNT   (sizeof(s_cmds) / sizeof(s_cmds[0]))

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
static volatile char     s_line[AUC_NFC_CARD_READER_CLI_LINE_MAX + 1u];
static volatile uint16_t s_line_len;
static volatile uint8_t  s_prev_was_cr = 0u; /* swallow LF that follows CR (CRLF) */

/* Deferred dispatch: when ISR sees CR/LF it copies the completed line here
 * and sets s_line_ready. Main-context auc_nfc_card_reader_cli_process() picks it up. */
static char              s_pending_line[AUC_NFC_CARD_READER_CLI_LINE_MAX + 1u];
static volatile uint8_t  s_line_ready = 0u;

static uint8_t           s_initialized = 0u;

/* One-shot "erase the next activated tag" request. Set by the `erase` CLI
 * command; consumed by the NFC main loop once it has an active tag. */
static volatile uint8_t  s_erase_armed = 0u;

/* One-shot "write a Text record to the next activated tag" request. Set by the
 * `write "..."` CLI command; consumed by the NFC main loop. */
static volatile uint8_t s_write_armed    = 0u;
static char             s_write_text[AUC_NFC_CARD_READER_CLI_WRITE_TEXT_MAX + 1u];
static uint16_t         s_write_text_len = 0u;

/*
 * ####################################################################################################################
 * SMALL HELPERS
 * ####################################################################################################################
 */
static void auc_nfc_card_reader_cli_write (const char *s)
{
    auc_nfc_card_reader_log_puts(s);
}

static void auc_nfc_card_reader_cli_write_byte (uint8_t b)
{
    auc_nfc_card_reader_log_write(&b, 1u);
}

void auc_nfc_card_reader_cli_prompt (void)
{
    auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_NEWLINE AUC_NFC_CARD_READER_CLI_PROMPT);
}

static int auc_nfc_card_reader_cli_streq_ci (const char *a, const char *b)
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

static void auc_nfc_card_reader_cli_cmd_version (const char *args)
{
    (void)args;
    auc_nfc_card_reader_cli_write("PTX IoT Reader (RA2E3 FPB) - CLI v1.0" AUC_NFC_CARD_READER_CLI_NEWLINE);
}

static void auc_nfc_card_reader_cli_cmd_write (const char *args)
{
    /* Expect: write "text to write"
     * The opening quote is mandatory so users can include spaces; the closing
     * quote terminates the payload (no escape sequences). */
    const char *p = args;
    while ((*p == ' ') || (*p == '\t')) { p++; }
    if (*p != '"')
    {
        auc_nfc_card_reader_cli_write("usage: write \"text to write\"" AUC_NFC_CARD_READER_CLI_NEWLINE);
        return;
    }
    p++;
    const char *start = p;
    while ((*p != '\0') && (*p != '"')) { p++; }
    if (*p != '"')
    {
        auc_nfc_card_reader_cli_write("write: missing closing '\"'" AUC_NFC_CARD_READER_CLI_NEWLINE);
        return;
    }
    size_t len = (size_t)(p - start);
    if (0u == len)
    {
        auc_nfc_card_reader_cli_clr_write_armed();
        auc_nfc_card_reader_cli_write("write: disarmed" AUC_NFC_CARD_READER_CLI_NEWLINE);
        return;
    }
    if (len > AUC_NFC_CARD_READER_CLI_WRITE_TEXT_MAX)
    {
        auc_nfc_card_reader_cli_write("write: text too long (max 96 bytes)" AUC_NFC_CARD_READER_CLI_NEWLINE);
        return;
    }

    auc_nfc_card_reader_cli_arm_write_next(start, (uint16_t)len);
    /* Arming write supersedes any pending erase. */
    s_erase_armed = 0u;

    auc_nfc_card_reader_cli_write("write: armed - present a TAG to write the NDEF Text record" AUC_NFC_CARD_READER_CLI_NEWLINE);
    auc_nfc_card_reader_cli_write("       (type 'write \"\"' to cancel)" AUC_NFC_CARD_READER_CLI_NEWLINE);
}

static void auc_nfc_card_reader_cli_cmd_erase (const char *args)
{
    (void)args;
    /* Toggle: a second `erase` cancels a pending arm. Arming erase also clears
     * any pending write since the unified NFC hook treats write as the
     * dominant op. */
    taskENTER_CRITICAL();
    if ((0u != s_erase_armed) || (0u != s_write_armed))
    {
        s_erase_armed    = 0u;
        s_write_armed    = 0u;
        s_write_text_len = 0u;
        taskEXIT_CRITICAL();
        auc_nfc_card_reader_cli_write("erase: disarmed (no tag will be erased)" AUC_NFC_CARD_READER_CLI_NEWLINE);
    }
    else
    {
        s_erase_armed = 1u;
        taskEXIT_CRITICAL();
        auc_nfc_card_reader_cli_write("erase: armed - present a TAG to erase its NDEF content" AUC_NFC_CARD_READER_CLI_NEWLINE);
        auc_nfc_card_reader_cli_write("       (type 'erase' again to cancel)" AUC_NFC_CARD_READER_CLI_NEWLINE);
    }
}

static void auc_nfc_card_reader_cli_cmd_help (const char *args)
{
    (void)args;
    auc_nfc_card_reader_cli_print_menu();
}

static void auc_nfc_card_reader_cli_cmd_menu (const char *args)
{
    (void)args;
    auc_nfc_card_reader_cli_print_menu();
}

static void auc_nfc_card_reader_cli_cmd_reboot (const char *args)
{
    (void)args;
    auc_nfc_card_reader_cli_write("rebooting..." AUC_NFC_CARD_READER_CLI_NEWLINE);
    /* Drain TX by waiting a moment, then issue an AIRCR system reset. */
    for (volatile uint32_t i = 0; i < 200000u; i++) { __asm volatile ("nop"); }
    NVIC_SystemReset();
}

static const cli_cmd_t s_cmds[] =
{
    { "help",    auc_nfc_card_reader_cli_cmd_help,    "show this menu"                 },
    { "?",       auc_nfc_card_reader_cli_cmd_help,    "alias of 'help'"                },
    { "menu",    auc_nfc_card_reader_cli_cmd_menu,    "reprint the menu"               },
    { "version", auc_nfc_card_reader_cli_cmd_version, "firmware identification"        },
    { "write",   auc_nfc_card_reader_cli_cmd_write,   "write \"text\" to next tag"     },
    { "erase",   auc_nfc_card_reader_cli_cmd_erase,   "arm: erase NDEF of the next tag"},
    { "reboot",  auc_nfc_card_reader_cli_cmd_reboot,  "soft-reset the MCU"             },
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

    for (size_t i = 0u; i < AUC_NFC_CARD_READER_CLI_CMD_COUNT; i++)
    {
        if (auc_nfc_card_reader_cli_streq_ci(line, s_cmds[i].name))
        {
            s_cmds[i].handler(args);
            return;
        }
    }

    auc_nfc_card_reader_cli_write("unknown command: '");
    auc_nfc_card_reader_cli_write(line);
    auc_nfc_card_reader_cli_write("'  (type 'help')" AUC_NFC_CARD_READER_CLI_NEWLINE);
}

static void auc_nfc_card_reader_cli_handle_byte (uint8_t b)
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
        auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_NEWLINE);
        s_line[s_line_len] = '\0';
        if (s_line_len > 0u)
        {
            /* Dispatch the command directly in ISR context. All command handlers
             * only set flags or call auc_nfc_card_reader_log_write (which is ISR-safe), so
             * no deferral to the main loop is required. */
            (void)memcpy(s_pending_line, (const char *)s_line, s_line_len + 1u);
            cli_dispatch(s_pending_line);
            auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_COLOR_KGRN AUC_NFC_CARD_READER_CLI_PROMPT);
        }
        else
        {
            /* Empty line — just reprint the prompt. */
            auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_COLOR_KGRN AUC_NFC_CARD_READER_CLI_PROMPT);
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
            auc_nfc_card_reader_cli_write("\b \b");
        }
        return;
    }

    /* Ignore other control chars. */
    if (b < 0x20u)
    {
        return;
    }

    if (s_line_len < AUC_NFC_CARD_READER_CLI_LINE_MAX)
    {
        s_line[s_line_len++] = (char)b;
        auc_nfc_card_reader_cli_write_byte(b); /* local echo */
    }
    else
    {
        /* Line full: beep. */
        auc_nfc_card_reader_cli_write_byte(0x07u);
    }
}

/*
 * ####################################################################################################################
 * ISR CALLBACK (registered with auc_nfc_card_reader_log_rx_callback)
 * ####################################################################################################################
 */
static void auc_nfc_card_reader_cli_rx_isr_callback (uint8_t byte)
{
    if (0u == s_initialized) { return; }
    auc_nfc_card_reader_cli_handle_byte(byte);
}

/*
 * ####################################################################################################################
 * PUBLIC API
 * ####################################################################################################################
 */
void auc_nfc_card_reader_cli_print_menu (void)
{
    auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_COLOR_KCYN);
    auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_NEWLINE);
    auc_nfc_card_reader_cli_write("=== PTX IoT Reader CLI ===" AUC_NFC_CARD_READER_CLI_NEWLINE);

    for (size_t i = 0u; i < AUC_NFC_CARD_READER_CLI_CMD_COUNT; i++)
    {
        /* Format: "  name        - help" with a simple fixed-width pad. */
        auc_nfc_card_reader_cli_write("  ");
        auc_nfc_card_reader_cli_write(s_cmds[i].name);
        size_t n = strlen(s_cmds[i].name);
        while (n < 10u) { auc_nfc_card_reader_cli_write_byte((uint8_t)' '); n++; }
        auc_nfc_card_reader_cli_write(" - ");
        auc_nfc_card_reader_cli_write(s_cmds[i].help);
        auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_NEWLINE);
    }

    auc_nfc_card_reader_cli_write(AUC_NFC_CARD_READER_CLI_COLOR_KNRM);
}

void auc_nfc_card_reader_cli_init (void)
{
    s_line_len    = 0u;
    s_prev_was_cr = 0u;
    s_line_ready  = 0u;
    s_initialized = 1u;

    /* Register our byte handler so the UART RX ISR feeds us directly. */
    auc_nfc_card_reader_log_rx_callback(auc_nfc_card_reader_cli_rx_isr_callback);

    auc_nfc_card_reader_cli_print_menu();
}

void auc_nfc_card_reader_cli_process (void)
{
    /* Command dispatch now happens directly in the UART RX ISR, so this
     * function is a no-op. Kept for backward compatibility. */
    (void)0;
}

void auc_nfc_card_reader_cli_poll (void)
{
    /* Legacy API kept for backward compatibility. Equivalent to Process(). */
    auc_nfc_card_reader_cli_process();
}

void auc_nfc_card_reader_cli_arm_erase_next (void)
{
    s_erase_armed = 1u;
}

uint8_t auc_nfc_card_reader_cli_is_erase_armed (void)
{
    return s_erase_armed;
}

void auc_nfc_card_reader_cli_clr_erase_armed (void)
{
    s_erase_armed = 0u;
}

void auc_nfc_card_reader_cli_arm_write_next (const char *text, uint16_t text_len)
{
    taskENTER_CRITICAL();

    if ((NULL == text) || (0u == text_len) || (text_len > AUC_NFC_CARD_READER_CLI_WRITE_TEXT_MAX))
    {
        s_write_armed    = 0u;
        s_write_text_len = 0u;
        taskEXIT_CRITICAL();
        return;
    }

    (void)memcpy(s_write_text, text, text_len);
    s_write_text[text_len] = '\0';
    s_write_text_len       = text_len;
    s_write_armed          = 1u;
    taskEXIT_CRITICAL();
}

uint8_t auc_nfc_card_reader_cli_is_write_armed (void)
{
    return s_write_armed;
}

const char * auc_nfc_card_reader_cli_get_write_text (uint16_t *out_len)
{
    taskENTER_CRITICAL();
    if (NULL != out_len) { *out_len = s_write_text_len; }
    const char *p = s_write_text;
    taskEXIT_CRITICAL();
    return p;
}

void auc_nfc_card_reader_cli_clr_write_armed (void)
{
    taskENTER_CRITICAL();
    s_write_armed    = 0u;
    s_write_text_len = 0u;
    taskEXIT_CRITICAL();
}

/*
 * app_nfc_reader_log.c
 *
 * Implementation of the application log/RX helper. See app_nfc_reader_log.h.
 *
 * Notes:
 *  - TX / stdout is handled by the pes-console-io stdio layer (printf); this
 *    module no longer owns a TX ring buffer.
 *  - RX bytes are delivered to a registered callback via
 *    app_nfc_reader_log_rx_dispatch(); the RX bridge is installed by
 *    app_nfc_reader_log_attach_rx().
 *    -- the generated g_bsp_pin_cfg does not include those pins.
 */
#include <stdio.h>
#include "app_nfc_reader_log.h"
#include "hal_data.h"
#include "r_ioport.h"
#include "ptxCOMMON.h"


/*
 * ####################################################################################################################
 * INTERNAL STATE
 * ####################################################################################################################
 */
static volatile uint8_t s_uart_initialized = 0u;

/* Optional per-byte RX callback (registered by CLI layer). */
static app_nfc_reader_log_rx_callback_t s_rx_callback = NULL;

/*
 * ####################################################################################################################
 * API
 * ####################################################################################################################
 */

void app_nfc_reader_log_rx_callback (app_nfc_reader_log_rx_callback_t cb)
{
    s_rx_callback = cb;
}

void app_nfc_reader_log_rx_dispatch (uint8_t byte)
{
    /* Forward to registered callback (e.g. CLI) if present. Runs in the
     * caller's context (typically the UART RX ISR). */
    if (NULL != s_rx_callback)
    {
        s_rx_callback(byte);
    }
}

/*
 * ####################################################################################################################
 * RX BRIDGE (keeps the pes-console-io submodule untouched)
 * ####################################################################################################################
 *
 * The stdio UART is owned by the pes-console-io submodule, whose ISR callback
 * uart_jlob_vcom_callback() only queues bytes for getchar(). Rather than edit
 * that submodule, we install our own wrapper callback here that forwards every
 * received byte to app_nfc_reader_log_rx_dispatch() (and thus the CLI) and then
 * chains to the original submodule callback so stdio (getchar/TX-complete)
 * keeps working. uart_jlob_vcom_callback is a non-static symbol, so we can
 * reference it directly.
 */
extern void uart_jlob_vcom_callback (uart_callback_args_t * p_args);

static void app_nfc_reader_log_rx_uart_cb (uart_callback_args_t * p_args)
{
    if ((NULL != p_args) && (UART_EVENT_RX_CHAR == p_args->event))
    {
        app_nfc_reader_log_rx_dispatch((uint8_t) p_args->data);
    }

    /* Preserve the submodule's stdio behaviour (getchar queue + TX complete). */
    uart_jlob_vcom_callback(p_args);
}

void app_nfc_reader_log_attach_rx (void)
{
    /* Must be called AFTER the stdio layer has opened g_uart_jlob_vcom and
     * installed its own callback (i.e. after the first printf/getchar). We
     * override that callback with a wrapper that first forwards RX bytes to the
     * CLI and then chains to the original stdio callback. */
    (void) g_uart_jlob_vcom.p_api->callbackSet(g_uart_jlob_vcom.p_ctrl,
                                               app_nfc_reader_log_rx_uart_cb,
                                               NULL,
                                               NULL);
}

/*
 * ####################################################################################################################
 * API IMPLEMENTATION
 * ####################################################################################################################
 */

void app_nfc_reader_log_print_buffer (uint8_t  * buffer,
                                      uint32_t   bufferOffset,
                                      uint32_t   bufferLength,
                                      uint8_t    addNewLine,
                                      uint8_t    printASCII)
{
    uint32_t i;
    uint8_t character_to_print;

    if (NULL != buffer)
    {
        if (0 != bufferLength)
        {
            for (i = 0; (i < bufferLength) && (i < (uint32_t) TX_BUFFER_SIZE); i++)
            {
                if ((i > 0) && ((i % (LINE_LENGTH - 5) == 0)))
                {
                    printf("\n     ");
                }

                if (0 == printASCII)
                {
                    printf("%02X", (uint8_t) buffer[i + bufferOffset]);
                }
                else
                {
                    character_to_print = (uint8_t) buffer[i + bufferOffset];

                    if (character_to_print < 0x20)
                    {
                        printf(".");
                    }
                    else
                    {
                        printf("%c", character_to_print);
                    }
                }
            }

            if (0 != addNewLine)
            {
                printf("\n");
            }
        }
    }
}

/*
 * ####################################################################################################################
 * APPLICATION-LEVEL CARD-INFO PRINTER
 * ####################################################################################################################
 *
 * Reads fields from rs_nfc_card_result_t and formats a human-readable block
 * to both RTT and UART.  Pure I/O - no LED or board interaction; the caller
 * is responsible for any visual feedback (blink, etc.).
 */
void app_nfc_reader_log_print_card_info (const rs_nfc_card_result_t * result)
{
    char rf_tech[16];

    if (NULL == result)
    {
        return;
    }

    switch (result->card_type)
    {
        case RS_NFC_CARD_TYPE_ISO14443A:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_2:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4A:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE A");
            break;

        case RS_NFC_CARD_TYPE_ISO14443B:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE B");
            break;

        case RS_NFC_CARD_TYPE_FELICA:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_3:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE F");
            break;

        case RS_NFC_CARD_TYPE_ISO15693:
        case RS_NFC_CARD_TYPE_NFC_TAG_TYPE_5:
            snprintf(rf_tech, sizeof(rf_tech), "TYPE V");
            break;

        default:
            snprintf(rf_tech, sizeof(rf_tech), "UNKNOWN");
            break;
    }

    printf("RF Technology  : "APP_NFC_READER_LOG_COL_BRIGHT_CYAN "%s\n" APP_NFC_READER_LOG_COL_RESET, rf_tech);

    /* Tag Type */
    printf("Tag Type       : %s\n",
           (NULL != result->tag_type_name)
               ? result->tag_type_name : "Unknown");

    /* Serial Number */
    printf("Serial Number  : ");

    if (0u == result->uid_len)
    {
        printf("N/A");
    }
    else
    {
        for (uint8_t i = 0; i < result->uid_len; i++)
        {
            if (i)
            {
                printf(":");
            }

            printf("%02X", result->uid[i]);
        }
    }

    printf("\n");

    /* Size / Writeable */
    if (result->data_area_size > 0u)
    {
        printf("Size           : %u bytes\n",
               (unsigned) result->data_area_size);
        printf("Writeable      : %s\n",
               result->writeable ? "Yes" : "No");
    }
    else
    {
        printf("Size           : N/A\n");
        printf("Writeable      : N/A\n");
    }

    /* NDEF records */
    if (result->ndef_present && (result->ndef_len > 0u))
    {
        printf("NDEF           : %u bytes\n",
               (unsigned) result->ndef_len);
        printf("  NDEF raw (%u bytes):", (unsigned) result->ndef_len);
        for (uint32_t k = 0u; k < result->ndef_len; k++)
        {
            if ((k > 0u) && (0u == (k % 16u)))
            {
                printf("\n                       ");
            }
            printf(" %02X", result->ndef_data[k]);
        }
        printf("\n");
    }
    else
    {
        printf("Records        : (none)\n");
    }
}

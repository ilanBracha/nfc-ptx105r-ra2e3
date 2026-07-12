/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of
       conditions and the following disclaimer in the documentation and/or other
       materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



    THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX1K
    Module      : IOT_READER Demo
    File        : app_main.c

    Description : IoT Reader demo application for PTX1xxR NFC Platform.
                  Thin application layer — all NFC protocol logic lives in the
                  PES NFC Card Reader module (src/nfc_card_reader/).
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "app_main_utils.h"
#include "app_main_cli.h"
#include "ptxCOMMON.h"
#include "ptx_IOT_READER.h"
#include "app_main.h"
#include "app_main_log.h"
#include "pes_nfc_card_reader.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/* RX/TX buffer sizes — used for raw-exchange print buffers */
#define APP_MAIN_RX_BUF_SIZE  300u
#define APP_MAIN_TX_BUF_SIZE  280u

/*
 * ####################################################################################################################
 * DATA EXCHANGE (CLI write/erase + card info + raw demo exchange)
 * ####################################################################################################################
 */
static void app_main_card_event (const pes_nfc_card_result_t *result)
{
    if (NULL == result) { return; }

    /* CLI: one-shot write / erase */
    if ((0u != app_main_cli_is_write_armed()) || (0u != app_main_cli_is_erase_armed()))
    {
        uint8_t      is_write = app_main_cli_is_write_armed();
        const char  *op_name  = is_write ? "Write" : "Erase";
        pes_status_t op_st    = PES_OK;

        if (0u != is_write)
        {
            uint16_t    txt_len = 0u;
            const char *txt     = app_main_cli_get_write_text(&txt_len);
            uint8_t     ndef_buf[APP_MAIN_CLI_WRITE_TEXT_MAX + 7u];
            uint16_t    ndef_len = 0u;

            op_st = PES_NDEF_BuildTextRecord(txt, txt_len, ndef_buf, &ndef_len);
            if (PES_OK == op_st)
            {
                op_st = PES_NFCCardReader_WriteNDEF(result->protocol,
                                                     ndef_buf, ndef_len);
            }
        }
        else
        {
            op_st = PES_NFCCardReader_EraseNDEF(result->protocol);
        }

        ptxCommon_PrintF("%s on protocol 0x%02X: %s\n",
                         op_name, (unsigned)result->protocol,
                         (PES_OK == op_st) ? "OK" : "ERROR");

        app_main_cli_clr_write_armed();
        app_main_cli_clr_erase_armed();
        return;
    }

    /* Read card info via PES - Cast away const — 
     * PES_NFCCardReader_ReadCardInfo populates the
     * extended fields (data_area_size, writeable, tag_type_name, ndef_*).
     * The result was handed to us by PES and is still alive. */
    (void)PES_NFCCardReader_ReadCardInfo(result->protocol,
                                         (pes_nfc_card_result_t *)result);

    /* LED blink by card type (board feedback kept out of the log module) */
    {
        app_main_utils_card_type_t led_ct;
        switch (result->card_type)
        {
            case PES_NFC_CARD_TYPE_ISO14443B:
            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_4B:
                led_ct = UserBoardUtils_CardType_B; break;
            case PES_NFC_CARD_TYPE_FELICA:
            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_3:
                led_ct = UserBoardUtils_CardType_F; break;
            case PES_NFC_CARD_TYPE_ISO15693:
            case PES_NFC_CARD_TYPE_NFC_TAG_TYPE_5:
                led_ct = UserBoardUtils_CardType_V; break;
            default:
                led_ct = UserBoardUtils_CardType_A; break;
        }
        app_main_utils_blink_for_card_type(led_ct);
    }

    app_main_log_print_card_info(result);

    /* Raw protocol exchange */
    if ((PES_NFC_PROT_ISODEP != result->protocol) &&
        (PES_NFC_PROT_UNDEFINED != result->protocol))
    {
        static uint8_t tx_buf[APP_MAIN_TX_BUF_SIZE];
        static uint8_t rx_buf[APP_MAIN_RX_BUF_SIZE];
        uint32_t tx_len = 0u;
        uint32_t rx_len = APP_MAIN_RX_BUF_SIZE;

        pes_status_t st = PES_NFCCardReader_RawExchange(
            result->protocol,
            result->uid, result->uid_len,
            tx_buf, &tx_len,
            rx_buf, &rx_len);

        ptxCommon_PrintF("=========== DATA EXCHANGE ================\n");
        ptxCommon_PrintF("TX = ");
        ptxCommon_Print_Buffer(tx_buf, 0, tx_len, 1, 0);
        if (PES_OK == st)
        {
            ptxCommon_PrintF("RX = ");
            ptxCommon_Print_Buffer(rx_buf, 0, rx_len, 1, 0);
        }
        else
        {
            ptxCommon_PrintF("ERROR - RF-Exchange failed (status=%d)\n",
                             (int)st);
        }
        ptxCommon_PrintF("==========================================\n");
    }
}

/*
 * ####################################################################################################################
 * CALLBACKS
 * ####################################################################################################################
 */
static void on_nfc_read_done (pes_status_t status,
                              const pes_nfc_card_result_t *result,
                              const char *summary,
                              void *p_context)
{
    (void)p_context;

    app_main_utils_set_stat_led(BSP_IO_LEVEL_HIGH);

    /* Informational / warning / fatal events (result == NULL) */
    if (NULL == result)
    {
        if (NULL != summary)
        {
            ptxCommon_PrintF("%s\n", summary);
        }

        app_main_utils_set_stat_led(BSP_IO_LEVEL_LOW);
        return;
    }

    ptxCommon_PrintF(RTT_CTRL_TEXT_BRIGHT_GREEN "\n\n%s" RTT_CTRL_RESET "\n",
                     (NULL != summary) ? summary : "CARD DETECTED!");

    app_main_card_event(result);
    (void)status;

    app_main_utils_set_stat_led(BSP_IO_LEVEL_LOW);
}

static void on_nfc_operation_done (pes_status_t status, void *p_context)
{
    (void)p_context;
    ptxCommon_PrintF("PES_NFCCardReader_Read completed (status=%d)\n", (int)status);
    g_ioport.p_api->pinWrite(g_ioport.p_ctrl, APP_MAIN_UTILS_LED_RD, APP_MAIN_UTILS_LED_INACTV);
}

/*
 * ####################################################################################################################
 * APPLICATION ENTRY POINT
 * ####################################################################################################################
 */
void app_main_entry (void)
{
    app_main_log_init();
    app_main_cli_init();
    app_main_init();
}

void app_main_init (void)
{
    g_ioport.p_api->pinWrite(g_ioport.p_ctrl, APP_MAIN_UTILS_LED_RD, APP_MAIN_UTILS_LED_ACTV);

    ptxCommon_PrintF("System Initialization (PES NFC Card Reader) ... starting\n");

    pes_nfc_card_reader_cfg_t cfg;
    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.reader                = PES_NFC_READER_PTX105R;
    cfg.tech_mask             = PES_NFC_TECH_ALL;
    cfg.timeout_ms            = UINT32_MAX;
    cfg.retry_count           = 0u;
    cfg.read_ndef             = true;
    cfg.max_ndef_bytes        = PES_NFC_NDEF_MAX_BYTES;
    cfg.callback              = on_nfc_operation_done;
    cfg.p_context             = NULL;
    cfg.on_card_event         = on_nfc_read_done;
    cfg.p_card_event_context  = NULL;
    cfg.validate_dependencies = false;

    static pes_nfc_card_result_t result;
    (void)memset(&result, 0, sizeof(result));

    pes_status_t st = PES_NFCCardReader_Read(&cfg, &result);
    if (PES_OK != st)
    {
        ptxCommon_PrintF("PES_NFCCardReader_Read launch FAILED (status=%d)\n",
                         (int)st);
    }
    else
    {
        ptxCommon_PrintF("PES NFC Card Reader launched (non-blocking)\n");
    }
}

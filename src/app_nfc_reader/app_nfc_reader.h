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
    File        : app_nfc_reader.h

    Description :
*/

#ifndef APP_NFC_READER_H_
#define APP_NFC_READER_H_

#include "SEGGER_RTT.h" /* Import RTT Library */
#include "hal_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES
 * ####################################################################################################################
 */

/* Enable on-board LED feedback (comment out to disable LED code entirely). */
#define APP_NFC_READER_LED_EN

#define APP_NFC_READER_SEPARATOR_TOP "=========== DATA EXCHANGE ================\n"
#define APP_NFC_READER_SEPARATOR_BOT "==========================================\n"

#if defined(APP_NFC_READER_LED_EN)
#define APP_NFC_READER_LED_1         BSP_IO_PORT_02_PIN_13
#define APP_NFC_READER_LED_2         BSP_IO_PORT_09_PIN_14

void app_nfc_reader_led_init(void);
void app_nfc_reader_led_set_all(bsp_io_level_t level);
#endif /* APP_NFC_READER_LED_EN */

/**
 * \brief IoT Reader application.
 *
 * This function is the entry point for the IoT Reader application with discovery loop.
 *
 */
void app_nfc_reader_init(void);
void app_nfc_reader_entry(void);

#ifdef __cplusplus
}
#endif

#endif /* Guard */
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
    Module      : HCE Loopback Function
    File        : ptxHCE_Loopback.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxCOMMON.h"
#include "ptxHCE_ISO7816.h"
#include <STACK/EXAMPLE/COMMON/ptxHCE_Loopback.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * Comment / Uncomment this #define to enable output via printf
 */
//#define ENABLE_PRINTF_OPTION

#ifdef ENABLE_PRINTF_OPTION
    #include <STACK/EXAMPLE/DEBUG_PORT/ptxDBG_PORT.h>
    #include <stdarg.h>
    #include <stdio.h>
#endif


/*
 * ####################################################################################################################
 * INTERNAL TYPES
 * ####################################################################################################################
 */


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */


/*
 * Process data for DTA
 */
static ptxStatus_t ptxHce_LoopbackProcessData(uint8_t *rxData, uint32_t rxDataLen, uint8_t *txData, uint16_t *txDataLen);

/*
 * set status word
 */
static void ptxHce_SetStatusWord(uint8_t *txData, uint32_t txDataOffset, uint16_t statusWord);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */


void ptxHce_Loopback_Demo(ptxHce_t *hce)
{
    ptxStatus_t st = ptxStatus_Success;
    ptxHce_EventRecord_t *hce_event = NULL;
    ptxHce_RfProtocol_t rf_prot = HceRfProt_Undefined;
    uint8_t deactivate_reason;
    uint16_t tx_len = 0;

    uint16_t tx_buffer_size = TX_BUFFER_SIZE;
    uint8_t tx_buffer[tx_buffer_size];

    if (NULL != hce)
    {
        /*
         * Consume all pending events as fast as possible.
         * Stay in HCE due to exclusivity of poll/listen.
         **/

        while (1)
        {
            tx_len = RX_BUFFER_SIZE;
            st = ptxHce_Get_Event (hce, &hce_event);

            switch (PTX_GET_STATUS(st))
            {
                case ptxStatus_Success:

                    switch (hce_event->EventID)
                    {
                        case HceEvent_ExtFieldOn:
                            rf_prot = HceRfProt_Undefined;
                            ptxCommon_PrintF("Event: FIELD ON\n");
                            break;

                        case HceEvent_ExtFieldOff:
                            rf_prot = HceRfProt_Undefined;
                            ptxCommon_PrintF("Event: FIELD OFF\n");
                            break;

                        case HceEvent_Activated_ListenA:
                            rf_prot = (ptxHce_RfProtocol_t)hce_event->RxMsgData[0];

                            switch (rf_prot)
                            {
                                case HceRfProt_IsoDep:
                                    ptxCommon_PrintF("Event: ACTIVATED (RF-Protocol ISO-DEP/T4T)\n");
                                    /* Reset (User) Application SELECT-status */
                                    break;

                                case HceRfProt_T2T:
                                    ptxCommon_PrintF("Event: ACTIVATED (RF-Protocol T2T)\n");
                                    break;

                                default:
                                    /* optional - inform upper layers */
                                    ptxCommon_PrintF("Event: ACTIVATED (RF-Protocol Unknown)\n");
                                    break;
                            }
                            break;

                        case HceEvent_Data:
                            if (NULL != hce_event->RxMsgData)
                            {
                                ptxCommon_PrintF("Event: DATA\n");
                                /* Process received data - call actual application handler */
                                ptxHce_LoopbackProcessData(&hce_event->RxMsgData[0], hce_event->RxMsgDataLen, &tx_buffer[0], &tx_len);
                            }
                            else
                            {
                                /*
                                 * RxMsgData == NULL means the provided application buffer is too small.
                                 **/
                                ptxCommon_PrintF("Provided application buffer too small\n");
                            }
                            if (0 < tx_len)
                            {
                                st = ptxHce_Send_Data(hce, tx_buffer, tx_len);
                            }
                            break;

                        case HceEvent_Deactivated:
                            /*
                             * Get DEACTIVATE-Reason
                             *  => 0x01 = DESELECT received by Reader
                             *  => 0x02 = RELEASE received by Reader
                             *  => 0x03 = Field turned off by Reader
                             */
                            deactivate_reason = hce_event->RxMsgData[0];

                            switch (deactivate_reason)
                            {
                                /* optional - inform upper layers */
                                case PTX_HCE_DEACTIVATE_REASON_DESELECT:
                                    ptxCommon_PrintF("Event: DEACTIVATED (Deselect)\n");
                                    break;

                                case PTX_HCE_DEACTIVATE_REASON_FIELD_OFF:
                                    ptxCommon_PrintF("Event: DEACTIVATED (Field-Off)\n");
                                    break;

                                case PTX_HCE_DEACTIVATE_REASON_RELEASE:
                                    ptxCommon_PrintF("Event: DEACTIVATED (Released)\n");
                                    break;

                                default:
                                    /* unknown reason */
                                    break;
                            }

                            /* Reset (User) Application SELECT-status */
                            rf_prot = HceRfProt_Undefined;
                            break;

                        default:
                            /* shall never happen */
                            break;
                    }
                    break;

                case ptxStatus_NoDataAvailable:
                    /* No event available */
                    break;

                default:
                    /* general error */
                    break;
            }
        }
    } else
    {
        st = PTX_STATUS(ptxStatus_Comp_IoTReader,ptxStatus_InvalidParameter);
    }
}

ptxStatus_t ptxHce_LoopbackProcessData(uint8_t *rxData, uint32_t rxDataLen, uint8_t *txData, uint16_t *txDdataLen)
{
    ptxStatus_t st = ptxStatus_Success;

    if ((NULL != rxData) && (NULL != txData) && (NULL != txDdataLen))
    {
        /* All supported custom commands fit into the minimum ISO-7816 Header-length (4 Byte) */
        if (rxDataLen >= PTX_HCE_ISO7816_HEADER_LEN)
        {
            if ((rxData[PTX_HCE_ISO7816_OFFSET_CLA] == (uint8_t)0x00) &&
                (rxData[PTX_HCE_ISO7816_OFFSET_INS] == (uint8_t)0xA4) &&
                (rxData[PTX_HCE_ISO7816_OFFSET_P1]  == (uint8_t)0x04) &&
                (rxData[PTX_HCE_ISO7816_OFFSET_P2]  == (uint8_t)0x00))
            {
                txData[0] = (uint8_t)0x01;
                txData[1] = (uint8_t)0x00;
                ptxHce_SetStatusWord(txData, (uint32_t)2, PTX_HCE_ISO7816_SW_SUCCESS);
                *txDdataLen = (uint8_t)(PTX_HCE_ISO7816_SW_LEN + 2);
            }
            else if ((rxData[PTX_HCE_ISO7816_OFFSET_CLA] == (uint8_t)0x80) &&
                     (rxData[PTX_HCE_ISO7816_OFFSET_INS] == (uint8_t)0xEE) &&
                     (rxData[PTX_HCE_ISO7816_OFFSET_P1]  == (uint8_t)0x00) &&
                     (rxData[PTX_HCE_ISO7816_OFFSET_P2]  == (uint8_t)0x00))
            {
                // CR13 requires to return the entire received APDU
                (void)memcpy(&txData[0], &rxData[PTX_HCE_ISO7816_OFFSET_CLA], rxDataLen);
                ptxHce_SetStatusWord(txData, (uint32_t)rxDataLen, PTX_HCE_ISO7816_SW_SUCCESS);
                *txDdataLen = (uint16_t)(PTX_HCE_ISO7816_SW_LEN + (uint16_t)rxDataLen);
            }
            else
            {
                ptxHce_SetStatusWord(txData, (uint32_t)0, PTX_HCE_ISO7816_SW_COMMAND_NOT_ALLOWED);
                *txDdataLen = PTX_HCE_ISO7816_SW_LEN;
            }
        }
        else
        {
            ptxHce_SetStatusWord(txData, (uint32_t)0, PTX_HCE_ISO7816_SW_WRONG_LENGTH);
            *txDdataLen = PTX_HCE_ISO7816_SW_LEN;
        }
    }
    else
    {
        st = ptxStatus_InternalError;
    }

    return st;
}

static void ptxHce_SetStatusWord(uint8_t *txData, uint32_t txDataOffset, uint16_t statusWord)
{
    /* This is a local helper-function; Pointers / Offsets are assumed to be valid / checked by the calling function */
    txData[txDataOffset + 0] = (uint8_t)((statusWord >> 8) & (uint8_t)0xFF);
    txData[txDataOffset + 1] = (uint8_t)((statusWord >> 0) & (uint8_t)0xFF);
}



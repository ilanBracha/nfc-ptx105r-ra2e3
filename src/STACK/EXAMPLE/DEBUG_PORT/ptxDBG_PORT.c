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
    Module      : DEBUG_PORT
    File        : ptxPDBG_PORT.c

    Description : implementation of debug port on uart.
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxDBG_PORT.h"
#include "ptxPLAT_EXT.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define PTX_DBGPORT_UART_INSTANCE       (uart_instance_t*)&g_uart_debug

/**
 * Platform-specific. Tx State types.
 */
typedef enum ptxDBGPORT_TxState
{
    DPGPORT_TxIdle,
    DPGPORT_TxOngoing,
    DPGPORT_TxDone,
    DPGPORT_TxError
}ptxDBGPORT_TxState_t;

/**
 * Platform-specific. Rx State types.
 */
typedef enum ptxDBGPORT_RxState
{
    DPGPORT_RxIdle,
    DPGPORT_RxOngoing,
    DPGPORT_RxDone,
    DPGPORT_RxError
}ptxDBGPORT_RxState_t;

/**
 * Platform-specific. UART Main structure.
 */
typedef struct ptxDBGPORT
{
        uart_instance_t                 *UartInstance;      /**< Renesas-specific Uart Instance.*/
        uint32_t                        IntfSpeed;          /**< Uart baudrate in bps. Initial should be 115200. */
        volatile ptxDBGPORT_TxState_t   TxState;            /**< State of the transmission operation */
        volatile ptxDBGPORT_RxState_t   RxState;            /**< State of the recepcion operation */
        ptxHostRequestHandler           RqHandler;          /**< Application provided callback to handle uart input (e.g. keyboard input) */
}ptxDBGPORT_t;

/**
 * Instance of DebugPort context. Set initial values.
 */
ptxDBGPORT_t dbgport_ctx =
{
        NULL,
        0,
        0,
        0,
        NULL
};

#ifdef _RENESAS_RA_
    #define ERROR_STATUS    fsp_err_t
    #define STATUS_SUCCESS  FSP_SUCCESS
#elif defined _RENESAS_SYNERGY_
    #define ERROR_STATUS    ssp_err_t
    #define STATUS_SUCCESS  SSP_SUCCESS
#else
    #warning  "No IDE set!"
#endif

#define PTX_DBGPORT_APP_CORE_FUNCTION_SIZE      (1004)
#define PTX_DBGPORT_APP_TEST_HEADER_SIZE        (20)

#define PTX_DBGPORT_RXBUF_SIZE      (PTX_DBGPORT_APP_CORE_FUNCTION_SIZE + PTX_DBGPORT_APP_TEST_HEADER_SIZE)
#define DBGPORT_STATUS_SUCCESS      0
#define DBGPORT_STATUS_ERROR        1U


/**
 * Allocate Rx buffer. No need for initialization. Only control parameters need to initialize.
 */
static uint8_t ptxDBGPort_RxBuffer[PTX_DBGPORT_RXBUF_SIZE];
static volatile uint32_t ptxDBGPort_RxBufferReadIndex = 0;
static volatile uint32_t ptxDBGPort_RxBufferWriteIndex = 0;
static volatile uint32_t ptxDBGPort_ReadBusy = 0;
static volatile uint32_t ptxDBGPort_WriteBusy = 0;

/**
 * Internal functions.
 */
static uint16_t ptxDBGPORT_Send (char *buff);
static uint16_t ptxDBGPORT_Send_Buffer (uint8_t *buffer, uint32_t bufferLen);
static void ptxDBGPORT_ParseHostRequest(void);


/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
uint16_t ptxDBGPORT_Open(void)
{
    uint16_t status = DBGPORT_STATUS_ERROR;

    dbgport_ctx.UartInstance = PTX_DBGPORT_UART_INSTANCE;

    ERROR_STATUS err;
#ifdef _RENESAS_SYNERGY_
    /* Copy default configuration to modify some parameters. BaudRate */
    uart_cfg_t g_uart0_cfg_temp;
    (void)memcpy(&g_uart0_cfg_temp, dbgport_ctx.UartInstance->p_cfg, sizeof(uart_cfg_t));
    g_uart0_cfg_temp.baud_rate = PTX_DBGPORT_DEFAULT_UART_BAUDRATE;
    err = dbgport_ctx.UartInstance->p_api->open(dbgport_ctx.UartInstance->p_ctrl, &g_uart0_cfg_temp);
#elif defined _RENESAS_RA_
    err = dbgport_ctx.UartInstance->p_api->open(dbgport_ctx.UartInstance->p_ctrl, dbgport_ctx.UartInstance->p_cfg);
    if(STATUS_SUCCESS == err)
    {
        baud_setting_t baud_setting;
        err = R_SCI_UART_BaudCalculate(PTX_DBGPORT_DEFAULT_UART_BAUDRATE, false, PTX_DBGPORT_MAX_UART_BAUDRATE_ERROR, &baud_setting);
        if(STATUS_SUCCESS == err)
        {
            err = R_SCI_UART_BaudSet(dbgport_ctx.UartInstance->p_ctrl, (void *) &baud_setting);
            if(STATUS_SUCCESS == err)
            {
                status = ptxStatus_Success;
            }
        }
    }
#endif
    if (STATUS_SUCCESS == err)
    {
        dbgport_ctx.IntfSpeed = PTX_DBGPORT_DEFAULT_UART_BAUDRATE;
        status = DBGPORT_STATUS_SUCCESS;
    } else
    {
        (void)memset(&dbgport_ctx, 0, sizeof(ptxDBGPORT_t));
    }

    return status;
}


uint16_t ptxDBGPORT_Close(void)
{
    uint16_t status = DBGPORT_STATUS_ERROR;

    dbgport_ctx.UartInstance = PTX_DBGPORT_UART_INSTANCE;

    ERROR_STATUS err = dbgport_ctx.UartInstance->p_api->close(dbgport_ctx.UartInstance->p_ctrl);

    if (STATUS_SUCCESS == err)
    {
        (void)memset(&dbgport_ctx, 0, sizeof(ptxDBGPORT_t));
        status = DBGPORT_STATUS_SUCCESS;
    }

    return status;
}

uint16_t ptxDBGPORT_Write(char *message)
{
    uint16_t status = DBGPORT_STATUS_ERROR;

    if(NULL != message)
    {
        if(NULL == dbgport_ctx.UartInstance)
        {
            /** Init Uart port. */
            status = ptxDBGPORT_Open();
        } else
        {
            status = DBGPORT_STATUS_SUCCESS;
        }

        if(DBGPORT_STATUS_SUCCESS == status)
        {
            status = ptxDBGPORT_Send(message);
        } else
        {
            status = ptxDBGPORT_Close();
        }
    }

    return status;
}

uint16_t ptxDBGPORT_Write_Buffer(uint8_t *buffer, uint32_t bufferLen)
{
    uint16_t status = DBGPORT_STATUS_ERROR;

    if((NULL != buffer) && (0 != bufferLen))
    {
        if(NULL == dbgport_ctx.UartInstance)
        {
            /** Init Uart port. */
            status = ptxDBGPORT_Open();
        } else
        {
            status = DBGPORT_STATUS_SUCCESS;
        }

        if(DBGPORT_STATUS_SUCCESS == status)
        {
            status = ptxDBGPORT_Send_Buffer(buffer, bufferLen);
        } else
        {
            status = ptxDBGPORT_Close();
        }
    }

    return status;
}

void ptxDBGPORT_Reset_FIFO_Level(void)
{
    ptxDBGPort_WriteBusy = 1U;
    ptxDBGPort_RxBufferWriteIndex = 0;
    ptxDBGPort_RxBufferReadIndex = 0;
    ptxDBGPort_WriteBusy = 0U;
}

uint32_t ptxDBGPORT_Get_Available_Bytes(void)
{
    uint32_t available_bytes = 0;

    while ((0 != ptxDBGPort_WriteBusy) && (0 != ptxDBGPort_ReadBusy));

    if (ptxDBGPort_RxBufferWriteIndex >= ptxDBGPort_RxBufferReadIndex)
    {
        available_bytes = ptxDBGPort_RxBufferWriteIndex - ptxDBGPort_RxBufferReadIndex;
    }
    else
    {
        available_bytes = (PTX_DBGPORT_RXBUF_SIZE - ptxDBGPort_RxBufferReadIndex) + ptxDBGPort_RxBufferWriteIndex;
    }

    return available_bytes;
}

uint16_t ptxDBGPORT_Read_Buffer_(uint8_t *buffer, uint32_t bufferOffset)
{
    uint16_t status = DBGPORT_STATUS_ERROR;
    uint32_t copy_len;

    if (NULL != buffer)
    {
        copy_len = ptxDBGPORT_Get_Available_Bytes();

        ptxDBGPort_ReadBusy = 1U;

        for (uint32_t i = 0; i < copy_len; i++)
        {
            buffer[bufferOffset + i] = ptxDBGPort_RxBuffer[ptxDBGPort_RxBufferReadIndex];
            ptxDBGPort_RxBufferReadIndex++;

            if (ptxDBGPort_RxBufferReadIndex >= PTX_DBGPORT_RXBUF_SIZE)
            {
                ptxDBGPort_RxBufferReadIndex = 0;
            }
        }

        ptxDBGPort_ReadBusy = 0U;
        status = DBGPORT_STATUS_SUCCESS;
    }

    return status;
}

#ifdef  DBGPORT_HOSTRQ_HANDLER_REGISTERED
uint16_t ptxDBGPORT_RegisterCB(void *cb)
{
    uint16_t status = DBGPORT_STATUS_ERROR;

    if(NULL != cb)
    {
        dbgport_ctx.RqHandler = cb;
        status = DBGPORT_STATUS_SUCCESS;
    }

    return status;
}
#endif

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static uint16_t ptxDBGPORT_Send (char *buff)
{
    uint16_t ret = DBGPORT_STATUS_ERROR;

    uint32_t buffLen = 0;

    /** Determine buffer length. */
    while(('\0' != buff[buffLen]) && (buffLen <= PTX_DBGPORT_MAX_TX_BUFF_SIZE))
    {
        buffLen++;
    }

    dbgport_ctx.TxState = DPGPORT_TxOngoing;
    ERROR_STATUS err = dbgport_ctx.UartInstance->p_api->write(dbgport_ctx.UartInstance->p_ctrl, (uint8_t*)buff, (uint32_t)buffLen);

    if (STATUS_SUCCESS == err)
    {
        while (DPGPORT_TxOngoing == dbgport_ctx.TxState)
        {
            /* Wait for on-going transfer to be finished. */
        }

        if (DPGPORT_TxDone == dbgport_ctx.TxState)
        {
            /* Successful operation. */
            ret = DBGPORT_STATUS_SUCCESS;
        }
    }

    return ret;
}

static uint16_t ptxDBGPORT_Send_Buffer (uint8_t *buffer, uint32_t bufferLen)
{
    uint16_t ret = DBGPORT_STATUS_ERROR;

    dbgport_ctx.TxState = DPGPORT_TxOngoing;
    ERROR_STATUS err = dbgport_ctx.UartInstance->p_api->write(dbgport_ctx.UartInstance->p_ctrl, buffer, bufferLen);

    if (STATUS_SUCCESS == err)
    {
        while (DPGPORT_TxOngoing == dbgport_ctx.TxState)
        {
            /* Wait for on-going transfer to be finished. */
        }

        if (DPGPORT_TxDone == dbgport_ctx.TxState)
        {
            /* Successful operation. */
            ret = DBGPORT_STATUS_SUCCESS;
        }
    }

    return ret;
}

static void ptxDBGPORT_ParseHostRequest(void)
{
    if(NULL != dbgport_ctx.RqHandler)
    {
        dbgport_ctx.RqHandler(&ptxDBGPort_RxBuffer[0], ptxDBGPort_RxBufferWriteIndex);
    }
}

/*
 * ####################################################################################################################
 * ISR
 * ####################################################################################################################
 */
void ptxDBGPORT_Callback(uart_callback_args_t *p_args)
{
    if(NULL != p_args)
    {
        switch(p_args->event)
        {
            case UART_EVENT_RX_CHAR:
                ptxDBGPort_WriteBusy = 1U;

                /** Just fill the Rx buffer with data and check limits. */
                if(ptxDBGPort_RxBufferWriteIndex >= PTX_DBGPORT_RXBUF_SIZE)
                {
                    ptxDBGPort_RxBufferWriteIndex = 0;
                }

                /** Store received RX character */
                ptxDBGPort_RxBuffer[ptxDBGPort_RxBufferWriteIndex] = (uint8_t)p_args->data;

                /** Index reflects the number of received bytes. */
                ptxDBGPort_RxBufferWriteIndex ++;

                ptxDBGPort_WriteBusy = 0U;

#if 0
                if(dbgRxBuffIdx >= 1u)
                {
                    ptxDBGPORT_ParseHostRequest();
                    dbgRxBuffIdx = 0;
                    dbgport_ctx.RxState = DPGPORT_RxDone;
                }
#endif
                break;

            case UART_EVENT_RX_COMPLETE:
                ptxDBGPORT_ParseHostRequest();
                dbgport_ctx.RxState = DPGPORT_RxDone;
                break;

            case UART_EVENT_TX_COMPLETE:
                dbgport_ctx.TxState = DPGPORT_TxDone;
                break;

            case UART_EVENT_ERR_PARITY:
            case UART_EVENT_ERR_FRAMING:
            case UART_EVENT_BREAK_DETECT:
            case UART_EVENT_ERR_OVERFLOW:
#ifdef _RENESAS_SYNERGY_
            case UART_EVENT_ERR_RXBUF_OVERFLOW:
                dbgport_ctx.RxState = DPGPORT_RxError;
#endif
                break;
            case UART_EVENT_TX_DATA_EMPTY:
                /* What to do ?*/
                break;
            default:
                /** Transfer error. */
                break;
        }
    } else
    {
        /** Transfer error. */
    }
}



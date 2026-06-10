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
    Module      : PLAT
    File        : ptxPLAT_UART.c

    Description : basic UART implementation on Renesas RA4M2 platform.
         1) Pins: UART_TXD = P2_05, UART_RXD = P2_06, UART_FC_CTS = P4_08, UART_FC_RTS = P4_01 (Attention: Flow Control (FC) mandatory!)
         2) UART instance is automatically generated during build. UART Open assigns this instance to the pointer.
         3) Callback function has to be defined - it is used to receive the incoming bytes from the UART interface.
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT_UART.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * UART instance type wrapper.
 */
typedef uart_instance_t ptxPLAT_UARTInstance_t;

/**
 * Instance of available (predefined) UART port driver.
 */
const ptxPLAT_UARTPort_t available_uart_port =
{
    .UARTInstance = (ptxPLAT_UARTInstance_t *)&g_uart0,
};

/**
 * Max. number of idle time slices are used to define timeout period in Read function while loop.
 * - shortest time slice is 5us
 * - default timeout value is set-tuned to provide timeout of cca 500ms
 * - if a different clock setting is used, this pause might need readjustment
 */
#define RX_IDLE_TIME_SLICE                  (5UL)
#define RX_IDLE_TIME_TIMEOUT                (125000UL)
#define MAX_RX_IDLE_TIME_COUNT_(timeout)    (uint32_t)((timeout < RX_IDLE_TIME_SLICE) ? 1UL : (timeout / RX_IDLE_TIME_SLICE))
#define MAX_RX_IDLE_TIME_COUNT              MAX_RX_IDLE_TIME_COUNT_(RX_IDLE_TIME_TIMEOUT)

/**
 * SW delay in microseconds.
 */
#define PTX_PLAT_SW_DELAY_MICROS(timeout)    R_BSP_SoftwareDelay(timeout, BSP_DELAY_UNITS_MICROSECONDS);

/**
 * Instance of UART context.
 */
ptxPLAT_UART_t uart_ctx;

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static ptxStatus_t ptxPLAT_UART_Send(ptxPLAT_UART_t *uart, uint8_t *buff, size_t buffLen);
static ptxStatus_t ptxPLAT_UART_Read(ptxPLAT_UART_t *uart, uint8_t *buff, size_t buffLen);
static ptxStatus_t ptxPLAT_UART_CheckRx(ptxPLAT_UART_t *uart, uint8_t *rxState);
static ptxStatus_t ptxPLAT_UART_GetLastMessage(ptxPLAT_UART_t *uart, uint8_t *msg, size_t *len);
static void ptxPLAT_UART_ResetState(ptxPLAT_UART_t *uart);
static ptxStatus_t ptxPLAT_UART_CheckSupportedBitrate(uint32_t bitrate);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxPLAT_UART_GetInitialized(ptxPLAT_UART_t **uart, ptxPLAT_UARTConfigPars_t *uartPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != uart) && (NULL != uartPars))
    {
        memset(&uart_ctx, 0, sizeof(ptxPLAT_UART_t));

        status = ptxPLAT_UART_CheckSupportedBitrate(uartPars->IntfSpeed);

        /** Assign input parameters. */
        if (ptxStatus_Success == status)
        {
            uart_ctx.IntfSpeed = uartPars->IntfSpeed;
            uart_ctx.TxState = PTX_PLAT_UART_StateIDLE;
            uart_ctx.RxState = PTX_PLAT_UART_StateIDLE;
            uart_ctx.RxHandler = PTX_PLAT_UART_RxHandlerPLAT;
            uart_ctx.UARTPortUsed = (ptxPLAT_UARTPort_t *)&available_uart_port;

            ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart_ctx.UARTPortUsed->UARTInstance;

            /** Copy default configuration to modify some parameters. BaudRate */
            fsp_err_t err = R_SCI_UART_Open(uart_instance->p_ctrl, uart_instance->p_cfg);

            if (FSP_SUCCESS == err)
            {
                *uart = &uart_ctx;

            } else
            {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
            }

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_Deinit(ptxPLAT_UART_t *uart)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

        fsp_err_t err = R_SCI_UART_Close(uart_instance->p_ctrl);

        if (FSP_SUCCESS != err)
        {
            (void)memset(uart, 0, sizeof(ptxPLAT_UART_t));

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_Reset(ptxPLAT_UART_t *uart)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

        fsp_err_t err = R_SCI_UART_Close(uart_instance->p_ctrl);

        if (FSP_SUCCESS == err)
        {
            memset(&uart->RxBuf[0], 0, PTX_PLAT_RXBUF_SIZE);
            memset(&uart->RxCtrl, 0, sizeof(ptxPLAT_UARTRxCtrl_t));
            uart_ctx.TxState = PTX_PLAT_UART_StateIDLE;
            uart_ctx.RxState = PTX_PLAT_UART_StateIDLE;

            err = R_SCI_UART_Open(uart_instance->p_ctrl, uart_instance->p_cfg);

            if (FSP_SUCCESS == err)
            {
                status = ptxPLAT_UART_SetIntfSpeed(uart, uart->IntfSpeed);
            } else
            {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
            }
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}


ptxStatus_t ptxPLAT_UART_TRx(ptxPLAT_UART_t *uart, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers,
                                                        uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags)
{
    ptxStatus_t status = ptxStatus_Success;

    size_t i;

    /**
     *  Note: All combinations (Tx only, Rx only or Tx & Rx) are possible!
     */

    if (NULL != uart)
    {
        /** Rx has to be prepared before Tx operation since it might be too late doing this just before Rx. */
        ptxPLAT_UART_SetCleanStateRx(uart);

        /** Tx operation.*/
        if ((NULL != txBuf) && (NULL != txLen) && (numTxBuffers > 0))
        {
            /** Tx part of the overall transaction. */
            i = 0;
            while ((ptxStatus_Success == status) && (i < numTxBuffers))
            {
                if ((txBuf[i] != NULL) && (txLen[i]>0))
                {
                    size_t trx_len = txLen[i];
                    status = ptxPLAT_UART_Send (uart, txBuf[i], trx_len);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
                }

                i++;
            }
        }

        /** Rx operation.*/
        if ((NULL != rxBuf) && (NULL != rxLen) && (numRxBuffers > 0))
        {
            i = 0;
            while ((ptxStatus_Success == status) && (i < numRxBuffers))
            {
                if ((rxBuf[i] != NULL) && (rxLen[i] != NULL) && (*rxLen[i] > 0))
                {
                    status = ptxPLAT_UART_Read(uart, rxBuf[i], *rxLen[i]);
                }
                else
                {
                    status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
                }

                i++;
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    (void)flags;

    return status;
}

ptxStatus_t ptxPLAT_UART_StartWaitForRx(ptxPLAT_UART_t *uart, pptxPlat_RxCallBack_t irqCb, void *ctxIrqCb)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != uart)  && (NULL != irqCb) && (NULL != ctxIrqCb))
    {
        /* Rx operation handle at NSC level*/
        uart->RxHandler = PTX_PLAT_UART_RxHandlerNSC;

        /* Register the callback function and the context, for future use */
        uart->RxCb = irqCb;
        uart->CtxRxCb = ctxIrqCb;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_StopWaitForRx(ptxPLAT_UART_t *uart)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        /* Rx operation handle at PLAT level*/
        uart->RxHandler = PTX_PLAT_UART_RxHandlerPLAT;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_SetIntfSpeed(ptxPLAT_UART_t *uart, uint32_t bitrate)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        status = ptxPLAT_UART_CheckSupportedBitrate(bitrate);

        if (ptxStatus_Success == status)
        {
            baud_setting_t baud_setting = {0};
            R_SCI_UART_BaudCalculate(bitrate, false, 3000, &baud_setting);

            ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

            fsp_err_t err = R_SCI_UART_BaudSet(uart_instance->p_ctrl, &baud_setting);

            if (FSP_SUCCESS != err)
            {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_SetCleanStateRx(ptxPLAT_UART_t *uart)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

        R_SCI_UART_Abort(uart_instance->p_ctrl, UART_DIR_RX);

        ptxPLAT_UART_ResetState(uart);
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

uint8_t ptxPLAT_UART_CheckRxActive(ptxPLAT_UART_t *uart)
{
    uint8_t rx_active = 0;

    if (NULL != uart)
    {
        uint8_t rx_state = 0;
        ptxPLAT_UART_CheckRx (uart, &rx_state);

        if (PTX_PLAT_UART_StateIDLE != rx_state)
        {
            rx_active = 1u;
        }
    }

    return rx_active;
}

ptxStatus_t ptxPLAT_UART_TriggerRx(ptxPLAT_UART_t *uart)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != uart)
    {
        uint8_t rxState = 0;
        ptxPLAT_UART_CheckRx (uart, &rxState);

        switch (rxState)
        {
            case PTX_PLAT_UART_StateDONE:
                if (PTX_PLAT_UART_RxHandlerNSC == uart->RxHandler)
                {
                    if(NULL != uart->RxCb)
                    {
                        uart->RxCb(uart->CtxRxCb);
                    }
                }

                uint16_t next_msg_idx = (uint16_t)(uart->RxBuf[uart->RxCtrl.MsgIndex] + uart->RxCtrl.MsgIndex + 1);
                uart->RxCtrl.MsgIndex = (uint16_t)((next_msg_idx >= PTX_PLAT_RXBUF_SIZE) ? (next_msg_idx-PTX_PLAT_RXBUF_SIZE) : next_msg_idx);
                break;

            case PTX_PLAT_UART_StateERROR:
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
                break;

        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_UART_GetReceivedMessage (ptxPLAT_UART_t *uart, uint8_t *rxMessageBuffer, size_t *rxMessageBufferLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != uart) && (NULL != rxMessageBuffer) && (NULL != rxMessageBufferLen))
    {
        uint8_t *rx_buffer = uart->RxBuf;

        /** Prepare received message. */
        uint16_t msg_index = uart->RxCtrl.MsgIndex;
        uint16_t next_msg_idx = (uint16_t)(rx_buffer[msg_index] + msg_index + 1);
        uint8_t fifo_overflow = (uint8_t)((next_msg_idx >= PTX_PLAT_RXBUF_SIZE) ? 1u : 0);
        uint8_t len = rx_buffer[msg_index];

        if ((0 != fifo_overflow) && (0 != len))
        {
            /** In case of an overflow, bytes must be copied manually */
            uint16_t idx;

            for(uint8_t i=0; i<len; i++)
            {
                idx = (uint16_t)(msg_index+1+i);
                if(idx >= PTX_PLAT_RXBUF_SIZE)
                {
                    idx = (uint16_t)(idx - PTX_PLAT_RXBUF_SIZE);
                }

                rxMessageBuffer[i] = rx_buffer[idx];
            }

        } else
        {
            /** In case of no overflow, copy the received message at once */
            (void)memcpy(&rxMessageBuffer[0], &rx_buffer[msg_index+1], (size_t)len);
        }

        *rxMessageBufferLen = (size_t)len;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */

static ptxStatus_t ptxPLAT_UART_Send(ptxPLAT_UART_t *uart, uint8_t *buff, size_t buffLen)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != buff) && (buffLen > 0))
    {
        ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

        uart_ctx.TxState = PTX_PLAT_UART_StateONGOING;

        fsp_err_t err = R_SCI_UART_Write(uart_instance->p_ctrl, buff, (uint32_t)buffLen);

        if (FSP_SUCCESS == err)
        {
            /** Wait for on-going transfer to be finished. */
            while (PTX_PLAT_UART_StateONGOING == uart_ctx.TxState);

            if (PTX_PLAT_UART_StateDONE != uart_ctx.TxState)
            {
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
            }

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
        }

        uart_ctx.TxState = PTX_PLAT_UART_StateIDLE;

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxPLAT_UART_Read(ptxPLAT_UART_t *uart, uint8_t *buff, size_t buffLen)
{
    ptxStatus_t ret = ptxStatus_Success;

    if ((NULL != buff) && (buffLen > 0))
    {
        uint32_t idle_time_count = 0;
        uint32_t max_idle_time = MAX_RX_IDLE_TIME_COUNT;

        uint8_t rx_state = PTX_PLAT_UART_StateONGOING;

        while ((idle_time_count < max_idle_time) && (PTX_PLAT_UART_StateDONE != rx_state) && (PTX_PLAT_UART_StateERROR != rx_state))
        {
            PTX_PLAT_SW_DELAY_MICROS(RX_IDLE_TIME_SLICE);
            idle_time_count++;

            ptxPLAT_UART_CheckRx (uart, &rx_state);
        }

        if (PTX_PLAT_UART_StateDONE == rx_state)
        {
            size_t len = buffLen;
            ret = ptxPLAT_UART_GetLastMessage (uart, buff, &len);
        } else
        {
            ret = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
        }

    } else
    {
        ret = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return ret;
}

static ptxStatus_t ptxPLAT_UART_GetLastMessage(ptxPLAT_UART_t *uart, uint8_t *msg, size_t *len )
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t msg_index = uart->RxCtrl.MsgIndex;
    uint8_t *fifo_buff = uart->RxBuf;
    size_t expected_length = *len;

    size_t message_length = (size_t)(fifo_buff[msg_index] + 1);
    size_t actual_length = 0;

    if(expected_length != message_length)
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);

    } else
    {
        for(size_t i=0; (i<expected_length) && (i<message_length); i++)
        {
            if(msg_index >= PTX_PLAT_RXBUF_SIZE)
            {
                msg_index = 0;
            }

            msg[i] = fifo_buff[msg_index];
            msg_index++;
            actual_length++;
        }

        *len = actual_length;
        uart->RxCtrl.MsgIndex = msg_index;
    }

    return status;
}

static ptxStatus_t ptxPLAT_UART_CheckRx(ptxPLAT_UART_t *uart, uint8_t *rxState)
{
    ptxStatus_t status = ptxStatus_Success;

    uint16_t fifo_index = uart->RxCtrl.Idx;
    uint16_t msg_index = uart->RxCtrl.MsgIndex;
    uint8_t *fifo_buff = uart->RxBuf;
    uint8_t state = PTX_PLAT_UART_StateIDLE;

    if (msg_index != fifo_index)
    {
        uint16_t new_msg_index = (uint16_t)(fifo_buff[msg_index] + msg_index + 1);

        state = PTX_PLAT_UART_StateONGOING;

        if (0 == fifo_buff[msg_index])
        {
            state = PTX_PLAT_UART_StateDONE;
        } else if(new_msg_index >= PTX_PLAT_RXBUF_SIZE)
        {
            /** Back to the beginning of the FIFO. */
            new_msg_index = (uint16_t)(new_msg_index - PTX_PLAT_RXBUF_SIZE);

            if(fifo_index >= new_msg_index)
            {
                state = PTX_PLAT_UART_StateDONE;
            }
        } else if(fifo_index >= new_msg_index)
        {
            state = PTX_PLAT_UART_StateDONE;
        } else
        {
            //uart->RxState = PTX_PLAT_UART_TRXStateONGOING;
        }
    }

    *rxState = state;

    return status;
}

static void ptxPLAT_UART_ResetState(ptxPLAT_UART_t *uart)
{
    ptxPLAT_UARTInstance_t *uart_instance = (ptxPLAT_UARTInstance_t *)uart->UARTPortUsed->UARTInstance;

    uart_cfg_t *p_cfg = (uart_cfg_t *)uart_instance->p_cfg;

    /** Enter critical part: disable RXI interrupt. */
    __NVIC_DisableIRQ(p_cfg->rxi_irq);

    /** Set clean Rx state for the next reception. */
    memset(&uart->RxBuf[0], 0, PTX_PLAT_RXBUF_SIZE);
    uart->RxCtrl.Idx = 0;
    uart->RxCtrl.MsgIndex = 0;
    uart->RxState = PTX_PLAT_UART_StateIDLE;

    /** Exit critical section. */
    __NVIC_EnableIRQ(p_cfg->rxi_irq);
}

static ptxStatus_t ptxPLAT_UART_CheckSupportedBitrate(uint32_t bitrate)
{
    ptxStatus_t status = ptxStatus_Success;

    switch (bitrate)
    {
        case PTX_PLAT_HOST_SPEED_UART_9600:
        case PTX_PLAT_HOST_SPEED_UART_14400:
        case PTX_PLAT_HOST_SPEED_UART_19200:
        case PTX_PLAT_HOST_SPEED_UART_28800:
        case PTX_PLAT_HOST_SPEED_UART_38400:
        case PTX_PLAT_HOST_SPEED_UART_57600:
        case PTX_PLAT_HOST_SPEED_UART_115200:
        case PTX_PLAT_HOST_SPEED_UART_230400:
        case PTX_PLAT_HOST_SPEED_UART_460800:
        case PTX_PLAT_HOST_SPEED_UART_921600:
        case PTX_PLAT_HOST_SPEED_UART_1843200:
        case PTX_PLAT_HOST_SPEED_UART_3000000:
            /* nothing to do - everything OK */
            break;

        default:
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
            break;
    }

    return status;
}

/*
 * ####################################################################################################################
 * ISR
 * ####################################################################################################################
 */

void ptxPLAT_UART_Callback(uart_callback_args_t *p_args)
{
    if(NULL != p_args)
    {
        switch(p_args->event)
        {
            case UART_EVENT_RX_CHAR:
                /** Just fill the Rx buffer with data and check limits. */
                if(uart_ctx.RxCtrl.Idx >= PTX_PLAT_RXBUF_SIZE)
                {
                    uart_ctx.RxCtrl.Idx = 0;
                }

                /** Store received RX character if index is within limits. */
                uart_ctx.RxBuf[uart_ctx.RxCtrl.Idx] = (uint8_t)p_args->data;

                /** Index reflects the number of received bytes. */
                uart_ctx.RxCtrl.Idx ++;
                break;

            case UART_EVENT_RX_COMPLETE:
                /** This event is set when read finished. */
                uart_ctx.RxState = PTX_PLAT_UART_StateDONE;
                break;

            case UART_EVENT_TX_COMPLETE:
                uart_ctx.TxState = PTX_PLAT_UART_StateDONE;
                break;

            case UART_EVENT_ERR_PARITY:
            case UART_EVENT_ERR_FRAMING:
            case UART_EVENT_BREAK_DETECT:
            case UART_EVENT_ERR_OVERFLOW:
                uart_ctx.RxState = PTX_PLAT_UART_StateERROR;
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



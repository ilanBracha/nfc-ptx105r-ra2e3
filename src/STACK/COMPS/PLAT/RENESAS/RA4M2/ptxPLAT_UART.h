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
    File        : ptxPLAT_UART.h

    Description :
*/

#ifndef COMPS_PLAT_PTXPLAT_UART_H_
#define COMPS_PLAT_PTXPLAT_UART_H_


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include "ptxPLAT_EXT.h"
#include <stddef.h>
#include "ptxPLAT.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * UART port context.
 */
typedef struct ptxPLAT_UARTPort
{
    void const              *UARTInstance;   /**< Pointer to generated UART interface instance (platform dependent). */
}ptxPLAT_UARTPort_t;

/**
 * Platform-specific UART configuration parameters.
 */
typedef struct ptxPLAT_UARTConfigPars
{
    uint32_t                IntfSpeed;      /**< I2C Speed/Rate */
}ptxPLAT_UARTConfigPars_t;

/**
 * Platform-specific. Rx Handler Mode.
 */
typedef enum ptxPLAT_UARTRxHandlerMode
{
    PTX_PLAT_UART_RxHandlerPLAT,
    PTX_PLAT_UART_RxHandlerNSC
}ptxPLAT_UARTRxHandlerMode_t;

/**
 * Platform-specific. Tx State types.
 */
typedef enum ptxPLAT_UARTState
{
    PTX_PLAT_UART_StateIDLE,
    PTX_PLAT_UART_StateONGOING,
    PTX_PLAT_UART_StateDONE,
    PTX_PLAT_UART_StateERROR
}ptxPLAT_UARTState_t;

/**
 * Rx control parameters structure.
 */
typedef struct ptxPLAT_UARTRxCtrl
{
    uint16_t                Idx;            /**< Index of the first unused location in Rx buffer. */
    uint16_t                MsgIndex;       /**< Index of the next message to be processed in Standard mode. */
}ptxPLAT_UARTRxCtrl_t;

/**
 * Platform-specific. UART Main structure.
 */
typedef struct ptxPLAT_UART
{
    ptxPLAT_UARTPort_t              *UARTPortUsed;              /**< Renesas-specific UART Instance.*/
    uint32_t                        IntfSpeed;                  /**< UART baudrate in bps. Initial should be 115200. */
    pptxPlat_RxCallBack_t           RxCb;                       /**< Rx Callback function */
    void                            *CtxRxCb;                   /**< Rx Callback Context */
    uint8_t                         CancelFlag;                 /**< Flag used for cancellation of a blocking read operation */
    ptxPLAT_UARTRxHandlerMode_t     RxHandler;                  /**< Rx Handler PLAT (HW Mode) or NSC(System Mode) */
    volatile ptxPLAT_UARTState_t    TxState;                    /**< State of the transmission operation */
    volatile ptxPLAT_UARTState_t    RxState;                    /**< State of the reception operation */
    uint8_t                         RxBuf[PTX_PLAT_RXBUF_SIZE]; /**< Pointer to an already allocated Rx buffer. */
    ptxPLAT_UARTRxCtrl_t            RxCtrl;                     /**< Pointer to Rx control structure. */
}ptxPLAT_UART_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Get an initialized UART Context.
 *
 * \note This function shall be successfully executed before any other call to the functions in this module.
 *       It initializes UART hardware wise.
 *
 * \param[out]       uart              Pointer to pointer where the allocated and initialized UART context is going to be provided.
 * \param[in]        uartPars          Configuration for UART.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_GetInitialized(ptxPLAT_UART_t **uart, ptxPLAT_UARTConfigPars_t *uartPars);


/**
 * \brief De initialize the UART.
 *
 * \note This function shall be called once that the caller has finished with UART.
 *
 * \param[in]       uart             Pointer to an initialized UART context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_Deinit(ptxPLAT_UART_t *uart);


/**
 * \brief Reset UART interface.
 *
 * \note This function is used to perform Close/Open in order to have a clean peripheral state in case
 *       an erroneous transmission occurs. UART mode and baudrate stay the same.
 *
 * \param[in]       uart             Pointer to an initialized UART context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_Reset(ptxPLAT_UART_t *uart);


/**
 * \brief UART transmit and receive function.
 *
 * \note Wrapper function for /ref ptxPLAT_TRx. See it for detailed description
 *
 * \param[in]       uart            Pointer to an initialized UART context.
 * \param[in]       txBuf           Array of buffers to transmit.
 * \param[in]       txLen           Array of lengths of buffers to transmit.
 * \param[in]       numTxBuffers    Number of buffers to transmit.
 * \param[out]      rxBuf           Array of buffers to receive.
 * \param[in,out]   rxLen           Array of lengths of buffers to receive.
 * \param[in]       numTxBuffers    Number of buffers to receive.
 * \param[in]       flags           General purpose flags for TRx-operation (specific to used host-interface).
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_TRx(ptxPLAT_UART_t *uart, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags);


/**
 * \brief Start waiting for Rx Event to be received.(Not-blocking)
 *
 * This function registers a CallBack function to be called by the platform (e.g. typically from ISR),
 * when a frame has been sent by the PTX1K.
 *
 * \param[in]           uart        Pointer to an initialized UART context.
 * \param[in]           irqCb       Callback function to be called from HW (PLatform) when the IRQ is triggered by PTX1K.
 * \param[in]           ctxIrqCb    Context to be used as first argument when invoking the callback.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_StartWaitForRx(ptxPLAT_UART_t *uart, pptxPlat_RxCallBack_t irqCb, void *ctxIrqCb);


/**
 * \brief Stop the waiting for IRQ
 *
 * This function stops the asynchronous wait for Rx event by /ref ptxPLAT_UART_StartWaitForRx.
 *
 * \param[in]           uart         Pointer to an initialized UART context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_StopWaitForRx(ptxPLAT_UART_t *uart);


/**
 * \brief Set UART bitrate.
 *
 * This function is used to change already set bitrate. It is called after NSC_INIT_CMD/RESP to update UART settings.
 *
 * \param[in]           uart         Pointer to an initialized UART context.
 * \param[in]           bitrate      New bitrate for the UART interface.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_SetIntfSpeed(ptxPLAT_UART_t *uart, uint32_t bitrate);


/**
 * \brief Set Rx control parameters to initial values.
 *
 * This is typically called before starting Tx operation, so that asynchronous Rx operation after Tx could
 * start with expected state.
 *
 * \param[in]           uart         Pointer to an initialized UART context.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_SetCleanStateRx (ptxPLAT_UART_t *uart);


/**
 * \brief Check if UART reception is taking place at the moment.
 *
 * \param[in]           uart         Pointer to an initialized UART context.
 *
 * \return Rx status: 0 - no rx activity, 1 - rx activity ongoing.
 */
uint8_t ptxPLAT_UART_CheckRxActive(ptxPLAT_UART_t *uart);


/**
 * \brief Triggers Rx callback, if Rx transfer is done.
 *
 * \param[in]           uart         Pointer to an initialized UART context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_TriggerRx (ptxPLAT_UART_t *uart);

/**
 * \brief Stores received UART message in given destination buffer and set received message length.
 *
 * \note Used only for UART interface.
 *
 * \param[in]           uart                Pointer to an initialized UART context.
 * \param[in]           rxMessageBuffer     Pointer to buffer where received message shall be stored. Buffer size must be >= PTX_PLAT_RXBUF_SIZE.
 * \param[out]          rxMessageBufferLen  Pointer to variable where received message length shall be stored.
 *
 * \return Status, indicating whether the operation was successful.See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_UART_GetReceivedMessage (ptxPLAT_UART_t *uart, uint8_t *rxMessageBuffer, size_t *rxMessageBufferLen);

#ifdef __cplusplus
}
#endif

#endif /* Guard */


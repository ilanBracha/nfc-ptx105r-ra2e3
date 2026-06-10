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
    File        : ptxPLAT_I2C.h

    Description :
*/

#ifndef COMPS_PLAT_PTXPLAT_I2C_H_
#define COMPS_PLAT_PTXPLAT_I2C_H_


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxStatus.h"
#include <stddef.h>
#include "ptxPLAT.h"
#include "ptxPLAT_GPIO.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * I2C port context.
 */
typedef struct ptxPLAT_I2CPort
{
    void const              *I2CInstance;   /**< Pointer to generated I2C interface instance (platform dependent). */
}ptxPLAT_I2CPort_t;

/**
 * Platform-specific I2C configuration parameters.
 */
typedef struct ptxPLAT_I2CConfigPars
{
    uint8_t                 DeviceAddress;  /**< I2C Device Address */
    uint32_t                IntfSpeed;      /**< I2C Speed/Rate */
}ptxPLAT_I2CConfigPars_t;

/**
 * Platform-specific I2C main structure.
 */
typedef struct ptxPLAT_I2C
{
    uint8_t                 DeviceAddress;  /**< I2C Device Address */
    uint32_t                IntfSpeed;      /**< I2C Speed/Rate */
    ptxPLAT_I2CPort_t       *I2CPortUsed;   /**< I2C port used. */
    ptxPlatGpio_t           *Gpio;          /**< Pointer to GPIO-IRQ Context. Platform dependent. */
    pptxPlat_RxCallBack_t   RxCb;           /**< Rx Callback function. */
    void                    *CtxRxCb;       /**< Rx Callback Context. */
    volatile uint8_t        TransferState;  /**< Current state of the ongoing transfer. */

}ptxPLAT_I2C_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */


/**
 * \brief Get an initialized I2C Context.
 *
 * \note This function shall be successfully executed before any other call to the functions in this module.
 *       It initializes I2C hardware wise.
 *
 * \param[out]       i2c              Pointer to pointer where the allocated and initialized I2C context is going to be provided.
 * \param[in]        i2CPars          Configuration for I2C.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_I2C_GetInitialized(ptxPLAT_I2C_t **i2c, ptxPLAT_I2CConfigPars_t *i2CPars);


/**
 * \brief De-initialize the I2C.
 *
 * \note This function shall be called once that the caller has finished with I2C.
 *
 * \param[in]       i2c             Pointer to an initialized I2C context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_I2C_Deinit(ptxPLAT_I2C_t *i2c);


/**
 * \brief I2C transmit and receive function.
 *
 * \note Wrapper function for /ref ptxPLAT_TRx. See it for detailed description
 *
 * \param[in]       i2c             Pointer to an initialized I2C context.
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
ptxStatus_t ptxPLAT_I2C_TRx(ptxPLAT_I2C_t *i2c, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags);


/**
 * \brief Start waiting for IRQ to be triggered.(Not-blocking)
 *
 * This function registers a CallBack function to be called by the platform (e.g. typically from ISR) when the IRQ
 * has been triggered by the PTX1K.
 *
 * \param[in]       i2c         Pointer to an initialized I2C context.
 * \param[in]       irqCb       Callback function to be called from HW (PLatform) when the IRQ is triggered by PTX1K.
 * \param[in]       ctxIrqCb    Context to be used as first argument when invoking the callback.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_I2C_StartWaitForRx(ptxPLAT_I2C_t *i2c, pptxPlat_RxCallBack_t irqCb, void *ctxIrqCb);


/**
 * \brief Stop the waiting for IRQ
 *
 * This function stops the asynchronous wait for IRQ triggered by /ref ptxPLAT_StartWaitForIRQ.
 *
 * \param[in]       i2c         Pointer to an initialized I2C context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_I2C_StopWaitForRx(ptxPLAT_I2C_t *i2c);


#ifdef __cplusplus
}
#endif

#endif /* Guard */


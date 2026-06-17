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
    File        : ptxPLAT_SPI.h

    Description :
*/

#ifndef COMPS_PLAT_PTXPLAT_SPI_H_
#define COMPS_PLAT_PTXPLAT_SPI_H_


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include <stddef.h>
#include <STACK/COMPS/ptxStatus.h>
#include <STACK/COMPS/PLAT/ptxPLAT.h>
#include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_GPIO.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
#define PTX_HAL_SPI_DUMMY       (0x55u)         /**< Dummy value used for SPI. */

/* Forward declaration to break circular include with ptxPLAT_GPIO.h
 * (ptxPLAT_GPIO.h -> ptxPLAT_EXT.h -> hal_data.h -> rm_nfc_reader_ptx.h -> ptxPLAT_SPI.h)
 */
struct ptxPlatGpio;

typedef struct ptxPLAT_SpiPin
{
    const ioport_instance_t *PortInstance;
    uint16_t                PinNumber;
}ptxPLAT_SpiPin_t;


/**
 * SPI port context.
 */
typedef struct ptxPLAT_SpiPort
{
    void const              *SpiInstance;       /**< Pointer to generated SPI interface instance (platform dependent). */
    ptxPLAT_SpiPin_t        Nss;                /**< GPIO pin used for SPI CS functionality. */
}ptxPLAT_SpiPort_t;

/**
 * Platform-specific SPI configuration parameters.
 */
typedef struct ptxPLAT_SpiConfigPars
{
    uint32_t                IntfSpeed;          /**< Clock speed in Hz to be configured. */
}ptxPLAT_SpiConfigPars_t;

/**
 * Platform-specific. SPI Main structure.
 */
typedef struct ptxPLAT_Spi
{
    uint32_t                IntfSpeed;      /**< Clock speed in Hz to be configured. */
    ptxPLAT_SpiPort_t       *SpiPortUsed;   /**< SPI port used. */
    struct ptxPlatGpio      *Gpio;          /**< Pointer to GPIO-IRQ Context. Platform dependent. */
    pptxPlat_RxCallBack_t   RxCb;           /**< Rx Callback function. */
    void                    *CtxRxCb;       /**< Rx Callback Context. */
    volatile uint8_t        TransferState;  /**< Current state of the ongoing transfer. */
}ptxPLAT_Spi_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */


/**
 * \brief Get an initialized SPI Context.
 *
 * \note This function shall be successfully executed before any other call to the functions in this module.
 *       It initializes SPI hardware wise.
 *
 * \param[out]       spi              Pointer to pointer where the allocated and initialized SPI context is going to be provided.
 * \param[in]        spiPars          Configuration for SPI.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_SPI_GetInitialized(ptxPLAT_Spi_t **spi, ptxPLAT_SpiConfigPars_t *spiPars);


/**
 * \brief De initialize the SPI.
 *
 * \note This function shall be called once that the caller has finished with SPI.
 *
 * \param[in]       spi             Pointer to an initialized SPI context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_SPI_Deinit(ptxPLAT_Spi_t *spi);


/**
 * \brief SPI transmit and receive function.
 *
 * \note Wrapper function for /ref ptxPLAT_TRx. See it for detailed description
 *
 * \param[in]       spi             Pointer to an initialized SPI context.
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
ptxStatus_t ptxPLAT_SPI_TRx(ptxPLAT_Spi_t *spi, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags);


/**
 * \brief Start waiting for IRQ to be triggered.(Not-blocking)
 *
 * This function registers a CallBack function to be called by the platform (e.g. typically from ISR) when the IRQ
 * has been triggered by the PTX1K.
 *
 * \param[in]           spi         Pointer to an initialized SPI context.
 * \param[in]           irqCb       Callback function to be called from HW (PLatform) when the IRQ is triggered by PTX1K.
 * \param[in]           ctxIrqCb    Context to be used as first argument when invoking the callback.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_SPI_StartWaitForRx(ptxPLAT_Spi_t *spi, pptxPlat_RxCallBack_t irqCb, void *ctxIrqCb);


/**
 * \brief Stop the waiting for IRQ
 *
 * This function stops the asynchronous wait for IRQ triggered by /ref ptxPLAT_StartWaitForIRQ.
 *
 * \param[in]           spi         Pointer to an initialized SPI context.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxPLAT_SPI_StopWaitForRx(ptxPLAT_Spi_t *spi);

#ifdef __cplusplus
}
#endif

#endif /* Guard */


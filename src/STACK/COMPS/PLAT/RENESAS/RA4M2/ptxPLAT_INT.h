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
    File        : ptxPLAT_INT.h

    Description :
*/

/**
 * \addtogroup grp_ptx_api_plat_int PTX NSC Platform API Internal
 *
 * @{
 */

#ifndef COMPS_PLAT_PTXPLAT_INT_H_
#define COMPS_PLAT_PTXPLAT_INT_H_

#include <STACK/COMPS/ptxStatus.h>
#include <STACK/COMPS/PLAT/ptxPLAT.h>
#include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_GPIO.h>
#include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_TIMER.h>

#if defined (PTX_INTF_UART)
    #include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_UART.h>
#elif defined(PTX_INTF_SPI)
    #include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_SPI.h>
#elif defined(PTX_INTF_I2C)
    #include <STACK/COMPS/PLAT/RENESAS/RA4M2/ptxPLAT_I2C.h>
#else
    #error Error - Missing or unsupported Host-Interface implementation used
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Forward declarations.
 */
struct ptxPlatGpio;

/**
 * Platform-specific configuration parameters.
 */
typedef struct ptxPlat
{
        ptxStatus_Comps_t           CompId;             /**< Component Id. */

#if defined (PTX_INTF_UART)
        ptxPLAT_UART_t              *Uart;              /**< Pointer to UART Context. Platform dependent. */
#elif defined(PTX_INTF_SPI)
        ptxPLAT_Spi_t               *Spi;               /**< Pointer to SPI Context. Platform dependent. */
#elif defined(PTX_INTF_I2C)
        ptxPLAT_I2C_t               *I2c;               /**< Pointer to I2C Context. Platform dependent. */
#else
    #error Error - Missing or unsupported Host-Interface implementation used
#endif

        struct ptxPlatGpio          *SENPin;            /**< Connection to SEN-pin (optional). */

        pptxPlat_RxCallBack_t       RxCb;               /**< Callback function */
        void                        *CtxRxCb;           /**< Callback Context */

        uint16_t                    IRQDisabledCnt;     /**< IRQ Enable/Disable Protection */
}ptxPlat_t;

#ifdef __cplusplus
}
#endif

#endif /* Guard */

/** @} */


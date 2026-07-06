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
    Module      : Transparent Data Channel (TDC) Timer Support API
    File        : ptxTDC_Timer.h

    Description : Platform dependent Timer API used by TDC Component.
                  Implements Stop-Watch functionality. 
*/

/**
 * \addtogroup grp_ptx_api_tdc_timer Transparent Data Channel (TDC) Timer API
 *
 * @{
 */

#ifndef APIS_PTX_TDC_TIMER_H_
#define APIS_PTX_TDC_TIMER_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include "ptxStatus.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief TDC Timer: Component Structure.
 */
typedef struct ptxTDC_Timer_Status
{
    uint8_t     IsElapsed;      /**< Status Info whether Timer elapsed or is still running. */
    uint32_t    ElapsedTime;    /**< Elapsed Time given in ms. */

} ptxTDC_Timer_Status_t;

/**
 * \brief Initializes and starts the TDC timer for a given duration.
 *
 * \param[in]   ms  Milliseconds until the timer elapses.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Timer_Start(uint32_t ms);

/**
 * \brief Retrieves status information from the timer.
 *
 * \param[in,out]   timerStatus Pointer to \ref ptxTDC_Timer_Status_t.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Timer_Status(ptxTDC_Timer_Status_t *timerStatus);

/**
 * \brief Stops and deinitializes the TDC timer.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Timer_Stop();

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */


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
    File        : ptxTDC_Timer.c

    Description : Platform dependent Timer API used by TDC Component.
                  Implements Stop-Watch functionality. 
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxTDC_Timer.h"
#include "ptxPERIPH_APPTIMER.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxTDC_Timer_Start(uint32_t ms)
{
    ptxStatus_t status;

    status = ptxPERIPH_APPTIMER_Start(ms);
    
    return status;
}

ptxStatus_t ptxTDC_Timer_Status(ptxTDC_Timer_Status_t *timerStatus)
{
    ptxStatus_t status = ptxStatus_Success;
    
    if (NULL != timerStatus)
    {
        TimerStatus_t app_timer_status = {0};

        status = ptxPERIPH_APPTIMER_Status(&app_timer_status);

        /* Map Elapsed-Status */
        if (ptxStatus_Success == status)
        {
            timerStatus->IsElapsed = app_timer_status.IsElapsed;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }
    
    return status;
}

ptxStatus_t ptxTDC_Timer_Stop()
{
    ptxStatus_t status;

    status = ptxPERIPH_APPTIMER_Stop();

    return status;
}


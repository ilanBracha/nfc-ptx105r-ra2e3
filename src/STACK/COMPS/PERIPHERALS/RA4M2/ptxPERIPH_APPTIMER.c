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

    Project     : PTX130
    Module      : PERIPHERALS
    File        : ptxPERIPH_APPTIMER.c

    Description :

 */
#include "hal_data.h"
#include "ptxPERIPH_APPTIMER.h"

static const timer_instance_t *timer = &g_timer1;

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
void ptxPERIPH_APPTIMER_IsrCallback(timer_callback_args_t *p_args);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
ptxStatus_t ptxPERIPH_APPTIMER_Start(uint32_t ms)
{
    ptxStatus_t status = ptxStatus_Success;

    fsp_err_t r_status = R_GPT_Open(timer->p_ctrl, timer->p_cfg);

    if (FSP_SUCCESS == r_status)
    {
        r_status = R_GPT_CallbackSet(timer->p_ctrl, ptxPERIPH_APPTIMER_IsrCallback, NULL, NULL);
    }

    if(FSP_SUCCESS == r_status)
    {
        uint32_t timer_freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKD) >>  timer->p_cfg->source_div;
        uint32_t period_counts = (uint32_t) (((uint64_t) timer_freq_hz * ms) / 1000);
        r_status = R_GPT_PeriodSet(timer->p_ctrl, period_counts);

        if (FSP_SUCCESS == r_status)
        {
            r_status = R_GPT_Start(timer->p_ctrl);
        }
    }

    if(FSP_SUCCESS != r_status)
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
    }

    return status;
}

ptxStatus_t ptxPERIPH_APPTIMER_Status(TimerStatus_t *timerStatus)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != timerStatus)
    {
        timer_status_t timer_status;

        fsp_err_t r_status = R_GPT_StatusGet(timer->p_ctrl, &timer_status);
        if (FSP_SUCCESS == r_status)
        {
            if(timer_status.state == TIMER_STATE_STOPPED)
            {
                timerStatus->IsElapsed = 1u;
                timerStatus->ElapsedTime = 0u;
            }
            else
            {
                timerStatus->IsElapsed = 0u;
                uint32_t timer_freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKD) >> timer->p_cfg->source_div;
                timerStatus->ElapsedTime = (uint32_t) (((uint64_t) timer_status.counter * 1000u) / timer_freq_hz);
            }
        }
        else
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPERIPH_APPTIMER_Stop()
{
    ptxStatus_t status = ptxStatus_Success;

    fsp_err_t r_status = R_GPT_Stop(timer->p_ctrl);
    if(FSP_SUCCESS == r_status)
    {
        r_status = R_GPT_Close(timer->p_ctrl);
    }

    if(FSP_SUCCESS != r_status)
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
    }

    return status;
}

void ptxPERIPH_APPTIMER_IsrCallback(timer_callback_args_t *p_args)
{
    R_GPT_Stop(timer->p_ctrl);
    (void)p_args;
}


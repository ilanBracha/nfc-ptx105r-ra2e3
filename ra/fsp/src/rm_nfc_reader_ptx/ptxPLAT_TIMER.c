/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxPLAT_TIMER.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

#define PTX_PLAT_TIMER_DIVIDER    (1000)

extern void ptxPLAT_TIMER_IsrCallback(timer_callback_args_t * p_args);

/**
 * Timer Context.
 */
ptxPlatTimer_t timer_ctx;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxPLAT_TIMER_Open (timer_instance_t * timer_instance)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != timer_instance)
    {
        /**
         * Initialization of the TIMER Context
         */
        memset(&timer_ctx, 0, sizeof(ptxPlatTimer_t));

        timer_ctx.TimerInstance = timer_instance;
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_TIMER_GetInitializedTimer (ptxPlatTimer_t ** timer)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != timer)
    {
        timer_instance_t * timer_instance = timer_ctx.TimerInstance;

        fsp_err_t r_status = timer_instance->p_api->open(timer_instance->p_ctrl, timer_instance->p_cfg);

        if (FSP_SUCCESS == r_status)
        {
            *timer = &timer_ctx;
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_TIMER_Start (ptxPlatTimer_t         * timer,
                                 uint32_t                 ms,
                                 uint8_t                  isBlock,
                                 pptxPlat_TimerCallBack_t fnISRCb,
                                 void                   * ISRCxt)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != timer) && (ms > 0) && ((0 == isBlock) || (1U == isBlock)))
    {
        /** Clear IsElpased state */
        timer->IsElapsed = 0;

        /** Stop Timer, first. Then, set period. Finally, start timer counter. */
        timer_instance_t * timer_instance = (timer_instance_t *) timer->TimerInstance;
        fsp_err_t          r_status       = timer_instance->p_api->stop(timer_instance->p_ctrl);

        if (FSP_SUCCESS == r_status)
        {
            /*
             * The AGT timer is only 16-bit.  At PCLKB = 24 MHz the counter
             * overflows after ~2.7 ms, so any timeout > 2 ms would silently
             * wrap and expire almost instantly.
             *
             * Fix: set the hardware period to exactly 1 ms and use the
             * software down-counter RemainingMs (decremented in the ISR)
             * to support arbitrarily long timeouts.
             */
            uint32_t timer_freq_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_PCLKB)
                                     >> timer_instance->p_cfg->source_div;
            uint32_t one_ms_counts = timer_freq_hz / PTX_PLAT_TIMER_DIVIDER;
            timer->RemainingMs = ms;
            r_status = timer_instance->p_api->periodSet(timer_instance->p_ctrl, one_ms_counts);
        }

        if (FSP_SUCCESS == r_status)
        {
            timer->ISRCallBack = fnISRCb;
            timer->ISRCxt      = ISRCxt;

            r_status = timer_instance->p_api->start(timer_instance->p_ctrl);

            if (FSP_SUCCESS == r_status)
            {
                timer->TimerState = Timer_InUse;
                if (1U == isBlock)
                {
                    /**
                     * WFI until Timer is elapsed. Suitable for Pause() functionality.
                     *
                     * Note: Usage of DSB and ISB here should ensure that MCU-internal memory and instruction operations should
                     *       be completed before and after WFI() to ensure checking the "IsElapsed"-flag is done "in order".
                     *       If the timer-ISR is called exactly between checking the flag and the WFI-instruction, the system might
                     *       stay in sleep-state. This case is prevented here, by letting the timer periodically trigger (kind of watchdog, to be ),
                     *       stopped at the calling level), but it is advised on certain system / MCUs to use - for example - a critical section
                     *       before the check and after exiting the WFI-state.
                     */
                    while (0 == timer->IsElapsed)
                    {
                        __DSB();
                        __WFI();
                        __ISB();
                        __NOP();
                    }
                }
            }
        }

        if (FSP_SUCCESS != r_status)
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

ptxStatus_t ptxPLAT_TIMER_IsElapsed (ptxPlatTimer_t * timer, uint8_t * isElapsed)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != timer) && (NULL != isElapsed))
    {
        *isElapsed = timer->IsElapsed;

        if (0 != *isElapsed)
        {
            ptxPLAT_TIMER_Stop(timer);
        }
    }
    else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_TIMER_Stop (ptxPlatTimer_t * timer)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != timer)
    {
        timer_instance_t * timer_instance = (timer_instance_t *) timer->TimerInstance;
        fsp_err_t          r_status       = timer_instance->p_api->stop(timer_instance->p_ctrl);
        if (FSP_SUCCESS == r_status)
        {
            timer->IsElapsed  = 0;
            timer->TimerState = Timer_Free;
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

ptxStatus_t ptxPLAT_TIMER_Deinit (ptxPlatTimer_t * timer)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != timer)
    {
        /** Timer driver API close stops the timer, disables interrupts and performs other cleanup. */
        timer_instance_t * timer_instance = (timer_instance_t *) timer->TimerInstance;
        fsp_err_t          r_status       = timer_instance->p_api->close(timer_instance->p_ctrl);
        if (FSP_SUCCESS == r_status)
        {
            memset(&timer_ctx, 0, sizeof(ptxPlatTimer_t));
            timer_ctx.TimerInstance = timer_instance;
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

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
void ptxPLAT_TIMER_IsrCallback (timer_callback_args_t * p_args)
{
    (void) p_args;

    /*
     * Each ISR tick represents 1 ms (hardware period set in ptxPLAT_TIMER_Start).
     * Decrement the software down-counter and only mark the timer as elapsed
     * when it reaches zero.  This allows the 16-bit AGT to support timeouts
     * of up to ~4 billion ms.
     */
    if (timer_ctx.RemainingMs > 0)
    {
        timer_ctx.RemainingMs--;
    }

    if (0 == timer_ctx.RemainingMs)
    {
        timer_ctx.IsElapsed = 1U;

        /* Call back if defined. */
        if (NULL != timer_ctx.ISRCallBack)
        {
            timer_instance_t * timer_instance = (timer_instance_t *) timer_ctx.TimerInstance;
            timer_instance->p_api->stop(timer_instance->p_ctrl);

            if (NULL != timer_ctx.ISRCxt)
            {
                timer_ctx.ISRCallBack(timer_ctx.ISRCxt);
            }
        }
    }
}

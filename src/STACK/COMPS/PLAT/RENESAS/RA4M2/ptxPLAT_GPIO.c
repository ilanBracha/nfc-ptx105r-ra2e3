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
    File        : ptxPLAT_GPIO.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxPLAT_GPIO.h"
#include "ptxPLAT_EXT.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */


#ifdef POLLER_BOARD                             // These #defines are used for the PTX1xxW WLC-Poller-Board
    #define PTX_EXT_IRQ_PIN_POLLER_BOARD        BSP_IO_PORT_00_PIN_02
    #define PTX_GPIO_SEN_PIN                    BSP_IO_PORT_00_PIN_14
#else                                           // These #defines are used for the PTX1xxR Reader-Board
    /* PTX1K IRQ on RA8M1 EK is routed to PMOD1 IRQ = P0_06 (ICU IRQ11). */
    #define PTX_EXT_IRQ_PIN                     BSP_IO_PORT_00_PIN_06
#endif

/*
 * Instance of GPIO-IRQ context.
 */
ptxPlatGpio_t gpio_irq_ctx;

#ifdef SEN_PIN_CONNECTION_AVAILABLE
    /*
     * Instance of PTX100x SEN-pin context.
     */
    ptxPlatGpio_t sen_pin_context;
#endif

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
    ptxStatus_t ptxPLAT_GPIO_GetInitialized(ptxPlatGpio_t **gpio, ptxPlatGpio_Config_t *gpioPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != gpio) && (NULL != gpioPars))
    {
        fsp_err_t r_status = FSP_SUCCESS;

        switch (gpioPars->Type)
        {
            case GPIO_Type_PTXExtIRQPin:
                /* Initialization of the GPIO Context */
                memset(&gpio_irq_ctx, 0, sizeof(ptxPlatGpio_t));

                /* GPIO used for IRQ purpose is the following one. */
                gpio_irq_ctx.PortInstance = &g_ioport;
                gpio_irq_ctx.ExtIrqInstance = (external_irq_instance_t*)&g_ext_irq;
#ifdef POLLER_BOARD
                gpio_irq_ctx.PortPin = PTX_EXT_IRQ_PIN_POLLER_BOARD;
#else
                gpio_irq_ctx.PortPin = PTX_EXT_IRQ_PIN;
#endif
                r_status = R_ICU_ExternalIrqOpen(gpio_irq_ctx.ExtIrqInstance->p_ctrl, gpio_irq_ctx.ExtIrqInstance->p_cfg);

                if (FSP_SUCCESS == r_status)
                {
                    r_status = R_ICU_ExternalIrqEnable(gpio_irq_ctx.ExtIrqInstance->p_ctrl);
                }

                if (FSP_SUCCESS == r_status)
                {
                    *gpio = &gpio_irq_ctx;
                } else
                {
                    status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
                }
                break;

            case GPIO_Type_SENOutPin:
#ifdef SEN_PIN_CONNECTION_AVAILABLE
                /* Initialization of the SEN-pin Context */
                memset(&sen_pin_context, 0, sizeof(ptxPlatGpio_t));

                sen_pin_context.PortInstance = &g_ioport;
                sen_pin_context.PortPin = PTX_GPIO_SEN_PIN;
                *gpio = &sen_pin_context;
#else
                *gpio = NULL;
#endif
                break;

            default:
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
                break;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}


ptxStatus_t ptxPLAT_GPIO_Deinit(ptxPlatGpio_t *gpio)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        /* Deinit GPIO. */
        if (NULL != gpio->ExtIrqInstance)
        {
            R_ICU_ExternalIrqDisable(g_ext_irq.p_ctrl);
            R_ICU_ExternalIrqClose(g_ext_irq.p_ctrl);
        }

        /* Clear Platform Context. */
        (void)memset(gpio, 0, sizeof(ptxPlatGpio_t));

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxPLAT_GPIO_EnableInterrupt (ptxPlatGpio_t *gpio)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        if (NULL != gpio->ExtIrqInstance)
        {
            fsp_err_t r_status = FSP_SUCCESS;

            r_status = R_ICU_ExternalIrqEnable(gpio_irq_ctx.ExtIrqInstance->p_ctrl);

            if (FSP_SUCCESS != r_status)
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

ptxStatus_t ptxPLAT_GPIO_DisableInterrupt (ptxPlatGpio_t *gpio)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        if (NULL != gpio->ExtIrqInstance)
        {
            fsp_err_t r_status = FSP_SUCCESS;
            r_status = R_ICU_ExternalIrqDisable(gpio_irq_ctx.ExtIrqInstance->p_ctrl);

            if (FSP_SUCCESS != r_status)
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

ptxStatus_t ptxPLAT_GPIO_GetIntEnableStatus (ptxPlatGpio_t *gpio, uint32_t *state)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        if (NULL != gpio->ExtIrqInstance)
        {
            icu_instance_ctrl_t * p_ctrl = (icu_instance_ctrl_t *) gpio_irq_ctx.ExtIrqInstance->p_ctrl;

            uint32_t value = NVIC_GetEnableIRQ(p_ctrl->irq);
            if((1u == value) || (0 == value))
            {
                *state = value;
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

ptxStatus_t ptxPLAT_GPIO_StartWaitForTrigger(ptxPlatGpio_t *gpio, pptxPlat_IRQCallBack_t irqCallBack, void *irqCallBackCtx)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        gpio->IrqCallBack = irqCallBack;
        gpio->IrqCallBackCtx = irqCallBackCtx;
        status = ptxPLAT_GPIO_EnableInterrupt (gpio);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxPLAT_GPIO_StopWaitForTrigger(ptxPlatGpio_t *gpio)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {

        status = ptxPLAT_GPIO_DisableInterrupt (gpio);
        gpio->IrqCallBack = NULL;
        gpio->IrqCallBackCtx = NULL;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxPLAT_GPIO_ReadLevel(ptxPlatGpio_t *gpio, uint8_t *value)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != gpio) && (NULL != value))
    {
        fsp_err_t r_status = FSP_SUCCESS;
        bsp_io_level_t p_pin_value = BSP_IO_LEVEL_LOW;

        r_status = R_IOPORT_PinRead(gpio->PortInstance->p_ctrl, gpio->PortPin, &p_pin_value);

        if (FSP_SUCCESS == r_status)
        {
            *value = (uint8_t)p_pin_value;
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

ptxStatus_t ptxPLAT_GPIO_TriggerRx(ptxPlatGpio_t *gpio)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != gpio)
    {
        uint8_t value;
        status =  ptxPLAT_GPIO_ReadLevel(gpio, &value);

        if ((ptxStatus_Success == status) && (1u == value))
        {
            if ((gpio_irq_ctx.IrqCallBack != NULL) && (gpio_irq_ctx.IrqCallBackCtx != NULL))
            {
                (void)gpio_irq_ctx.IrqCallBack(gpio_irq_ctx.IrqCallBackCtx);
            }
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxPLAT_GPIO_WriteLevel(ptxPlatGpio_t *gpio, uint8_t value)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != gpio))
    {
        if (NULL == gpio->ExtIrqInstance)
        {
            fsp_err_t r_status;
            bsp_io_level_t p_pin_value;

            p_pin_value = (0 == value) ? BSP_IO_LEVEL_LOW : BSP_IO_LEVEL_HIGH;

            r_status = R_IOPORT_PinWrite(gpio->PortInstance->p_ctrl, gpio->PortPin, p_pin_value);

            if (FSP_SUCCESS != r_status)
            {
                status = PTX_STATUS(ptxStatus_Comp_PERIPH, ptxStatus_InternalError);
            }
        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_PERIPH, ptxStatus_InvalidParameter);
        }
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

/*
 * ####################################################################################################################
 * INTERRUPT HANDLERS
 * ####################################################################################################################
 */
void ptxPLAT_GPIO_IsrCallback(external_irq_callback_args_t *p_args)
{
    (void)p_args;
}


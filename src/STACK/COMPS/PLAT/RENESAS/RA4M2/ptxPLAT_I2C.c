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
    File        : ptxPLAT_I2C.c

    Description : basic I2C implementation on Renesas RA4M2 platform.
         1) Pins: I2C_SCL = P4_08, I2C_SDA = P4_09
         2) I2C instance is automatically generated during build. I2C Open assigns this instance to the pointer.
         3) Callback function has to be defined - it is used to report the actual status of the transfer.

 */

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxPLAT.h"
#include "ptxPLAT_I2C.h"
#include "ptxPLAT_EXT.h"
#include <string.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * I2C instance type wrapper.
 */
typedef i2c_master_instance_t ptxPLAT_I2CInstance_t;

/*
 * ####################################################################################################################
 * DEFINES
 * ####################################################################################################################
 */

#define PTX_PLAT_I2C_CHANNEL                (3u)

#define PTX_PLAT_TRX_STATE_IDLE             (0)
#define PTX_PLAT_TRX_STATE_ERROR            (1)
#define PTX_PLAT_TRX_STATE_TX_COMPLETE      (2)
#define PTX_PLAT_TRX_STATE_RX_COMPLETE      (3)

/*
 * ####################################################################################################################
 * STATIC VARIABLES
 * ####################################################################################################################
 */

/**
 * Instance of available (predefined) I2C port driver.
 */
const ptxPLAT_I2CPort_t available_i2C_port =
{
    .I2CInstance = (ptxPLAT_I2CInstance_t *)&g_pmod1_I2C,
};

/**
 * Instance of I2C context.
 */
ptxPLAT_I2C_t i2c_ctx;


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static ptxStatus_t ptxPLAT_I2C_PortOpen(ptxPLAT_I2CPort_t **i2cPort, ptxPLAT_I2CConfigPars_t *i2cPars);
static ptxStatus_t ptxPLAT_I2C_PortClose(ptxPLAT_I2CPort_t *i2cPort);
static ptxStatus_t ptxPLAT_I2C_GetMappedBitrate(uint32_t targetRate, i2c_master_rate_t *mappedRate);
static ptxStatus_t ptxPLAT_I2C_TRx_Helper(ptxPLAT_I2C_t *i2c, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags);
static ptxStatus_t ptxPLAT_I2C_TxAux(ptxPLAT_I2C_t *i2c, uint8_t *txBuff, size_t *len, uint8_t setRestartCondition);
static ptxStatus_t ptxPLAT_I2C_RxAux(ptxPLAT_I2C_t *i2c, uint8_t *rxBuff, size_t *len);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxPLAT_I2C_GetInitialized(ptxPLAT_I2C_t **i2c, ptxPLAT_I2CConfigPars_t *i2CPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != i2c) && (NULL != i2CPars))
    {
        /**
         * Initialization of the I2C Context
         */
        ptxPLAT_I2CPort_t *i2c_port = NULL;
        memset(&i2c_port, 0, sizeof(ptxPLAT_I2CPort_t));

        memset(&i2c_ctx, 0, sizeof(ptxPLAT_I2C_t));

        status = ptxPLAT_I2C_PortOpen(&i2c_port, i2CPars);

        if(ptxStatus_Success == status)
        {
            ptxPlatGpio_Config_t gpio_pars;
            gpio_pars.Type = GPIO_Type_PTXExtIRQPin;

            status = ptxPLAT_GPIO_GetInitialized(&i2c_ctx.Gpio, &gpio_pars);

            if((ptxStatus_Success == status) && (NULL != i2c_port))
            {
                i2c_ctx.IntfSpeed = i2CPars->IntfSpeed;
                i2c_ctx.DeviceAddress = i2CPars->DeviceAddress;
                i2c_ctx.TransferState = PTX_PLAT_TRX_STATE_IDLE;
                i2c_ctx.I2CPortUsed = i2c_port;
                *i2c = &i2c_ctx;

            } else
            {
                /**
                 *  Clean up here: close SPI port instance since it has been successfully opened.
                 */
                (void)ptxPLAT_I2C_PortClose(i2c_port);

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

ptxStatus_t ptxPLAT_I2C_Deinit(ptxPLAT_I2C_t *i2c)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != i2c)
    {
        /**
         * De-Init tasks shall be performed here:
         * - close I2C port - power down the peripheral
         * - de-init external IRQ GPIO module
         */
        ptxPLAT_I2C_PortClose(i2c->I2CPortUsed);

        ptxPLAT_GPIO_Deinit(i2c->Gpio);

        /** Clean I2C context structure. */
        memset(&i2c_ctx, 0, sizeof(ptxPLAT_I2C_t));

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }
    return status;
}

ptxStatus_t ptxPLAT_I2C_TRx(ptxPLAT_I2C_t *i2c, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != i2c)
    {
        /**
         * For any transmission operation over any interface the interrupt from GPIO-IRQ shall be disabled
         * to avoid potential mutual use of I2C driver.
         */

        uint32_t int_enable_status = 0xFFFFFFFFU;

        /** If function call is successful, returned value will be 0 or 1. */
        ptxPLAT_GPIO_GetIntEnableStatus (i2c->Gpio, &int_enable_status);

        ptxPLAT_GPIO_DisableInterrupt (i2c->Gpio);

        status = ptxPLAT_I2C_TRx_Helper(i2c, txBuf, txLen, numTxBuffers, rxBuf, rxLen, numRxBuffers, flags);

        if (1u == int_enable_status)
        {
            /** Restore interrupt state. */
            ptxPLAT_GPIO_EnableInterrupt (i2c->Gpio);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_I2C_StartWaitForRx(ptxPLAT_I2C_t *i2c, pptxPlat_RxCallBack_t irqCb, void *ctxIrqCb)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != i2c) && (NULL != irqCb) && (NULL != ctxIrqCb))
    {
        /** This function shall ensure that IRQ is not high before starting the wait for the event. */

        /** First register the callback function and the context, for future use */
        i2c->RxCb = irqCb;
        i2c->CtxRxCb = ctxIrqCb;

        /** Read IRQ first to check if it is already HIGH.*/
        uint8_t value_IRQ = 0;
        status = ptxPLAT_GPIO_ReadLevel(i2c->Gpio, &value_IRQ);

        if(1u == value_IRQ)
        {
            /** IRQ is already high, so let's trigger the asynchronous event now to prevent than race condition has happened. */
            i2c->RxCb(i2c->CtxRxCb);
        }

        /** Enable waiting for the trigger on IRQ (Not blocking). ISR on rising edge. */
        status = ptxPLAT_GPIO_StartWaitForTrigger(i2c->Gpio, (pptxPlat_IRQCallBack_t)irqCb, ctxIrqCb);

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxPLAT_I2C_StopWaitForRx(ptxPLAT_I2C_t *i2c)
{
    ptxStatus_t status = ptxStatus_Success;

    if (NULL != i2c)
    {
        /** Stop the wait for the trigger on IRQ (Not blocking) */
        status = ptxPLAT_GPIO_StopWaitForTrigger(i2c->Gpio);
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

static ptxStatus_t ptxPLAT_I2C_PortOpen(ptxPLAT_I2CPort_t **i2cPort, ptxPLAT_I2CConfigPars_t *i2cPars)
{
    ptxStatus_t status = ptxStatus_Success;

    if((NULL != i2cPars) && (NULL != i2cPort))
    {
        /**
         * It is assumed that the driver (interface API + data structures) for I2C port has already been generated.
         * Here we only set the speed and the device address
         */

        ptxPLAT_I2CInstance_t *i2c_instance = (ptxPLAT_I2CInstance_t *)available_i2C_port.I2CInstance;

        i2c_master_cfg_t tmp_i2c_master_cfg;
        memcpy((i2c_master_cfg_t*)&tmp_i2c_master_cfg, i2c_instance->p_cfg, sizeof(i2c_master_cfg_t));

        tmp_i2c_master_cfg.slave = (uint32_t)i2cPars->DeviceAddress;

        status = ptxPLAT_I2C_GetMappedBitrate(i2cPars->IntfSpeed, &tmp_i2c_master_cfg.rate);

        if (ptxStatus_Success == status)
        {
            fsp_err_t r_status;

            r_status = R_SCI_I2C_Open(i2c_instance->p_ctrl, &tmp_i2c_master_cfg);

            if(FSP_SUCCESS == r_status)
            {
                *i2cPort = (ptxPLAT_I2CPort_t *)&available_i2C_port;

            } else
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

static ptxStatus_t ptxPLAT_I2C_PortClose(ptxPLAT_I2CPort_t *i2cPort)
{
    ptxStatus_t status = ptxStatus_Success;

    if(NULL != i2cPort)
    {
        /**
         * Close I2C port: power down the peripheral.
         */
        ptxPLAT_I2CInstance_t *i2c_instance = (ptxPLAT_I2CInstance_t *)available_i2C_port.I2CInstance;

        fsp_err_t r_status = R_SCI_I2C_Close(i2c_instance->p_ctrl);

        if(FSP_SUCCESS != r_status)
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InternalError);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxPLAT_I2C_GetMappedBitrate(uint32_t targetRate, i2c_master_rate_t *mappedRate)
{
    ptxStatus_t status = ptxStatus_Success;

    if(NULL != mappedRate)
    {
        switch (targetRate)
        {
            case PTX_PLAT_HOST_SPEED_I2C_100000:
                *mappedRate = I2C_MASTER_RATE_STANDARD;
                break;

            case PTX_PLAT_HOST_SPEED_I2C_400000:
                *mappedRate = I2C_MASTER_RATE_FAST;
                break;

            case PTX_PLAT_HOST_SPEED_I2C_1000000:
                *mappedRate = I2C_MASTER_RATE_FASTPLUS;
                break;

            /** Note: High-Speed-Mode not supported on this target platform */
            case PTX_PLAT_HOST_SPEED_I2C_3400000:
                status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
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

static ptxStatus_t ptxPLAT_I2C_TRx_Helper(ptxPLAT_I2C_t *i2c, uint8_t *txBuf[], size_t txLen[], size_t numTxBuffers, uint8_t *rxBuf[], size_t *rxLen[], size_t numRxBuffers, uint8_t flags)
{
    ptxStatus_t status = ptxStatus_Success;

    /**
     * Tx operation is required always: to send and to receive anything on SPI. So, tx buffers have to be provided always.
     */

    if ((NULL != i2c) && (NULL != i2c->I2CPortUsed))
    {
        size_t i;
        uint8_t restart_required;

        /** Tx part of the overall transaction (Attention: I2C may not send anything at all - just receive). */
        i = 0;
        while ((ptxStatus_Success == status) && (i < numTxBuffers))
        {
            if ((txBuf[i] != NULL) && (txLen[i]>0))
            {
                if ((PTX_PLAT_TRX_FLAGS_I2C_RESTART_CONDITION == (flags & PTX_PLAT_TRX_FLAGS_I2C_RESTART_CONDITION)) && (i == (numTxBuffers - 1)))
                {
                    restart_required = 1;

                } else
                {
                    restart_required = 0;
                }

                size_t trx_len = txLen[i];
                status = ptxPLAT_I2C_TxAux (i2c, txBuf[i], &trx_len, restart_required);

            }

            i++;
        }

        if (ptxStatus_Success == status)
        {
            /** Rx part of the overall transaction. */
            if ((NULL != rxBuf) && (NULL != rxLen))
            {
                i = 0;
                while((ptxStatus_Success == status) && (i < numRxBuffers))
                {
                    if ((rxBuf[i] != NULL) && (rxLen[i] != NULL) && (*rxLen[i] > 0))
                    {
                        status = ptxPLAT_I2C_RxAux(i2c, rxBuf[i], rxLen[i]);
                    } else
                    {
                        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
                    }
                    i++;
                }
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InvalidParameter);
    }

    return status;
}

static ptxStatus_t ptxPLAT_I2C_TxAux(ptxPLAT_I2C_t *i2c, uint8_t *txBuff, size_t *len, uint8_t setRestartCondition)
{
    /**
     * All input arguments shall be checked by the caller. Here, it is expected all are valid i.e. not NULL and
     * allocated enough memory for Rx operation.
     *
     */
    ptxStatus_t status = ptxStatus_Success;

    ptxPLAT_I2CInstance_t *i2c_instance = (ptxPLAT_I2CInstance_t *)i2c->I2CPortUsed->I2CInstance;
    uint32_t length = (uint32_t)*len;

    i2c->TransferState = PTX_PLAT_TRX_STATE_IDLE;

    fsp_err_t _st = R_SCI_I2C_Write(i2c_instance->p_ctrl, txBuff, length, (0 != setRestartCondition) ? 1 : 0);

    if(FSP_SUCCESS != _st)
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
    }

    if(ptxStatus_Success == status)
    {
        /** Wait until transfer notification is received via callback. */
        while (PTX_PLAT_TRX_STATE_IDLE == i2c->TransferState);

        if (PTX_PLAT_TRX_STATE_TX_COMPLETE != i2c->TransferState)
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
        }
    }

    return status;
}

static ptxStatus_t ptxPLAT_I2C_RxAux(ptxPLAT_I2C_t *i2c, uint8_t *rxBuff, size_t *len)
{
    /**
     * All input arguments shall be checked by the caller. Here, it is expected all are valid i.e. not NULL and
     * allocated enough memory for Rx operation.
     *
     * What about setting a safeguard timer before the wait-for-transfer-end loop?
     */
    ptxStatus_t status = ptxStatus_Success;

    ptxPLAT_I2CInstance_t *i2c_instance = (ptxPLAT_I2CInstance_t *)i2c->I2CPortUsed->I2CInstance;
    uint32_t length = (uint32_t)*len;

    i2c->TransferState = PTX_PLAT_TRX_STATE_IDLE;

    fsp_err_t r_status = R_SCI_I2C_Read(i2c_instance->p_ctrl, rxBuff, length, 0);

    if(FSP_SUCCESS != r_status)
    {
        status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
    }

    if(ptxStatus_Success == status)
    {
        while (PTX_PLAT_TRX_STATE_IDLE == i2c->TransferState);

        if (PTX_PLAT_TRX_STATE_RX_COMPLETE != i2c->TransferState)
        {
            status = PTX_STATUS(ptxStatus_Comp_PLAT, ptxStatus_InterfaceError);
        }
    }

    return status;
}

void ptxPLAT_I2C_TransferCallback(i2c_master_callback_args_t *p_args)
{
    if (NULL != p_args)
    {
        switch(p_args->event)
        {
            case I2C_MASTER_EVENT_TX_COMPLETE:
                i2c_ctx.TransferState = PTX_PLAT_TRX_STATE_TX_COMPLETE;
                break;

            case I2C_MASTER_EVENT_RX_COMPLETE:
                i2c_ctx.TransferState = PTX_PLAT_TRX_STATE_RX_COMPLETE;
                break;

            default:
                /** Transfer error. */
                i2c_ctx.TransferState = PTX_PLAT_TRX_STATE_ERROR;
                break;
        }
    }
}

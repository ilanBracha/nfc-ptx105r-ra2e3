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
    Module      : Transparent Data Channel (TDC) API
    File        : ptxTDC.h

    Description : Dedicated API to use the Renesas-proprietary Transparent Data Channel feature.
                  The TDCI API can be used for Renesas NFC WLC Listener products such as the PTX30W.
*/

/**
 * \addtogroup grp_ptx_api_tdc Transparent Data Channel (TDC) API
 *
 * @{
 */

#ifndef APIS_PTX_TDC_H_
#define APIS_PTX_TDC_H_

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include <stdint.h>
#include <STACK/COMPS/ptxStatus.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/**
 * \brief Transparent-Mode Initialization Parameters
 */
#define PTX_TDC_BUFFER_LEN_MASK             (0x3F)      /**< Buffer size mask for TDC operation */
#define PTX_TDC_MAX_BUFFER_LEN              (0x3F)      /**< Max. Buffer size for TDC operation (Payload only) */
#define PTX_TDC_DEFAULT_TIMEOUT             (20u)       /**< Default Timeout given in ms for all Read- and Write operations */
#define PTX_TDC_CMD_NFC_FORUM_T2T_WRITE     (0xA2)      /**< Command for NFC Forum T2T Write operation*/
#define PTX_TDC_CMD_NFC_FORUM_T2T_READ      (0x30)      /**< Command for NFC Forum T2T Read operation*/
#define PTX_TDC_LEN_NFC_FORUM_T2T_READ      (16u)       /**< Length of response for NFC Forum T2T Read */
#define PTX_TDC_CMD_PROP_WRITE              (0xAF)      /**< Command for proprietary Write operation */
#define PTX_TDC_CMD_PROP_READ               (0x3F)      /**< Command for proprietary Read operation */
#define PTX_TDC_LEN_PROP_WRITE              (64u)       /**< Max. Number of Bytes for proprietary Write operation (Header + Payload) */
#define PTX_TDC_LEN_PROP_READ               (64u)       /**< Max. Number of Bytes for proprietary Read operation (Header + Payload)*/
#define PTX_TDC_PROP_BLOCK_SIZE             (4u)        /**< Block size of Listener device */
#define PTX_TDC_BLOCK_NUM_POL_BUFF          (48u)       /**< Start Block address for TDC Write Operation */
#define PTX_TDC_BLOCK_NUM_LIS_BUFF          (64u)       /**< Start Block address for TDC Read Operation */
#define PTX_TDC_CMD_NSC_DATA_MSG            (0x80)      /**< Command for internal NSC Data operation */

/*
 * ####################################################################################################################
 * TYPES
 * ####################################################################################################################
 */

/**
 * \brief Transparent-Mode Initialization Parameters
 */
typedef struct ptxTDC_InitParams
{
#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd         *IoTRdComp;         /**< Main Stack Component */
#else
    struct ptxPOS           *POSComp;           /**< Main Stack Component */
#endif
} ptxTDC_InitParams_t;

/**
 * \brief Transparent-Mode Component
 */
typedef struct ptxTDC
{
    /* Components */
    ptxStatus_Comps_t       CompId;             /**< Component Id */

#ifdef PTX_PRODUCT_TYPE_IOT_READER
    struct ptxIoTRd         *IoTRdComp;         /**< Main Stack Component */
#else
    struct ptxPOS           *POSComp;           /**< Main Stack Component */
#endif

} ptxTDC_t;

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

/**
 * \brief Initializes the TDC Component.
 *
 * \param[in]   tdcComp                 Pointer to an allocated instance of the TDC component.
 * \param[in]   initParams              Pointer to initialization parameters.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Init (ptxTDC_t *tdcComp, ptxTDC_InitParams_t *initParams);

/**
 * \brief Deinitializes the TDC Component.
 *
 * \param[in]   tdcComp                  Pointer to an initialized instance of the TDC component.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Deinit (ptxTDC_t *tdcComp);

/**
 * \brief Writes messages to the listener's buffer. Customer can choose between NFC Forum compliant write access
 *          and proprietary write access (= faster) via compile switch ("TDC_NFC_FORUM_COMPLIANT").
 *
 * \param[in]   tdcComp                  Pointer to an initialized instance of the TDC component.
 * \param[in]   txData                   Pointer to the data which shall be transmitted.
 * \param[in]   txLen                    Size of the buffer.
 * \param[in]   ackTimeoutMs             Timeout for the message to be received.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Write(ptxTDC_t *tdcComp, uint8_t *txData, uint8_t txLen, uint32_t ackTimeoutMs);

/**
 * \brief Reads messages from the listener's buffer. Customer can choose between NFC Forum compliant read access
 *          and proprietary read access (= faster) via compile switch ("TDC_NFC_FORUM_COMPLIANT").
 *
 * \param[in]       tdcComp             Pointer to an initialized instance of the TDC component.
 * \param[in,out]   rxData              Pointer to a buffer for storing the received data.
 * \param[in,out]   rxDataLen           Size of the buffer.
 * \param[in]       rxTimeoutMs         Timeout in ms for the transaction.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_Read(ptxTDC_t *tdcComp, uint8_t *rxData, uint8_t *rxDataLen, uint32_t rxTimeoutMs);

/**
 * \brief Checks if the Listener's host MCU has read the previously sent message.
 *
 * \param[in]       tdcComp     Pointer to an initialized instance of the TDC component.
 * \param[in,out]   received    Pointer to store the status, if the message has been read by the listener.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_IsReceived(ptxTDC_t *tdcComp, uint8_t *received);

/**
 * \brief Issues a T2T read command over the RF interface
 *
 * \param[in]       tdcComp     Pointer to an initialized instance of the TDC component.
 * \param[in]       block       Block number to read from.
 * \param[in,out]   rxData      Pointer to a buffer to store the received data.
 * \param[in,out]   rxDataLen   Size of the buffer.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_ReadT2T(ptxTDC_t *tdcComp, uint8_t block, uint8_t *rxData, uint32_t *rxDataLen);

/**
 * \brief Issues a T2T write command over the RF interface.
 *
 * \param[in]       tdcComp     Pointer to an initialized instance of the TDC component.
 * \param[in]       block       Block number to write to.
 * \param[in]       txData      Pointer to a buffer containing the data to be written to the tag.
 * \param[in]       txLen       Size of the buffer.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_WriteT2T(ptxTDC_t *tdcComp, uint8_t block, uint8_t *txData, uint32_t txLen);

/**
 * \brief Issues a proprietary read command over the RF interface
 *
 * \param[in]       tdcComp     Pointer to an initialized instance of the TDC component.
 * \param[in]       block       Block number to read from.
 * \param[in,out]   rxData      Pointer to a buffer to store the received data.
 * \param[in,out]   rxDataLen   Size of the buffer.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_ReadProprietary(ptxTDC_t *tdcComp, uint8_t block, uint8_t *rxData, uint32_t *rxDataLen);

/**
 * \brief Issues a proprietary write command over the RF interface.
 *
 * \param[in]       tdcComp     Pointer to an initialized instance of the TDC component.
 * \param[in]       block       Block number to write to.
 * \param[in]       txData      Pointer to a buffer containing the data to be written to the tag.
 * \param[in]       txLen       Size of the buffer.
 *
 * \return Status, indicating whether the operation was successful. See \ref ptxStatus_t.
 */
ptxStatus_t ptxTDC_WriteProprietary(ptxTDC_t *tdcComp, uint8_t block, uint8_t *txData, uint32_t txLen);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* Guard */


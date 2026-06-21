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
    File        : ptxTDC.c

    Description : Dedicated API to use the Renesas-proprietary Transparent Data Channel feature.
                  The TDCI API can be used for Renesas NFC WLC Listener products such as the PTX30W.
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */
#include "ptxTDC.h"
#include "ptxTDC_Timer.h"
#include <string.h>
#include "ptx_IOT_READER.h"

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
#define PTX_TDC_NFC_READ_T2T_RX_LEN      (PTX_TDC_LEN_NFC_FORUM_T2T_READ + 1U)
#define PTX_TDC_NFC_READ_T2T_TX_LEN      (2U)
#define PTX_TDC_NFC_WRITE_T2T_RX_LEN     (2U)
#define PTX_TDC_NFC_WRITE_T2T_TX_LEN     (6U)
#define PTX_TDC_NFC_READ_PROP_RX_LEN     (PTX_TDC_LEN_PROP_READ + 1U)
#define PTX_TDC_NFC_READ_PROP_TX_LEN     (3U)
#define PTX_TDC_NFC_WRITE_PROP_RX_LEN    (2)
#define PTX_TDC_NFC_WRITE_PROP_TX_LEN    (PTX_TDC_LEN_PROP_WRITE + 2U)


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
ptxStatus_t ptxTDC_Data_Exchange (ptxTDC_t *tdcComp, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */

ptxStatus_t ptxTDC_Init (ptxTDC_t *tdcComp, ptxTDC_InitParams_t *initParams)
{
    ptxStatus_t status = ptxStatus_Success;

    if ((NULL != tdcComp) && (NULL != initParams))
    {
        if ((NULL != initParams->IoTRdComp))
        {
            /* clear component */
            (void)memset(tdcComp, 0, sizeof(ptxTDC_t));

            /* set members */
            tdcComp->IoTRdComp = initParams->IoTRdComp;

            /* assign Component ID */
            tdcComp->CompId = ptxStatus_Comp_TDC;

        } else
        {
            status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TransparentMode, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_Deinit (ptxTDC_t *tdcComp)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC))
    {
        /* Clear the component ID. */
        tdcComp->CompId = ptxStatus_Comp_None;
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_Write(ptxTDC_t *tdcComp, uint8_t *txData, uint8_t txLen, uint32_t ackTimeoutMs)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t received = 0;

    if ((PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC)) && (NULL != txData) && (PTX_TDC_MAX_BUFFER_LEN >= txLen))
    {
#ifdef TDC_NFC_FORUM_COMPLIANT
        size_t buffer_pos = PTX_TDC_PROP_BLOCK_SIZE - 1u;   /**< Subtract header byte. */
        uint8_t block = (uint8_t)(PTX_TDC_BLOCK_NUM_POL_BUFF + 1u);
        size_t req_writ_op = txLen / PTX_TDC_PROP_BLOCK_SIZE;

        /**
         * The poller buffer within the listener's memory starts on block 48. As soon as block 48 gets modified,
         * by the poller, the listener will indicate the modification to the it's host MCU by throwing an interrupt.
         * Hence, we need to make sure, to write the complete remaining payload FIRST, and only afterwards we update
         * block 48!
         * Using the T2T write command, we can only write 4 bytes at once!
         */
        while ((ptxStatus_Success == status) &&  (0u != req_writ_op))
        {
            /** Write the data in chunks of 4 bytes (NFC_FORUM compliant). */
            status = ptxTDC_WriteT2T(tdcComp, block++, &txData[buffer_pos], PTX_TDC_PROP_BLOCK_SIZE);

            /** Increment block number. */
            buffer_pos += PTX_TDC_PROP_BLOCK_SIZE;

            /** Decrement number of remaining write operations. */
            --req_writ_op;
        }

        /** Write the remaining data into block 48. */
        if(ptxStatus_Success == status)
        {
            uint8_t header_block[PTX_TDC_PROP_BLOCK_SIZE] = {0};

            /** The first byte in block 48 indicates the number of written bytes to the listener. */
            header_block[0] = PTX_TDC_CMD_NSC_DATA_MSG | txLen;

            /** Copy remaining data into reserved memory. */
            (void)memcpy(&header_block[1], &txData[0], PTX_TDC_PROP_BLOCK_SIZE - 1u);

            /** Write the data in chunks of 4 bytes (NFC_FORUM compliant). */
            status = ptxTDC_WriteT2T(tdcComp, PTX_TDC_BLOCK_NUM_POL_BUFF, &header_block[0], 4);
        }
#else
        /** Reserve 64 bytes at once. */
        uint8_t header_block[PTX_TDC_LEN_PROP_WRITE] = {0};

        /** The first byte in block 48 indicates the number of written bytes to the listener. */
        header_block[0] = (uint8_t)(PTX_TDC_CMD_NSC_DATA_MSG | txLen);

        /** Copy remaining data into reserved memory. */
        (void)memcpy(&header_block[1], &txData[0], txLen);

        /** Write the data using one single write operation (PTX proprietary command). */
        status = ptxTDC_WriteProprietary(tdcComp, PTX_TDC_BLOCK_NUM_POL_BUFF, header_block, txLen + 1u);
#endif

        /** Check if we want to wait for the read acknowledge. */
        if( (ptxStatus_Success == status) && (0 != ackTimeoutMs) )
        {
            ptxTDC_Timer_Status_t timer_status;

            /** Prepare the timer. */
            (void) ptxTDC_Timer_Start(ackTimeoutMs);

            do
            {
                /** Did the listener host MCU read the package? */
                status = ptxTDC_IsReceived(tdcComp, &received);

                /** Check the timer status. */
                (void) ptxTDC_Timer_Status(&timer_status);

            } while ((ptxStatus_Success == status) && (0 == received) && (0 == timer_status.IsElapsed));

            (void) ptxTDC_Timer_Stop();

            /** Did the listener's host MCU read the package within the given timout? */
            if (0 == received)
            {
                status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_TimeOut);
            }
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_Read(ptxTDC_t *tdcComp, uint8_t *rxData, uint8_t *rxDataLen, uint32_t rxTimeoutMs)
{
    ptxStatus_t status = ptxStatus_Success;

    uint8_t rx_data[PTX_TDC_LEN_PROP_READ];
    uint32_t rx_data_len;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != rxData) && (NULL != rxDataLen))
    {
        ptxTDC_Timer_Status_t timer_status;

        do
        {
            /** Do we actually need the timer? Customer has the choice. */
            if (0 != rxTimeoutMs)
            {
                status = ptxTDC_Timer_Start(rxTimeoutMs);
            }

#ifdef TDC_NFC_FORUM_COMPLIANT
            size_t buffer_pos = PTX_TDC_LEN_NFC_FORUM_T2T_READ;
            rx_data_len = PTX_TDC_LEN_NFC_FORUM_T2T_READ;
            uint8_t block_num = PTX_TDC_BLOCK_NUM_LIS_BUFF;

            /** Use the "standard" T2T read command to retrieve the data (16 bytes per transaction). */
            status = ptxTDC_ReadT2T(tdcComp, block_num, rx_data, &rx_data_len);

            size_t req_read_op = (rx_data[0] & PTX_TDC_BUFFER_LEN_MASK) / PTX_TDC_LEN_NFC_FORUM_T2T_READ;

            while( (ptxStatus_Success == status) &&  (0u != req_read_op) )
            {
                block_num += (PTX_TDC_LEN_NFC_FORUM_T2T_READ / PTX_TDC_PROP_BLOCK_SIZE);
                rx_data_len = PTX_TDC_LEN_NFC_FORUM_T2T_READ;

                status = ptxTDC_ReadT2T(tdcComp, block_num, &rx_data[buffer_pos], &rx_data_len);

                buffer_pos += PTX_TDC_LEN_NFC_FORUM_T2T_READ;
                --req_read_op;
            }
#else
            rx_data_len = PTX_TDC_PROP_BLOCK_SIZE;

            /** Do the proprietary read operation (max. 64 bytes per transaction). */
            status = ptxTDC_ReadProprietary(tdcComp, PTX_TDC_BLOCK_NUM_LIS_BUFF, rx_data, &rx_data_len);
            uint8_t req_read_op = (rx_data[0] & PTX_TDC_BUFFER_LEN_MASK);

            if(req_read_op > (PTX_TDC_PROP_BLOCK_SIZE - 1u))
            {
                rx_data_len = (uint32_t)(req_read_op + 1); // + header_byte
                status = ptxTDC_ReadProprietary(tdcComp, PTX_TDC_BLOCK_NUM_LIS_BUFF, rx_data, &rx_data_len);
            }
#endif
            /** Check the timer status. Did it elapse?*/
            ptxTDC_Timer_Status(&timer_status);

            /** Read out the until time elapses or until (rx_data[0] != 0). */
        } while(    (ptxStatus_Success == status)
                &&  (0 == (rx_data[0] & PTX_TDC_BUFFER_LEN_MASK))
                &&  (0 != rxTimeoutMs)
                &&  (0 == timer_status.IsElapsed) );

        /** If the timer was running, we need to stop it! */
        if (0 != rxTimeoutMs)
        {
            (void) ptxTDC_Timer_Stop();
        }

        /** If everything was successful, copy the relevant information. */
        if (ptxStatus_Success == status)
        {
            *rxDataLen = rx_data[0] & PTX_TDC_BUFFER_LEN_MASK;
            (void)memcpy(rxData, &rx_data[1], *rxDataLen);
        }
        else
        {
            *rxDataLen = 0; /**< Explicitly set to 0 in case of error. */
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_IsReceived(ptxTDC_t *tdcComp, uint8_t *received)
{
    const uint32_t RX_BUFFER_LEN = PTX_TDC_LEN_NFC_FORUM_T2T_READ;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t read_data[PTX_TDC_LEN_NFC_FORUM_T2T_READ];
    uint32_t read_data_len = RX_BUFFER_LEN;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != received))
    {
        /** Explicitly set "received" variable to zero. */
        *received = 0;

#ifdef TDC_NFC_FORUM_COMPLIANT
        /** Read the first block of the poller buffer on the listener device. */
        status = ptxTDC_ReadT2T(tdcComp, PTX_TDC_BLOCK_NUM_POL_BUFF, read_data, &read_data_len);
#else
        /** Do the proprietary read operation (reads 4 bytes per transaction instead of 16 byte). */
        read_data_len = PTX_TDC_PROP_BLOCK_SIZE;
        status = ptxTDC_ReadProprietary(tdcComp, PTX_TDC_BLOCK_NUM_POL_BUFF, read_data, &read_data_len);
#endif
        /**
         * In case the host MCU (of the listener) has read the message, the first byte in the
         * poller buffer (indicating the message length) will be set to zero.
         */
        if ((ptxStatus_Success == status) && (0u == read_data[0]))
        {
            *received = 1;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_ReadT2T(ptxTDC_t *tdcComp, uint8_t block, uint8_t *rxData, uint32_t *rxDataLen)
{
    const uint32_t RX_BUFFER_LEN = PTX_TDC_NFC_READ_T2T_RX_LEN;
    const uint32_t TX_BUFFER_LEN = PTX_TDC_NFC_READ_T2T_TX_LEN;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t tx_buffer[PTX_TDC_NFC_READ_T2T_TX_LEN];
    uint32_t tx_buffer_len = TX_BUFFER_LEN;
    uint8_t rx_buffer[PTX_TDC_NFC_READ_T2T_RX_LEN];
    uint32_t rx_buffer_len = RX_BUFFER_LEN;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != rxData) && (NULL != rxDataLen) && (PTX_TDC_LEN_NFC_FORUM_T2T_READ <= *rxDataLen))
    {
        tx_buffer[0] = PTX_TDC_CMD_NFC_FORUM_T2T_READ;
        tx_buffer[1] = block;

        status = ptxTDC_Data_Exchange(tdcComp, tx_buffer, tx_buffer_len, rx_buffer, &rx_buffer_len, PTX_TDC_DEFAULT_TIMEOUT);

        if ((ptxStatus_Success == status) && ((RX_BUFFER_LEN) == rx_buffer_len) && (0u == rx_buffer[rx_buffer_len - 1]))
        {
            rx_buffer_len = rx_buffer_len - 1u;
            memcpy(rxData, rx_buffer, rx_buffer_len);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_WriteT2T(ptxTDC_t *tdcComp, uint8_t block, uint8_t *txData, uint32_t txLen)
{
    const uint32_t RX_BUFFER_LEN = PTX_TDC_NFC_WRITE_T2T_RX_LEN;
    const uint32_t TX_BUFFER_LEN = PTX_TDC_NFC_WRITE_T2T_TX_LEN;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t rx_buffer[PTX_TDC_NFC_WRITE_T2T_RX_LEN];
    uint32_t rx_buffer_len = RX_BUFFER_LEN;
    uint8_t tx_buffer[PTX_TDC_NFC_WRITE_T2T_TX_LEN];
    uint32_t tx_buffer_len = TX_BUFFER_LEN;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != txData))
    {
        tx_buffer[0] = PTX_TDC_CMD_NFC_FORUM_T2T_WRITE;
        tx_buffer[1] = block;
        memcpy(&tx_buffer[2], txData, txLen);

        status = ptxTDC_Data_Exchange(tdcComp, tx_buffer, tx_buffer_len, rx_buffer, &rx_buffer_len, PTX_TDC_DEFAULT_TIMEOUT);

        if ((ptxStatus_Success == status) && ((2u != rx_buffer_len) || (4u != rx_buffer[rx_buffer_len - 1])))
        {
            status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_ProtocolError);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_ReadProprietary(ptxTDC_t *tdcComp, uint8_t block, uint8_t *rxData, uint32_t *rxDataLen)
{
    const uint32_t RX_BUFFER_LEN = PTX_TDC_NFC_READ_PROP_RX_LEN;
    const uint32_t TX_BUFFER_LEN = PTX_TDC_NFC_READ_PROP_TX_LEN;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t rx_buffer[PTX_TDC_NFC_READ_PROP_RX_LEN];
    uint32_t rx_buffer_len = RX_BUFFER_LEN;
    uint8_t tx_buffer[PTX_TDC_NFC_READ_PROP_TX_LEN];
    uint32_t tx_buffer_len = TX_BUFFER_LEN;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != rxData) && (NULL != rxDataLen))
    {
        tx_buffer[0] = PTX_TDC_CMD_PROP_READ;
        tx_buffer[1] = block;
        tx_buffer[2] = (uint8_t) ((*rxDataLen / PTX_TDC_PROP_BLOCK_SIZE) + (*rxDataLen % PTX_TDC_PROP_BLOCK_SIZE ? 1u : 0u));

        status = ptxTDC_Data_Exchange(tdcComp, tx_buffer, tx_buffer_len, rx_buffer, &rx_buffer_len, PTX_TDC_DEFAULT_TIMEOUT);

        if ( (ptxStatus_Success == status) && (((tx_buffer[2] * PTX_TDC_PROP_BLOCK_SIZE) + 1u) == rx_buffer_len))
        {
           *rxDataLen = rx_buffer_len - 1u;
           memcpy(rxData, rx_buffer, *rxDataLen);
        }
        else
        {
            *rxDataLen = 0;
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_WriteProprietary(ptxTDC_t *tdcComp, uint8_t block, uint8_t *txData, uint32_t txLen)
{
    const uint32_t RX_BUFFER_LEN = PTX_TDC_NFC_WRITE_PROP_RX_LEN;
    const uint32_t TX_BUFFER_LEN = PTX_TDC_NFC_WRITE_PROP_TX_LEN;

    ptxStatus_t status = ptxStatus_Success;

    uint8_t rx_buffer[PTX_TDC_NFC_WRITE_PROP_RX_LEN];
    uint32_t rx_buffer_len = RX_BUFFER_LEN;
    uint8_t tx_buffer[PTX_TDC_NFC_WRITE_PROP_TX_LEN];

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != txData) && (txLen <= PTX_TDC_LEN_PROP_WRITE))
    {
        (void)memset(tx_buffer, 0, TX_BUFFER_LEN);

        uint8_t blocks = (uint8_t) ((txLen / PTX_TDC_PROP_BLOCK_SIZE) + (txLen % PTX_TDC_PROP_BLOCK_SIZE ? 1u : 0u));

        tx_buffer[0] = PTX_TDC_CMD_PROP_WRITE;
        tx_buffer[1] = block;

        (void)memcpy(&tx_buffer[2], txData, txLen);

        uint8_t data_to_transmit = (uint8_t)((blocks * PTX_TDC_PROP_BLOCK_SIZE) + 2u); /** accommodate for the two additional header bytes */

        status = ptxTDC_Data_Exchange(tdcComp, tx_buffer, data_to_transmit, rx_buffer, &rx_buffer_len, PTX_TDC_DEFAULT_TIMEOUT);

        if ((ptxStatus_Success == status) && ((2u != rx_buffer_len) || (4u != rx_buffer[rx_buffer_len - 1])))
        {
            status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_ProtocolError);
        }

    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}

ptxStatus_t ptxTDC_Data_Exchange (ptxTDC_t *tdcComp, uint8_t *tx, uint32_t txLength, uint8_t *rx, uint32_t *rxLength, uint32_t msAppTimeout)
{
    ptxStatus_t status = ptxStatus_Success;

    if (PTX_COMP_CHECK(tdcComp, ptxStatus_Comp_TDC) && (NULL != tx) && (txLength > 0) && (NULL != rx) && (NULL != rxLength) && (*rxLength > 0) && (msAppTimeout > 0))
    {
        status = ptxIoTRd_Data_Exchange(tdcComp->IoTRdComp, tx, txLength, rx, rxLength, msAppTimeout);
    } else
    {
        status = PTX_STATUS(ptxStatus_Comp_TDC, ptxStatus_InvalidParameter);
    }

    return status;
}


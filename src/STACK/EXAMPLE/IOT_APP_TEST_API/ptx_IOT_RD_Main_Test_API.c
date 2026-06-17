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
    Module      : Example Code / Application for POS including RF-Test and FeliCa-DTE
    File        : ptx_POS_Main_Test_API.c

    Description :
*/

/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#include "ptxCOMMON.h"
#include "ptx_IOT_READER.h"
#include "ptxFeliCa_DTE.h"
#include "ptxRF_Test.h"
#include "ptxPLAT.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <STACK/COMPS/COMMON/ptxIoTRd_COMMON.h>
#include <STACK/EXAMPLE/DEBUG_PORT/ptxDBG_PORT.h>
#include <STACK/EXAMPLE/IOT_APP_TEST_API/ptx_IOT_RD_Main_Test_API.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */
#define PTX_TEST_API_MAX_ARG_LEN     20
#define PTX_TEST_API_MAX_ARGS        10

/*
 * ####################################################################################################################
 * INTERNAL TYPES
 * ####################################################################################################################
 */
typedef struct ptxTestAPI_Argument
{
    uint8_t Arg[PTX_TEST_API_MAX_ARG_LEN];
    uint8_t ArgLen;
} ptxTestAPI_Argument_t;

typedef struct ptxTestAPI_ArgumentParams
{
    ptxTestAPI_Argument_t Arguments[PTX_TEST_API_MAX_ARGS];
    uint8_t NrArguments;
} ptxTestAPI_ArgumentParams_t;


/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static void ptxIOT_ProcessRequests(ptxIoTRd_t *iotRd, ptxFeliCa_DTE_t *feliCaComp, ptxRF_Test_t *rfTestComp);
static void ptxIOT_ShowHelp();
static void ptxIOT_HandleStatus(ptxStatus_t st, uint8_t isSilentMode);
static void ptxIoT_ParseArgumentParams(uint8_t *argBuffer, uint8_t argBufferLen, ptxTestAPI_ArgumentParams_t * argParams);
static void ptxIoT_PrintFeliCaPerformanceResult (uint8_t *resultBuffer, uint8_t nrResults);
static void ptxIoT_PrintFeliCaDigitalProtocolSequences (uint8_t *resultBuffer, uint32_t nrResults, int *sequenceCounter);
static int ptxIoT_ATOI(char * s);
static int ptxIoT_ATOHEX(char *input, uint32_t inputLen, uint32_t maxInputLen, uint8_t *output);

/*
 * ####################################################################################################################
 * API FUNCTIONS
 * ####################################################################################################################
 */
int ptxAPP_Entry(void);

int ptxAPP_Entry(void)
{
    ptxIOT_RFTestFeliCaExampleApp();
    return 1;
}

void ptxIOT_RFTestFeliCaExampleApp(void)
{
    ptxStatus_t st = ptxStatus_Success;

    uint8_t start_temperature_sensor_calibration = 1;

    /* IoT Reader Context. */
    ptxIoTRd_t iotRd;
    (void)memset(&iotRd, 0, sizeof(ptxIoTRd_t));

    /* FeliCa-DTE Context/Component */
    ptxFeliCa_DTE_t felica_comp;
    ptxFeliCa_DTE_InitParams_t felica_init_params;
    (void)memset(&felica_comp,         0, sizeof(ptxFeliCa_DTE_t));
    (void)memset(&felica_init_params,  0, sizeof(ptxFeliCa_DTE_InitParams_t));

    /* RF-Test Context/Component */
    ptxRF_Test_t rftest_comp;
    ptxRF_Test_InitParams_t rf_test_init_params;
    (void)memset(&rftest_comp,         0, sizeof(ptxRF_Test_t));
    (void)memset(&rf_test_init_params, 0, sizeof(ptxRF_Test_InitParams_t));

    /* IoT Reader configuration */
    ptxIoTRd_InitPars_t initParams;
    ptxIoTRd_TempSense_Params_t tempSens;
    ptxIoTRd_ComInterface_Params_t comIntf;

    (void)memset(&initParams, 0, sizeof(ptxIoTRd_InitPars_t));
    (void)memset(&tempSens, 0, sizeof(ptxIoTRd_TempSense_Params_t));
    (void)memset(&comIntf, 0, sizeof(ptxIoTRd_ComInterface_Params_t));

    /* Define communication interface settings. */
#if defined (PTX_INTF_UART)
    comIntf.Speed = PTX_IOTRD_HOST_SPEED_UART_115200;
#elif defined(PTX_INTF_SPI)
    comIntf.Speed = PTX_IOTRD_HOST_SPEED_SPI_1M;
#elif defined(PTX_INTF_I2C)
    comIntf.Speed = PTX_IOTRD_HOST_SPEED_I2C_100000;
    comIntf.DeviceAddress = 0x4FU;
#else
    #error Error - Missing or unsupported Host-Interface implementation used
#endif

    if(1u == start_temperature_sensor_calibration)
    {
        /* Calibration required. Set ambient temperature and expected shutdown temperature treshold. */
        tempSens.Calibrate = 1;
        tempSens.Tambient = 25;
        tempSens.Tshutdown = 100;

        /* Calibration will take place now. Doesn´t need to be done anymore if ptxPOS_Init is successful. */
        start_temperature_sensor_calibration = 0;
    } else
    {
        /* Calibration not needed, it has already been done. Provide compensated treshold temperature word. */
        tempSens.Tshutdown = 223u;
    }

    /* Initial parameters for temperature sensor are ready. */
    initParams.TemperatureSensor = &tempSens;
    initParams.ComInterface = &comIntf;

    ptxCommon_PrintF("***************************************************\n");
    ptxCommon_PrintF("****  PTX FeliCa-DTE and RF-Test Example V1.0  ****\n");
    ptxCommon_PrintF("***************************************************\n");

    /* Initiate IoT-Reader System. */
    st = ptxIoTRd_Init(&iotRd, &initParams);

    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF("System Initialization ... OK\n");

    } else
    {
        ptxCommon_PrintF("System Initialization ... ERROR\n");
    }

    if (ptxStatus_Success == st)
    {
        /* initialize FeliCa-DTE component */
        felica_init_params.IoTRdComp = &iotRd;
        st = ptxFeliCa_DTE_Init(&felica_comp, &felica_init_params);

        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("FeliCa-DTE Initialization ... OK\n");
        } else
        {
            ptxCommon_PrintF("FeliCa-DTE Initialization ... ERROR\n");
        }
    }

    if (ptxStatus_Success == st)
    {
        /* initialize RF-Test component */
        rf_test_init_params.Nsc = iotRd.Nsc;
        st = ptxRF_Test_Init(&rftest_comp, &rf_test_init_params);

        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("RF-Test Initialization ... OK\n");
        } else
        {
            ptxCommon_PrintF("RF-Test Initialization ... ERROR\n");
        }
    }

    if (ptxStatus_Success == st)
    {
        /* From here, process incoming requests from (serial) command-line */
        ptxIOT_ProcessRequests(&iotRd, &felica_comp, &rftest_comp);
    }

    /* Clean up */
    (void)ptxRF_Test_Deinit(&rftest_comp);
    (void)ptxFeliCa_DTE_Deinit(&felica_comp);
    (void)ptxIoTRd_Deinit(&iotRd);

}

/*
 * ####################################################################################################################
 * INTERNAL FUNCTIONS
 * ####################################################################################################################
 */
static void ptxIOT_ProcessRequests(ptxIoTRd_t *iotRd, ptxFeliCa_DTE_t *feliCaComp, ptxRF_Test_t *rfTestComp)
{
    const uint8_t NR_FELICA_PERFORMANCE_TESTS = (uint8_t)100;
    const uint16_t UART_RXBUFFER_SIZE = (uint16_t)286;
    const uint32_t RESULT_BUFFER_SIZE = (uint16_t)256;
    const uint32_t GENERIC_BUFFER_SIZE = (uint16_t)20;

    ptxStatus_t st = ptxStatus_Success;
    uint8_t system_state = PTX_SYSTEM_STATUS_OK;

    uint8_t uart_rx_buffer[UART_RXBUFFER_SIZE];
    uint32_t uart_rx_bytes;
    uint32_t uart_rx_index = 0;
    uint8_t reset_uart_reception = 0;
    ptxTestAPI_ArgumentParams_t arg_params;
    uint8_t is_invalid_param;
    uint8_t arg_index;
    uint8_t felica_result_buffer[RESULT_BUFFER_SIZE];

    int sequence_counter;

    uint8_t felica_generic_cmd_buffer[GENERIC_BUFFER_SIZE];

    /* FeliCa-DTE test parameters */
    ptxFeliCa_DTE_TestParams_t felica_test_params;
    (void)memset(&felica_test_params,  0, sizeof(ptxFeliCa_DTE_TestParams_t));

    /* RF-Test test parameters */
    ptxRF_Test_TestParams_t rf_test_test_params;
    (void)memset(&rf_test_test_params, 0, sizeof(ptxRF_Test_TestParams_t));

    (void)memset(&arg_params, 0, sizeof(ptxTestAPI_ArgumentParams_t));
    (void)memset(&uart_rx_buffer[0], 0, UART_RXBUFFER_SIZE);

    while (1)
    {
        /* check regularly for critical system errors */
        st = ptxIoTRd_Get_Status_Info (iotRd, StatusType_System, &system_state);

        if (PTX_SYSTEM_STATUS_OK != system_state)
        {
            /* Handle system-error */
            ptxCommon_PrintF("System Error detected - please restart Application\n");
        }

        uart_rx_bytes = ptxDBGPORT_Get_Available_Bytes();

        if (0 != uart_rx_bytes)
        {
            (void)ptxDBGPORT_Read_Buffer_(&uart_rx_buffer[0], uart_rx_index);
            uart_rx_index = (uint32_t)(uart_rx_index + uart_rx_bytes);

            if (0 != uart_rx_index)
            {
                switch (uart_rx_buffer[0])
                {
                    /* FeliCa-DTE Silent Mode Command (Enable) */
                    case 'E':
                    case 'e':
                        felica_test_params.Params.PerformanceTest.Bitrate = FELICA_DTE_BITRATE_212;
                        felica_test_params.Params.PerformanceTest.TimeOutMS = (uint32_t)300;
                        st = ptxFeliCa_DTE_EnableMode(feliCaComp, (uint8_t)1, &felica_test_params);
                        ptxIOT_HandleStatus(st, 1U);
                        reset_uart_reception = (uint8_t)1;
                        break;

                    /* FeliCa-DTE Silent Mode Command (Disable) */
                    case 'D':
                    case 'd':
                        st = ptxFeliCa_DTE_EnableMode(feliCaComp, 0, NULL);
                        ptxIOT_HandleStatus(st, 1U);
                        reset_uart_reception = (uint8_t)1;
                        break;

                    /* FeliCa-DTE Silent Mode Command (Run Performance-Test with specified values) */
                    case 'P':
                    case 'p':
                        felica_test_params.ID = FELICA_DTE_TestID_PerformanceTest;
                        /* Silent Mode runs only a single performance run*/
                        felica_test_params.Params.PerformanceTest.NrTests = (uint8_t)1;
                        felica_test_params.Params.PerformanceTest.ResultBuffer = &felica_result_buffer[0];
                        felica_test_params.Params.PerformanceTest.ResultBufferSize = felica_test_params.Params.PerformanceTest.NrTests;
                        felica_test_params.ProgressCB = NULL;

                        st = ptxFeliCa_DTE_RunTest(feliCaComp, &felica_test_params);
                        if (ptxStatus_Success == st)
                        {
                            (void)ptxDBGPORT_Write_Buffer(&felica_result_buffer[0], felica_test_params.Params.PerformanceTest.NrTests);
                        } else
                        {
                            ptxCommon_PrintF("1\n");
                        }
                        reset_uart_reception = (uint8_t)1;
                        break;

                    /* Help-Menu */
                    case '?':
                        ptxIOT_ShowHelp();
                        reset_uart_reception = (uint8_t)1;
                        break;

                    case ':':
                        if ((uart_rx_buffer[uart_rx_index - 1] == '\n') || (uart_rx_buffer[uart_rx_index - 1] == '\r'))
                        {
                            ptxIoT_ParseArgumentParams(&uart_rx_buffer[1], (uint8_t)(uart_rx_index - 2),  &arg_params);

                            is_invalid_param = 0;
                            arg_index = 0;
                            sequence_counter = 1;

                            /* FeliCa-DTE Performance Test with formatted output */
                            if (0 == strncmp("FP", &((char*)arg_params.Arguments[0].Arg)[0], arg_params.Arguments[0].ArgLen))
                            {
                                felica_test_params.ID = FELICA_DTE_TestID_PerformanceTest;
                                felica_test_params.Params.PerformanceTest.ResultBuffer = &felica_result_buffer[0];
                                felica_test_params.Params.PerformanceTest.ResultBufferSize = NR_FELICA_PERFORMANCE_TESTS;
                                felica_test_params.Params.PerformanceTest.NrTests = NR_FELICA_PERFORMANCE_TESTS;
                                felica_test_params.Params.PerformanceTest.Bitrate = FELICA_DTE_BITRATE_212;
                                felica_test_params.Params.PerformanceTest.TimeOutMS = (uint32_t)200;
                                felica_test_params.ProgressCB = NULL;
                                st = ptxFeliCa_DTE_EnableMode(feliCaComp, (uint8_t)1, &felica_test_params);

                                if (ptxStatus_Success == st)
                                {
                                    (void)memset(&felica_result_buffer[0], 0, RESULT_BUFFER_SIZE);
                                    st = ptxFeliCa_DTE_RunTest(feliCaComp, &felica_test_params);

                                    if (ptxStatus_Success == st)
                                    {
                                        ptxIoT_PrintFeliCaPerformanceResult(&felica_result_buffer[0], NR_FELICA_PERFORMANCE_TESTS);

                                        st = ptxFeliCa_DTE_EnableMode(feliCaComp, 0, NULL);

                                        if (ptxStatus_Success != st)
                                        {
                                            ptxCommon_PrintF("Error - FeliCa-DTE mode couldn't be disabled\n");
                                        }
                                    } else
                                    {
                                        ptxCommon_PrintF("Error - Performance-Test execution failed\n");
                                    }

                                } else
                                {
                                    ptxCommon_PrintF("Error - FeliCa-DTE mode couldn't be enabled\n");
                                }

                            /* STOP-command (only needed for PRBS) */
                            } else if (0 == strncmp("STOP", &((char*)arg_params.Arguments[0].Arg)[0], arg_params.Arguments[0].ArgLen))
                            {
                                st = ptxRF_Test_StopTest(rfTestComp);
                                ptxIOT_HandleStatus(st, 0U);

                            } else if ((0 == strncmp("PRBS9", &((char*)arg_params.Arguments[0].Arg)[0], arg_params.Arguments[0].ArgLen)) ||
                                       (0 == strncmp("PRBS15", &((char*)arg_params.Arguments[0].Arg)[0], arg_params.Arguments[0].ArgLen)))
                            {
                                /* PRBSx-command including parameters for RF-technology, -bitrate and flags  */
                                for (uint8_t i = 0; i < arg_params.NrArguments; i++)
                                {
                                    if (0 != is_invalid_param)
                                    {
                                        break;
                                    }

                                    switch (i)
                                    {
                                        case 0:
                                            if (0 == strncmp("PRBS9", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.ID = RF_TEST_ID_PRBS_9;

                                            } else if (0 == strncmp("PRBS15", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.ID = RF_TEST_ID_PRBS_15;

                                            } else
                                            {
                                                ptxCommon_PrintF("Unknown or unsupported RF-Test-Mode received\n");
                                                is_invalid_param = (uint8_t)1;
                                            }
                                            break;

                                        case 1:
                                            if (0 == strncmp("A", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Technology = RF_TEST_TECH_A;

                                            } else if (0 == strncmp("B", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Technology = RF_TEST_TECH_B;

                                            } else if (0 == strncmp("F", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Technology = RF_TEST_TECH_F;

                                            } else if (0 == strncmp("V", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Technology = RF_TEST_TECH_V;

                                            }
                                            else
                                            {
                                                ptxCommon_PrintF("Unknown or unsupported RF-Technology received\n");
                                                is_invalid_param = (uint8_t)1;
                                            }
                                            break;

                                        case 2:
                                            if (0 == strncmp("106", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Bitrate = RF_TEST_BITRATE_106;

                                            } else if (0 == strncmp("212", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Bitrate = RF_TEST_BITRATE_212;

                                            } else if (0 == strncmp("424", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Bitrate = RF_TEST_BITRATE_424;

                                            } else if (0 == strncmp("848", &((char*)arg_params.Arguments[i].Arg)[0], arg_params.Arguments[i].ArgLen))
                                            {
                                                rf_test_test_params.Params.PRBS.Bitrate = RF_TEST_BITRATE_848;

                                            }
                                            else
                                            {
                                                ptxCommon_PrintF("Unknown or unsupported RF-Technology received\n");
                                                is_invalid_param = (uint8_t)1;
                                            }
                                            break;

                                        case 3:
                                            rf_test_test_params.Params.PRBS.Flags = (uint8_t)((int)strtol(&((char*)arg_params.Arguments[i].Arg)[0], NULL, 16));
                                            break;

                                        default:
                                            /* not possible */;
                                            break;
                                    }
                                }

                                if (0 == is_invalid_param)
                                {
                                    st = ptxRF_Test_RunTest(rfTestComp, &rf_test_test_params);
                                    ptxIOT_HandleStatus(st, 0U);
                                }

                            } else if (0 == strncmp("FDP", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                            {
                                felica_test_params.ID = FELICA_DTE_TestID_RWDigProtTest;
                                arg_index++;

                                (void)memset(&felica_result_buffer[0], 0, RESULT_BUFFER_SIZE);

                                /* prepare additional test parameters */
                                felica_test_params.Params.RWDigProtTest.ResultBuffer = &felica_result_buffer[0];
                                felica_test_params.Params.RWDigProtTest.ResultBufferSize = RESULT_BUFFER_SIZE;
                                felica_test_params.Params.RWDigProtTest.TimeOutMS = (uint32_t)500;

                                /* use internal default values for T-On/-Off Guard Time for RF-Field - can be overwritten by setting these pointers */
                                felica_test_params.Params.RWDigProtTest.T_OffGuardTime = NULL;
                                felica_test_params.Params.RWDigProtTest.T_OnGuardTime = NULL;

                                /* assign generic buffer (Note: Optional - not needed if any other test gets executed) */
                                felica_test_params.Params.RWDigProtTest.GenericCmdBuffer = &felica_generic_cmd_buffer[0];

                                felica_test_params.ProgressCB = NULL;

                                st = ptxFeliCa_DTE_EnableMode(feliCaComp, (uint8_t)1, &felica_test_params);

                                if (ptxStatus_Success == st)
                                {
                                    ptxCommon_PrintF("FeliCa-DTE Command Processor started\n");

                                    while ((arg_index < arg_params.NrArguments) && (ptxStatus_Success == st) && (0 == is_invalid_param))
                                    {
                                        if (0 == strncmp("eMoney1", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: eMoney1\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant1;
                                            arg_index++;

                                        } else if (0 == strncmp("eMoney2", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: eMoney2\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_eMoney_Variant2;
                                            arg_index++;

                                        } else if (0 == strncmp("eMoneyNFC", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: eMoneyNFC\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_eMoney_NFCDEP;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure1", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 1\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure1;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure2", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 2\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure2;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure3", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 3\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure3;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure4", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 4\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure4;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure5", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 5\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure5;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure6", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)15))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure 6\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure6;
                                            arg_index++;

                                        } else if (0 == strncmp("FrameStructure", &((char*)arg_params.Arguments[arg_index].Arg)[0], (size_t)14))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Frame-Structure\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_FrameStructure;
                                            arg_index++;

                                        } else if (0 == strncmp("SetPOSEncoding", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Set DP-Board Positive Encoding\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_Setup_DP_POS;
                                            arg_index++;

                                        } else if (0 == strncmp("SetNEGEncoding", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Set DP-Board Negative Encoding\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_Setup_DP_NEG;
                                            arg_index++;

                                        } else if (0 == strncmp("Generic", &((char*)arg_params.Arguments[arg_index].Arg)[0], arg_params.Arguments[arg_index].ArgLen))
                                        {
                                            ptxCommon_PrintF("Sub-Test-ID: Generic Command\n");
                                            felica_test_params.Params.RWDigProtTest.SubTestID = FELICA_DTE_SubTestID_RWDigProt_Generic;
                                            arg_index++;

                                            /* Generic-Sequence requires 2 additional parameters: Length + Actual Sequence */
                                            if ((arg_params.NrArguments - arg_index) >= 2)
                                            {
                                                felica_test_params.Params.RWDigProtTest.GenericCmdBufferLen = (uint32_t)ptxIoT_ATOI(&((char*)arg_params.Arguments[arg_index].Arg)[0]);
                                                arg_index++;

                                                /* clear Generic-Buffer upfront */
                                                (void)memset(&felica_generic_cmd_buffer[0], 0, GENERIC_BUFFER_SIZE);

                                                int ret_val = ptxIoT_ATOHEX(&((char*)arg_params.Arguments[arg_index].Arg)[0], felica_test_params.Params.RWDigProtTest.GenericCmdBufferLen, GENERIC_BUFFER_SIZE, &felica_generic_cmd_buffer[0]);

                                                if (0 == ret_val)
                                                {
                                                    arg_index++;

                                                } else
                                                {
                                                    ptxCommon_PrintF("Invalid Generic-Sequence Syntax detected\n");
                                                    is_invalid_param = (uint8_t)1;
                                                }

                                            } else
                                            {
                                                ptxCommon_PrintF("Invalid Generic-Sequence Syntax detected\n");
                                                is_invalid_param = (uint8_t)1;
                                            }
                                        } else
                                        {
                                            ptxCommon_PrintF("Unknown or unsupported FeliCa Digital-Protocol Sub-Testmode received\n");
                                            is_invalid_param = (uint8_t)1;
                                        }

                                        if (0 == is_invalid_param)
                                        {
                                            /* reset result buffer size */
                                            felica_test_params.Params.RWDigProtTest.ResultBufferSize = RESULT_BUFFER_SIZE;
                                            (void)memset(&felica_result_buffer[0], 0, RESULT_BUFFER_SIZE);

                                            st = ptxFeliCa_DTE_RunTest(feliCaComp, &felica_test_params);

                                            /* print result-buffer regardless of the test-result */
                                            ptxIoT_PrintFeliCaDigitalProtocolSequences(&felica_result_buffer[0], RESULT_BUFFER_SIZE, &sequence_counter);

                                            (void)ptxPLAT_Sleep(feliCaComp->IoTRdComp->Plat, (uint32_t)5);
                                        }
                                    }

                                    if (ptxStatus_Success == st)
                                    {
                                        ptxCommon_PrintF("Digital-Protocol-Test execution OK\n");

                                    } else
                                    {
                                        ptxCommon_PrintF("Error - Digital-Protocol-Test execution failed\n");
                                    }

                                    ptxCommon_PrintF("FeliCa-DTE Command Processor stopped\n");

                                    st = ptxFeliCa_DTE_EnableMode(feliCaComp, 0, NULL);

                                    if (ptxStatus_Success != st)
                                    {
                                        ptxCommon_PrintF("Error - FeliCa-DTE mode couldn't be disabled\n");
                                    }
                                } else
                                {
                                    ptxCommon_PrintF("Error - FeliCa-DTE mode couldn't be enabled\n");
                                }
                            } else
                            {
                                ptxCommon_PrintF("Unknown or unsupported Command received\n");
                            }

                            reset_uart_reception = (uint8_t)1;
                        }
                        break;

                    default:
                        reset_uart_reception = (uint8_t)1;
                        break;
                }

            }

            if (0 != reset_uart_reception)
            {
                ptxDBGPORT_Reset_FIFO_Level();
                uart_rx_index = 0;
                (void)memset(&uart_rx_buffer[0], 0, UART_RXBUFFER_SIZE);
                (void)memset(&arg_params, 0, sizeof(ptxTestAPI_ArgumentParams_t));
                reset_uart_reception = 0;
            }
        }
    }
}

static void ptxIOT_ShowHelp()
{
    ptxCommon_PrintF("============================================================================================================================================\n");
    ptxCommon_PrintF("Supported Commands:                                                                                                                         \n");
    ptxCommon_PrintF("- FeliCa-DTE Silent Mode                                                                                                                    \n");
    ptxCommon_PrintF("-- 'E' or 'e': Enables FeliCa-DTE Mode (Performance Tests), returns '0' (OK) or '1' (Error)                                                                     \n");
    ptxCommon_PrintF("-- 'P' or 'p': Runs single performance Test, returns performance test result ('O' or '.') or '1' (Error)                                    \n");
    ptxCommon_PrintF("-- 'E' or 'e': Disables FeliCa-DTE Mode (Performance Tests), returns '0' (OK) or '1' (Error)                                                                    \n");
    ptxCommon_PrintF("============================================================================================================================================\n");
    ptxCommon_PrintF("- Standard Command for FeliCa-DTE Performance Test (100 Tests)                                                                              \n");
    ptxCommon_PrintF("-- \":FP<LF>\"                                                                                                                              \n");
    ptxCommon_PrintF("- Standard Command for FeliCa-DTE Reader/Writer Digital Protocol Requirements                                                               \n");
    ptxCommon_PrintF("-- \":FDP, [Sub-Test-ID]<LF>\"                                                                                                              \n");
    ptxCommon_PrintF("--- Sub-Test-ID:........: \"eMoney1\", \"eMoney2\", \"eMoneyNFC\", \"FrameStructure\", \"FrameStructure[1-6]\", \"SetPOSEncoding\", \"SetNEGEncoding\" or \"Generic\"\n");
    ptxCommon_PrintF("- Standard Command for PRBSxx Tests                                                                                                         \n");
    ptxCommon_PrintF("-- \":[Test-Name],[RF-Technology],[RF-Bitrate],[Flags]<LF>\"                                                                                \n");
    ptxCommon_PrintF("--- Test-Name...........: \"PRBS9\", \"PRBS15\" or \"STOP\"                                                                                 \n");
    ptxCommon_PrintF("--- RF-Technology.......: \"A\", \"B\", \"F\" or \"V\"                                                                                      \n");
    ptxCommon_PrintF("--- RF-Bitrate..........: \"106\", \"212\", \"424\" or \"848\"                                                                              \n");
    ptxCommon_PrintF("--- Flags(2-digit Hex.).: \"00\" or \"01\" (enable Parity-Generation for A)                                                                 \n");
    ptxCommon_PrintF("Example(s):                                                                                                                                 \n");
    ptxCommon_PrintF("\":FP<LF>\"                   // performs the FeliCa-DTE Performance Test 100 times                                                         \n");
    ptxCommon_PrintF("\":PRBS9,B,106,01<LF>\"       // start PRBS9 test using technology B @106 kBit/s with flags (bit 0) set                                     \n");
    ptxCommon_PrintF("============================================================================================================================================\n");
}

static void ptxIOT_HandleStatus(ptxStatus_t st, uint8_t isSilentMode)
{
    volatile ptxStatus_t tmp_status = st;

    if (0 != isSilentMode)
    {
        if (ptxStatus_Success == tmp_status)
        {
            ptxCommon_PrintF("0\n");
        } else
        {
            ptxCommon_PrintF("1\n");
        }
    } else
    {
        if (ptxStatus_Success == tmp_status)
        {
            ptxCommon_PrintF("OK\n");
        } else
        {
            ptxCommon_PrintF("ERROR\n");
        }
    }
}

static void ptxIoT_ParseArgumentParams(uint8_t *argBuffer, uint8_t argBufferLen, ptxTestAPI_ArgumentParams_t * argParams)
{
    uint8_t *p = argBuffer;
    uint8_t arg_len = argBufferLen;
    uint8_t arg_pending = 0;

    while (arg_len)
    {
        if (*p != ',')
        {
            if (argParams->Arguments[argParams->NrArguments].ArgLen < PTX_TEST_API_MAX_ARG_LEN)
            {
                argParams->Arguments[argParams->NrArguments].Arg[argParams->Arguments[argParams->NrArguments].ArgLen] = *p;
                argParams->Arguments[argParams->NrArguments].ArgLen++;
            }
            else
            {
                ptxCommon_PrintF("ERROR - Invalid Argument/Parameter Format detected\n");
                return;
            }

            arg_pending = 1;
        }
        else
        {
            if (*p == ',')
            {
                argParams->NrArguments++;
                arg_pending = 0;
            }
        }

        p++;
        arg_len--;
    }

    if (0 != arg_pending)
    {
        argParams->NrArguments++;
    }
}

static void ptxIoT_PrintFeliCaPerformanceResult (uint8_t *resultBuffer, uint8_t nrResults)
{
    const int RESULT_LENGTH = (uint32_t)50;

    int total_nr_pass_tests = 0;
    int current_nr_pass_tests = 0;
    int total_nr_processed_tests = 0;
    int current_nr_processed_tests = 0;
    int nr_tests = nrResults;

    if ((NULL != resultBuffer) && (0 != nrResults))
    {
        ptxCommon_PrintF("\n\nPTX Polling Checker ver.1.xx\n");

        for (int i = 0; i < nrResults; i++)
        {
            ptxCommon_PrintF("%c", resultBuffer[i]);

            if ((char)resultBuffer[i] == 'O')
            {
                total_nr_pass_tests++;
                current_nr_pass_tests++;
            }
            else if ((char)resultBuffer[i] == '.')
            {
                // ignore fail - only PASS-info relevant
            }
            else
            {
                /* quit loop */
                break;
            }

            current_nr_processed_tests++;
            total_nr_processed_tests++;

            if ((current_nr_processed_tests == RESULT_LENGTH) || (total_nr_processed_tests == nr_tests))
            {
                if (total_nr_processed_tests == nr_tests)
                {
                    int ceiled_nr_lines = 0;

                    do
                    {
                        ceiled_nr_lines += RESULT_LENGTH;

                    } while (ceiled_nr_lines < nr_tests);

                    for (int j = 0; j < (ceiled_nr_lines - nr_tests); j++)
                    {
                        ptxCommon_PrintF(" ");
                    }
                }

                ptxCommon_PrintF(" [");
                ptxCommon_PrintF("%3d", current_nr_pass_tests);
                ptxCommon_PrintF("/");
                ptxCommon_PrintF("%3d", current_nr_processed_tests);
                ptxCommon_PrintF("]");
                ptxCommon_PrintF(" ");
                ptxCommon_PrintF("%3d", (( 100 / current_nr_processed_tests) * current_nr_pass_tests));
                ptxCommon_PrintF("%%");
                ptxCommon_PrintF("\n");
                current_nr_processed_tests = 0;
                current_nr_pass_tests = 0;
            }
        }

        if (0 != total_nr_processed_tests)
        {
            ptxCommon_PrintF("(Total) [");
            ptxCommon_PrintF("%3d", total_nr_pass_tests);
            ptxCommon_PrintF("/");
            ptxCommon_PrintF("%3d", total_nr_processed_tests);
            ptxCommon_PrintF("]... ");
            /* Note:
             * The following percentage number is actually of data type float. Support for float-operations
             * are dependent on target-system and needs to be adapted in case needed */
            ptxCommon_PrintF("%3d", ( 100 / total_nr_processed_tests) * total_nr_pass_tests);
            ptxCommon_PrintF("%%");
            ptxCommon_PrintF("\n\n\n");
        } else
        {
            ptxCommon_PrintF("No Test-Results available\n");
        }

    } else
    {
        ptxCommon_PrintF("Invalid result parameters detected\n");
    }

    if (total_nr_processed_tests < nrResults)
    {
        ptxCommon_PrintF("Invalid result format detected\n");
    }

    (void)resultBuffer;
    (void)nrResults;
}

static void ptxIoT_PrintFeliCaDigitalProtocolSequences (uint8_t *resultBuffer, uint32_t nrResults, int *sequenceCounter)
{
    uint32_t index = 0;
    uint8_t quit_processing = 0;
    uint32_t sequence_len;

    if ((NULL != resultBuffer) && (0 != nrResults) && (NULL != sequenceCounter))
    {
        /* Note:
         * "resultBuffer" structured as follows: Command-0 (+ prepended length), Response-0 (+ prepended length), Command-1, ...
         */
        while (0 == quit_processing)
        {
            if ((index + 1U) <= nrResults)
            {
                if (0 != resultBuffer[index])
                {
                    sequence_len = (uint32_t)resultBuffer[index];

                    if ((index + sequence_len) <= nrResults)
                    {
                        if (0 != (*sequenceCounter % 2))
                        {
                            ptxCommon_PrintF("%02d TX Sequence = ", *sequenceCounter);

                        } else
                        {
                            ptxCommon_PrintF("%02d RX Sequence = ", *sequenceCounter);
                        }

                        ptxCommon_Print_Buffer(&resultBuffer[0], index, sequence_len, 1U, 0);

                        index += sequence_len;
                        (*sequenceCounter)++;

                    } else
                    {
                        quit_processing = 1U;
                    }
                } else
                {
                    /* no more sequences stored - processing done */
                    quit_processing = 1U;
                }
            } else
            {
                /* no more sequences stored - processing done */
                quit_processing = 1U;
            }
        }
    } else
    {
        ptxCommon_PrintF("Invalid result format detected\n");
    }
}

static int ptxIoT_ATOI(char * input)
{
     int i = 0;
     int int_value = 0;

     while(input[i] != '\0')
     {
         int_value = (int_value * 10) + (input[i] - '0');
         i++;
     }

     return int_value;
}

static int ptxIoT_ATOHEX(char *input, uint32_t inputLen, uint32_t maxInputLen, uint8_t *output)
{
    int ret_val = 0;

    char *pos = input;
    size_t count = 0;

    if ((input[0] == '\0') || (strlen(input) % 2) || (inputLen > maxInputLen))
    {
        ret_val = -1;
    }

    if (0 == ret_val)
    {
        for(count = 0; count < inputLen; count++)
        {
            char buf[3] = {pos[0], pos[1], 0};
            output[count] = (uint8_t)((int)strtol(buf, NULL, 16));
            pos += 2 * sizeof(char);
        }
    }

    return ret_val;
}

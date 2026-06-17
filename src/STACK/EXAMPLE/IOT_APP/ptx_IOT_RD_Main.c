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
    Module      : IOT_READER Demo
    File        : ptx_IOT_RD_Main.c

    Description : IoT Reader demo application for PTX1xxR NFC Platform.

    The demo provides the user with the possibility to evaluate the PTX1xxR Reader
    platform for IoT applications, running on non-OS based host controller.

    Requirements for the successful start of the application:
     1) PTX1xxR NFC board (further referenced as "NFC Board") connected via SPI to a host controller platform without operating system
        - Supported Boards:
          -- "PTX100R-EB-ST-QFN56-POS/IoT"
          -- "PTX105R-EB-ST-QFN56-POS/IoT"
          -- "PTX105R-DB-RB-QFN56-IoT QC"
     2) PTX1xxR SW stack

    The steps to start the evaluation:
     1) Connect NFC Board to power supply

     2) Connect NFC Board to the SPI interface.

     3) Download .elf binary to RA4M2-board and reset the MCU board.

     4) In case of successful start, program starts running in an endless card-discovery loop.

     5) Continue with the demo by placing different cards near board antenna.
        The messages are printed out in debug port if debug port is enabled.

        *** NOTE:   Debug port is not provided in project source files.
        ***         It is up to the user to implement debug port functionality e.g. via UART/USB or similar.

        When a card is detected, it performs a single example data exchange depending on the card type and protocol.

        Example Data Exchange for T3T / FeliCa => Read Block 0
            Card activated ... OK!
            01. RF-Technology = Type-F; SENSF_RES: 1201012E3D23BA0BA14100F1000000014300; Protocol....: T3T
            =========== DATA EXCHANGE ================
            TX = 06012E3D23BA0BA14100F1010900018000
            RX = 07012E3D23BA0BA141FFA100
            ==========================================

        Example Data Exchange for T2T / Mifare => Read Block 0
            Card activated ... OK!
            01. RF-Technology = Type-A; SENS_RES: 4400; NFCID1_LEN: 07; NFCID1: 0489FF02E53F80; SEL_RES: 00; Protocol: T2T
            =========== DATA EXCHANGE ================
            TX = 3000
            RX = 0489FFFA02E53F8058480000E110120000
            ==========================================

        Example Data Exchange for T4T / ISO-DEP.B => Select PPSE APDU
            Card activated ... OK!
            01. RF-Technology = Type-B; SENSB_RES: 5057ABD7DC0000000080817100; Protocol....: ISO-DEP; ATTRIB_RES: 00
            =========== DATA EXCHANGE ================
            TX = 00A404000E325041592E5359532E444446303100
            RX = 00B20104009000
            ==========================================

        Example Data Exchange for T5T / ISO 15693 => Read Block 0
            Card activated ... OK!
            01. RF-Technology = Type-V; DSFID: 00; RES_FLAGS: 00; UID: E0 04 01 50 96 13 9B 04 ; Protocol: T5T
            =========== DATA EXCHANGE ================
            TX = 2220049B1396500104E000
            RX = 000000000000
            ==========================================

        If there are more than one card placed into RF field, the card specific infos are printed as a list in the
        terminal. The following example shows the output for 3 T5T cards (Type-V, ISO 15693).

            Multiple Card(s) detected - resolved ... OK!
            01. RF-Technology = Type-V; DSFID: 00; RES_FLAGS: 00; UID: E0 04 01 50 96 13 3F 72
            02. RF-Technology = Type-V; DSFID: 00; RES_FLAGS: 00; UID: E0 04 01 50 96 13 9B 04
            03. RF-Technology = Type-V; DSFID: 00; RES_FLAGS: 00; UID: E0 04 01 08 0A 1B 25 D6
            Selecting first detected card/protocol (RF-Protocol == T5T)...  ... OK!
            01. RF-Technology = Type-V; DSFID: 00; RES_FLAGS: 00; UID: E0 04 01 50 96 13 3F 72 ; Protocol: T5T
            =========== DATA EXCHANGE ================
            TX = 2220723F1396500104E000
            RX = 000000000000
            ==========================================

        The demo application then automatically selects the first card / protocol in the registry and performs
        again the example data exchange with the activated / selected card.

        Note: When using Card-Technology Type-A, it is possible that the SEL_RES-byte (also referenced as SAK according to ISO 14443-3A)
              reports support for ISO-DEP and the NFC-DEP protocol (SEL_RES / SAK = 0x6x).
              In this case, the single remote device is handled as if there were multiple devices i.e. in this case
              protocols.

              An example output for this particular case is shown below:

                Multiple Card(s) detected - resolved ... OK!
                01. RF-Technology = Type-A; SENS_RES: 0803; NFCID1_LEN: 04; NFCID1: 01020304; SEL_RES: 60
                Selecting first detected card/protocol (RF-Protocol == NFC_DEP)...  ... OK!
                01. RF-Technology = Type-A; SENS_RES: 0803; NFCID1_LEN: 04; NFCID1: 01020304; SEL_RES: 60; Protocol: NFC-DEP; ATR_RES: 26D50101FE83DC567B35DF0000000000073246666D010112020207FF03020013040164070103
                =========== DATA EXCHANGE ================
                TX = 0000
                RX = 0000
                ==========================================

              The demo application selects NFC_DEP as first found protocol and tries to activate it (incl. support for LLCP).
              The following data exchange performs a SYMM-block (0x0000) exchange at LLCP-level (on top of NFC-DEP).

              Alternatively, the functions from Native-Tag and NDEF API can be used as well to exchange RF-data.

     6) Exit the loop and the application by ....
*/


#include "user_board_utils.h"
#include "user_cli.h"

#include <string.h>
#include <STACK/COMPS/COMMON/ptxCOMMON.h>
#include <STACK/COMPS/COMMON/ptxIoTRd_COMMON.h>
#include <STACK/COMPS/COMMON/ptxT4T.h>
#include <STACK/COMPS/IOT_READER/ptx_IOT_READER.h>
#include <STACK/COMPS/NATIVE_TAG/ptxNativeTag_T5T.h>
#include <STACK/COMPS/NDEF/ptxNDEF.h>
#include <STACK/COMPS/NDEF/ptxNDEF_T2TOP.h>
#include <STACK/COMPS/NDEF/ptxNDEF_T3TOP.h>
#include <STACK/COMPS/NDEF/ptxNDEF_T4TOP.h>
#include <STACK/COMPS/NDEF/ptxNDEF_T5TOP.h>
#include <STACK/COMPS/PLAT/ptxPLAT.h>
#include <STACK/EXAMPLE/COMMON/ptxHCE_Loopback.h>
#include <STACK/EXAMPLE/IOT_APP/ptx_IOT_RD_Main.h>

/*
 * ####################################################################################################################
 * DEFINES / TYPES
 * ####################################################################################################################
 */

/*
 * Example Code-Delays/-Sleeps; used for better readability of exchanges RF-data on the console application
 * Can be disabled by defining/setting compile-switch "PTX_DISABLE_EXAMPLE_DELAYS" globally
 */
#define PTX_IOTRD_EXCHANGE_WAIT_TIME            (10u)

/**
 * Comment/Uncomment the following line to use NDEF-operations instead of the raw RF data-exchanges or commands from the NativeTag-API
 */
//#define USE_NDEF

#define NDEF_BUFFER_SIZE                        (256u)                          /**< NDEF-Buffer. Length */

/**
 * Default timeout-values for for RAW-protocols (e.g. T2T, T3T, ...) and standard-protocols (ISO-/NFC-DEP)
 */
#define DEFAULT_APP_TIMEOUT_RAW                 (uint32_t)200                 /**< Application-timeout for raw-protocols */
#define DEFAULT_APP_TIMEOUT_PROT                (uint32_t)50000               /**< Application-timeout for standard-protocols */

/**
 * Default NDEF-File
 */
#if defined(USE_PTX_IOTRD_DEMO)
static uint8_t NDEF_FILE_TEMPLATE[] = {
        0xD1, 0x01, 0x10, 0x55, 0x01, 0x70, 0x61, 0x6E,
        0x74, 0x68, 0x72, 0x6F, 0x6E, 0x69, 0x63, 0x73,
        0x2E, 0x63, 0x6F, 0x6D};
static uint16_t NDEF_FILE_TEMPLATE_SIZE = 20;
#endif

/*
 * ####################################################################################################################
 * INTERNALS
 * ####################################################################################################################
 */

/*
 * ####################################################################################################################
 * LOCAL INTEGRATION FUNCTIONS / HELPERS
 * ####################################################################################################################
 */
/*
 * Main demo application loop. Implements NFC-Forum polling and example data exchanges.
 * It is called only if stack components and NFC hardware have been successfully initialized prior to this.
 */
#if defined(USE_PTX_IOTRD_DEMO)
static void ptxIoTRdInt_Run_Demo_Loop(ptxIoTRd_t *iotRd, ptxT4T_t *t4t);
#endif

/*
 * Function representing demo state "data exchange" when NDEF should be used
 */
ptxStatus_t ptxIoTRdInt_DemoState_DataExchange(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *skipTxDataExchange, uint8_t *skipRxProcessing,
                                               ptxNativeTag_T5T_t* t5tComp, ptxNativeTag_T5T_InitParams_t* t5tInitParams, ptxNDEF_T2TOP_t* t2tOpComp, ptxNDEF_T2TOP_InitParams_t* t2tOpInitParams,
                                               ptxNDEF_T3TOP_t* t3tOpComp, ptxNDEF_T3TOP_InitParams_t* t3tOpInitParams, ptxNDEF_T4TOP_t* t4tOpComp, ptxNDEF_T4TOP_InitParams_t* t4tOpInitParams,
                                               ptxNDEF_T5TOP_t* t5tOpComp, ptxNDEF_T5TOP_InitParams_t* t5tOpInitParams, ptxNDEF_t* ndefComp, ptxNDEF_InitParams_t* ndefInitParams);

static void ptxIoTRdInt_Print_Revision_Info(ptxIoTRd_t *iotRd);

/*
 * ####################################################################################################################
 * APPLICATION MAIN
 * ####################################################################################################################
 */
int ptxAPP_Entry(void)
{
    ptxIOT_READER_App();
    return 1;
}

/*
 * \brief   Start of IoT Reader application.
 *
 * \return      none
 */
void ptxIOT_READER_App(void)
{
    ptxStatus_t st = ptxStatus_Success;

    uint8_t start_temperature_sensor_calibration = 1;

    /* IoT Reader Context. */
    ptxIoTRd_t iotRd;
    (void)memset(&iotRd, 0, sizeof(ptxIoTRd_t));

    /* RF-Discover configuration */
    ptxIoTRd_DiscConfig_t rf_disc_config;
    (void)memset(&rf_disc_config, 0, sizeof(ptxIoTRd_DiscConfig_t));

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
        /* Calibration required. Set ambient temperature and expected shutdown temperature threshold. */
        tempSens.Calibrate = 1;
        tempSens.Tambient = 25;
        tempSens.Tshutdown = 100;

        /* Calibration will take place now. Doesn´t need to be done anymore if ptxPOS_Init is successful. */
        start_temperature_sensor_calibration = 0;
    } else
    {
        /* Calibration not needed, it has already been done. Provide compensated threshold temperature word. */
        tempSens.Tshutdown = 223u;
    }

    /* Initial parameters for temperature sensor are ready. */
    initParams.TemperatureSensor = &tempSens;
    initParams.ComInterface = &comIntf;

    // ILAN: NEW ADD
    RM_NFC_READER_PTX_Open(&g_nfc_reader_ptx0_ctrl, &g_nfc_reader_ptx0_cfg);

    /* Initiate IoT-Reader System. */
    st = ptxIoTRd_Init(&iotRd, &initParams);

    if (ptxStatus_Success == st)
    {
        /* Initialization complete */
        g_ioport.p_api->pinWrite(g_ioport.p_ctrl, LED_IOT_RD, LED_ACTIVE);

        ptxCommon_PrintF("System Initialization ... OK\n");

        /* Print available revisions */
        ptxIoTRdInt_Print_Revision_Info(&iotRd);

#if defined(USE_PTX_IOTRD_DEMO)
        /*
         * Initiate polling for Type-A, -B, -F and -V cards.
         * Note: Parameter can also be set to NULL -> internal default values are used
         */
        rf_disc_config.PollTypeA    = 1u;
        rf_disc_config.PollTypeB    = 1u;
        rf_disc_config.PollTypeF212 = 1u;
        rf_disc_config.PollTypeV    = 1u;
        rf_disc_config.ListenTypeA  = 1u;
        rf_disc_config.IdleTime     = 100u;

        st = ptxIoTRd_Initiate_Discovery (&iotRd, &rf_disc_config);

        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("Start of RF-Discovery ... OK\n");

            ptxT4T_InitParams_t T4TInitParams;
            T4TInitParams.DefaultNDEFMessage = NDEF_FILE_TEMPLATE;
            T4TInitParams.DefaultNDEFMessageLength = NDEF_FILE_TEMPLATE_SIZE;

            ptxT4T_t t4tComp;
            ptxT4T_Init(&t4tComp, &T4TInitParams);


            /* Demo IoT discovery loop. */
            ptxIoTRdInt_Run_Demo_Loop(&iotRd, &t4tComp);

            ptxT4T_DeInit(&t4tComp);
        } else
        {
            ptxCommon_PrintF("Start of RF-Discovery ... ERROR\n");
        }
#elif defined(USE_PTX_HCE_LOOPBACK_DEMO)
        rf_disc_config.ListenTypeA = 1u;
        rf_disc_config.PollTypeA    = 0u;
        rf_disc_config.PollTypeB    = 0u;
        rf_disc_config.PollTypeF212 = 0u;
        rf_disc_config.PollTypeF424 = 0u;
        rf_disc_config.PollTypeV    = 0u;

        rf_disc_config.IdleTime    = 100u;

        st = ptxIoTRd_Initiate_Discovery (&iotRd, &rf_disc_config);

        if (ptxStatus_Success == st)
        {
            ptxCommon_PrintF("Start of RF-Discovery ... OK\n");

            ptxHce_Loopback_Demo(&iotRd.Hce);
        } else
        {
            ptxCommon_PrintF("Start of RF-Discovery ... ERROR\n");
        }

#else
#error("Neither POS, nor HCE demo activated!")
#endif

        /* Deactivate the Reader. */
        (void) ptxIoTRd_Reader_Deactivation (&iotRd, PTX_IOTRD_RF_DEACTIVATION_TYPE_IDLE);
    } else
    {
        ptxCommon_PrintF("System Initialization ... ERROR (Status-Code = 0x%04X, Comp = %u, St = %u)\n",
                         st, (unsigned)((st >> 8) & 0xFFu), (unsigned)(st & 0xFFu));
    }

    /* Clean up: de-initialize IOT Reader L1 System. */
    (void)ptxIoTRd_Deinit(&iotRd);
}


/*
 * ####################################################################################################################
 * MAIN DEMO LOOP
 * ####################################################################################################################
 */

#if defined(USE_PTX_IOTRD_DEMO)
static void ptxIoTRdInt_Run_Demo_Loop(ptxIoTRd_t *iotRd, ptxT4T_t *t4t)
{
    uint8_t exit_loop = 0;

    ptxIotRdInt_Demo_State_t demo_state = IoTRd_DemoState_WaitForActivation;

    uint8_t system_state = PTX_SYSTEM_STATUS_OK;
    uint8_t last_rf_error = PTX_RF_ERROR_NTF_CODE_NO_ERROR;

    ptxIoTRd_CardRegistry_t *card_registry = NULL;

    ptxStatus_t st = ptxStatus_Success;
    uint8_t skip_tx_data_exchange;
    uint8_t skip_rx_processing;

    /* Native-Tag Component for T5T */
    ptxNativeTag_T5T_t t5t_comp;
    ptxNativeTag_T5T_InitParams_t t5t_init_params;

    /* NDEF-OP Component T2T */
    ptxNDEF_T2TOP_t t2top_comp;
    ptxNDEF_T2TOP_InitParams_t t2top_init_params;

    /* NDEF-OP Component T3T */
    ptxNDEF_T3TOP_t t3top_comp;
    ptxNDEF_T3TOP_InitParams_t t3top_init_params;

    /* NDEF-OP Component T4T */
    ptxNDEF_T4TOP_t t4top_comp;
    ptxNDEF_T4TOP_InitParams_t t4top_init_params;

    /* NDEF-OP Component T5T */
    ptxNDEF_T5TOP_t t5top_comp;
    ptxNDEF_T5TOP_InitParams_t t5top_init_params;

    /* Generic NDEF-OP Component (Tag-independent) */
    ptxNDEF_t ndef_comp;
    ptxNDEF_InitParams_t ndef_init_params;

    if (ptxStatus_Success == st)
    {
        /* get reference to the internal card registry */
        (void)ptxIoTRd_Get_Card_Registry (iotRd, &card_registry);

        if (NULL == card_registry)
        {
            exit_loop = 1u;
        }
    } else
    {
        exit_loop = 1u;
    }

    if (0 == exit_loop)
    {
        ptxCommon_PrintF("Entering Application-Mode ... OK\n");
        ptxCommon_PrintF("Waiting for discovered Cards or external Fields ...\n");
    } else
    {
        ptxCommon_PrintF("Entering Application-Mode ... ERROR\n");
    }

    /*
     * Perform loop endlessly
     */
    while (0 == exit_loop)
    {
        /* Service the cooperative UART CLI between every demo iteration. */
        UserCli_Poll();

        /* check regularly for critical system errors */
        st = ptxIoTRd_Get_Status_Info (iotRd, StatusType_System, &system_state);

        if (PTX_SYSTEM_STATUS_OK != system_state)
        {
            /* Handle system-error */
            demo_state = IoTRd_DemoState_SystemError;
        } else
        {
            if (ptxStatus_Success != st)
            {
                demo_state = IoTRd_DemoState_DeactivateReader;
            }
        }

        /* check optionally if PA current-limiter got activated */
        (void)ptxIoTRd_Get_Status_Info (iotRd, StatusType_LastRFError, &last_rf_error);

        if (PTX_RF_ERROR_NTF_CODE_WARNING_PA_OVERCURRENT_LIMIT == last_rf_error)
        {
            ptxCommon_PrintF("Warning - PA Overcurrent Limiter activated!\n");
        }
        if (((RF_DISCOVER_STATUS_LISTEN_A == iotRd->DiscoverState) || (0 != iotRd->Hce.EventQ.NrOfEntries)) && (PTX_SYSTEM_STATUS_OK == system_state))
        {
            demo_state = IoTRd_DemoState_HostCardEmulation;
        }

        switch (demo_state)
        {
            case IoTRd_DemoState_WaitForActivation:
                ptxIoTRdInt_DemoState_WaitForActivation(iotRd, card_registry, &demo_state);
                break;

            case IoTRd_DemoState_SelectCard:
                UserBoardUtils_SetStatusLed(BSP_IO_LEVEL_HIGH);
                st = ptxIoTRdInt_DemoState_SelectCard(iotRd, card_registry, &demo_state, &exit_loop);
                break;

            case IoTRd_DemoState_DataExchange:
                UserBoardUtils_SetStatusLed(BSP_IO_LEVEL_HIGH);
                skip_tx_data_exchange = 0;
                skip_rx_processing = 0;

                st = ptxIoTRdInt_DemoState_DataExchange(iotRd, card_registry, &demo_state, &skip_tx_data_exchange,
                                                        &skip_rx_processing, &t5t_comp, &t5t_init_params, &t2top_comp,
                                                        &t2top_init_params, &t3top_comp, &t3top_init_params, &t4top_comp,
                                                        &t4top_init_params, &t5top_comp, &t5top_init_params, &ndef_comp,
                                                        &ndef_init_params);
                break;

            case IoTRd_DemoState_HostCardEmulation:
                st = ptxIoTRdInt_DemoState_HostCardEmulation(&demo_state, &iotRd->Hce, t4t);
                break;

            case IoTRd_DemoState_DeactivateReader:
                UserBoardUtils_SetStatusLed(BSP_IO_LEVEL_LOW);
                st = ptxIoTRdInt_DemoState_DeactivateReader(iotRd, &demo_state, &exit_loop);
                break;

            case IoTRd_DemoState_SystemError:
                UserBoardUtils_SetStatusLed(BSP_IO_LEVEL_LOW);
                ptxIoTRdInt_DemoState_SystemError(iotRd, card_registry, &demo_state, &system_state);
                break;

            default:
                break;
        }
    }

    (void)ptxNativeTag_T5TClose(&t5t_comp);
#ifdef USE_NDEF
    (void)ptxNDEF_T2TOpClose(&t2top_comp);
    (void)ptxNDEF_T3TOpClose(&t3top_comp);
    (void)ptxNDEF_T4TOpClose(&t4top_comp);
    (void)ptxNDEF_T5TOpClose(&t5top_comp);
    (void)ptxNDEF_Close(&ndef_comp);
#endif
}
#endif


/*
 * ####################################################################################################################
 * CARD INFO & RECORD READER
 * ####################################################################################################################
 */
#if defined(USE_PTX_IOTRD_DEMO) && !defined(USE_NDEF)

/* Lightweight BER-TLV search (1- or 2-byte tags, multi-byte length, recursive into constructed). */
static inline uint8_t ptxIoTRdInt_TlvFind(const uint8_t *buf, uint32_t len, uint16_t tag,
                                   const uint8_t **val, uint32_t *valLen)
{
    uint32_t i = 0;
    while (i < len)
    {
        if ((0x00u == buf[i]) || (0xFFu == buf[i])) { i++; continue; }
        uint16_t ct = (uint16_t)buf[i];
        uint8_t  constr = (uint8_t)(buf[i] & 0x20u);
        i++;
        if (((ct & 0x1Fu) == 0x1Fu) && (i < len)) { ct = (uint16_t)((ct << 8) | buf[i]); i++; }
        if (i >= len) break;
        uint32_t cl = (uint32_t)buf[i]; i++;
        if (0u != (cl & 0x80u)) { uint8_t n = (uint8_t)(cl & 0x7Fu); cl = 0; while (n-- && (i < len)) { cl = (cl << 8) | buf[i]; i++; } }
        if ((i + cl) > len) break;
        if (ct == tag) { *val = &buf[i]; *valLen = cl; return 1u; }
        if (constr && ptxIoTRdInt_TlvFind(&buf[i], cl, tag, val, valLen)) return 1u;
        i += cl;
    }
    return 0u;
}

/*
 * Compare an NDEF type field against a C-string literal.
 */
static inline uint8_t ptxIoTRdInt_TypeEq(const uint8_t *type, uint8_t type_len, const char *s)
{
    uint32_t n = 0;
    while (s[n] != '\0') { n++; }
    if (n != (uint32_t)type_len) { return 0u; }
    for (uint32_t k = 0; k < n; k++)
    {
        if (type[k] != (uint8_t)s[k]) { return 0u; }
    }
    return 1u;
}

/*
 * Case-sensitive "string starts with prefix" for a (non null-terminated) buffer.
 */
static inline uint8_t ptxIoTRdInt_StartsWith(const char *str, uint32_t str_len, const char *prefix)
{
    uint32_t n = 0;
    while (prefix[n] != '\0')
    {
        if ((n >= str_len) || (str[n] != prefix[n])) { return 0u; }
        n++;
    }
    return 1u;
}

/*
 * Search a Wi-Fi Simple Config (WSC) TLV blob for a given attribute id.
 * Recurses into the Credential attribute (0x100E). Big-endian 2-byte type/len.
 */
static uint8_t ptxIoTRdInt_WscFind(const uint8_t *buf, uint32_t len, uint16_t want,
                                   const uint8_t **val, uint16_t *vlen)
{
    uint32_t i = 0;
    while ((i + 4u) <= len)
    {
        uint16_t t = (uint16_t)(((uint16_t)buf[i] << 8) | buf[i + 1u]);
        uint16_t l = (uint16_t)(((uint16_t)buf[i + 2u] << 8) | buf[i + 3u]);
        i += 4u;
        if (((uint32_t)i + l) > len) { break; }
        if (t == want) { *val = &buf[i]; *vlen = l; return 1u; }
        if (0x100Eu == t)   /* Credential -> nested attributes */
        {
            if (ptxIoTRdInt_WscFind(&buf[i], l, want, val, vlen)) { return 1u; }
        }
        i += l;
    }
    return 0u;
}

/*
 * Decode a Wi-Fi Simple Config (vnd.wfa.wsc) MIME record.
 */
static inline void ptxIoTRdInt_PrintWifi(const uint8_t *p, uint32_t len)
{
    const uint8_t *v;
    uint16_t vl;

    ptxCommon_PrintF("    Value   : Wi-Fi Network\n");

    if (ptxIoTRdInt_WscFind(p, len, 0x1045u, &v, &vl))     /* SSID */
    {
        ptxCommon_PrintF("    SSID    : ");
        for (uint16_t k = 0; k < vl; k++) { ptxCommon_PrintF("%c", v[k]); }
        ptxCommon_PrintF("\n");
    }
    if (ptxIoTRdInt_WscFind(p, len, 0x1003u, &v, &vl) && (vl >= 2u))   /* Auth type */
    {
        uint16_t at = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
        const char *s = "Unknown";
        switch (at)
        {
            case 0x0001: s = "Open";              break;
            case 0x0002: s = "WPA-Personal";      break;
            case 0x0004: s = "Shared";            break;
            case 0x0008: s = "WPA-Enterprise";    break;
            case 0x0010: s = "WPA2-Enterprise";   break;
            case 0x0020: s = "WPA2-Personal";     break;
            case 0x0022: s = "WPA/WPA2-Personal"; break;
            default:                              break;
        }
        ptxCommon_PrintF("    Auth    : %s\n", s);
    }
    if (ptxIoTRdInt_WscFind(p, len, 0x100Fu, &v, &vl) && (vl >= 2u))   /* Encryption type */
    {
        uint16_t et = (uint16_t)(((uint16_t)v[0] << 8) | v[1]);
        const char *s = "Unknown";
        switch (et)
        {
            case 0x0001: s = "None";     break;
            case 0x0002: s = "WEP";      break;
            case 0x0004: s = "TKIP";     break;
            case 0x0008: s = "AES";      break;
            case 0x000C: s = "AES/TKIP"; break;
            default:                     break;
        }
        ptxCommon_PrintF("    Encrypt : %s\n", s);
    }
    if (ptxIoTRdInt_WscFind(p, len, 0x1027u, &v, &vl))     /* Network Key */
    {
        ptxCommon_PrintF("    Password: ");
        for (uint16_t k = 0; k < vl; k++) { ptxCommon_PrintF("%c", v[k]); }
        ptxCommon_PrintF("\n");
    }
    if (ptxIoTRdInt_WscFind(p, len, 0x1020u, &v, &vl) && (vl >= 6u))   /* MAC address */
    {
        ptxCommon_PrintF("    MAC Addr: ");
        for (uint16_t k = 0; k < 6u; k++)
        {
            ptxCommon_PrintF("%02X", v[k]);
            if (k < 5u) { ptxCommon_PrintF(":"); }
        }
        ptxCommon_PrintF("\n");
    }
}

/*
 * Decode a Bluetooth OOB (BR/EDR or LE) MIME record. Prints device address
 * (BR/EDR) and local name (from EIR/AD structures) if present.
 */
static inline void ptxIoTRdInt_PrintBt(const uint8_t *p, uint32_t len, uint8_t isLE)
{
    ptxCommon_PrintF("    Value   : Bluetooth %s\n", isLE ? "LE" : "BR/EDR");

    /* BR/EDR OOB: [0..1]=total len (LE), [2..7]=BD_ADDR (LE) then EIR data */
    uint32_t eir = isLE ? 0u : 8u;
    if ((!isLE) && (len >= 8u))
    {
        ptxCommon_PrintF("    Address : ");
        for (int8_t k = 5; k >= 0; k--)
        {
            ptxCommon_PrintF("%02X", p[2u + (uint8_t)k]);
            if (k) { ptxCommon_PrintF(":"); }
        }
        ptxCommon_PrintF("\n");
    }

    /* Scan EIR/AD structures: [len][type][data...] for a local name */
    uint32_t i = eir;
    while ((i + 1u) < len)
    {
        uint8_t l = p[i];
        if (0u == l) { break; }
        if (((uint32_t)i + 1u + l) > len) { break; }
        uint8_t adt = p[i + 1u];
        if ((0x09u == adt) || (0x08u == adt))    /* complete / shortened local name */
        {
            ptxCommon_PrintF("    Name    : ");
            for (uint8_t k = 1; k < l; k++) { ptxCommon_PrintF("%c", p[i + 1u + k]); }
            ptxCommon_PrintF("\n");
        }
        i += (uint32_t)l + 1u;
    }
}

/* Forward declaration (Smart Poster recursion). */
static void ptxIoTRdInt_PrintNDEFMsg(const uint8_t *msg, uint32_t len, uint8_t depth);

/*
 * Parse and print an NDEF message (one or more records). Dispatches on TNF and
 * record type to decode Text, URI/URL, Smart Poster, Wi-Fi, Bluetooth, external
 * (app) records, absolute URIs and generic data.
 */
static void ptxIoTRdInt_PrintNDEFMsg(const uint8_t *msg, uint32_t len, uint8_t depth)
{
    /* Slim raw-hex dump. The original NDEF Forum decoder (URI prefix table,
     * URL/Text/Smart Poster/Wi-Fi/Bluetooth/MIME/external dispatch and format
     * strings) was about 1.7 KB and got removed to free flash for the CLI
     * 'erase' path on the RA2E3. The helper printers (TypeEq/StartsWith/
     * PrintWifi/PrintBt/WscFind) are no longer referenced and will be dropped
     * by the linker's --gc-sections pass. */
    (void)depth;
    if ((NULL == msg) || (0u == len))
    {
        ptxCommon_PrintF("  (empty NDEF message)\n");
        return;
    }
    ptxCommon_PrintF("  NDEF raw (%u bytes):", (unsigned)len);
    for (uint32_t k = 0u; k < len; k++)
    {
        if ((k > 0u) && (0u == (k % 16u))) { ptxCommon_PrintF("\n                       "); }
        ptxCommon_PrintF(" %02X", msg[k]);
    }
    ptxCommon_PrintF("\n");
}

/*
 * Convenience wrapper: print an NDEF message starting at recursion depth 0.
 */
static void ptxIoTRdInt_PrintNDEF(const uint8_t *msg, uint32_t len)
{
    ptxIoTRdInt_PrintNDEFMsg(msg, len, 0u);
}

/*
 * Send a Type 4 Tag C-APDU and verify the status word is 0x9000.
 * Returns 1 on success (SW=9000), 0 otherwise. On success *rx_len holds the
 * full response length (including the 2 SW bytes).
 */
static uint8_t ptxIoTRdInt_T4Exchange(ptxIoTRd_t *iotRd, uint8_t *cmd, uint32_t cmd_len,
                                      uint8_t *rx, uint32_t *rx_len, uint32_t tmo)
{
    *rx_len = RX_BUFFER_SIZE;
    ptxStatus_t st = ptxIoTRd_Data_Exchange(iotRd, cmd, cmd_len, rx, rx_len, tmo);
    if ((ptxStatus_Success != st) || (*rx_len < 2u) ||
        (0x90u != rx[*rx_len - 2u]) || (0x00u != rx[*rx_len - 1u]))
    {
        return 0u;
    }
    return 1u;
}

/*
 * Try to read an NFC Forum Type 4 Tag NDEF message over ISO-DEP and print it.
 * Returns 1 if the tag is a Type 4 NDEF tag (Info/Size/Writeable/Records were
 * printed), 0 if it is not an NDEF tag (caller may fall back to EMV/PPSE).
 *
 * Uses the caller's tx buffer to accumulate the NDEF message and rx buffer for
 * the APDU exchanges.
 */
static uint8_t ptxIoTRdInt_ReadType4NDEF(ptxIoTRd_t *iotRd, uint8_t *tx, uint8_t *rx)
{
    const uint32_t tmo = DEFAULT_APP_TIMEOUT_PROT;
    uint32_t rx_len;
    uint8_t  cmd[16];

    /* 1. SELECT NDEF Tag Application (AID = D2 76 00 00 85 01 01) */
    static const uint8_t sel_app[] = {0x00,0xA4,0x04,0x00,0x07,0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00};
    rx_len = RX_BUFFER_SIZE;
    (void)ptxIoTRd_Data_Exchange(iotRd, (uint8_t *)sel_app, (uint32_t)sizeof(sel_app), rx, &rx_len, tmo);
    if ((rx_len < 2u) || (0x90u != rx[rx_len - 2u]) || (0x00u != rx[rx_len - 1u]))
    {
        ptxCommon_PrintF("[T4T] SELECT NDEF App (D276000085010100) -> SW=%02X%02X (FAILED)\n",
                         (rx_len >= 2u) ? rx[rx_len-2u] : 0u,
                         (rx_len >= 2u) ? rx[rx_len-1u] : 0u);
        ptxCommon_PrintF("[T4T] Android HCE app must register AID D276000085010100\n");
        return 0u;
    }
    ptxCommon_PrintF("[T4T] SELECT NDEF App -> OK\n");

    /* 2. SELECT Capability Container file (EF = E103) */
    static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
    rx_len = RX_BUFFER_SIZE;
    (void)ptxIoTRd_Data_Exchange(iotRd, (uint8_t *)sel_cc, (uint32_t)sizeof(sel_cc), rx, &rx_len, tmo);
    if ((rx_len < 2u) || (0x90u != rx[rx_len - 2u]) || (0x00u != rx[rx_len - 1u]))
    {
        ptxCommon_PrintF("[T4T] SELECT CC (E103) -> SW=%02X%02X (FAILED)\n",
                         (rx_len >= 2u) ? rx[rx_len-2u] : 0u,
                         (rx_len >= 2u) ? rx[rx_len-1u] : 0u);
        return 0u;
    }
    ptxCommon_PrintF("[T4T] SELECT CC (E103) -> OK\n");

    /* 3. READ CC (15 bytes) */
    static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
    rx_len = RX_BUFFER_SIZE;
    (void)ptxIoTRd_Data_Exchange(iotRd, (uint8_t *)read_cc, (uint32_t)sizeof(read_cc), rx, &rx_len, tmo);
    if ((rx_len < 2u) || (0x90u != rx[rx_len - 2u]) || (0x00u != rx[rx_len - 1u]) || (rx_len < 17u))
    {
        ptxCommon_PrintF("[T4T] READ CC -> SW=%02X%02X len=%u (FAILED)\n",
                         (rx_len >= 2u) ? rx[rx_len-2u] : 0u,
                         (rx_len >= 2u) ? rx[rx_len-1u] : 0u,
                         (unsigned)rx_len);
        return 0u;
    }
    ptxCommon_PrintF("[T4T] READ CC -> OK (%u bytes)\n", (unsigned)(rx_len - 2u));

    /* CC layout: [0..1]=CCLEN [2]=Ver [3..4]=MLe [5..6]=MLc
     *            [7]=0x04(NDEF FCTLV) [8]=0x06 [9..10]=FileID
     *            [11..12]=MaxNDEFSize [13]=ReadAccess [14]=WriteAccess */
    uint16_t mle     = (uint16_t)(((uint16_t)rx[3] << 8) | rx[4]);
    uint8_t  fid_hi  = rx[9];
    uint8_t  fid_lo  = rx[10];
    uint16_t maxfile = (uint16_t)(((uint16_t)rx[11] << 8) | rx[12]);
    uint8_t  wa      = rx[14];

    /* 4. SELECT NDEF file */
    cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C; cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 7u, rx, &rx_len, tmo))
    {
        return 0u;
    }

    /* 5. READ NLEN (first 2 bytes of the NDEF file) */
    cmd[0]=0x00; cmd[1]=0xB0; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 5u, rx, &rx_len, tmo) || (rx_len < 4u))
    {
        return 0u;
    }
    uint16_t nlen = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);

    ptxCommon_PrintF("Info           : NDEF (Type 4 Tag)\n");
    ptxCommon_PrintF("Size           : %u bytes (NDEF msg %u bytes)\n", maxfile, nlen);
    ptxCommon_PrintF("Writeable      : %s\n", (0x00u == wa) ? "Yes" : "No");

    if (0u == nlen)
    {
        ptxCommon_PrintF("Records        : (empty NDEF message)\n");
        return 1u;
    }

    /* 6. READ the NDEF message (starts at offset 2), in chunks limited by MLe
     *    and the available tx buffer. */
    uint32_t chunk = ((0u == mle) || (mle > 0xFFu)) ? 0xFFu : (uint32_t)mle;
    if (chunk > (uint32_t)TX_BUFFER_SIZE) { chunk = (uint32_t)TX_BUFFER_SIZE; }
    uint32_t total  = ((uint32_t)nlen > (uint32_t)TX_BUFFER_SIZE) ? (uint32_t)TX_BUFFER_SIZE : (uint32_t)nlen;
    uint32_t got    = 0;
    uint16_t offset = 2u;

    while (got < total)
    {
        uint32_t want = total - got;
        if (want > chunk) { want = chunk; }

        cmd[0]=0x00; cmd[1]=0xB0;
        cmd[2]=(uint8_t)(offset >> 8);
        cmd[3]=(uint8_t)(offset & 0xFFu);
        cmd[4]=(uint8_t)want;
        if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 5u, rx, &rx_len, tmo) || (rx_len < 2u))
        {
            break;
        }

        uint32_t data = rx_len - 2u;
        if (data > want) { data = want; }
        if (0u == data)  { break; }

        (void)memcpy(&tx[got], rx, data);
        got    += data;
        offset  = (uint16_t)(offset + data);
    }

    ptxCommon_PrintF("Records        :\n");
    ptxIoTRdInt_PrintNDEF(tx, got);
    return 1u;
}

/*
 * Build a single NFC Forum well-known Text NDEF record (RTD-Text) into `out`.
 * Language is fixed to "en" (2 bytes) and encoding is UTF-8.
 *
 * Layout (Short-Record form, MB=ME=1, TNF=0x01 Well-known, type 'T'):
 *   [0] 0xD1                   header
 *   [1] 0x01                   type length
 *   [2] payload_len            (1 status + 2 lang + text_len)
 *   [3] 0x54                   type 'T'
 *   [4] 0x02                   status: UTF-8, lang_len=2
 *   [5..6] 'e','n'             language code
 *   [7..]  text                UTF-8 payload
 *
 * Caller must ensure `out` has room for (text_len + 7) bytes. text_len is
 * capped at 248 by the Short-Record payload byte (255 - 1 - 2 - 4 header).
 */
static void ptxIoTRdInt_BuildTextRecord(const char *text, uint16_t text_len,
                                        uint8_t *out, uint16_t *out_len)
{
    const uint8_t payload_len = (uint8_t)(1u + 2u + text_len);  /* status + lang + text */

    out[0] = 0xD1u;
    out[1] = 0x01u;
    out[2] = payload_len;
    out[3] = 0x54u;
    out[4] = 0x02u;
    out[5] = 0x65u;
    out[6] = 0x6Eu;
    (void)memcpy(&out[7], text, text_len);

    *out_len = (uint16_t)(7u + text_len);
}

/*
 * Write or erase an NDEF message on an NFC Forum Type 4 Tag.
 *
 * Sequence:
 *   SELECT NDEF AID -> SELECT CC -> READ CC (learn NDEF file id and verify
 *   write access) -> SELECT NDEF file -> UPDATE BINARY NLEN=0 (file now
 *   appears empty; this is also the erase primitive) -> [if ndef_len > 0]
 *   UPDATE BINARY at offset 2 with the NDEF body, then UPDATE BINARY at
 *   offset 0 with the new NLEN.
 *
 * ndef_len == 0 stops after the NLEN=0 step and is therefore the canonical
 * "erase NDEF" path. ndef_len is capped at 248 (single UPDATE BINARY).
 *
 * Returns 1 on success, 0 on any APDU / tag failure. This deliberately
 * bypasses the ptxNDEF_T4TOP layer: a full Open/Check/Write sequence pulls
 * in ~3 KB of code on the RA2E3 which won't fit.
 */
static uint8_t ptxIoTRdInt_WriteType4NDEF(ptxIoTRd_t *iotRd, const uint8_t *ndef,
                                          uint16_t ndef_len, uint8_t *rx)
{
    const uint32_t tmo = DEFAULT_APP_TIMEOUT_PROT;
    uint32_t rx_len;
    uint8_t  cmd[5u + 248u];

    if (ndef_len > 248u) { return 0u; }

    /* SELECT NDEF Tag Application */
    static const uint8_t sel_app[] = {0x00,0xA4,0x04,0x00,0x07,0xD2,0x76,0x00,0x00,0x85,0x01,0x01,0x00};
    if (!ptxIoTRdInt_T4Exchange(iotRd, (uint8_t *)sel_app, (uint32_t)sizeof(sel_app), rx, &rx_len, tmo)) { return 0u; }

    /* SELECT CC */
    static const uint8_t sel_cc[] = {0x00,0xA4,0x00,0x0C,0x02,0xE1,0x03};
    if (!ptxIoTRdInt_T4Exchange(iotRd, (uint8_t *)sel_cc, (uint32_t)sizeof(sel_cc), rx, &rx_len, tmo)) { return 0u; }

    /* READ CC -> learn NDEF file id and check write access */
    static const uint8_t read_cc[] = {0x00,0xB0,0x00,0x00,0x0F};
    if (!ptxIoTRdInt_T4Exchange(iotRd, (uint8_t *)read_cc, (uint32_t)sizeof(read_cc), rx, &rx_len, tmo) || (rx_len < 17u)) { return 0u; }
    uint8_t fid_hi = rx[9];
    uint8_t fid_lo = rx[10];
    if (0x00u != rx[14]) { return 0u; }  /* tag is read-only */

    /* SELECT NDEF file */
    cmd[0]=0x00; cmd[1]=0xA4; cmd[2]=0x00; cmd[3]=0x0C; cmd[4]=0x02; cmd[5]=fid_hi; cmd[6]=fid_lo;
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 7u, rx, &rx_len, tmo)) { return 0u; }

    /* UPDATE BINARY @0: NLEN = 0 (erase / start of partial write) */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02; cmd[5]=0x00; cmd[6]=0x00;
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 7u, rx, &rx_len, tmo)) { return 0u; }

    if (0u == ndef_len) { return 1u; }  /* erase complete */

    /* UPDATE BINARY @2: NDEF body */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x02; cmd[4]=(uint8_t)ndef_len;
    (void)memcpy(&cmd[5], ndef, ndef_len);
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, (uint32_t)(5u + ndef_len), rx, &rx_len, tmo)) { return 0u; }

    /* UPDATE BINARY @0: NLEN = ndef_len (commit, big-endian) */
    cmd[0]=0x00; cmd[1]=0xD6; cmd[2]=0x00; cmd[3]=0x00; cmd[4]=0x02;
    cmd[5]=(uint8_t)(ndef_len >> 8);
    cmd[6]=(uint8_t)(ndef_len & 0xFFu);
    if (!ptxIoTRdInt_T4Exchange(iotRd, cmd, 7u, rx, &rx_len, tmo)) { return 0u; }

    return 1u;
}

/*
 * Try to read an NFC Forum Type 2 Tag NDEF message and print it.
 * Returns 1 if the tag is NDEF-formatted (Info/Size/Writeable/Records were
 * printed), 0 otherwise.
 *
 * T2T uses the raw READ command (0x30 <block>) which returns 16 bytes
 * (4 pages) per call. Block 3 holds the Capability Container (CC); the NDEF
 * data area (TLV blocks) starts at block 4.
 */
static uint8_t ptxIoTRdInt_ReadType2NDEF(ptxIoTRd_t *iotRd, uint8_t *tx, uint8_t *rx)
{
    const uint32_t tmo = DEFAULT_APP_TIMEOUT_PROT;
    uint32_t rx_len;
    ptxStatus_t st;
    uint8_t cmd[2];

    /* READ block 3 -> CC (response = blocks 3..6, 16 bytes) */
    cmd[0] = 0x30; cmd[1] = 0x03;
    rx_len = RX_BUFFER_SIZE;
    st = ptxIoTRd_Data_Exchange(iotRd, cmd, 2u, rx, &rx_len, tmo);
    if ((ptxStatus_Success != st) || (rx_len < 4u))
    {
        return 0u;
    }

    /* CC: [0]=magic(0xE1) [1]=version [2]=size(x8) [3]=access(RW nibbles) */
    if (0xE1u != rx[0])
    {
        return 0u;   /* not NDEF formatted */
    }

    uint32_t data_area = (uint32_t)rx[2] * 8u;
    uint8_t  wa_nibble = (uint8_t)(rx[3] & 0x0Fu);

    ptxCommon_PrintF("Info           : NDEF (Type 2 Tag)\n");
    ptxCommon_PrintF("Size           : %u bytes (data area)\n", data_area);
    ptxCommon_PrintF("Writeable      : %s\n", (0x00u == wa_nibble) ? "Yes" : "No");

    /* Read the data area (starting at block 4) into the tx buffer. */
    uint32_t cap = (data_area > (uint32_t)TX_BUFFER_SIZE) ? (uint32_t)TX_BUFFER_SIZE : data_area;
    if (0u == cap) { cap = (uint32_t)TX_BUFFER_SIZE; }
    uint32_t got   = 0;
    uint8_t  block = 4u;

    while (got < cap)
    {
        cmd[0] = 0x30; cmd[1] = block;
        rx_len = RX_BUFFER_SIZE;
        st = ptxIoTRd_Data_Exchange(iotRd, cmd, 2u, rx, &rx_len, tmo);
        if ((ptxStatus_Success != st) || (rx_len < 4u))
        {
            break;
        }

        uint32_t take = (rx_len < 16u) ? rx_len : 16u;
        if ((got + take) > cap) { take = cap - got; }
        (void)memcpy(&tx[got], rx, take);
        got += take;

        if ((uint32_t)block + 4u > 0xFFu) { break; }   /* block overflow guard */
        block = (uint8_t)(block + 4u);
    }

    /* Walk the TLV area, locate the NDEF Message TLV (tag 0x03). */
    uint32_t p     = 0;
    uint8_t  found = 0;
    while (p < got)
    {
        uint8_t t = tx[p++];
        if (0x00u == t) { continue; }     /* NULL TLV       */
        if (0xFEu == t) { break;    }     /* Terminator TLV */
        if (p >= got) { break; }

        uint32_t l = tx[p++];
        if (0xFFu == l)                   /* 3-byte length form */
        {
            if ((p + 2u) > got) { break; }
            l = ((uint32_t)tx[p] << 8) | tx[p + 1u];
            p += 2u;
        }

        if (0x03u == t)                   /* NDEF Message TLV */
        {
            if ((p + l) > got) { l = got - p; }
            ptxCommon_PrintF("Records        :\n");
            ptxIoTRdInt_PrintNDEF(&tx[p], l);
            found = 1u;
            break;
        }

        p += l;                           /* skip Lock/Memory/other TLVs */
    }

    if (!found)
    {
        ptxCommon_PrintF("Records        : (no NDEF TLV found)\n");
    }

    return 1u;
}

/*
 * Print structured card information.
 * Uses the CALLER's tx/rx buffers (already on the stack in the data-exchange function).
 */
static void ptxIoTRdInt_PrintCardInfo(ptxIoTRd_t *iotRd,
                                      ptxIoTRd_CardRegistry_t *reg,
                                      uint8_t *tx, uint8_t *rx)
{
    ptxIoTRd_CardParams_t *card = reg->ActiveCard;
    ptxStatus_t st;
    uint32_t tx_len, rx_len;
    const uint8_t *val = NULL;
    uint32_t val_len = 0;
    const uint32_t tmo = DEFAULT_APP_TIMEOUT_PROT;

    /* Blink LEDs based on card technology type. */
    {
        UserBoardUtils_CardType_t led_ct;
        switch (card->TechType)
        {
            case Tech_TypeB: led_ct = UserBoardUtils_CardType_B; break;
            case Tech_TypeF: led_ct = UserBoardUtils_CardType_F; break;
            case Tech_TypeV: led_ct = UserBoardUtils_CardType_V; break;
            default:         led_ct = UserBoardUtils_CardType_A; break; /* Type-A and any other */
        }
        UserBoardUtils_BlinkForCardType(led_ct);
    }

    ptxCommon_PrintF("============ CARD INFO =======================\n");

    /* ---- Tag Type ---- */
    const char *tag_type = "Unknown";
    switch (reg->ActiveCardProtType)
    {
        case Prot_T2T:    tag_type = "NFC Forum Type 2 Tag (T2T)";        break;
        case Prot_T3T:    tag_type = "NFC Forum Type 3 Tag (T3T/FeliCa)"; break;
        case Prot_ISODEP: tag_type = "ISO-DEP (Type 4 Tag / ISO 14443-4)"; break;
        case Prot_NFCDEP: tag_type = "NFC-DEP (Peer-to-Peer)";            break;
        case Prot_T5T:    tag_type = "NFC Forum Type 5 Tag (T5T/ISO 15693)"; break;
        default:          break;
    }
    ptxCommon_PrintF("Tag Type       : %s\n", tag_type);

    /* ---- Serial Number ---- */
    ptxCommon_PrintF("Serial Number  : ");
    switch (card->TechType)
    {
        case Tech_TypeA:
            for (uint8_t i = 0; i < card->TechParams.CardAParams.NFCID1_LEN; i++)
            {
                if (i) ptxCommon_PrintF(":");
                ptxCommon_PrintF("%02X", card->TechParams.CardAParams.NFCID1[i]);
            }
            break;
        case Tech_TypeB:
            /* PUPI is bytes 1-4 of SENSB_RES */
            for (uint8_t i = 1; i <= 4; i++)
            {
                if (i > 1) ptxCommon_PrintF(":");
                ptxCommon_PrintF("%02X", card->TechParams.CardBParams.SENSB_RES[i]);
            }
            break;
        case Tech_TypeF:
            /* NFCID2 is bytes 2..9 of SENSF_RES */
            for (uint8_t i = 2; i < 10; i++)
            {
                if (i > 2) ptxCommon_PrintF(":");
                ptxCommon_PrintF("%02X", card->TechParams.CardFParams.SENSF_RES[i]);
            }
            break;
        case Tech_TypeV:
            for (uint8_t i = 0; i < 8; i++)
            {
                if (i) ptxCommon_PrintF(":");
                ptxCommon_PrintF("%02X", card->TechParams.CardVParams.UID[7u - i]);
            }
            break;
        default:
            ptxCommon_PrintF("N/A");
            break;
    }
    ptxCommon_PrintF("\n");

    /* ---- ISO-DEP: try Type 4 NDEF; otherwise just report as non-NDEF ---- */
    if (Prot_ISODEP == reg->ActiveCardProtType)
    {
        /* Try NFC Forum Type 4 Tag NDEF reading (covers most NDEF T4T tags
         * and Android HCE apps that implement the T4T NDEF protocol). */
        if (!ptxIoTRdInt_ReadType4NDEF(iotRd, tx, rx))
        {
            /* EMV PPSE/AID/GPO/AFL decoding was stripped to free flash for the
             * CLI 'erase' path; non-NDEF ISO-DEP cards (e.g. payment cards,
             * DESFire without NDEF app) are reported only at a high level. */
            ptxCommon_PrintF("Info           : Non-NDEF ISO-DEP card\n");
            ptxCommon_PrintF("Records        : (EMV decoder disabled in this build)\n");
        }
        (void)tmo; (void)val; (void)val_len; (void)tx_len; (void)rx_len; (void)st;
        ptxCommon_PrintF("==============================================\n");
        return;
    }

    /* ---- T2T: read NDEF directly ---- */
    if (Prot_T2T == reg->ActiveCardProtType)
    {
        if (!ptxIoTRdInt_ReadType2NDEF(iotRd, tx, rx))
        {
            ptxCommon_PrintF("Info           : NFC Tag (not NDEF formatted)\n");
            ptxCommon_PrintF("Size           : N/A\n");
            ptxCommon_PrintF("Writeable      : N/A\n");
            ptxCommon_PrintF("Records        : (none)\n");
        }
    }
    /* ---- T5T: NDEF-style info (if applicable) ---- */
    else if (Prot_T5T == reg->ActiveCardProtType)
    {
        ptxCommon_PrintF("Info           : NFC Tag\n");
        ptxCommon_PrintF("Size           : (read CC for details)\n");
        ptxCommon_PrintF("Writeable      : (read CC for details)\n");
        ptxCommon_PrintF("Records        : (use NDEF API to read)\n");
    }
    else
    {
        /* T3T, NFC-DEP, etc. */
        ptxCommon_PrintF("Info           : NFC Device\n");
        ptxCommon_PrintF("Size           : N/A\n");
        ptxCommon_PrintF("Writeable      : N/A\n");
        ptxCommon_PrintF("Records        : N/A\n");
    }

    ptxCommon_PrintF("==============================================\n");
}

#endif /* USE_PTX_IOTRD_DEMO && !USE_NDEF */


/*
 * ####################################################################################################################
 * DATA EXCHANGE FUNCTION
 * ####################################################################################################################
 */
ptxStatus_t ptxIoTRdInt_DemoState_DataExchange(ptxIoTRd_t *iotRd, ptxIoTRd_CardRegistry_t *cardRegistry, ptxIotRdInt_Demo_State_t *demoState, uint8_t *skipTxDataExchange, uint8_t *skipRxProcessing,
                                               ptxNativeTag_T5T_t* t5tComp, ptxNativeTag_T5T_InitParams_t* t5tInitParams, ptxNDEF_T2TOP_t* t2tOpComp, ptxNDEF_T2TOP_InitParams_t* t2tOpInitParams,
                                               ptxNDEF_T3TOP_t* t3tOpComp, ptxNDEF_T3TOP_InitParams_t* t3tOpInitParams, ptxNDEF_T4TOP_t* t4tOpComp, ptxNDEF_T4TOP_InitParams_t* t4tOpInitParams,
                                               ptxNDEF_T5TOP_t* t5tOpComp, ptxNDEF_T5TOP_InitParams_t* t5tOpInitParams, ptxNDEF_t* ndefComp, ptxNDEF_InitParams_t* ndefInitParams)
{
    ptxStatus_t st = ptxStatus_Success;
    uint8_t tx_data[TX_BUFFER_SIZE];
    uint32_t tx_data_length = 0;

    uint8_t rx_data[RX_BUFFER_SIZE];
    uint32_t rx_data_length = 0;

    uint32_t app_timeout = DEFAULT_APP_TIMEOUT_RAW;

#ifndef USE_NDEF
    /* T2T Protocol Example => READ BLOCK 0 */
    const uint8_t PROT_T2T_EXAMPLE[] = {0x30, 0x00};

    /* T3T Protocol Example => CHECK BLOCK 0 (NFCID2 to be inserted) */
    const uint8_t PROT_T3T_EXAMPLE[] = {0x06, 0x01, 0x0B, 0x00, 0x01, 0x80, 0x00};

    /* P2P/NFC-DEP Protocol Example => LLCP - SYMM-Packet = 0x0000 */
    const uint8_t PROT_NFC_DEP_EXAMPLE[] = {0x00, 0x00};
#endif

    /* T5T Protocol Example => READ BLOCK 0 (UID to be inserted) */
    // const uint8_t PROT_T5T_EXAMPLE[] = {0x22, 0x20, 0x00};

    /* Needed by the generic NDEF component used for the CLI 'erase' path even
     * when USE_NDEF is not defined for the normal read flow. */
    uint8_t work_buffer[NDEF_BUFFER_SIZE];
#ifdef USE_NDEF
    /* General NDEF-exchange buffer */
    uint8_t ndef_msg_buffer[NDEF_BUFFER_SIZE];
    uint32_t ndef_msg_buffer_len;
#endif

    if ((NULL != iotRd) && (NULL != cardRegistry) && (NULL != demoState) && (NULL != skipTxDataExchange) && (NULL != skipRxProcessing)
        && (t5tComp) && (t5tInitParams) && (t2tOpComp) && (t2tOpInitParams) && (t3tOpComp) && (t3tOpInitParams) && (t4tOpComp) && (t4tOpInitParams)
        && (t5tOpComp) && (t5tOpInitParams) && (ndefComp) && (ndefInitParams))
    {
        /* initialize the Native-Tag component for T5T */
        (void)memset(t5tComp, 0, sizeof(ptxNativeTag_T5T_t));
        (void)memset(t5tInitParams, 0, sizeof(ptxNativeTag_T5T_InitParams_t));

        t5tInitParams->IotRd = iotRd;
        t5tInitParams->TxBuffer = &tx_data[0];
        t5tInitParams->TxBufferSize = TX_BUFFER_SIZE;
        t5tInitParams->UID = NULL;
        t5tInitParams->UIDLen = 0;

        st = ptxNativeTag_T5TOpen(t5tComp, t5tInitParams);

#ifdef USE_NDEF
        if (ptxStatus_Success == st)
        {
            /* initialize the NDEF-OP component for T2T */
            (void)memset(t2tOpComp, 0, sizeof(ptxNDEF_T2TOP_t));
            (void)memset(t2tOpInitParams, 0, sizeof(ptxNDEF_T2TOP_InitParams_t));

            t2tOpInitParams->T2TInitParams.IotRd = iotRd;
            t2tOpInitParams->T2TInitParams.TxBuffer = &tx_data[0];
            t2tOpInitParams->T2TInitParams.TxBufferSize = TX_BUFFER_SIZE;
            t2tOpInitParams->WorkBuffer = &work_buffer[0];
            t2tOpInitParams->WorkBufferSize = NDEF_BUFFER_SIZE;
            t2tOpInitParams->RxBuffer = &rx_data[0];
            t2tOpInitParams->RxBufferSize = RX_BUFFER_SIZE;

            st = ptxNDEF_T2TOpOpen (t2tOpComp, t2tOpInitParams);
        }

        if (ptxStatus_Success == st)
        {
            /* initialize the NDEF-OP component for T3T */
            (void)memset(t3tOpComp, 0, sizeof(ptxNDEF_T3TOP_t));
            (void)memset(t3tOpInitParams, 0, sizeof(ptxNDEF_T3TOP_InitParams_t));

            t3tOpInitParams->T3TInitParams.IotRd = iotRd;
            t3tOpInitParams->T3TInitParams.NFCID2 = &cardRegistry->ActiveCard->TechParams.CardFParams.SENSF_RES[2];
            t3tOpInitParams->T3TInitParams.NFCID2Len = PTX_T3T_NFCID2_SIZE;
            t3tOpInitParams->T3TInitParams.MRTI_Check = cardRegistry->ActiveCard->TechParams.CardFParams.SENSF_RES[15];
            t3tOpInitParams->T3TInitParams.MRTI_Update = cardRegistry->ActiveCard->TechParams.CardFParams.SENSF_RES[16];
            t3tOpInitParams->T3TInitParams.TxBuffer = &tx_data[0];
            t3tOpInitParams->T3TInitParams.TxBufferSize = TX_BUFFER_SIZE;
            t3tOpInitParams->RxBuffer = &rx_data[0];
            t3tOpInitParams->RxBufferSize = RX_BUFFER_SIZE;

            st = ptxNDEF_T3TOpOpen (t3tOpComp, t3tOpInitParams);
        }

        if (ptxStatus_Success == st)
        {
            /* initialize the NDEF-OP component for T4T */
            (void)memset(t4tOpComp, 0, sizeof(ptxNDEF_T4TOP_t));
            (void)memset(t4tOpInitParams, 0, sizeof(ptxNDEF_T4TOP_InitParams_t));

            t4tOpInitParams->T4TInitParams.IotRd = iotRd;
            t4tOpInitParams->T4TInitParams.TxBuffer = &tx_data[0];
            t4tOpInitParams->T4TInitParams.TxBufferSize = TX_BUFFER_SIZE;
            t4tOpInitParams->RxBuffer = &rx_data[0];
            t4tOpInitParams->RxBufferSize = RX_BUFFER_SIZE;

            st = ptxNDEF_T4TOpOpen (t4tOpComp, t4tOpInitParams);
        }

        if (ptxStatus_Success == st)
        {
            /* initialize the NDEF-OP component for T5T */
            (void)memset(t5tOpComp, 0, sizeof(ptxNDEF_T5TOP_t));
            (void)memset(t5tOpInitParams, 0, sizeof(ptxNDEF_T5TOP_InitParams_t));

            t5tOpInitParams->T5TInitParams.IotRd = iotRd;
            t5tOpInitParams->T5TInitParams.TxBuffer = &tx_data[0];
            t5tOpInitParams->T5TInitParams.TxBufferSize = TX_BUFFER_SIZE;
            t5tOpInitParams->RxBuffer = &rx_data[0];
            t5tOpInitParams->RxBufferSize = RX_BUFFER_SIZE;
            t5tOpInitParams->WorkBuffer = &work_buffer[0];
            t5tOpInitParams->WorkBufferSize = NDEF_BUFFER_SIZE;
            t5tOpInitParams->T5TInitParams.UID = NULL;
            t5tOpInitParams->T5TInitParams.UIDLen = 0;

            st = ptxNDEF_T5TOpOpen (t5tOpComp, t5tOpInitParams);

            /* Note: The usage of the Tag-specific NDEF-operation API or the generic NDEF-API is treated equally.
             *       The only difference is that the generic NDEF-API takes internally care which Tag / RF-protocol
             *       is currently active and then calls the specific Tag NDEF-operation function(s).
             *       Both APIs work completely independent of each other.
             *
             *       Using both APIs in this application is for demonstration purposes only.
             *
             **/
        }

        if (ptxStatus_Success == st)
        {
                /* initialize generic NDEF-OP component */
                (void)memset(ndefComp, 0, sizeof(ptxNDEF_t));
                (void)memset(ndefInitParams, 0, sizeof(ptxNDEF_InitParams_t));

                ndefInitParams->IotRd = iotRd;
                ndefInitParams->TxBuffer = &tx_data[0];
                ndefInitParams->TxBufferSize = TX_BUFFER_SIZE;
                ndefInitParams->RxBuffer = &rx_data[0];
                ndefInitParams->RxBufferSize = RX_BUFFER_SIZE;
                ndefInitParams->WorkBuffer = &work_buffer[0];
                ndefInitParams->WorkBufferSize = NDEF_BUFFER_SIZE;

                st = ptxNDEF_Open (ndefComp, ndefInitParams);
        }
#endif

        if (ptxStatus_Success == st)
        {
            /* CLI: one-shot "erase / write" hook. A single armed flag triggers
             * a Write of the CLI-supplied NDEF message - when no text was set
             * (cmd_erase), ndef_len is 0 and the operation collapses to "set
             * NLEN=0 / write empty NDEF", i.e. an erase. T2T uses the
             * ptxNDEF_T2TOp layer; T4T uses a raw-APDU helper (the full T4T
             * Op layer costs ~3 KB which won't fit on the RA2E3). T3T/T5T are
             * intentionally not supported so their Op code is dropped by
             * --gc-sections to stay inside the 63 KB flash budget. */
            if ((0u != UserCli_IsWriteArmed()) || (0u != UserCli_IsEraseArmed()))
            {
                ptxStatus_t  op_st        = ptxStatus_Success;
                const char  *op_proto     = "Unknown";
                uint8_t      op_supported = 1u;
                uint8_t      is_write     = UserCli_IsWriteArmed();
                /* NDEF Text record max: 7 header/lang bytes + payload. The
                 * buffer doubles as the non-NULL placeholder for the erase case
                 * (T2T WriteMessage rejects a NULL ptr even when msgLen==0). */
                uint8_t      ndef_buf[USER_CLI_WRITE_TEXT_MAX + 7u];
                uint16_t     ndef_len = 0u;
                const char  *op_name  = is_write ? "Write" : "Erase";

                if (0u != is_write)
                {
                    uint16_t    txt_len = 0u;
                    const char *txt     = UserCli_GetWriteText(&txt_len);
                    if ((NULL != txt) && (txt_len > 0u) && (txt_len <= USER_CLI_WRITE_TEXT_MAX))
                    {
                        ptxIoTRdInt_BuildTextRecord(txt, txt_len, ndef_buf, &ndef_len);
                    }
                    else
                    {
                        op_st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                    }
                }

                if (ptxStatus_Success == op_st)
                {
                    switch (cardRegistry->ActiveCardProtType)
                    {
                        case Prot_T2T:
                            op_proto = "T2T";
                            (void)memset(t2tOpComp,       0, sizeof(ptxNDEF_T2TOP_t));
                            (void)memset(t2tOpInitParams, 0, sizeof(ptxNDEF_T2TOP_InitParams_t));
                            t2tOpInitParams->T2TInitParams.IotRd        = iotRd;
                            t2tOpInitParams->T2TInitParams.TxBuffer     = &tx_data[0];
                            t2tOpInitParams->T2TInitParams.TxBufferSize = TX_BUFFER_SIZE;
                            t2tOpInitParams->WorkBuffer                 = &work_buffer[0];
                            t2tOpInitParams->WorkBufferSize             = NDEF_BUFFER_SIZE;
                            t2tOpInitParams->RxBuffer                   = &rx_data[0];
                            t2tOpInitParams->RxBufferSize               = RX_BUFFER_SIZE;
                            op_st = ptxNDEF_T2TOpOpen(t2tOpComp, t2tOpInitParams);
                            if (ptxStatus_Success == op_st)
                            {
                                op_st = ptxNDEF_T2TOpCheckMessage(t2tOpComp);
                                if (ptxStatus_Success == op_st)
                                {
                                    op_st = ptxNDEF_T2TOpWriteMessage(t2tOpComp, &ndef_buf[0], (uint32_t)ndef_len);
                                }
                                (void)ptxNDEF_T2TOpClose(t2tOpComp);
                            }
                            break;

#ifndef USE_NDEF
                        case Prot_ISODEP:
                            op_proto = "T4T";
                            if (0u == ptxIoTRdInt_WriteType4NDEF(iotRd, ndef_buf, ndef_len, &rx_data[0]))
                            {
                                op_st = PTX_STATUS(ptxStatus_Comp_IoTReader, ptxStatus_InvalidParameter);
                            }
                            break;
#endif

                        default:
                            op_supported = 0u;
                            break;
                    }
                }

                if (0u != op_supported)
                {
                    ptxCommon_PrintF("%s on %s (NDEF %u B): %s\n",
                                     op_name, op_proto, (unsigned)ndef_len,
                                     (ptxStatus_Success == op_st) ? "OK" : "ERROR");
                }
                else
                {
                    ptxCommon_PrintF("%s: proto 0x%02X not supported\n",
                                     op_name, (unsigned)cardRegistry->ActiveCardProtType);
                }

                UserCli_ClearWriteArmed();
                UserCli_ClearEraseArmed();
                *skipTxDataExchange = 1u;
                *skipRxProcessing   = 1u;
                goto data_exchange_done;
            }

#ifndef USE_NDEF
            /* Print structured card info and read records (ISO-DEP: EMV flow). */
            ptxIoTRdInt_PrintCardInfo(iotRd, cardRegistry, &tx_data[0], &rx_data[0]);
            /* Card info already performed all relevant reads; skip the raw
             * protocol example exchange (TX=/RX=) to keep the output clean. */
            *skipTxDataExchange = 1u;
            *skipRxProcessing   = 1u;
#endif

            switch (cardRegistry->ActiveCardProtType)
            {
                case Prot_T2T:
    #ifndef USE_NDEF
                    tx_data_length = sizeof(PROT_T2T_EXAMPLE);
                    memcpy(&tx_data[0], &PROT_T2T_EXAMPLE[0], tx_data_length);
                    app_timeout = DEFAULT_APP_TIMEOUT_RAW;
    #else
                    /* check if the Tag supports NDEF */
                    ptxCommon_PrintF("Check NDEF-compatibility via T2T-OP API ... ");
                    st = ptxNDEF_T2TOpCheckMessage (t2tOpComp);

                    if (ptxStatus_Success == st)
                    {
                        ptxCommon_PrintF("OK\n");

                        ptxCommon_PrintF("Read NDEF-message via T2T-OP API ... ");
                        ndef_msg_buffer_len = NDEF_BUFFER_SIZE;
                        st = ptxNDEF_T2TOpReadMessage (t2tOpComp, &ndef_msg_buffer[0], &ndef_msg_buffer_len);

                        if ((ptxStatus_Success == st) && (0 != ndef_msg_buffer_len))
                        {
                            ptxCommon_PrintF("OK, NDEF-Message Length = %04d Byte(s)\n", ndef_msg_buffer_len);
                            ptxCommon_PrintF("NDEF-Message Content = \n");
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 0);
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 1);

                        } else
                        {
                            ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                        }

                    } else
                    {
                        ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                    }

                    *skipRxProcessing = 1u;
                    *skipTxDataExchange = 1u;
    #endif
                    break;

                case Prot_T3T:
    #ifndef USE_NDEF
                    /*  7 = 1x Command-Code + 1x Number of Services + 2x Service Code List + 1 Number of Blocks + 2x Block List */
                    /* 8 = Length of NFCID2 */
                    /* Note: LEN-byte managed internally! */
                    tx_data_length = 7u + 8u;
                    memcpy(&tx_data[0], &PROT_T3T_EXAMPLE[0], 1u);
                    memcpy(&tx_data[1], &cardRegistry->ActiveCard->TechParams.CardFParams.SENSF_RES[2], 8u);
                    memcpy(&tx_data[9], &PROT_T3T_EXAMPLE[1], 6u);
                    app_timeout = DEFAULT_APP_TIMEOUT_RAW;
    #else
                    /* check if the Tag supports NDEF */
                    ptxCommon_PrintF("Check NDEF-compatibility via T3T-OP API ... ");
                    st = ptxNDEF_T3TOpCheckMessage (t3tOpComp);

                    if (ptxStatus_Success == st)
                    {
                        ptxCommon_PrintF("OK\n");

                        ptxCommon_PrintF("Read NDEF-message via T3T-OP API ... ");
                        ndef_msg_buffer_len = NDEF_BUFFER_SIZE;
                        st = ptxNDEF_T3TOpReadMessage (t3tOpComp, &ndef_msg_buffer[0], &ndef_msg_buffer_len);

                        if ((ptxStatus_Success == st) && (0 != ndef_msg_buffer_len))
                        {
                            ptxCommon_PrintF("OK, NDEF-Message Length = %04d Byte(s)\n", ndef_msg_buffer_len);
                            ptxCommon_PrintF("NDEF-Message Content = \n");
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 0);
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 1);

                        } else
                        {
                            ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                        }

                    } else
                    {
                        ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                    }

                    *skipRxProcessing = 1u;
                    *skipTxDataExchange = 1u;
    #endif
                    break;

                case Prot_ISODEP:
    #ifndef USE_NDEF
                    /* Card info + EMV records already printed above by ptxIoTRdInt_PrintCardInfo. */
                    *skipRxProcessing = 1u;
                    *skipTxDataExchange = 1u;
    #else
                    /* check if the Tag supports NDEF */
                    ptxCommon_PrintF("Check NDEF-compatibility via T4T-OP API ... ");
                    st = ptxNDEF_T4TOpCheckMessage (t4tOpComp);

                    if (ptxStatus_Success == st)
                    {
                        ptxCommon_PrintF("OK\n");

                        ptxCommon_PrintF("Read NDEF-message via T4T-OP API ... ");
                        ndef_msg_buffer_len = NDEF_BUFFER_SIZE;
                        st = ptxNDEF_T4TOpReadMessage (t4tOpComp, &ndef_msg_buffer[0], &ndef_msg_buffer_len);

                        if ((ptxStatus_Success == st) && (0 != ndef_msg_buffer_len))
                        {
                            ptxCommon_PrintF("OK, NDEF-Message Length = %04d Byte(s)\n", ndef_msg_buffer_len);
                            ptxCommon_PrintF("NDEF-Message Content = \n");
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 0);
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 1);

                        } else
                        {
                            ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                        }

                    } else
                    {
                        ptxCommon_PrintF("Error (Status-Code = %04X)\n", st);
                    }

                    *skipRxProcessing = 1u;
                    *skipTxDataExchange = 1u;
    #endif
                    break;

                case Prot_NFCDEP:
    #ifndef USE_NDEF
                    tx_data_length = sizeof(PROT_NFC_DEP_EXAMPLE);
                    memcpy(&tx_data[0], &PROT_NFC_DEP_EXAMPLE[0], tx_data_length);
                    app_timeout = DEFAULT_APP_TIMEOUT_PROT;
    #endif
                    break;

                case Prot_T5T:
    #ifndef USE_NDEF
                    /* read Block-0 via raw data-exchange (3 = Flags + Command Byte + Block number, 8 = Length of UID) */
                    /*
                    tx_data_length = 3u + 8u;
                    memcpy(&tx_data[0], &PROT_T5T_EXAMPLE[0], 2u);
                    memcpy(&tx_data[2], &card_registry->ActiveCard->TechParams.CardVParams.UID[0], 8u);
                    memcpy(&tx_data[10], &PROT_T5T_EXAMPLE[2], 1u);
                    */

                    app_timeout = DEFAULT_APP_TIMEOUT_RAW;
                    rx_data_length = RX_BUFFER_SIZE;

                    /* set UID if adressed-mode shall be used */
                    (void)ptxNativeTag_T5TSetUID(t5tComp, &cardRegistry->ActiveCard->TechParams.CardVParams.UID[0], 8u);

                    /* read Block-0 via Native-Tag command-set */
                    st = ptxNativeTag_T5TReadSingleBlock (t5tComp, 0, 0, &rx_data[0], (size_t*)&rx_data_length, app_timeout);
                    ptxCommon_PrintStatusMessage("Execute \"READ_SINGLE_BLOCK\"-command (Block 0)", st);
                    *skipTxDataExchange = 1u;
    #else
                    /* check if the Tag supports NDEF */
                    st = ptxNDEF_T5TOpCheckMessage (t5tOpComp);
                    ptxCommon_PrintF("Check NDEF-compatibility via T5T-OP API", st);

                    if (ptxStatus_Success == st)
                    {
                        ndef_msg_buffer_len = NDEF_BUFFER_SIZE;
                        st = ptxNDEF_T5TOpReadMessage (t5tOpComp, &ndef_msg_buffer[0], &ndef_msg_buffer_len);
                        ptxCommon_PrintF("Read NDEF-message via T5T-OP API", st);
                        ptxCommon_PrintF("OK, NDEF-Message Length = %04d Byte(s)\n", ndef_msg_buffer_len);
                        ptxCommon_PrintF("NDEF-Message Content = \n");
                        ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 0);
                        ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 1);
                    }

                    if (ptxStatus_Success == st)
                    {
                        /* check if the Tag supports NDEF */
                        st = ptxNDEF_CheckMessage (ndefComp);

                        if (ptxStatus_Success == st)
                        {
                            ndef_msg_buffer_len = NDEF_BUFFER_SIZE;
                            st = ptxNDEF_ReadMessage (ndefComp, &ndef_msg_buffer[0], &ndef_msg_buffer_len);
                            ptxCommon_PrintF("Read NDEF-message via NDEF-OP API", st);
                            ptxCommon_PrintF("OK, NDEF-Message Length = %04d Byte(s)\n", ndef_msg_buffer_len);
                            ptxCommon_PrintF("NDEF-Message Content = \n");
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 0);
                            ptxCommon_Print_Buffer(&ndef_msg_buffer[0], 0, ndef_msg_buffer_len, 1, 1);
                        }
                    }

                    *skipRxProcessing = 1u;
                    *skipTxDataExchange = 1u;
    #endif
                    break;

                default:
                    /* Undefined Protocol - Restart RF-Discovery */
                    *skipTxDataExchange = 1u;
                    break;
            }

data_exchange_done:
            if (0 == *skipTxDataExchange)
            {
                ptxIoTRdInt_Sleep(iotRd, PTX_IOTRD_EXCHANGE_WAIT_TIME);

                rx_data_length = RX_BUFFER_SIZE;
                ptxCommon_PrintF("TX = ");
                ptxCommon_Print_Buffer(&tx_data[0], 0, tx_data_length, 1, 0);
                st = ptxIoTRd_Data_Exchange(iotRd, &tx_data[0], tx_data_length, &rx_data[0], &rx_data_length, app_timeout);
            }

            if (0 == *skipRxProcessing)
            {
                if (ptxStatus_Success == st)
                {
                    ptxCommon_PrintF("RX = ");
                    ptxCommon_Print_Buffer(&rx_data[0], 0, rx_data_length, 1, 0);
                } else
                {
                    ptxCommon_PrintF("ERROR - RF-Exchange failed! (Error-Code = %04X, RF)\n", st);
                }
            }
        } else
        {
            ptxCommon_PrintF("ERROR - Module initialization failed! (Error-Code = %04X, RF)\n", st);
        }

        *demoState = IoTRd_DemoState_DeactivateReader;
    }

    return st;
}

static void ptxIoTRdInt_Print_Revision_Info(ptxIoTRd_t *iotRd)
{
    ptxStatus_t st = ptxStatus_Success;
    uint32_t rev_info;

    ptxCommon_PrintF ("\n");
    ptxCommon_PrintF ("Print Revision Information...\n\n");

    /* get C-Stack Revision */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_C_Stack, &rev_info);
    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("C-Stack Revision.......: 0x%04X\n", rev_info);
    }

    /* any local modifications ? */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_Local_Changes, &rev_info);
    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("Local Modifications....: 0x%04X\n", rev_info);
    }

    /* get NSC (DFY)-Code Revision */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_DFY_Code, &rev_info);
    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("NSC-Code Revision......: %04d\n", rev_info);
    }

    /* get NSC (DFY)-Toolchain Revision */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_DFY_Toolchain, &rev_info);
    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("NSC-Toolchain Revision.: %04d\n", rev_info);
    }

    /* get Chip-ID/-revision */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_ChipID, &rev_info);
    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("Chip-ID................: 0x%02X\n", rev_info);
    }

    /* get Product-ID/-revision */
    st = ptxIoTRd_Get_Revision_Info(iotRd, RevInfo_ProductID, &rev_info);
    if (ptxStatus_Success == st)
    {
        switch ((uint8_t)rev_info)
        {
            case PTX_HW_PRODUCT_ID_PTX100X:
                ptxCommon_PrintF ("Product-ID.............: 0x%02X (PTX100x)\n", rev_info);
                break;

            case PTX_HW_PRODUCT_ID_PTX105X:
                ptxCommon_PrintF ("Product-ID.............: 0x%02X (PTX105x)\n", rev_info);
                break;

            case PTX_HW_PRODUCT_ID_PTX130X:
                ptxCommon_PrintF ("Product-ID.............: 0x%02X (PTX130x)\n", rev_info);
                break;

            default:
                ptxCommon_PrintF ("Product-ID.............: Unknown\n");
                break;
        }
    }

    ptxCommon_PrintF ("\n");

    if (ptxStatus_Success == st)
    {
        ptxCommon_PrintF ("Print Revision Information...OK\n");
    }
    else
    {
        ptxCommon_PrintF ("Print Revision Information...FAILED (Internal Error)\n");
    }
}



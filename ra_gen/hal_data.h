/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_gpt.h"
#include "r_timer_api.h"
#include "r_dtc.h"
#include "r_transfer_api.h"
#include "r_sci_spi.h"
#include "r_spi_api.h"
#include "rm_comms_spi.h"
#include "rm_comms_api.h"
#include "rm_nfc_reader_ptx.h"
#include "r_sci_uart.h"
#include "r_uart_api.h"
FSP_HEADER
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer0;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

#ifndef ptxPLAT_TIMER_IsrCallback
void ptxPLAT_TIMER_IsrCallback(timer_callback_args_t *p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer1;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer1_ctrl;
extern const timer_cfg_t g_timer1_cfg;

#ifndef ptxPLAT_TIMER_IsrCallback
void ptxPLAT_TIMER_IsrCallback(timer_callback_args_t *p_args);
#endif
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer1;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer1_ctrl;
extern const transfer_cfg_t g_transfer1_cfg;
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer0;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer0_ctrl;
extern const transfer_cfg_t g_transfer0_cfg;
/** SPI on SCI Instance. */
extern const spi_instance_t ptx_pmod_spi;

/** Access the SCI_SPI instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_spi_instance_ctrl_t ptx_pmod_spi_ctrl;
extern const spi_cfg_t ptx_pmod_spi_cfg;

/** Called by the driver when a transfer has completed or an error has occurred (Must be implemented by the user). */
#ifndef rm_comms_spi_callback
void rm_comms_spi_callback(spi_callback_args_t *p_args);
#endif
/* SPI Communication Device */
extern const rm_comms_instance_t g_comms_spi_device0;
extern rm_comms_spi_instance_ctrl_t g_comms_spi_device0_ctrl;
extern const rm_comms_cfg_t g_comms_spi_device0_cfg;
#ifndef ptxPLAT_SPI_TransferCallback
void ptxPLAT_SPI_TransferCallback(rm_comms_callback_args_t *p_args);
#endif
/** NFC Reader PTX Device*/
extern nfc_reader_ptx_instance_ctrl_t g_nfc_reader_ptx0_ctrl;
extern const nfc_reader_ptx_cfg_t g_nfc_reader_ptx0_cfg;
/** UART on SCI Instance. */
extern const uart_instance_t g_uart_jlob_vcom;

/** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_uart_instance_ctrl_t g_uart_jlob_vcom_ctrl;
extern const uart_cfg_t g_uart_jlob_vcom_cfg;
extern const sci_uart_extended_cfg_t g_uart_jlob_vcom_cfg_extend;

#ifndef NULL
void NULL(uart_callback_args_t *p_args);
#endif
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */

/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_sci_b_spi.h"
#include "r_spi_api.h"
#include "r_gpt.h"
#include "r_timer_api.h"
FSP_HEADER
/** SPI on SCI Instance. */
extern const spi_instance_t ptx_pmod_spi;

/** Access the SCI_B_SPI instance using these structures when calling API functions directly (::p_api is not used). */
extern sci_b_spi_instance_ctrl_t ptx_pmod_spi_ctrl;
extern const spi_cfg_t ptx_pmod_spi_cfg;

/** Called by the driver when a transfer has completed or an error has occurred (Must be implemented by the user). */
#ifndef ptxPLAT_SPI_TransferCallback
void ptxPLAT_SPI_TransferCallback(spi_callback_args_t *p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer0;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer0_ctrl;
extern const timer_cfg_t g_timer0_cfg;

#ifndef ptxPLAT_TIMER_IsrCallback
void ptxPLAT_TIMER_IsrCallback(timer_callback_args_t *p_args);
#endif
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */

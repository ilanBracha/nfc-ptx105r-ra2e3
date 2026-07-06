/* generated common header file - do not edit */
#ifndef COMMON_DATA_H_
#define COMMON_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "r_icu.h"
#include "r_external_irq_api.h"
#include "../ra/fsp/src/rm_comms_lock/rm_comms_lock.h"
#include "rm_comms_spi.h"
#include "rm_comms_api.h"
#include "r_ioport.h"
#include "bsp_pin_cfg.h"
FSP_HEADER
/** External IRQ on ICU Instance. */
extern const external_irq_instance_t g_ext_irq;

/** Access the ICU instance using these structures when calling API functions directly (::p_api is not used). */
extern icu_instance_ctrl_t g_ext_irq_ctrl;
extern const external_irq_cfg_t g_ext_irq_cfg;

#ifndef ptxPLAT_GPIO_IsrCallback
void ptxPLAT_GPIO_IsrCallback(external_irq_callback_args_t *p_args);
#endif
/* SPI Shared Bus */
extern rm_comms_spi_bus_extended_cfg_t g_comms_spi_bus0_extended_cfg;
#define IOPORT_CFG_NAME g_bsp_pin_cfg
#define IOPORT_CFG_OPEN R_IOPORT_Open
#define IOPORT_CFG_CTRL g_ioport_ctrl

/* IOPORT Instance */
extern const ioport_instance_t g_ioport;

/* IOPORT control structure. */
extern ioport_instance_ctrl_t g_ioport_ctrl;
void g_common_init(void);
FSP_FOOTER
#endif /* COMMON_DATA_H_ */

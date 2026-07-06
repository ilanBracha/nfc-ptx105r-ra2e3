/* generated common source file - do not edit */
#include "common_data.h"
icu_instance_ctrl_t g_ext_irq_ctrl;

/** External IRQ extended configuration for ICU HAL driver */
const icu_extended_cfg_t g_ext_irq_ext_cfg =
{ .filter_src = EXTERNAL_IRQ_DIGITAL_FILTER_PCLK_DIV, };

const external_irq_cfg_t g_ext_irq_cfg =
{ .channel = 7, .trigger = EXTERNAL_IRQ_TRIG_RISING, .filter_enable = false, .clock_source_div =
          EXTERNAL_IRQ_CLOCK_SOURCE_DIV_64,
  .p_callback = ptxPLAT_GPIO_IsrCallback,
  /** If NULL then do not add & */
#if defined(NULL)
    .p_context           = NULL,
#else
  .p_context = (void*) &NULL,
#endif
  .p_extend = (void*) &g_ext_irq_ext_cfg,
  .ipl = (2),
#if defined(VECTOR_NUMBER_ICU_IRQ7)
    .irq                 = VECTOR_NUMBER_ICU_IRQ7,
#else
  .irq = FSP_INVALID_VECTOR,
#endif
        };
/* Instance structure to use this module. */
const external_irq_instance_t g_ext_irq =
{ .p_ctrl = &g_ext_irq_ctrl, .p_cfg = &g_ext_irq_cfg, .p_api = &g_external_irq_on_icu };
#if BSP_CFG_RTOS
#if BSP_CFG_RTOS == 1
#if !defined(g_comms_spi_bus0_recursive_mutex)
rm_comms_mutex_t g_comms_spi_bus0_recursive_mutex =
{
    .p_name = "g_comms_spi_bus0 recursive mutex",
};
#endif
#if !defined(g_comms_spi_bus0_blocking_semaphore)
rm_comms_semaphore_t g_comms_spi_bus0_blocking_semaphore =
{
    .p_name =  "g_comms_spi_bus0 blocking semaphore",
};
#endif
#elif BSP_CFG_RTOS == 2
#if !defined(g_comms_spi_bus0_recursive_mutex)
rm_comms_mutex_t g_comms_spi_bus0_recursive_mutex;
#endif
#if !defined(g_comms_spi_bus0_blocking_semaphore)
rm_comms_semaphore_t g_comms_spi_bus0_blocking_semaphore;
#endif
#endif

#endif

/* Shared SPI Bus */
rm_comms_spi_bus_extended_cfg_t g_comms_spi_bus0_extended_cfg =
{
#if BSP_CFG_RTOS

#if !defined(g_comms_spi_bus0_recursive_mutex)
    .p_mutex = &g_comms_spi_bus0_recursive_mutex,
#else
    .p_mutex = NULL,
#endif

#if !defined(g_comms_spi_bus0_blocking_semaphore)
    .p_semaphore = &g_comms_spi_bus0_blocking_semaphore,
#else
    .p_semaphore = NULL,
#endif

    .mutex_timeout  = 0xFFFFFFFF,
    .initialized = false,
#endif

  .p_current_ctrl = NULL, };
ioport_instance_ctrl_t g_ioport_ctrl;
const ioport_instance_t g_ioport =
{ .p_api = &g_ioport_on_ioport, .p_ctrl = &g_ioport_ctrl, .p_cfg = &g_bsp_pin_cfg, };
void g_common_init(void)
{
}

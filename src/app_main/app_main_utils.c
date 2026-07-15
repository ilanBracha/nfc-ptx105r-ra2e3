/*
 * app_main_utils.c
 *
 *  Created on: 1 Apr 2025
 *      Author: a5154862
 */

#include "app_main_utils.h"

/* LED pins shared by both utilities.
 * Mapped for RA2E3 FPB
 */
static const bsp_io_port_pin_t led_pins[] =
{
    APP_MAIN_UTILS_LED_1,
    APP_MAIN_UTILS_LED_2,
};

static const uint32_t LED_COUNT = (uint32_t)(sizeof(led_pins) / sizeof(led_pins[0]));

/* Configure all LED pins as outputs (called once). */
void app_main_utils_led_init (void)
{
    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
        g_ioport.p_api->pinCfg(g_ioport.p_ctrl, led_pins[i],
                               (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT | (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW);
    }
}

/*
 * Drive every LED known to this module (on-board LED1/LED2 + any PMOD1
 * user-board LEDs) to the requested level. This is used by the CLI
 * `ledon` / `ledoff` commands so the user gets visible feedback on the
 * bare RA2E3 FPB even when no PMOD daughter-board is attached.
 *
 * NOTE: `g_bsp_pin_cfg` (used by R_IOPORT_Open at boot) does not include
 * the on-board LED pins (P02_13, P09_14). app_main_utils_led_init() configures them
 * here at first use as outputs.
 */
void app_main_utils_led_set_all(bsp_io_level_t level)
{
    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
        (void)g_ioport.p_api->pinWrite(g_ioport.p_ctrl, led_pins[i], level);
    }
}


/*
 * Blink all on-board LEDs once:  ON -> 150 ms -> OFF.
 * Called by the CLI `blink` command and on card-detection events.
 * (The hold time used to be 500 µs which is far too short to be visible.)
 */
void app_main_utils_blink_leds(void)
{
    app_main_utils_led_set_all(BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(150U, BSP_DELAY_UNITS_MILLISECONDS);
    app_main_utils_led_set_all(BSP_IO_LEVEL_LOW);
}


/*
 * user_board_utils.c
 *
 *  Created on: 1 Apr 2025
 *      Author: a5154862
 */

#include "auc_nfc_card_reader_utils.h"

/* LED pins shared by both utilities.
 * Mapping for RA2E3 FPB:
 *   LED1 = P02_13 (on-board LED1 of the RA2E3 FPB)
 *   LED2 = P09_14 (PMOD1_GPIO9 / external user-board LED)
 *   LED3 = P09_13 (PMOD1_GPIO10 / external user-board LED, == LED_STATUS)
 * Additional user-board LEDs on PMOD1 (P09_13/14/15) are kept for compatibility.
 */
static const bsp_io_port_pin_t led_pins[] =
{
    BSP_IO_PORT_02_PIN_13,   /* LED1 (on-board RA2E3 FPB LED) */
    BSP_IO_PORT_09_PIN_14,   /* LED2 (PMOD1_GPIO9)            */
    BSP_IO_PORT_09_PIN_13,   /* LED3 (PMOD1_GPIO10)           */
    BSP_IO_PORT_09_PIN_15,   /* user-board LED (PMOD1_RESET)  */
};
static const uint32_t LED_COUNT = (uint32_t)(sizeof(led_pins) / sizeof(led_pins[0]));

/* Configure all LED pins as outputs (called once). */
static void led_init(void)
{
    static uint8_t initialized = 0u;
    if (0u == initialized)
    {
        for (uint32_t i = 0; i < LED_COUNT; i++)
        {
            (void)g_ioport.p_api->pinCfg(g_ioport.p_ctrl, led_pins[i],
                                         (uint32_t)IOPORT_CFG_PORT_DIRECTION_OUTPUT |
                                         (uint32_t)IOPORT_CFG_PORT_OUTPUT_LOW);
        }
        initialized = 1u;
    }
}

/* Set all LED pins to the given level. */
static void led_set_all(bsp_io_level_t level)
{
    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
        (void)g_ioport.p_api->pinWrite(g_ioport.p_ctrl, led_pins[i], level);
    }
}

/*
 * Drive every LED known to this module (on-board LED1/LED2 + any PMOD1
 * user-board LEDs) to the requested level. This is used by the CLI
 * `ledon` / `ledoff` commands so the user gets visible feedback on the
 * bare RA2E3 FPB even when no PMOD daughter-board is attached.
 *
 * NOTE: `g_bsp_pin_cfg` (used by R_IOPORT_Open at boot) does not include
 * the on-board LED pins (P02_13, P09_14). led_init() configures them
 * here at first use as outputs.
 */
void UserBoardUtils_SetStatusLed(uint8_t status)
{
    bsp_io_level_t level = (0u != status) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW;
    led_init();
    led_set_all(level);
}

/*
 * Blink all on-board LEDs once:  ON -> 150 ms -> OFF.
 * Called by the CLI `blink` command and on card-detection events.
 * (The hold time used to be 500 µs which is far too short to be visible.)
 */
void UserBoardUtils_BlinkAllLeds(void)
{
    led_init();
    led_set_all(BSP_IO_LEVEL_HIGH);
    R_BSP_SoftwareDelay(150U, BSP_DELAY_UNITS_MILLISECONDS);
    led_set_all(BSP_IO_LEVEL_LOW);
}

/*
 * Blink specific LEDs depending on the detected card technology type:
 *   Type A  ->  LED1 + LED2 + LED3  ON for 10 ms, then OFF
 *   Type B  ->  LED1 only           ON for 20 ms, then OFF
 *   Type F  ->  LED2 only           ON for 20 ms, then OFF
 *   Type V  ->  LED3 only           ON for 20 ms, then OFF
 *
 * LED index mapping (matches the led_pins[] array):
 *   [0] = BSP_IO_PORT_06_PIN_00  (LED1)
 *   [1] = BSP_IO_PORT_04_PIN_14  (LED2)
 *   [2] = BSP_IO_PORT_01_PIN_07  (LED3)
 */
void UserBoardUtils_BlinkForCardType(UserBoardUtils_CardType_t cardType)
{
    /* Bit-mask: bit N = enable led_pins[N].  Bits 0..2 cover LED1/2/3. */
    static const struct
    {
        uint8_t  mask_bits;   /* which of led_pins[] to turn on (bitmask) */
        uint32_t duration_ms; /* how long to keep them on                 */
    } cfg[] =
    {
        /* CardType_A */ { 0x07u, 10u },   /* LED1+LED2+LED3, 10 ms */
        /* CardType_B */ { 0x01u, 20u },   /* LED1 only,      20 ms */
        /* CardType_F */ { 0x02u, 20u },   /* LED2 only,      20 ms */
        /* CardType_V */ { 0x04u, 20u },   /* LED3 only,      20 ms */
    };

    led_init();

    /* Clamp to valid range; fall back to "all LEDs" if unknown. */
    uint32_t idx = (uint32_t)cardType;
    if (idx >= (uint32_t)(sizeof(cfg) / sizeof(cfg[0])))
    {
        idx = 0u;
    }

    uint8_t  mask = cfg[idx].mask_bits;
    uint32_t dur  = cfg[idx].duration_ms;

    /* Turn on selected LEDs. */
    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
        if (0u != (mask & (uint8_t)(1u << i)))
        {
            (void)g_ioport.p_api->pinWrite(g_ioport.p_ctrl, led_pins[i], BSP_IO_LEVEL_HIGH);
        }
    }

    /* Hold for the requested duration. */
    R_BSP_SoftwareDelay(dur, BSP_DELAY_UNITS_MILLISECONDS);

    /* Turn off selected LEDs. */
    for (uint32_t i = 0; i < LED_COUNT; i++)
    {
        if (0u != (mask & (uint8_t)(1u << i)))
        {
            (void)g_ioport.p_api->pinWrite(g_ioport.p_ctrl, led_pins[i], BSP_IO_LEVEL_LOW);
        }
    }
}

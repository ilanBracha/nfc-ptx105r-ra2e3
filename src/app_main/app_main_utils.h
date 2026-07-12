/*
 * user_board_utils.h
 *
 *  Created on: 1 Apr 2025
 *      Author: a5154862
 */

#ifndef APP_MAIN_UTILS_H_
#define APP_MAIN_UTILS_H_

#include "hal_data.h"

#define APP_MAIN_UTILS_LED_RD     BSP_IO_PORT_09_PIN_15
#define APP_MAIN_UTILS_LED_ACTV   BSP_IO_LEVEL_HIGH
#define APP_MAIN_UTILS_LED_INACTV BSP_IO_LEVEL_LOW

/**
 * Selects which LEDs light up and for how long on card detection.
 *   CardType_A  ->  LED1 + LED2 + LED3  (10 ms)
 *   CardType_B  ->  LED1 only           (20 ms)
 *   CardType_F  ->  LED2 only           (20 ms)
 *   CardType_V  ->  LED3 only           (20 ms)
 */
typedef enum
{
    UserBoardUtils_CardType_A = 0,  /**< Type-A: all 3 LEDs, 10 ms */
    UserBoardUtils_CardType_B,      /**< Type-B: LED1 only,  20 ms */
    UserBoardUtils_CardType_F,      /**< Type-F: LED2 only,  20 ms */
    UserBoardUtils_CardType_V,      /**< Type-V: LED3 only,  20 ms */
} app_main_utils_card_type_t;

    /**
     * \brief Set the red status led
     */
extern void app_main_utils_set_stat_led(uint8_t status);

    /**
     * \brief Blink all on-board user LEDs once on card detection:
     *        LEDs turn ON, hold for 500 µs, then turn OFF.
     */
extern void app_main_utils_blink_leds(void);

    /**
     * \brief Blink LEDs according to detected card technology type.
     *        Type A -> all 3 LEDs 10 ms | B -> LED1 20 ms |
     *        F -> LED2 20 ms | V -> LED3 20 ms
     */
extern void app_main_utils_blink_for_card_type(app_main_utils_card_type_t cardType);

#endif /* APP_MAIN_UTILS_H_ */

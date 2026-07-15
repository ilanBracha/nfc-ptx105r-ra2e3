/*
 * user_board_utils.h
 *
 *  Created on: 1 Apr 2025
 *      Author: a5154862
 */

#ifndef APP_MAIN_UTILS_H_
#define APP_MAIN_UTILS_H_

#include "hal_data.h"

#define APP_MAIN_UTILS_LED_1      BSP_IO_PORT_02_PIN_13
#define APP_MAIN_UTILS_LED_2      BSP_IO_PORT_09_PIN_14

void app_main_utils_led_init(void);
void app_main_utils_blink_leds(void);
void app_main_utils_led_set_all(bsp_io_level_t level);

#endif /* APP_MAIN_UTILS_H_ */

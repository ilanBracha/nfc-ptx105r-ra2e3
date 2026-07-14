/**
 * rs_timeout.c
 *
 * Timeout helper. Uses the HAL sleep for now; a hardware-timer version
 * could be added for non-blocking support.
 */

#include "rs_nfc_ptx105r.h"

void rs_timeout_sleep_ms(uint32_t ms)
{
    rs_nfc_ptx_sleep(ms);
}

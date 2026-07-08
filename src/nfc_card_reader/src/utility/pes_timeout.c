/**
 * pes_timeout.c
 *
 * Timeout helper. Uses the HAL sleep for now; a hardware-timer version
 * could be added for non-blocking support.
 */

#include "pes_nfc_ptx105r.h"

void pes_timeout_sleep_ms(uint32_t ms)
{
    pes_nfc_ptx_sleep(ms);
}

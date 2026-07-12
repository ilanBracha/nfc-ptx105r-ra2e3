#include "new_thread0.h"
#include "app_main.h"

/* New Thread entry function */
/* pvParameters contains TaskHandle_t */
void new_thread0_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    /* Start the NFC card reader application.
     * This runs the UART/CLI init then the blocking NFC event loop.
     * The function only returns on fatal error. */
    app_main_entry();

    /* Should not reach here; suspend if it does. */
    vTaskSuspend(NULL);
}

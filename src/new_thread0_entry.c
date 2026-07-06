#include "new_thread0.h"
#include "auc_nfc_card_reader.h"

/* New Thread entry function */
/* pvParameters contains TaskHandle_t */
void new_thread0_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    /* Start the NFC card reader application.
     * This runs the UART/CLI init then the blocking NFC event loop.
     * The function only returns on fatal error. */
    auc_nfc_card_reader_entry();

    /* Should not reach here; suspend if it does. */
    vTaskSuspend(NULL);
}

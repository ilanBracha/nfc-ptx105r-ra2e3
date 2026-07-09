/**
 * pes_nfc_ptx105r.h
 *
 * Hardware Abstraction Layer for the PES NFC Card Reader module.
 * Isolates PES business logic from the vendor-specific NFC stack
 * (currently: Renesas PTX105R, calling the PTX NFC SDK's ptxIoTRd_*
 * functions directly — no RM_NFC_READER_PTX FSP wrapper).
 *
 * Each function below is implemented directly by the PTX105R backend
 * (pes_nfc_ptx105r.c) and called directly by name — no function-
 * pointer vtable indirection.
 */

#ifndef PES_NFC_PTX105R_H
#define PES_NFC_PTX105R_H

#include "pes_nfc_card_reader.h"
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations — full definitions in ptxNDEF_T4TOP.h / ptxNDEF_T5TOP.h
 * (PTX SDK). Do NOT typedef here to avoid collision with the SDK headers'
 * own typedefs. We use only the lean T4T/T5T NDEF-OP components (not the
 * generic ptxNDEF dispatcher) — see pes_nfc_ptx105r.c for why. */
struct ptxNDEF_T4TOP;
struct ptxNDEF_T5TOP;

#ifdef __cplusplus
extern "C" {
#endif

/**********************************************************************************************************************
 * Discovery status
 **********************************************************************************************************************/
typedef enum {
    PES_NFC_DISC_NO_CARD = 0,
    PES_NFC_DISC_CARD_ACTIVE,
    PES_NFC_DISC_RUNNING,
    PES_NFC_DISC_DONE,
} pes_nfc_disc_status_t;

/**********************************************************************************************************************
 * Activated card info (filled after activation)
 **********************************************************************************************************************/
#define PES_NFC_PTX_RX_BUF_SIZE   300U
#define PES_NFC_PTX_TX_BUF_SIZE   280U

typedef struct {
    pes_nfc_card_type_t card_type;
    pes_nfc_protocol_t  protocol;
    uint8_t             uid[PES_NFC_UID_MAX_BYTES];
    uint8_t             uid_len;
} pes_nfc_ptx_card_info_t;

/**********************************************************************************************************************
 * PTX functions
 **********************************************************************************************************************/
pes_status_t pes_nfc_ptx_open(pes_nfc_reader_device_t device);
pes_status_t pes_nfc_ptx_close(void);
/** Returns true if pes_nfc_ptx_open() has succeeded and pes_nfc_ptx_close()
 * has not since been called. Replaces the FSP ctrl block's `open` flag. */
bool pes_nfc_ptx_is_open(void);
pes_status_t pes_nfc_ptx_configure_polling(pes_nfc_tech_mask_t tech_mask);
pes_status_t pes_nfc_ptx_start_polling(void);
pes_status_t pes_nfc_ptx_stop_polling(void);
pes_status_t pes_nfc_ptx_wait_for_card(uint32_t timeout_ms,
                                       pes_nfc_disc_status_t *out_status);
pes_status_t pes_nfc_ptx_activate_card(pes_nfc_ptx_card_info_t *card_info);
pes_status_t pes_nfc_ptx_get_card_type(pes_nfc_card_type_t *out_type);
pes_status_t pes_nfc_ptx_get_uid(uint8_t *uid, uint8_t *uid_len);
void pes_nfc_ptx_sleep(uint32_t ms);
pes_status_t pes_nfc_ptx_data_exchange(const uint8_t *tx, uint32_t tx_len,
                                       uint8_t *rx, uint32_t *rx_len);
pes_status_t pes_nfc_ptx_deactivate(void);
pes_status_t pes_nfc_ptx_get_system_state(uint8_t *out_state);
pes_status_t pes_nfc_ptx_get_last_rf_error(uint8_t *out_err);
void pes_nfc_ptx_wake_waiting_task(void);

/** Open the SDK T4T NDEF component. Call after activating an ISO-DEP
 *  (Type 4 Tag) card, before PES_NFCCardReader_ReadCardInfo/WriteNDEF. */
pes_status_t pes_nfc_ptx_ndef_open(void);
/** Close the SDK T4T NDEF component (call after NDEF operations are done). */
void pes_nfc_ptx_ndef_close(void);
/** Get a pointer to the static ptxNDEF_T4TOP_t instance. Valid after ndef_open(). */
struct ptxNDEF_T4TOP * pes_nfc_ptx_get_ndef_comp(void);

/** Open the SDK T5T NDEF component. Call after activating a T5T
 *  (ISO 15693) card, before PES_NFCCardReader_ReadCardInfo/WriteNDEF. */
pes_status_t pes_nfc_ptx_ndef_t5t_open(void);
/** Close the SDK T5T NDEF component (call after NDEF operations are done). */
void pes_nfc_ptx_ndef_t5t_close(void);
/** Get a pointer to the static ptxNDEF_T5TOP_t instance. Valid after ndef_t5t_open(). */
struct ptxNDEF_T5TOP * pes_nfc_ptx_get_ndef_t5t_comp(void);

/**********************************************************************************************************************
 * Internal forward declarations
 **********************************************************************************************************************/

/* detection/pes_nfc_detect.c */
pes_status_t pes_nfc_detect_wait(uint32_t timeout_ms,
                                 pes_nfc_disc_status_t *out_status);

/* utility/pes_retry.c */
pes_status_t pes_nfc_retry(const void *cfg, void *result_out,
                           uint8_t max_retries);

/* utility/pes_timeout.c */
void pes_timeout_sleep_ms(uint32_t ms);

/* pes_nfc_card_reader.c  (used by retry) */
pes_status_t pes_nfc_card_reader_try_once(const void *cfg, void *result_out);

/* pes_nfc_card_reader.c — stop flag (checked by detect_wait & event loop) */
bool pes_nfc_card_reader_is_stop_requested(void);

/* pes_nfc_card_reader.c — card summary */
uint32_t pes_card_summary_build(const pes_nfc_card_result_t *res,
                                char *buf, uint32_t buf_size);

/* utility/pes_nfc_uid.c */
uint32_t pes_nfc_uid_to_hex(const uint8_t *uid, uint8_t uid_len,
                            char *out, uint32_t out_size);
uint32_t pes_nfc_uid_to_hex_colon(const uint8_t *uid, uint8_t uid_len,
                                  char *out, uint32_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* PES_NFC_PTX105R_H */

#include "pes_nfc_card_reader.h"

/**
 * Detect and read one NFC card/tag using the configured reader.
 *
 * Non-blocking default:
 *   Set cfg->callback to a non-NULL function.
 *   Returns PES_OK immediately after the background task is started.
 *   result_out is populated before the callback fires.
 *
 * Blocking opt-in:
 *   Set cfg->callback = NULL.
 *   Returns only after card read completes, times out, or fails.
 *   result_out is populated on PES_OK.
 *
 * Maximum blocking duration:
 *   cfg->timeout_ms + implementation-defined activation/read timeout.
 *
 * Not re-entrant. Only one call may be in flight at a time.
 *
 * Precondition:
 *   PTX105R board, host interface, clocks, IRQ, and lower-level NFC stack
 *   are initialized by the caller or board support layer.
 *
 * @param cfg        [in]  Configuration. Must not be NULL.
 * @param result_out [out] Populated on PES_OK. Caller owns the buffer.
 *                         May be NULL in non-blocking mode if result
 *                         is not needed after the callback.
 *
 * @return PES_OK on success, or on non-blocking start,
 *         otherwise PES_ERR_*.
 */
pes_status_t PES_NFCCardReader_Read(const pes_nfc_card_reader_cfg_t * cfg, pes_nfc_card_result_t * result_out)
{
    pes_status_t status = PES_OK;

    PES_COMMON_UNUSED(cfg);
    PES_COMMON_UNUSED(result_out);

    /* TODO: Implement NFC card reading logic */

    return status;
}

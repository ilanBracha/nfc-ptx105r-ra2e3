/**
 * pes_common.h
 *
 * Shared types, status codes, and callback convention for all PES modules.
 * This is the only header that every PES module and every AUC must include.
 *
 * RULES:
 *   - This file must never include any FSP header.
 *   - This file must never include any FreeRTOS header.
 *   - New status codes are added here when a module needs them.
 *     PES_OK must always equal 0 (= FSP_SUCCESS).
 */

#ifndef PES_COMMON_H
#define PES_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PES_COMMON_UNUSED(x) (void)(x)

/* ── Status codes ────────────────────────────────────────────────────
   Every PES function that can fail returns pes_status_t.
   PES_OK = 0 = FSP_SUCCESS — the only alignment point with FSP.    */
typedef enum {
    PES_OK                  =   0,  /* success                               */
    PES_ERR_TIMEOUT         =  -1,  /* operation timed out                   */
    PES_ERR_CRED_INVALID    =  -2,  /* credential format validation failed   */
    PES_ERR_CONN_FAIL       =  -3,  /* connection attempt failed             */
    PES_ERR_NO_IP           =  -4,  /* DHCP did not assign an IP             */
    PES_ERR_NO_CLOUD        =  -5,  /* cloud endpoint unreachable            */
    PES_ERR_DEPENDENCY      =  -6,  /* precondition / dependency check fail  */
    PES_ERR_INVALID_CFG     =  -7,  /* cfg field missing or invalid          */
    PES_ERR_NOT_FOUND       =  -8,  /* item not found (e.g. NDEF record)     */
    PES_ERR_BUFFER_OVERFLOW =  -9,  /* output buffer too small               */
    PES_ERR_INTERNAL        = -99,  /* unexpected internal error             */
} pes_status_t;

/* ── Callback convention ─────────────────────────────────────────────
   All non-blocking PES functions use this callback type.

   @param status    PES_OK on success, or PES_ERR_* on failure.
   @param p_context Blind pointer registered by the caller alongside
                    the callback. PES passes it back unmodified.
                    PES never reads or writes through p_context.
                    The caller casts it back to their own struct type
                    inside the callback to recover application state. */
typedef void (*pes_callback_t)(pes_status_t status, void *p_context);

/* ── PES_LOG macro ───────────────────────────────────────────────────
   PES never calls printf() directly. This macro defaults to silent.
   Enable in CMakeLists.txt:
     target_compile_definitions(my_app PRIVATE
       'PES_LOG(fmt,...)=my_log("[PES] " fmt, ##__VA_ARGS__)') */
#ifndef PES_LOG
#define PES_LOG(fmt, ...)   /* default: silent */
#endif

#ifdef __cplusplus
}
#endif

#endif /* PES_COMMON_H */

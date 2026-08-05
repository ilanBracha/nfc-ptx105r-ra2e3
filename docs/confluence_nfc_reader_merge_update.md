PES Creation Progress

Candidate Acceptance: Review and Accept AUC Requirements

Architect & Design: Define PES Structure and Interfaces

Develop & Test: Implement and Validate on Target Hardware

Document & Package: Prepare PES for Consumption

Release & Permutate: Deploy and Expand Across Targets

Project Repo: https://gitlab.global.renesas.com/mcusys/pes_il/-/tree/main/pes/nfc?ref_type=heads

Candidate Acceptance

Tasks:

Get the official O&D from Sree

Which RA hardware is targeted?
Which communication protocols between RA and PTX105R are supported?
Which NFC card types are supported?
What is the functionality needed (to ensure the APIs cover them)?
AUC Definition

The following AUC information was extracted from the O&Ds: 
Official O&D and Related Files: https://confluence.eng.renesas.com/spaces/PESKB/pages/422342839/NFC+Card+Reader+Using+PTX105R+Common+AUC

Setup Configuration - PTX105R attached to FPB-RA2E3 via PMOD


 




























Expected functionality

Self-initialization upon power on  

PTX105R should read NFC tags every time then they attached

The read information is printed in CLI via UART

























The NFC reader AUC project has following building blocks
AUC system blocks are unique per project. They define specific combination of HW components, SW configuration and should be manually adopted for every change.














































BSP Configuration
LLD Configuration
r_gpt - General Porpoise Timer
r_dtc - Data Transfer Controller
r_icu - External Interrupt
r_ioport - I/O Port
r_sci_spi - SPI serial communication interface
r_sci_uart - UART serial communication interface
Communication interfaces initialization
SPI - communication with PTX105R module
UART - communication with the PC terminal
CLI - output + input (optional)
Print the output data from PES to the terminal on PC




RA Hardware is targeted
According to combined information from MRD, Demos and Hands-on marketing presentation the mentioned MCUs with NFC module are:

RA2E3 (main, explicitly required by MRD)

RA6E2, RA8E1 (optional, NFC module mentioned in marketing presentations)

PES Definition
Abstract

This document defines the architecture, API contracts, internal structure, and integration guidelines for the NFC Card Reader PES module targeting the Renesas PTX105R NFC Reader. The module provides a reusable, chip-portable library that customer applications can call to detect, activate, and read NFC cards or tags with minimal integration effort.

The PTX105R is a highly integrated NFC reader intended for contactless communication and optimized for reader performance and interoperability. It supports common NFC reader/writer use cases including ISO/IEC 14443-A/B, NFC Forum reader/writer mode, NFC Tag Types 2, 3, 4A/4B, and 5, FeliCa, and ISO/IEC 15693.

Version 1.0 of this PES design focuses on single-card detection and read, using PTX105R as the NFC reader front end. Non-blocking operation is the default integration model, with blocking mode available as an option.

Its purpose is to allow a customer application to:

Initialize a card-read operation.
Multi-card detection & read via a per-card event callback (IRQ-driven, no host-side wake-up timer, no periodic register access).
Non-blocking as the recommended integration model; blocking is available as an opt-in.
Activate the detected card.
Initialize a card-read operation.
Wait for an NFC card or tag (RF discovery done by PTX105R IC + LPCD).
Activate the detected card.
Read basic card identity information (protocol, UID, tag-type name).
Read NDEF payload data.
Optionally trigger a raw protocol frame exchange (T2T / T3T / T5T / NFC-DEP)
Return card data to the application through a small public API.
Avoid direct dependency on PTX105R-specific driver calls in customer code.
Read basic card identity information.
Read NDEF payload data.
Per-card event callback delivering identity, NDEF, extended tag info and (optionally) a raw protocol exchange.
Return card data to the application through a simple public API.
Avoid direct dependency on PTX105R-specific driver calls in customer code.

The PTX105R uses a split-stack architecture in which time-critical NFC operations are handled by the IC and higher-level protocol logic runs on the host processor through the PTX software stack and NSC interface. This PES wraps that integration behind a small customer-facing API. On the host side, the PESis interrupt-driven: the PTX105R signals card events on its IRQ line, theadapter's ISR wakes the read task, and results are delivered to theapplication via callback. There is no host-side wake-up timer and nohost-side periodic register access; the task blocks on FreeRTOS task-notifyat 0% CPU between events. The PTX105R adapter module is the only translation unit in the PES that calls PTX105R SDK / NSC / board driver APIs; all other PES sources depend only on this adapter's plain C functions. On the host side, the PES is fully interrupt-driven: the PTX105R signals card events on its IRQ line, the adapter's ISR wakes the read task, and the result is delivered to the application via callback. No periodic host task or software polling loop is required. RF-level polling remains a property of the PTX105R IC and its LPCD configuration. (renesas.com)

Scope

This document covers:

PTX105R-based NFC reader integration.
Card/tag detection.
Card activation.
UID, protocol and tag-type reporting.
Extended card-info readout (data area size, writeable flag, human-readable tag type).
NDEF read (T2T / T3T / T4T / T5T).
Optional raw protocol exchange (T2T READ, T3T CHECK, T5T READ_SINGLE_BLOCK, NFC-DEP SYMM).
Runtime dependency validation.
Graceful stop of an in-flight read.
Blocking (opt-in) and non-blocking execution modes.
Out of Scope
Payment / EMVCo transaction logic.
Secure element integration.
Card emulation / HCE application behavior.
Peer-to-peer application protocol logic.
Writing cards or tags.
Cryptographic authentication for protected cards.
MIFARE Classic crypto handling.
Antenna tuning and RF certification.
PTX105R firmware image generation.
Board bring-up, pinmux, clock setup, and FSP stack initialization.
Persistent storage of card data.
UI, LEDs, buzzer, display, or access-control policy decisions.
Multi tag
MCU Power Save
Architect & Design
Design Goals
Goal	Description


Ease of adoption

	

A customer should complete a working NFC card-read integration in <1 hour using only the public header and this document.




Non-blocking by default

	

Card polling can run in the background without blocking the customer RTOS task. Blocking mode is available as an opt-in.




PTX105R isolation

	

Application code does not call PTX105R SDK, NSC, SPI, I2C, or UART APIs directly. Inside the PES, only the PTX105R adapter module (pes_nfc_ptx105r.c) calls PTX105R SDK / NSC APIs.




No MCU vendor SDK dependency

	

The PES does not link against or include Renesas FSP or any other MCU vendor SDK. The adapter depends only on the PTX105R SDK / NSC stack




Testability

	

Card-read sequencing is decoupled from PTX105R/FSP via an internal HAL interface so logic can be host-tested.




No dynamic allocation

	

all state is static or caller-provided; task/buffers are declared static.




Protocol flexibility

	

customer selects technologies via tech_mask.




Single responsibility

	

Orchestrator coordinates; protocol, NDEF, and timing logic remain separate.




Portable structure

	

The public PES API should allow future NFC reader ICs without changing customer code.




IRQ-only host wait

	

no host-side wake-up timer and no host-side periodic register access. The task blocks on the PTX105R IRQ; it is unblocked only by (a) a card / fault event surfaced by the SDK IRQ handler, (b) rs_nfc_reader_Stop(), or (c) the caller-supplied timeout_ms expiry. Idle CPU / SPI activity = 0.

Layer Responsibilities
Layer	File(s)	Responsibility	PTX105R SDK / NSC dependency


Public API

	

rs_nfc_reader.h

	

Customer-facing types, constants, and function signatures.

	

None




Orchestrator + Detection

	

rs_nfc_reader.c

	

Validates config, controls blocking/non-blocking flow, sequences detect/read operation. Detection wait, retry, and raw exchange were merged into this file.

	

Indirect via PTX105R adapter




NDEF Engine

	

rs_ndef_read.c

	

Reads NDEF data when requested and supported by detected card; decodes the NDEF message into result->decoded.

	

Indirect via PTX105R adapter




PTX105R Adapter

	

rs_nfc_ptx105r.h 

rs_nfc_ptx105r.c

	

The only translation unit that includes PTX105R SDK / NSC headers. Maps PES-internal NFC calls to PTX105R SDK / NSC calls. Uses board-provided callbacks for host interface transfers, GPIO IRQ hook, delay, and timestamp. Does not include FSP or any other MCU vendor SDK.

	

Yes (PTX105R SDK / NSC only)




Dependency Module

	

rs_nfc_reader.c (merged)

	

Optional runtime dependency validation.

	

Yes

Public API

The customer-facing API (declared in rs_nfc_reader.h) consists of two functions:

rs_nfc_reader_Read() - Detect and read NFC cards according to the supplied configuration. Blocking or non-blocking; multicard event mode when on_card_event is set, single-shot when on_card_event is NULL.

rs_nfc_reader_Stop() - Request graceful stop of a running Read. Safe if no operation is in flight. Always returns RS_OK.

NOTE: Card-info reading and NDEF decoding are NOT public. rs_ndef_read_card_info() and rs_ndef_decode_message() are module-internal helpers: during Read() the module reads the card info and decodes the NDEF message into result->decoded, so the application consumes result->decoded directly. The Wi-Fi/Bluetooth handover decoders and the TLV/string helpers have been removed.

NOTE: Config validation and the raw-frame exchange are NOT public. rs_nfc_reader_validate() and rs_nfc_reader_raw_exchange() are module-private (static) helpers inside rs_nfc_reader.c. Validation is invoked automatically on entry to rs_nfc_reader_Read() and gated by cfg->cfg_valid_check_en; the raw exchange is driven automatically by cfg->run_raw_exchange.

Public Types
Transport / Interface Selection
PTX105R hardware supports SPI, I2C and UART host interfaces. The host  interface is selected by board configuration and the adapter  implementation; it is NOT exposed by the public API.  (The MRD's rs_nfc_reader_device_t enum was removed - one device today.)




NFC Technology Mask (include from SDK code)

typedef uint32_t rs_nfc_tech_mask_t;

#define RS_NFC_TECH_ISO14443A      (1UL << 0)
#define RS_NFC_TECH_ISO14443B      (1UL << 1)
#define RS_NFC_TECH_FELICA             (1UL << 2)
#define RS_NFC_TECH_ISO15693        (1UL << 3)
#define RS_NFC_TECH_NFC_FORUM   (1UL << 4)

#define RS_NFC_TECH_ALL  (RS_NFC_TECH_ISO14443A | \
                                               RS_NFC_TECH_ISO14443B | \
                                               RS_NFC_TECH_FELICA        | \
                                               RS_NFC_TECH_ISO15693    | \
                                               RS_NFC_TECH_NFC_FORUM)




PTX105R supports ISO/IEC 14443-A/B reader/writer mode, NFC Forum reader/writer mode, FeliCa reader/writer mode, ISO/IEC 15693 reader/writer mode, and NFC Tag Types 2, 3, 4A/4B, and 5:

NFC Forum Tag Type        RF Technology      Standard
T1T                                   NFC-A                   ISO 14443A
T2T                                   NFC-A                   ISO 14443A
T3T                                   NFC-F (FeliCa)      JIS X 6319-4 / FeliCa
T4T-A                               NFC-A                   ISO 14443A
T4T-B                               NFC-B                   ISO 14443B
T5T                                   NFC-V                  ISO 15693

Detected Card Type

typedef enum {

RS_NFC_CARD_TYPE_UNKNOWN = 0,
RS_NFC_CARD_TYPE_ISO14443A,
RS_NFC_CARD_TYPE_ISO14443B,
RS_NFC_CARD_TYPE_FELICA,
RS_NFC_CARD_TYPE_ISO15693,
RS_NFC_CARD_TYPE_NFC_TAG_TYPE_2,
RS_NFC_CARD_TYPE_NFC_TAG_TYPE_3,
RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4A,
RS_NFC_CARD_TYPE_NFC_TAG_TYPE_4B,
RS_NFC_CARD_TYPE_NFC_TAG_TYPE_5,

} rs_nfc_card_type_t;




Active RF Protocol
typedef enum { 

RS_NFC_PROT_UNDEFINED = 0,
RS_NFC_PROT_T2T,
RS_NFC_PROT_T3T,
RS_NFC_PROT_ISODEP,
RS_NFC_PROT_NFCDEP,
RS_NFC_PROT_T5T,
RS_NFC_PROT_EXTENSION,

} rs_nfc_protocol_t;

Callback Type
/* Fires once when a non-blocking rs_nfc_reader_Read() completes * (timeout, fatal error, or Stop() was called).*/
typedef void (*rs_nfc_callback_t)(rs_status_t status, void *p_context);   

/* Fires per detected & activated card during Read(). result/summary * are read-only and MUST NOT be retained past the call - buffers are      * reused on the next event. */
typedef void (*rs_nfc_card_event_cb_t)(rs_status_t status, const rs_nfc_card_result_t *result, const char *summary, void *p_context);
Configuration Struct (actual — rs_nfc_reader.h)
typedef struct {
    /* RF technologies to enable during discovery (bitmask of
     * RS_NFC_TECH_* flags, e.g. RS_NFC_TECH_ALL). Must be non-zero. */
    rs_nfc_tech_mask_t      tech_mask;

    /* Run duration of Read() in ms. UINT32_MAX = loop forever. */
    uint32_t                timeout_ms;
    uint8_t                 retry_count;

    /* Read options */
    bool                    read_ndef;
    uint16_t                max_ndef_bytes;   /* clamped to RS_NFC_NDEF_MAX_BYTES */

    /* Opt-in demo: protocol-appropriate raw frame exchange after activation
     * (T2T READ / T3T CHECK / T5T READ_SINGLE_BLOCK / NFC-DEP SYMM). Exposed
     * to on_card_event via result->raw_exchange. No effect for ISO-DEP or
     * when on_card_event is NULL. */
    bool                    run_raw_exchange;

    /* Non-blocking support:
     * callback == NULL -> blocking
     * callback != NULL -> non-blocking (spawns static task, fires on done) */
    rs_nfc_callback_t       callback;
    void                  * p_context;

    /* Per-card event (fires once per detected/activated card during Read()).
     * When set, the orchestrator runs in continuous-loop mode. */
    rs_nfc_card_event_cb_t  on_card_event;
    void                  * p_card_event_context;

    /* Optional runtime dependency validation */
    bool                    validate_dependencies;

    /* Gate for the internal validator inside Read(). Because
     * memset(&cfg,0,...) zero-inits this to false, callers MUST set it to
     * true explicitly to keep validation enabled. */
    bool                    cfg_valid_check_en;
} rs_nfc_reader_cfg_t;

NOTE: There is no polling_interval_ms field in the current implementation.
The RF poll cadence is a property of the PTX105R LPCD configuration and is
not exposed by the PES config.
Result Struct (actual — rs_nfc_reader.h)

#define RS_NFC_UID_MAX_BYTES       10U
#define RS_NFC_NDEF_MAX_BYTES      512U /* absolute cap for the NDEF buffer */

typedef struct st_rs_nfc_raw_exchange {
    bool           valid;
    rs_status_t    status;
    const uint8_t *tx;
    uint32_t       tx_len;
    const uint8_t *rx;
    uint32_t       rx_len;
} rs_nfc_raw_exchange_t;

struct st_rs_nfc_card_result {
    rs_nfc_card_type_t   card_type;
    rs_nfc_protocol_t    protocol;      /* active RF protocol */
    uint8_t              uid[RS_NFC_UID_MAX_BYTES];
    uint8_t              uid_len;
    bool                 ndef_present;
    uint8_t              ndef_data[RS_NFC_NDEF_MAX_BYTES];
    uint16_t             ndef_len;
    rs_ndef_decoded_t    decoded;        /* NDEF records parsed from ndef_data */
    int8_t               rssi_dbm;       /* optional, HAL may return 0 */
    uint32_t             read_time_ms;

    /* Extended card-info (populated internally during Read()) */
    uint32_t             data_area_size;
    bool                 writeable;
    const char         * tag_type_name;  /* static string — do NOT free */

    /* Last raw exchange (check raw_exchange.valid before use) */
    rs_nfc_raw_exchange_t raw_exchange;
};

Status Codes

The module uses the shared rs_status_t enum (defined in rs_nfc_reader.h). The
current, actual values are:

Code | Value | Meaning
RS_OK | 0 | Success. result_out is valid.
RS_ERR_TIMEOUT | -1 | Run duration elapsed (or no card before timeout_ms in single-shot).
RS_ERR_CRED_INVALID | -2 | Reserved (shared status code, not used by the NFC reader).
RS_ERR_CONN_FAIL | -3 | Reserved (shared status code, not used by the NFC reader).
RS_ERR_NO_IP | -4 | Reserved (shared status code, not used by the NFC reader).
RS_ERR_NO_CLOUD | -5 | Reserved (shared status code, not used by the NFC reader).
RS_ERR_DEPENDENCY | -6 | Dependency check failed (validate_dependencies == true).
RS_ERR_INVALID_CFG | -7 | Required config field is missing or invalid.
RS_ERR_NOT_FOUND | -8 | Requested data not present (e.g. card is not NDEF formatted).
RS_ERR_BUFFER_OVERFLOW | -9 | Payload exceeds the destination buffer (e.g. NDEF > max_ndef_bytes).
RS_ERR_INTERNAL | -99 | Unexpected internal error (adapter/SDK failure, busy re-entry, PTX system fault).

NOTE: There are no card-specific status codes (RS_ERR_CARD_*, RS_ERR_NDEF_*,
RS_ERR_BUSY) in the current implementation. Card activation / read / NDEF
failures surface as RS_ERR_INTERNAL, RS_ERR_NOT_FOUND, or
RS_ERR_BUFFER_OVERFLOW, and a rejected non-blocking re-entry returns
RS_ERR_INTERNAL.
PES Main Function Definition (actual — rs_nfc_reader.h)
/**
 * Read NFC cards according to the supplied configuration.
 *
 * Blocking mode (cfg->callback == NULL):
 *   Blocks until timeout_ms elapses or a fatal error occurs.
 *
 * Non-blocking mode (cfg->callback != NULL):
 *   Spawns a dedicated static FreeRTOS task, returns RS_OK immediately.
 *   The callback fires once when the operation completes. Only one
 *   non-blocking Read() may be active at a time; a second call while a
 *   task is running returns RS_ERR_INTERNAL.
 *
 * In both modes, if on_card_event is set the orchestrator runs in
 * continuous-loop mode and fires one event per detected card.
 */
rs_status_t rs_nfc_reader_Read(const rs_nfc_reader_cfg_t * cfg,
                               rs_nfc_card_result_t      * result_out);

/* Request graceful stop of a running Read. Task context only. Returns RS_OK. */
rs_status_t rs_nfc_reader_Stop(void);
Validation (internal — not part of the public API)
/*
 * Config validation is a module-private (static) helper inside
 * rs_nfc_reader.c, invoked automatically at the top of rs_nfc_reader_Read()
 * and gated by cfg->cfg_valid_check_en:
 *
 *   cfg_valid_check_en == true  -> validate cfg fields on entry
 *   cfg_valid_check_en == false -> skip validation (a NULL cfg is still
 *                                  rejected regardless of this flag)
 *
 * Because memset(&cfg,0,sizeof(cfg)) zero-inits cfg_valid_check_en to false,
 * callers MUST set it to true explicitly to keep validation enabled.
 *
 * Checks: cfg non-NULL; tech_mask non-zero; timeout_ms > 0 for single-shot;
 * if read_ndef: max_ndef_bytes > 0 and <= RS_NFC_NDEF_MAX_BYTES. When
 * validate_dependencies == true, also checks rs_nfc_ptx_is_open().
 * Returns RS_OK, RS_ERR_INVALID_CFG, or RS_ERR_DEPENDENCY.
 */
Application Integration in AUC
Non-Blocking Integration

typedef struct {

                rs_nfc_card_result_t result;

                app_ui_handle_t      *p_ui;

} app_nfc_ctx_t;

static app_nfc_ctx_t g_nfc_ctx;

static void on_nfc_read_done(rs_status_t status, void *p_context)

{

    app_nfc_ctx_t *ctx = (app_nfc_ctx_t *)p_context;

    if (status == RS_OK) {

        app_access_handle_card_uid(ctx->result.uid, ctx->result.uid_len);

        if (ctx->result.ndef_present) {

            app_ndef_handle_payload(ctx->result.ndef_data,

                                    ctx->result.ndef_len);

        }

        app_ui_show_nfc_success(ctx->p_ui);

    } else {

        app_ui_show_nfc_error(ctx->p_ui, status);

    }

}

void app_start_nfc_read(void)

{

    g_nfc_ctx.p_ui = app_get_ui_handle();

    rs_nfc_reader_cfg_t cfg = {

        .tech_mask                    = RS_NFC_TECH_ALL,

        .timeout_ms                  = 10000,

        .retry_count                   = 1,

        .read_ndef                     = true,

        .max_ndef_bytes           = sizeof(g_nfc_ctx.result.ndef_data),

        .callback                       = on_nfc_read_done,

        .p_context                     = &g_nfc_ctx,

        .validate_dependencies = true,

        .cfg_valid_check_en      = true,

    };




    s = rs_nfc_reader_Read(&cfg, &g_nfc_ctx.result);

    if (s != RS_OK) {

        app_ui_show_nfc_error(g_nfc_ctx.p_ui, s);

    }

    /* Returns immediately. Callback fires when card read completes. */

}

Blocking Integration (optional)

rs_nfc_reader_Read() must not be called from ISR context.

Only one NFC card-read operation may be in flight at a time.

PES Internal Design
Card Reader Sequence
Step	Owner	Action	Error path
1	Orchestrator	Validate config (gated by cfg_valid_check_en). If callback is non-NULL, spawn task and return RS_OK.	RS_ERR_INVALID_CFG
2	Orchestrator	Reject a second non-blocking Read() while one is in flight.	RS_ERR_INTERNAL
3	Dependency check (merged into orchestrator)	Optional runtime dependency validation.	RS_ERR_DEPENDENCY
4	HAL adapter	Ensure PTX105R reader stack is ready (open).	RS_ERR_DEPENDENCY / RS_ERR_INTERNAL
5	Orchestrator	Configure polling technologies from tech_mask.	RS_ERR_INVALID_CFG
6	HAL adapter	Enable RF field and start discovery.	RS_ERR_INTERNAL
7	Orchestrator	Wait for card until timeout_ms (IRQ-driven).	RS_ERR_TIMEOUT
8	HAL adapter	Activate detected card.	RS_ERR_INTERNAL
9	HAL adapter	Extract UID and card type.	RS_ERR_INTERNAL
10	NDEF engine	If requested, attempt NDEF discovery/read + decode.	RS_ERR_NOT_FOUND / RS_ERR_BUFFER_OVERFLOW
11	Orchestrator	Populate result_out.	—
12	Orchestrator	Clear busy flag, return status or fire callback.	—
Non-blocking Implementation
When cfg->callback is non-NULL, the orchestrator starts a dedicated RTOS task that runs the same internal read sequence used by blocking mode. The task fires the callback and deletes itself.
No heap allocation is used. The task argument block is static because the module is not re-entrant.
PTX105 Adapter

The detection engine, NDEF engine, and orchestrator never call PTX105R SDK, NSC, SPI, I2C, or UART APIs directly. All such calls are confined to the PTX105R adapter module.

src/ptx105r/rs_nfc_ptx105r.h— internal header declaring plain C functions used by the orchestrator and engines (e.g. rs_nfc_ptx_open, rs_nfc_ptx_configure_discovery, rs_nfc_ptx_start_discovery, rs_nfc_ptx_stop_discovery, rs_nfc_ptx_wait_for_card, rs_nfc_ptx_activate_card, rs_nfc_ptx_get_card_type, rs_nfc_ptx_get_uid, rs_nfc_ptx_data_exchange, rs_nfc_ptx_deactivate, rs_nfc_ptx_sleep, rs_nfc_ptx_close).
src/ptx105r/rs_nfc_ptx105r.c— the only translation unit that includes PTX105R SDK / NSC headers. Each rs_nfc_ptx_* function maps directly to the corresponding PTX105R SDK / NSC call.

Board interface (customer-supplied, vendor-neutral):

The adapter does not include FSP or any other MCU vendor SDK. Instead, the AUC code registers a small set of plain C callbacks the adapter needs from the host platform. Suggested surface:

/* src/ptx105r/rs_nfc_ptx105r_board.h — user-supplied bindings */

typedef struct {

    /* Host interface transfer (SPI, I2C, or UART — chosen by user). */

    int  (*host_xfer)(const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);

    /* PTX105R IRQ line: register the adapter's ISR hook. */

    int  (*irq_register)(void (*isr)(void *ctx), void *ctx);

    void (*irq_enable)(bool enable);

    /* Time services. */

    void     (*delay_ms)(uint32_t ms);

    uint32_t (*now_ms)(void);

    /**

   * Configure PTX105R polling.

   *

   * @param tech_mask         Bitmask of rs_nfc_tech_mask_t technologies to poll for.

   * @param polling_interval_ms interval in ms. Must be within

   *                          [RS_NFC_POLLONG_INTERVAL_MIN_MS,

   *                           RS_NFC_POLLING_INTERVAL_MAX_MS].

   *                          Adapter may clamp to the nearest value the

   *                          PTX105R supports.

   * @return 0 on success, negative on error.

   */

  int rs_ptx105r_configure_polling(uint32_t tech_mask, uint32_t polling_interval_ms);

} rs_nfc_ptx105r_board_ops_t;




/* Called once by the AUC code before rs_nfc_reader_Read(). */

int rs_nfc_ptx105r_board_bind(const rs_nfc_ptx105r_board_ops_t *ops);




These callbacks may be implemented on top of FSP, a vendor HAL, bare-metal drivers, or any other framework — that choice is entirely the customer's and invisible to the PES.
Host-side testing: rs_nfc_ptx105r.c and rs_nfc_ptx105r_board_bind() can both be replaced at build time by test doubles exposing the same signatures, so no PTX105R hardware or SDK is required for unit tests.




Dependency Management
Dependency	Minimum requirement	Checked at runtime?
PTX105R hardware	PTX105R connected and powered	Yes, when validation enabled
PTX105R host interface	SPI, I2C, or UART configured by BSP/HAL	Yes, when validation enabled
PTX105R IRQ line	GPIO interrupt configured	Yes, when validation enabled
PTX105R lower-level software stack	Initialized before PES call	Yes, when validation enabled
System timer	Running	Yes, when validation enabled
FreeRTOS	Scheduler running for non-blocking mode	Yes, when validation enabled
Calling task stack	4096 bytes minimum	Yes, when validation enabled
Card protocol support	Selected by tech_mask	Partially
NDEF support	Required only when read_ndef = true	Yes, during read
Repository Structure

Current (merged) layout as implemented in src/nfc_reader/. Several originally
planned files (rs_nfc_detect.c, rs_retry.c, rs_nfc_uid.c, rs_timeout.c,
rs_nfc_reader_deps.{c,h}) were merged into rs_nfc_reader.c during code review;
the interim rs_nfc_utils.{c,h} was later folded into rs_nfc_reader.c as well and
removed:

nfc_reader/
- include/rs_nfc_reader.h              : public API (types, status, cfg, result, Read/Stop)
- src/rs_nfc_reader.c                   : orchestrator + detection wait + retry + validate + dependency check + raw exchange + card summary (all merged in)
- src/rs_ndef_read.c                    : NDEF read (rs_ndef_read_card_info dispatch) + internal NDEF record decoder
- src/ptx105r/rs_nfc_ptx105r.h          : internal adapter API (plain C, rs_nfc_ptx_*)
- src/ptx105r/rs_nfc_ptx105r.c          : only TU that includes PTX SDK / NSC + FSP
- src/ptx105r/rs_nfc_ptx105r_board.h    : integrator-supplied board bindings
- test/test_nfc_reader.c, test_ndef_read.c, mocks/

NOTE: The per-target FSP pack (configuration.xml per board) is maintained
separately and is not part of the source tree above.
Lifecycle State Machine and Public Function Gating

The NFC Reader PES exposes one public lifecycle with three states: CLOSED, DISCOVERY, and ACTIVATED. These states are logical — the caller never reads or writes them. Internally they are implemented by two flags in the PTX105R adapter (g_ptx_opened tracks CLOSED vs. open; g_active_reg tracks DISCOVERY vs. ACTIVATED, i.e. whether a card is currently selected) and stepped through by the per-Read() FSM in rs_nfc_reader.c. Only three inputs move the module between states: a call to rs_nfc_reader_Read(), a call to rs_nfc_reader_Stop(), and the timeout / system-error conditions checked on every iteration of the event loop.

Figure 2.15.1 — NFC Reader public lifecycle. Internal flags g_ptx_opened (CLOSED vs. open) and g_active_reg (DISCOVERY vs. ACTIVATED) implement the three states in src/nfc_reader/src/ptx105r/rs_nfc_ptx105r.c.

State transition table
From	To	Trigger	Source reference
CLOSED	DISCOVERY	rs_nfc_reader_Read() → rs_nfc_ptx_open → rs_nfc_ptx_configure_discovery → rs_nfc_ptx_start_discovery	rs_nfc_reader.c
DISCOVERY	ACTIVATED	External IRQ wakes the task blocked in rs_nfc_ptx_wait_for_card; rs_nfc_ptx_activate_card succeeds; g_active_reg is set	rs_nfc_reader.c, rs_nfc_ptx105r.c
ACTIVATED	DISCOVERY	on_card_event returns; rs_nfc_ptx_deactivate restarts RF discovery	rs_nfc_reader.c, rs_nfc_ptx105r.c
DISCOVERY / ACTIVATED	CLOSED	rs_nfc_reader_Stop() sets the stop flag, or cfg->timeout_ms elapses → deactivate + close	rs_nfc_reader.c
Any	CLOSED (with RS_ERR_INTERNAL)	PTX system status ≠ PTX_SYS_STATUS_OK (0x00); event loop enters LOOP_SYSTEM_ERROR and exits	rs_nfc_reader.c

Public Function State Gating Table
Public Function	CLOSED	DISCOVERY	ACTIVATED	Non-blocking in flight
rs_nfc_reader_Read() — blocking (callback = NULL)	✅	—	—	✅ from another task (HAL is not mutex-guarded — risky)
rs_nfc_reader_Read() — non-blocking (callback ≠ NULL)	✅	—	—	❌ RS_ERR_INTERNAL
rs_nfc_reader_Stop()	✅ (no-op)	✅ 🔥	✅ 🔥	✅ 🔥

🔥 = safe to call while a Read() is in flight (concurrent with the event loop / async worker).

NOTE: rs_nfc_reader_validate() and rs_nfc_reader_raw_exchange() are internal (static) and are not callable from application code; validation runs automatically inside Read() and the raw exchange is driven by cfg->run_raw_exchange.

Only one non-blocking Read() at a time. When Read() is invoked with cfg->callback ≠ NULL, the orchestrator spawns the static FreeRTOS task "RS_NFC" (priority 1, 3072-byte stack, statically allocated) and sets g_async_ctx.active = true. A second non-blocking Read() while this flag is set is rejected with RS_ERR_INTERNAL. The async worker itself moves the module through CLOSED → DISCOVERY → ACTIVATED → DISCOVERY → ... → CLOSED.

Card-scoped ops run while ACTIVATED, inside the read loop. During Read() the module internally calls rs_ndef_read_card_info() on the currently activated card (reading CC / NDEF / tag metadata) and decodes the NDEF message into result->decoded, before firing cfg->on_card_event. ACTIVATED is guaranteed for the duration of the callback. Result buffers referenced by the callback (including result->decoded[].payload and result->raw_exchange.tx / .rx) alias internal statics / the result's own ndef_data and must not be retained past the callback — they are reused on the next card event.

Stop() is not ISR-safe. It calls rs_nfc_ptx_wake_waiting_task(), which uses the non-FromISR xTaskNotifyGive. Call it from task context only. The external-IRQ callback is the only ISR-safe path into the module.

Internal Functional Decomposition
Per-Read Orchestrator FSM

Every Read() call in event-mode drives the same four-state FSM inside run_event_loop(). The enum loop_state_t has states LOOP_WAIT_FOR_ACTIVATION, LOOP_DATA_EVENT, LOOP_DEACTIVATE, LOOP_SYSTEM_ERROR. The wait step is IRQ-driven: the task blocks on ulTaskNotifyTake with portMAX_DELAY when cfg->timeout_ms == UINT32_MAX — no host-side wake-up timer and no periodic register access.

Read-Path Data Flow

A single Read() call composes three concerns: RF discovery, card activation, and data (identity + optional NDEF + optional raw demo exchange). The orchestrator selects one of three execution profiles based on the callback / on_card_event combination:

Mode	callback	on_card_event	Executes on	Loop behaviour
Continuous event, blocking	NULL	non-NULL	Caller thread	Runs run_event_loop until Stop(), timeout, or system error. Every card fires on_card_event(RS_OK, result, summary, ctx).
Continuous event, non-blocking	non-NULL	non-NULL	Static task "RS_NFC" (prio 1, 3072-byte stack)	Same event loop as above; Read() returns immediately. cfg->callback fires exactly once when the loop terminates.
Single-shot blocking	NULL	NULL	Caller thread	Returns after first card or timeout. If cfg->retry_count > 0, the internal retry helper runs up to retry_count + 1 attempts with deactivate between.

Effective single-shot semantics for finite-timeout event mode. The event loop advances elapsed_ms to cfg->timeout_ms after the first wait returns unless timeout_ms == UINT32_MAX, so a finite timeout_ms in event mode delivers at most one card. Continuous event mode therefore requires timeout_ms = UINT32_MAX with Stop() as the exit condition — this is what the reference app does.

The result buffer rs_nfc_card_result_t *result_out is caller-owned. In event modes it is reused across iterations — the on_card_event callback must consume it before returning. When cfg->run_raw_exchange is set and the active protocol is not ISO-DEP / UNDEFINED, the orchestrator performs the raw exchange itself and fills result->raw_exchange for the callback — the app does not need to call anything explicitly to see the frames.

Internal Module Responsibilities
Module	File(s)	Responsibility
Public API / Orchestrator	nfc_reader/include/rs_nfc_reader.h, nfc_reader/src/rs_nfc_reader.c	Public entry points (Read / Stop); internal validate, detection wait, retry, and raw-exchange composition (all merged in); execution-mode selection; per-Read() loop_state_t FSM; stop-flag and async re-entry guard; card-summary builder.
PTX105R HAL Adapter	nfc_reader/src/ptx105r/rs_nfc_ptx105r.{c,h}, rs_nfc_ptx105r_board.h	The only translation unit that includes ptxIoTRd_*. Peripheral bring-up (SPI / GPIO / Timer / AppTimer via ptxPLAT_*_Open); IRQ-driven wait using FSP external IRQ; card activation and UID / type mapping; T3T / T4T / T5T NDEF-OP component lifetimes; system-state / RF-error read-back.
NDEF Read + ReadCardInfo dispatch	nfc_reader/src/rs_ndef_read.c	rs_ndef_read_card_info (module-internal): dispatches T2T (hand-rolled) / T3T / T4T / T5T reads via the SDK NDEF-OP components, then decodes the NDEF message into result->decoded via the internal (static) rs_ndef_decode_message record parser.

Note on rs_nfc_ptx_configure_discovery(tech_mask). The HAL function currently ignores its argument: actual RF discovery enablement is taken from the FSP-generated discovery config (poll_type_a / b / f / v) at rs_nfc_ptx_start_discovery(). Setting different cfg->tech_mask values from the application has no runtime effect until this pass-through is wired.

Execution Models
Blocking (cfg->callback == NULL) (OPTIONAL)

The full sequence — open → configure_discovery → start_discovery → single-shot or event loop → deactivate → close — runs on the caller's thread. Two variants:

Single-shot (on_card_event == NULL): one attempt + internal retry if retry_count > 0. Populates *result_out and returns.
Continuous event (on_card_event != NULL): runs run_event_loop directly on the caller's thread until Stop(), timeout, or system error.
Non-blocking (cfg->callback != NULL)

Read() validates cfg, deep-copies it into g_async_ctx, clears the stop flag, and creates the static task "RS_NFC". The task runs the same blocking flow (either variant), fires cfg->callback(st, p_context) on completion, clears g_async_ctx.active, and self-deletes with vTaskDelete(NULL). A second non-blocking Read() while g_async_ctx.active == true returns RS_ERR_INTERNAL.

FreeRTOS Task and Interrupt Structure
Task / ISR	Mode	Priority	Ownership	Responsibility
"RS_NFC" async worker	Non-blocking only	ASYNC_TASK_PRIORITY = 1U	PES-owned — static TCB + 3072-byte stack	Runs full open → configure_discovery → start_discovery → event loop → close; fires the operation-done callback; self-deletes.
Application thread	Both modes	Application-chosen (FreeRTOS thread from FSP config)	Application-owned	Constructs cfg; calls rs_nfc_reader_Read(); optionally calls rs_nfc_reader_Stop(). Reference app entry: app_nfc_reader_entry().
External-IRQ callback	Both modes	ISR	PES-owned (installed for the duration of a wait_for_card call)	Wakes the task blocked in rs_nfc_ptx_wait_for_card via vTaskNotifyGiveFromISR + portYIELD_FROM_ISR.

IRQ-only wait — zero host-side activity between cards. When cfg->timeout_ms == UINT32_MAX the task blocks on ulTaskNotifyTake(pdTRUE, portMAX_DELAY). It is unblocked only by (a) the PTX105R external IRQ forwarded via vTaskNotifyGiveFromISR, (b) Stop() via rs_nfc_ptx_wake_waiting_task → xTaskNotifyGive, or (c) the caller-supplied finite timeout when timeout_ms != UINT32_MAX. There is no host-side wake-up timer and no periodic PTX register access.

Customer Adaptation Points

The following are the primary areas an integrator would modify. All references are relative to src/nfc_reader/.

Adaptation area	Location	What to change
Host interface (SPI / I²C / UART) and IRQ line	FSP config (nfc_reader ptx0 cfg) and rs_nfc_ptx105r_board.h	Select the PTX105R host bus and route its data-ready pin to the external-IRQ instance. All peripheral instance pointers (comms, gpio, irq, timer, app_timer) are wired here.
Enabled RF technologies	FSP config poll_type_a / b / f / v	Enable / disable ISO14443-A / B, FeliCa, and ISO15693 in the discovery configuration. Note: cfg->tech_mask is not currently forwarded to the HAL.
Temperature calibration	FSP config temp_sensor_calibrate / ambient / shutdown	Populates the temp-sense params on the first Open(). Subsequent opens only set Tshutdown = PTX105R_SHUTDOWN_TEMP (223).
Timeouts	cfg->timeout_ms + constants in rs_nfc_reader.c / rs_nfc_ptx105r.c	DEFAULT_TIMEOUT_MS = 5000 (safety fallback), raw-exchange timeout ~200 ms. Use UINT32_MAX for continuous event mode.
Async task tuning	rs_nfc_reader.c	ASYNC_TASK_STACK_WORDS = 3072U / sizeof(StackType_t), ASYNC_TASK_PRIORITY = 1U, ASYNC_TASK_NAME = "RS_NFC".
NDEF payload cap	cfg->max_ndef_bytes, hard cap RS_NFC_NDEF_MAX_BYTES = 512	Reduce to save RAM in tight applications.
Raw-exchange demo	cfg->run_raw_exchange	Set true to have the orchestrator issue a protocol-appropriate raw frame per card (T2T READ / T3T CHECK / T5T READ_SINGLE_BLOCK / NFC-DEP SYMM). Ignored for ISO-DEP and when on_card_event is NULL. Frames land in result->raw_exchange.
Callback plumbing	cfg->callback, cfg->on_card_event, cfg->p_context, cfg->p_card_event_context	Operation-done callback fires exactly once at the end of a non-blocking Read(). Per-card callback fires once per activated card (also fires with result==NULL for RF warnings and activation failures).

System Lifecycle

Per the PES Architecture standard: the PES lifecycle is the only public state machine. The PTX105R HAL adapter's g_ptx_opened flag, the PTX NFC SDK's ptxIoTRd_* initialisation, and the FSP peripheral open state are all internal implementation details driven by PES transitions triggered from rs_nfc_reader_Read() and rs_nfc_reader_Stop(). The NFC Reader PES integrates one HAL — the PTX105R SDK / NSC stack — so this section documents the single system lifecycle from power-on to steady-state event mode.

Peripheral bring-up order (inside rs_nfc_ptx_open)
The PTX105R adapter opens FSP peripherals in a fixed order, then hands them to the PTX NFC SDK. Each step returns RS_ERR_INTERNAL on failure and aborts the open:

ptxPLAT_GPIO_Open(p_gpio_context, p_irq_context, interrupt_pin)
ptxPLAT_TIMER_Open(p_timer_context)
ptxPERIPH_APPTIMER_Open(p_app_timer)
ptxPLAT_SPI_Open(p_comms_instance_ctrl, p_gpio_context)
ptxIoTRd_Init(iot_reader_context, &init_params) — one-shot retry via ptxIoTRd_Deinit + Init on first-attempt failure
Set g_ptx_opened = true; latch g_start_temp_calibration = false.

Temperature calibration is a one-shot per power-cycle. On the first rs_nfc_ptx_open, the temp-sense params are populated from the FSP config; on subsequent opens only Tshutdown = PTX105R_SHUTDOWN_TEMP (223) is set.

Teardown

Every exit path — Stop(), timeout, or SYSTEM_ERROR — funnels through the same two-step teardown:

rs_nfc_ptx_deactivate() — ptxIoTRd_Reader_Deactivation(..., PTX_IOTRD_RF_DEACTIVATION_TYPE_DISCOVER)
rs_nfc_ptx_close() — clears g_active_reg, calls ptxIoTRd_Deinit, sets g_ptx_opened = false

Peripherals stay open across re-open. rs_nfc_ptx_close does not call ptxPLAT_*_Close. SPI, GPIO, and timer instances remain initialised, so a subsequent Read() reopens only the SDK layer. Temperature calibration is not repeated (g_start_temp_calibration stays false).

System health monitoring during operation

Every iteration of run_event_loop reads two cached PTX status registers. These are cached values that the PTX SDK updates inside its own IRQ handler — the check does not generate any host-side SPI traffic:

rs_nfc_ptx_get_system_state() — if not PTX_SYS_STATUS_OK (0x00), the FSM transitions to LOOP_SYSTEM_ERROR and the loop exits with RS_ERR_INTERNAL. Teardown still runs.
rs_nfc_ptx_get_last_rf_error() — if PTX_RF_ERR_WARNING_PA_OVERCURRENT_LIMIT (0x06), the loop emits a non-fatal warning through on_card_event(status, NULL, "WARN: PA overcurrent limiter activated", ctx) and continues.

SPI Peripheral Portability

The PTX105R supports 3 communication protocols: SPI, UART, I2C. On the FPB-RA2E3 the MCU is the SPI-Master and the PTX105R is the SPI-Device.

In the context of this project we use rm_nfc_reader_ptx, part of the FSP package. rm_nfc_reader_ptx includes rm_comms_spi only, so the user chooses one of the 2 SPI drivers:

r_sci_spi (selected driver for the project - SCI0)
r_spi (dedicated SPI peripheral - SPI0)

Both drivers give a master SPI interface but sit on different peripherals with different capabilities (r_spi = dedicated SPI, higher clock, HW chip-select, native 8/16/32-bit frames; r_sci_spi = SPI over SCI, lower clock, GPIO chip-select, 8-bit frames, more channels available).

HOW-TO switch from r_sci_spi to r_spi in the e2studio project: open the configuration, delete the r_sci_spi module, add r_spi, name it "ptx_pmod_spi", set RX and TX interrupt priority to 2, set rate to 1 Mbps, route MISO0=P100 / MOSI0=P101 / RSPCK0=P102, then Save + Generate Project.

Timer Configuration Notes (FPB-RA2E3 – NFC Reader PTX105R)

The NFC sleep timer (p_timer_context) and the TDC/app timer (p_app_timer) are both mapped to GPT channel 0, the only 32-bit GPT on the FPB-RA2E3. rm_nfc_reader_ptx contains 2 separate driver instances (g_timer1, g_timer0). Both are 32-bit so neither can silently overflow; they share one hardware channel and one interrupt and therefore must never run concurrently (holds by design).

Constraints / rules (do not break):
- Do not modify vendor files under ra/ including ra/fsp/src/rm_nfc_reader_ptx/ptxPLAT_TIMER.{c,h} and ptxPERIPH_APPTIMER.{c,h}. These must remain pristine vendor code.
- Keep both timers on GPT channel 0 so both remain 32-bit. Do not move the app/TDC timer to a 16-bit channel (GPT ch4 or AGT).
- Do not use the sleep timer and the TDC timer concurrently. If a future feature needs both at once they cannot share channel 0 — a redesign is required.

Develop & Test

Current demo application targeting the EK-RA2E3 / FPB-RA2E3 and PTX105R.

Tasks:
- Port the demo application project for the RAxxx (from O&D)
- Separate the application logic into the defined APIs
- Note any issues, limitations, restrictions encountered
- Write, review and approve test plan
- Implement automated tests

Testing Strategy
Layer	Environment	What's real	What's substituted	Card required?
Unit	Host PC, gcc/clang	Orchestrator, NDEF decoders, utils	Mock adapter for rs_nfc_ptx_*; FreeRTOS shim	No
Integration	FPB-RA2E3 + PTX105R (PMOD), later RA6E2 / RA8E1	Everything, including FSP + PTX SDK/NSC	Nothing	No (adapter-level probes only)
Functional	Same target + physical tag/card matrix	Everything	Nothing	Yes
Robustness	Mixed	Depends on fault injected	Fault-injecting mock or real HW	Depending
Non-functional	Target	Real	—	Some

Document & Package

Tasks:
- Document the project (based on the defined format)
- Create a pack based on the configs and APIs in the FSP Functional Decomposition section
- Package the demo project for release (proper comments and style)

Release & Permutate

Expand the application from the RAxxx to any RA board with PTX105R.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*macro_payload_write_byte_fn)(uint8_t byte, void *context);
typedef bool (*macro_payload_read_byte_fn)(uint16_t offset, uint8_t *byte, void *context);

// One window of compiled program. A stored macro longer than this plays as a
// series of windows decoded from storage (macro_payload_decode_qmk_window).
#define MACRO_PAYLOAD_IR_MAX_BYTES 512
#define MACRO_PAYLOAD_MAX_TAP_KEYS 16

typedef enum {
    MACRO_PAYLOAD_IR_OP_TEXT = 1,
    MACRO_PAYLOAD_IR_OP_DELAY,
    MACRO_PAYLOAD_IR_OP_KEY_DOWN,
    MACRO_PAYLOAD_IR_OP_KEY_UP,
    MACRO_PAYLOAD_IR_OP_TAP_LIST,
    MACRO_PAYLOAD_IR_OP_UNICODE, // Three little-endian bytes, one Unicode scalar.
    // A count: Backspace tapped that many times at the text pace. A correction
    // plays it (D-F69); no stored macro holds it.
    MACRO_PAYLOAD_IR_OP_ERASE,
} macro_payload_ir_opcode_t;

typedef enum {
    MACRO_PAYLOAD_TEXT_OUTPUT_PLAIN = 0,
    MACRO_PAYLOAD_TEXT_OUTPUT_DELAYED,
} macro_payload_text_output_t;

typedef struct {
    uint16_t length;
    uint8_t  protection; // 0 automatic, 1 on, 2 off; bank prefix 1,5,value.
    bool     more;       // Another window of the same macro follows this one.
    uint8_t  bytes[MACRO_PAYLOAD_IR_MAX_BYTES];
} macro_payload_ir_t;

typedef struct {
    uint8_t keycodes[MACRO_PAYLOAD_MAX_TAP_KEYS];
    uint8_t count;
} macro_payload_hold_balance_t;

// Where the next window of a stored macro starts. Zeroed, it is the start.
typedef struct {
    uint16_t                     offset;
    uint8_t                      protection;
    macro_payload_hold_balance_t balance;
} macro_payload_stream_cursor_t;

// Playback checks, carried across every window of one macro before it starts.
typedef struct {
    const void                  *layout; // host_layout_t, latched for playback.
    uint8_t                      unicode_mode;
    bool                         iso;
    bool                         needs_unicode;
    macro_payload_hold_balance_t balance;
} macro_payload_preflight_t;

// Replaces the playing window with the next one of the same macro.
typedef bool (*macro_payload_load_window_fn)(void *context);

typedef enum {
    MACRO_PAYLOAD_SOURCE_DIRECT = 0,
    MACRO_PAYLOAD_SOURCE_SLOT,
    // A correction (D-F69): its output is isolated as an uninterruptible
    // macro's is, but presses meanwhile are held back by the typing stage and
    // come out afterwards, never ignored.
    MACRO_PAYLOAD_SOURCE_CORRECTION,
    // The stored macro a correction plays in place of text (D-F69): isolated
    // and its presses held back the same way, whatever its own protection.
    MACRO_PAYLOAD_SOURCE_REPLACEMENT,
} macro_payload_source_t;

typedef enum {
    MACRO_PAYLOAD_START_STARTED = 0,
    MACRO_PAYLOAD_START_BUSY,
    MACRO_PAYLOAD_START_INVALID,
    MACRO_PAYLOAD_START_EMPTY,
} macro_payload_start_result_t;

typedef enum {
    MACRO_PAYLOAD_FINISH_NONE = 0,
    MACRO_PAYLOAD_FINISH_SUCCESS,
    MACRO_PAYLOAD_FINISH_CANCELLED,
    MACRO_PAYLOAD_FINISH_RUNTIME_ERROR,
} macro_payload_finish_result_t;

typedef enum {
    MACRO_PAYLOAD_ENGINE_IDLE = 0,
    MACRO_PAYLOAD_ENGINE_READY,
    MACRO_PAYLOAD_ENGINE_WAITING,
    MACRO_PAYLOAD_ENGINE_OUTPUT,
    MACRO_PAYLOAD_ENGINE_CLEANUP,
} macro_payload_engine_state_t;

typedef void (*macro_payload_finish_fn)(macro_payload_finish_result_t result, void *context);

typedef struct {
    macro_payload_engine_state_t  state;
    macro_payload_finish_result_t last_finish;
    macro_payload_source_t        active_source;
    uint8_t                       active_slot;
    uint8_t                       active_hold_count;
    uint8_t                       active_hold_high_water;
    uint32_t                      operations_executed;
    uint16_t                      completed_count;
    uint16_t                      busy_rejection_count;
    uint16_t                      cancellation_count;
    uint16_t                      runtime_error_count;
    uint32_t                      maximum_lateness_ms;
} macro_payload_debug_snapshot_t;

// The host text is typed for and read back from, as playback latches it:
// the layout (a host_layout_t), whether an ISO Mac exchanges two of its keys,
// and whether Caps Lock follows macOS (capitals whatever Shift does) or
// exchanges the two cases.
typedef struct {
    const void *layout;
    bool        iso;
    bool        macos;
} macro_payload_typing_host_t;

macro_payload_typing_host_t  macro_payload_typing_host(void);
// The host's Caps Lock, as its LED report says.
bool                         macro_payload_unicode_caps_lock(void);
bool                         macro_payload_validate(const char *payload);
bool                         macro_payload_compile(const char *payload, macro_payload_ir_t *ir);
macro_payload_start_result_t macro_payload_start_ir(const macro_payload_ir_t *ir, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context);
void                         macro_payload_preflight_begin(macro_payload_preflight_t *preflight);
bool                         macro_payload_preflight_window(macro_payload_preflight_t *preflight, const macro_payload_ir_t *ir);
bool                         macro_payload_preflight_end(const macro_payload_preflight_t *preflight);
// Plays a macro whose every window passed one preflight, starting at its first
// window in ir. When a window with more set ends, load_next refills ir.
macro_payload_start_result_t macro_payload_start_windows(const macro_payload_ir_t *ir, const macro_payload_preflight_t *preflight, macro_payload_load_window_fn load_next, void *load_context, macro_payload_text_output_t text_output, uint8_t interval, macro_payload_source_t source, uint8_t slot, macro_payload_finish_fn finish, void *context);
void                         macro_payload_engine_scan(void);
bool                         macro_payload_engine_cancel(void);
bool                         macro_payload_engine_protected(void);
// Uninterruptible playback that ignores new presses (D-F17): a protected
// macro a key or a client started, never a correction or the macro one plays.
bool                         macro_payload_engine_ignores_presses(void);
bool                         macro_payload_engine_idle(void);
// How many of the playing macro's leases hold `usage`. False while the macro
// is itself taking or letting go of a lease, when a change in the ledger is
// its own and not yet in its leases.
bool                         macro_payload_engine_leases(uint8_t usage, uint8_t *leases);
void                         macro_payload_engine_init(void);
void                         macro_payload_debug_snapshot(macro_payload_debug_snapshot_t *out);
bool                         macro_payload_decode_qmk_stream(macro_payload_ir_t *ir, uint16_t length, macro_payload_read_byte_fn read_byte, void *context);
// Decodes from cursor up to a full window, ending on a whole step; sets
// ir->more and advances cursor when the macro continues past it.
bool                         macro_payload_decode_qmk_window(macro_payload_ir_t *ir, macro_payload_stream_cursor_t *cursor, uint16_t length, macro_payload_read_byte_fn read_byte, void *context);
bool                         macro_payload_encode_ir(const macro_payload_ir_t *ir, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool                         macro_payload_encode_ir_write(const macro_payload_ir_t *ir, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);
bool                         macro_payload_encode(const char *payload, uint8_t *buffer, uint16_t capacity, uint16_t *written);
bool                         macro_payload_encode_write(const char *payload, macro_payload_write_byte_fn write_byte, void *context, uint16_t *written);

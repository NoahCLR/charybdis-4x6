#pragma once

#include <stdint.h>

#include "users/noah/lib/key/runtime/key_runtime_process.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"

typedef enum {
    KEY_RUNTIME_INTEGRATION_STEP_PRESS = 0,
    KEY_RUNTIME_INTEGRATION_STEP_RELEASE,
    KEY_RUNTIME_INTEGRATION_STEP_ADVANCE_MS,
    KEY_RUNTIME_INTEGRATION_STEP_SCAN,
} key_runtime_integration_step_kind_t;

typedef struct {
    key_runtime_integration_step_kind_t kind;
    union {
        struct {
            uint16_t keycode;
            keypos_t key_pos;
        } key_event;
        uint16_t advance_ms;
    } data;
} key_runtime_integration_step_t;

#define KEY_RUNTIME_INTEGRATION_PRESS(keycode_, row_, col_) \
    {                                                       \
        .kind = KEY_RUNTIME_INTEGRATION_STEP_PRESS,         \
        .data.key_event =                                   \
            {                                               \
                .keycode = (keycode_),                      \
                .key_pos = {.row = (row_), .col = (col_)},  \
            },                                              \
    }

#define KEY_RUNTIME_INTEGRATION_RELEASE(keycode_, row_, col_) \
    {                                                         \
        .kind = KEY_RUNTIME_INTEGRATION_STEP_RELEASE,         \
        .data.key_event =                                     \
            {                                                 \
                .keycode = (keycode_),                        \
                .key_pos = {.row = (row_), .col = (col_)},    \
            },                                                \
    }

#define KEY_RUNTIME_INTEGRATION_ADVANCE(ms_)                 \
    {                                                        \
        .kind            = KEY_RUNTIME_INTEGRATION_STEP_ADVANCE_MS, \
        .data.advance_ms = (ms_),                            \
    }

#define KEY_RUNTIME_INTEGRATION_SCAN()        \
    {                                         \
        .kind = KEY_RUNTIME_INTEGRATION_STEP_SCAN, \
    }

void key_runtime_integration_run(uint16_t *time, const key_runtime_integration_step_t *steps, uint8_t step_count);
void key_runtime_integration_advance(uint16_t *time, uint16_t advance_ms);
void key_runtime_integration_scan(void);
bool key_runtime_integration_process_record(uint16_t keycode, keypos_t key_pos, bool pressed);
handled_key_resolution_t key_runtime_integration_multi_tap_handled_key(uint16_t keycode, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term);
bool key_runtime_integration_process_handled_press(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution);
bool key_runtime_integration_process_handled_release(uint16_t keycode, keypos_t key_pos, handled_key_resolution_t resolution);
void key_runtime_integration_debug_snapshot(noah_runtime_debug_snapshot_t *out);
uint16_t key_runtime_integration_snapshot_slot_owner_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint16_t key_runtime_integration_snapshot_slot_held_action_keycode(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
uint8_t  key_runtime_integration_snapshot_slot_pending_multi_tap_count(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     key_runtime_integration_snapshot_slot_pending_multi_tap_holding(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     key_runtime_integration_snapshot_slot_has_pending_multi_tap(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);
bool     key_runtime_integration_snapshot_slot_hold_is_complete(const noah_runtime_debug_snapshot_t *snapshot, keypos_t key_pos);

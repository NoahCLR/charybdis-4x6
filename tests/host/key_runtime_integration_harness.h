#pragma once

#include <stdint.h>

#include "users/noah/lib/key/runtime/reducer/runtime.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"

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

#define KEY_RUNTIME_INTEGRATION_ADVANCE(ms_)                        \
    {                                                               \
        .kind            = KEY_RUNTIME_INTEGRATION_STEP_ADVANCE_MS, \
        .data.advance_ms = (ms_),                                   \
    }

#define KEY_RUNTIME_INTEGRATION_SCAN()             \
    {                                              \
        .kind = KEY_RUNTIME_INTEGRATION_STEP_SCAN, \
    }

void key_runtime_integration_run(uint16_t *time, const key_runtime_integration_step_t *steps, uint8_t step_count);
void key_runtime_integration_advance(uint16_t *time, uint16_t advance_ms);
void key_runtime_integration_scan(void);
bool key_runtime_integration_apply_core_event(uint16_t *time, const runtime_event_t *event);
bool key_runtime_integration_pre_userspace_record(uint16_t keycode, keyrecord_t *record);
bool key_runtime_integration_process_record(uint16_t keycode, keypos_t key_pos, bool pressed);

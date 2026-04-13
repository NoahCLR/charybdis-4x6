#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/interaction/key_behavior_lookup.h"
#include "users/noah/lib/key/runtime/effects/key_runtime_effect.h"
#include "users/noah/lib/pointing/defs/pd_mode_flags.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"

typedef key_runtime_effect_t key_runtime_scenario_effect_t;

typedef enum {
    KEY_RUNTIME_SCENARIO_STEP_PRESS = 0,
    KEY_RUNTIME_SCENARIO_STEP_RELEASE,
    KEY_RUNTIME_SCENARIO_STEP_ADVANCE_MS,
    KEY_RUNTIME_SCENARIO_STEP_SCAN,
} key_runtime_scenario_step_kind_t;

typedef struct {
    key_runtime_scenario_step_kind_t kind;
    union {
        struct {
            uint16_t keycode;
            keypos_t key_pos;
        } key_event;
        uint16_t advance_ms;
    } data;
} key_runtime_scenario_step_t;

typedef struct {
    uint8_t             tap_count;
    key_behavior_step_t step;
} key_runtime_scenario_multi_tap_entry_t;

#define KEY_RUNTIME_SCENARIO_PRESS(keycode_, row_, col_)   \
    {                                                      \
        .kind = KEY_RUNTIME_SCENARIO_STEP_PRESS,           \
        .data.key_event =                                  \
            {                                              \
                .keycode = (keycode_),                     \
                .key_pos = {.row = (row_), .col = (col_)}, \
            },                                             \
    }

#define KEY_RUNTIME_SCENARIO_RELEASE(keycode_, row_, col_) \
    {                                                      \
        .kind = KEY_RUNTIME_SCENARIO_STEP_RELEASE,         \
        .data.key_event =                                  \
            {                                              \
                .keycode = (keycode_),                     \
                .key_pos = {.row = (row_), .col = (col_)}, \
            },                                             \
    }

#define KEY_RUNTIME_SCENARIO_ADVANCE(ms_)                        \
    {                                                            \
        .kind            = KEY_RUNTIME_SCENARIO_STEP_ADVANCE_MS, \
        .data.advance_ms = (ms_),                                \
    }

#define KEY_RUNTIME_SCENARIO_SCAN()             \
    {                                           \
        .kind = KEY_RUNTIME_SCENARIO_STEP_SCAN, \
    }

void key_runtime_scenario_reset(void);
void key_runtime_scenario_clear_effects(void);
key_behavior_view_t key_runtime_scenario_pressable_handled_key(uint16_t keycode);
void key_runtime_scenario_add_behavior_view(key_behavior_view_t behavior);
void key_runtime_scenario_add_behavior_step(uint16_t keycode, uint8_t tap_count, key_behavior_step_t step);
void key_runtime_scenario_add_pending_multi_tap_behavior(key_behavior_view_t behavior, const key_runtime_scenario_multi_tap_entry_t *entries, uint8_t entry_count);
void key_runtime_scenario_add_pending_multi_tap_chain(uint16_t keycode, key_behavior_step_t single, const key_runtime_scenario_multi_tap_entry_t *entries, uint8_t entry_count);
void key_runtime_scenario_add_pd_mode(uint16_t keycode, pd_mode_mask_t mode);
void key_runtime_scenario_define_pd_mode_key(uint16_t keycode, pd_mode_mask_t mode, bool locked);
void key_runtime_scenario_set_pd_locked_modes(pd_mode_mask_t modes);
void key_runtime_scenario_set_hold_survives_flush(bool survives_flush);
void key_runtime_scenario_run(const key_runtime_scenario_step_t *steps, uint8_t step_count);
bool key_runtime_scenario_layer_locked(uint8_t layer);
void key_runtime_scenario_debug_snapshot(noah_runtime_debug_snapshot_t *out);
uint16_t key_runtime_scenario_slot_owner_keycode(keypos_t key_pos);
uint16_t key_runtime_scenario_slot_held_action_keycode(keypos_t key_pos);
bool     key_runtime_scenario_slot_has_pending_multi_tap(keypos_t key_pos);
bool     key_runtime_scenario_slot_hold_is_complete(keypos_t key_pos);

uint8_t                              key_runtime_scenario_effect_count(void);
const key_runtime_scenario_effect_t *key_runtime_scenario_effect_at(uint8_t index);
uint16_t                             key_runtime_scenario_now(void);

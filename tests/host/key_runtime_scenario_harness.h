#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "users/noah/lib/key/delayed_action.h"
#include "users/noah/lib/key/key_behavior_lookup.h"
#include "users/noah/lib/pointing/pd_mode_flags.h"
#include "users/noah/lib/state/runtime_shared_state.h"

typedef enum {
    KEY_RUNTIME_SCENARIO_EFFECT_NONE = 0,
    KEY_RUNTIME_SCENARIO_EFFECT_DISPATCH_ACTION,
    KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER,
    KEY_RUNTIME_SCENARIO_EFFECT_HELD_UNREGISTER,
    KEY_RUNTIME_SCENARIO_EFFECT_RELEASE_OWNED_BY_KEY,
    KEY_RUNTIME_SCENARIO_EFFECT_REPEAT_START,
    KEY_RUNTIME_SCENARIO_EFFECT_LAYER_PRESS,
    KEY_RUNTIME_SCENARIO_EFFECT_LAYER_RELEASE,
    KEY_RUNTIME_SCENARIO_EFFECT_FEEDBACK_PULSE,
    KEY_RUNTIME_SCENARIO_EFFECT_DELAYED_ACTION,
    KEY_RUNTIME_SCENARIO_EFFECT_PD_MODE_LOCK_TOGGLE,
} key_runtime_scenario_effect_kind_t;

typedef struct {
    key_runtime_scenario_effect_kind_t kind;
    uint16_t                           action;
    keypos_t                           key_pos;
    uint8_t                            layer;
    uint16_t                           repeat_hz;
    bool                               long_hold_level;
    delayed_action_mods_t              mods;
    pd_mode_mask_t                     pd_mode;
} key_runtime_scenario_effect_t;

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

#define KEY_RUNTIME_SCENARIO_PRESS(keycode_, row_, col_) \
    {                                                    \
        .kind = KEY_RUNTIME_SCENARIO_STEP_PRESS,         \
        .data.key_event =                                \
            {                                            \
                .keycode = (keycode_),                   \
                .key_pos = {.row = (row_), .col = (col_)}, \
            },                                           \
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

#define KEY_RUNTIME_SCENARIO_ADVANCE(ms_)            \
    {                                                \
        .kind            = KEY_RUNTIME_SCENARIO_STEP_ADVANCE_MS, \
        .data.advance_ms = (ms_),                    \
    }

#define KEY_RUNTIME_SCENARIO_SCAN()              \
    {                                            \
        .kind = KEY_RUNTIME_SCENARIO_STEP_SCAN,  \
    }

void key_runtime_scenario_reset(void);
void key_runtime_scenario_clear_effects(void);
void key_runtime_scenario_add_behavior_view(key_behavior_view_t behavior);
void key_runtime_scenario_add_behavior_step(uint16_t keycode, uint8_t tap_count, key_behavior_step_t step);
void key_runtime_scenario_add_pd_mode(uint16_t keycode, pd_mode_mask_t mode);
void key_runtime_scenario_set_pd_locked_modes(pd_mode_mask_t modes);
void key_runtime_scenario_set_hold_survives_flush(bool survives_flush);
void key_runtime_scenario_run(const key_runtime_scenario_step_t *steps, uint8_t step_count);
bool key_runtime_scenario_layer_locked(uint8_t layer);

uint8_t key_runtime_scenario_effect_count(void);
const key_runtime_scenario_effect_t *key_runtime_scenario_effect_at(uint8_t index);
const runtime_shared_state_t        *key_runtime_scenario_state(void);
uint16_t                             key_runtime_scenario_now(void);

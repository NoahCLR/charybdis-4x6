#include "key_runtime_scenario_harness.h"

#include <string.h>

#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/key/handled_key.h"
#include "users/noah/lib/key/held_action.h"
#include "users/noah/lib/key/key_runtime_process.h"
#include "users/noah/lib/key/key_runtime_state.h"
#include "users/noah/lib/pointing/pd_modes.h"
#include "users/noah/noah_runtime.h"

enum {
    KEY_RUNTIME_SCENARIO_MAX_BEHAVIORS = 16,
    KEY_RUNTIME_SCENARIO_MAX_STEPS     = 16,
    KEY_RUNTIME_SCENARIO_MAX_PD_MODES  = 8,
    KEY_RUNTIME_SCENARIO_MAX_EFFECTS   = 32,
};

typedef struct {
    uint16_t            keycode;
    key_behavior_view_t behavior;
} key_runtime_scenario_behavior_entry_t;

typedef struct {
    uint16_t            keycode;
    uint8_t             tap_count;
    key_behavior_step_t step;
} key_runtime_scenario_step_entry_t;

typedef struct {
    uint16_t       keycode;
    pd_mode_mask_t mode;
} key_runtime_scenario_pd_mode_entry_t;

static uint16_t                                   key_runtime_scenario_time;
static bool                                       key_runtime_scenario_hold_survives_flush;
static key_runtime_scenario_behavior_entry_t      key_runtime_scenario_behaviors[KEY_RUNTIME_SCENARIO_MAX_BEHAVIORS];
static uint8_t                                    key_runtime_scenario_behavior_count;
static key_runtime_scenario_step_entry_t          key_runtime_scenario_steps[KEY_RUNTIME_SCENARIO_MAX_STEPS];
static uint8_t                                    key_runtime_scenario_step_entry_count;
static key_runtime_scenario_pd_mode_entry_t       key_runtime_scenario_pd_modes[KEY_RUNTIME_SCENARIO_MAX_PD_MODES];
static uint8_t                                    key_runtime_scenario_pd_mode_count;
static pd_mode_mask_t                             key_runtime_scenario_pd_locked_modes;
static key_runtime_scenario_effect_t              key_runtime_scenario_effects[KEY_RUNTIME_SCENARIO_MAX_EFFECTS];
static uint8_t                                    key_runtime_scenario_effect_count_value;

static void key_runtime_scenario_log_effect(key_runtime_scenario_effect_t effect) {
    if (key_runtime_scenario_effect_count_value >= ARRAY_SIZE(key_runtime_scenario_effects)) {
        return;
    }

    key_runtime_scenario_effects[key_runtime_scenario_effect_count_value++] = effect;
}

static keyrecord_t key_runtime_scenario_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static key_behavior_view_t key_runtime_scenario_default_behavior(uint16_t keycode) {
    return (key_behavior_view_t){
        .keycode            = keycode,
        .handled            = IS_QK_MOMENTARY(keycode),
        .is_momentary_layer = IS_QK_MOMENTARY(keycode),
        .is_layer_tap       = false,
        .has_multi_tap      = false,
        .tap_hold_term      = CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term   = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term     = CUSTOM_MULTI_TAP_TERM,
    };
}

void key_runtime_scenario_reset(void) {
    key_runtime_scenario_time               = 1000;
    key_runtime_scenario_hold_survives_flush = false;
    key_runtime_scenario_behavior_count     = 0;
    key_runtime_scenario_step_entry_count   = 0;
    key_runtime_scenario_pd_mode_count      = 0;
    key_runtime_scenario_pd_locked_modes    = 0;
    key_runtime_scenario_effect_count_value = 0;

    memset(key_runtime_scenario_behaviors, 0, sizeof(key_runtime_scenario_behaviors));
    memset(key_runtime_scenario_steps, 0, sizeof(key_runtime_scenario_steps));
    memset(key_runtime_scenario_pd_modes, 0, sizeof(key_runtime_scenario_pd_modes));
    memset(key_runtime_scenario_effects, 0, sizeof(key_runtime_scenario_effects));

    noah_runtime_shared_state = (runtime_shared_state_t){0};
    for (uint8_t index = 0; index < KEY_RUNTIME_SLOT_TABLE_CAPACITY; index++) {
        noah_runtime_shared_state.key.slots_by_position[index] = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
    }
}

void key_runtime_scenario_clear_effects(void) {
    key_runtime_scenario_effect_count_value = 0;
    memset(key_runtime_scenario_effects, 0, sizeof(key_runtime_scenario_effects));
}

void key_runtime_scenario_add_behavior_view(key_behavior_view_t behavior) {
    if (key_runtime_scenario_behavior_count >= ARRAY_SIZE(key_runtime_scenario_behaviors)) {
        return;
    }

    if (behavior.tap_hold_term == 0) {
        behavior.tap_hold_term = CUSTOM_TAP_HOLD_TERM;
    }
    if (behavior.longer_hold_term == 0) {
        behavior.longer_hold_term = CUSTOM_LONGER_HOLD_TERM;
    }
    if (behavior.multi_tap_term == 0) {
        behavior.multi_tap_term = CUSTOM_MULTI_TAP_TERM;
    }

    key_runtime_scenario_behaviors[key_runtime_scenario_behavior_count++] = (key_runtime_scenario_behavior_entry_t){
        .keycode   = behavior.keycode,
        .behavior  = behavior,
    };
}

void key_runtime_scenario_add_behavior_step(uint16_t keycode, uint8_t tap_count, key_behavior_step_t step) {
    if (key_runtime_scenario_step_entry_count >= ARRAY_SIZE(key_runtime_scenario_steps)) {
        return;
    }

    key_runtime_scenario_steps[key_runtime_scenario_step_entry_count++] = (key_runtime_scenario_step_entry_t){
        .keycode   = keycode,
        .tap_count = tap_count,
        .step      = step,
    };
}

void key_runtime_scenario_add_pd_mode(uint16_t keycode, pd_mode_mask_t mode) {
    if (key_runtime_scenario_pd_mode_count >= ARRAY_SIZE(key_runtime_scenario_pd_modes)) {
        return;
    }

    key_runtime_scenario_pd_modes[key_runtime_scenario_pd_mode_count++] = (key_runtime_scenario_pd_mode_entry_t){
        .keycode = keycode,
        .mode    = mode,
    };
}

void key_runtime_scenario_set_pd_locked_modes(pd_mode_mask_t modes) {
    key_runtime_scenario_pd_locked_modes = modes;
}

void key_runtime_scenario_set_hold_survives_flush(bool survives_flush) {
    key_runtime_scenario_hold_survives_flush = survives_flush;
}

void key_runtime_scenario_run(const key_runtime_scenario_step_t *steps, uint8_t step_count) {
    for (uint8_t index = 0; index < step_count; index++) {
        switch (steps[index].kind) {
            case KEY_RUNTIME_SCENARIO_STEP_PRESS: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, true);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_SCENARIO_STEP_RELEASE: {
                keyrecord_t record = key_runtime_scenario_record(steps[index].data.key_event.key_pos, false);
                (void)noah_process_record_user(steps[index].data.key_event.keycode, &record);
                break;
            }
            case KEY_RUNTIME_SCENARIO_STEP_ADVANCE_MS:
                key_runtime_scenario_time = (uint16_t)(key_runtime_scenario_time + steps[index].data.advance_ms);
                break;
            case KEY_RUNTIME_SCENARIO_STEP_SCAN:
                noah_key_runtime_scan();
                break;
        }
    }
}

uint8_t key_runtime_scenario_effect_count(void) {
    return key_runtime_scenario_effect_count_value;
}

const key_runtime_scenario_effect_t *key_runtime_scenario_effect_at(uint8_t index) {
    if (index >= key_runtime_scenario_effect_count_value) {
        return NULL;
    }

    return &key_runtime_scenario_effects[index];
}

const runtime_shared_state_t *key_runtime_scenario_state(void) {
    return &noah_runtime_shared_state;
}

uint16_t key_runtime_scenario_now(void) {
    return key_runtime_scenario_time;
}

uint16_t timer_read(void) {
    return key_runtime_scenario_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(key_runtime_scenario_time - last);
}

uint32_t timer_read32(void) {
    return key_runtime_scenario_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return (uint32_t)(key_runtime_scenario_time - last);
}

uint8_t get_mods(void) {
    return 0;
}

uint8_t get_weak_mods(void) {
    return 0;
}

uint8_t get_oneshot_mods(void) {
    return 0;
}

uint8_t get_oneshot_locked_mods(void) {
    return 0;
}

void set_mods(uint8_t mods) {
    (void)mods;
}

void set_weak_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_mods(uint8_t mods) {
    (void)mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    (void)mods;
}

void clear_mods(void) {}
void clear_weak_mods(void) {}
void clear_oneshot_mods(void) {}
void clear_oneshot_locked_mods(void) {}
void add_mods(uint8_t mods) {
    (void)mods;
}
void del_mods(uint8_t mods) {
    (void)mods;
}
void send_keyboard_report(void) {}

bool noah_synthetic_record_active(void) {
    return false;
}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool action_dispatch_is_layer_lock(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_is_raw_qmk_layer_action(uint16_t action) {
    return IS_QK_MOMENTARY(action) || IS_QK_LAYER_TAP(action);
}

bool action_dispatch_is_layer_action(uint16_t action) {
    return action_dispatch_is_raw_qmk_layer_action(action) || action_dispatch_is_layer_lock(action);
}

bool action_dispatch_is_macro(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_is_qmk_behavior_keycode(uint16_t action) {
    (void)action;
    return false;
}

bool action_dispatch_layer_is_locked(uint8_t layer) {
    (void)layer;
    return false;
}

noah_action_hold_kind_t noah_action_hold_kind(uint16_t action) {
    (void)action;
    return NOAH_ACTION_HOLD_KIND_SHARED;
}

void action_dispatch(uint16_t action) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind   = KEY_RUNTIME_SCENARIO_EFFECT_DISPATCH_ACTION,
        .action = action,
    });
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    key_runtime_scenario_pd_locked_modes ^= mode;
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_PD_MODE_LOCK_TOGGLE,
        .pd_mode = mode,
    });
    return true;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    for (uint8_t index = 0; index < key_runtime_scenario_pd_mode_count; index++) {
        if (key_runtime_scenario_pd_modes[index].keycode == keycode) {
            return key_runtime_scenario_pd_modes[index].mode;
        }
    }

    return 0;
}

bool pd_mode_locked(pd_mode_mask_t mode) {
    return (key_runtime_scenario_pd_locked_modes & mode) != 0;
}

key_behavior_view_t key_behavior_lookup(uint16_t keycode) {
    for (uint8_t index = 0; index < key_runtime_scenario_behavior_count; index++) {
        if (key_runtime_scenario_behaviors[index].keycode == keycode) {
            return key_runtime_scenario_behaviors[index].behavior;
        }
    }

    key_behavior_view_t behavior = key_runtime_scenario_default_behavior(keycode);
    if (pd_mode_for_keycode(keycode) != 0) {
        behavior.handled = true;
    }
    return behavior;
}

key_behavior_step_t key_behavior_step_lookup(uint16_t keycode, uint8_t tap_count) {
    for (uint8_t index = 0; index < key_runtime_scenario_step_entry_count; index++) {
        if (key_runtime_scenario_steps[index].keycode == keycode && key_runtime_scenario_steps[index].tap_count == tap_count) {
            return key_runtime_scenario_steps[index].step;
        }
    }

    return key_behavior_step_none();
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    for (uint8_t index = 0; index < key_runtime_scenario_step_entry_count; index++) {
        if (key_runtime_scenario_steps[index].keycode == keycode && key_runtime_scenario_steps[index].tap_count > count && key_behavior_step_present(key_runtime_scenario_steps[index].step)) {
            return true;
        }
    }

    return false;
}

void key_behavior_validate_all(void) {}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind   = KEY_RUNTIME_SCENARIO_EFFECT_DELAYED_ACTION,
        .action = action,
        .mods   = mods,
    });
}

void held_action_register(keypos_t key_pos, uint16_t action) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_HELD_REGISTER,
        .action  = action,
        .key_pos = key_pos,
    });
}

void held_action_unregister(keypos_t key_pos, uint16_t action) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_HELD_UNREGISTER,
        .action  = action,
        .key_pos = key_pos,
    });
}

void held_action_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind      = KEY_RUNTIME_SCENARIO_EFFECT_REPEAT_START,
        .action    = action,
        .key_pos   = key_pos,
        .repeat_hz = repeat_hz,
    });
}

void held_action_repeat_tick(void) {}

bool held_action_survives_flush(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
    return key_runtime_scenario_hold_survives_flush;
}

bool held_action_release_owned_by_key(keypos_t key_pos) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_RELEASE_OWNED_BY_KEY,
        .key_pos = key_pos,
    });
    return true;
}

bool held_modifier_release_owned_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_LAYER_PRESS,
        .key_pos = key_pos,
        .layer   = layer,
    });
}

bool layer_ownership_momentary_release(keypos_t key_pos) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind    = KEY_RUNTIME_SCENARIO_EFFECT_LAYER_RELEASE,
        .key_pos = key_pos,
    });
    return true;
}

void key_feedback_pulse_arm(bool long_hold_level) {
    key_runtime_scenario_log_effect((key_runtime_scenario_effect_t){
        .kind            = KEY_RUNTIME_SCENARIO_EFFECT_FEEDBACK_PULSE,
        .long_hold_level = long_hold_level,
    });
}

void split_runtime_sync(void) {}

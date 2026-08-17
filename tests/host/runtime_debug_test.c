#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/action/owned_keycode.h"
#include "users/noah/lib/key/behavior/handled_key.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/ownership/held_repeat.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/deferred_release.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/key/runtime/api.h"
#include "users/noah/lib/key/runtime/transition.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/runtime/pd_mode_internal.h"
#include "users/noah/lib/key/runtime/planning/effect_plan.h"
#include "users/noah/lib/key/runtime/reducer/ownership_state.h"
#include "users/noah/lib/key/runtime/queue/pending_release_queue.h"
#include "users/noah/lib/key/runtime/reducer/runtime.h"
#include "users/noah/lib/key/runtime/projection/projection.h"
#include "users/noah/lib/key/runtime/planning/release_internal.h"
#include "users/noah/lib/key/runtime/planning/scan_planner.h"
#include "users/noah/lib/key/runtime/reducer/state_query.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/diagnostics/runtime_debug.h"
#include "users/noah/lib/state/shared/runtime_reset.h"
#include "users/noah/lib/state/diagnostics/runtime_trace.h"
#include "users/noah/noah_runtime.h"

enum {
    TEST_ACTION                = NOAH_KEYMAP_SAFE_RANGE + 0x10,
    TEST_SECOND_ACTION         = NOAH_KEYMAP_SAFE_RANGE + 0x11,
    TEST_PENDING_MULTI_TAP_KEY = NOAH_KEYMAP_SAFE_RANGE + 0x12,
    TEST_INTERRUPTED_LAYER_KEY = NOAH_KEYMAP_SAFE_RANGE + 0x13,
    TEST_RELEASE_PRIMARY_KEY   = NOAH_KEYMAP_SAFE_RANGE + 0x14,
    TEST_THRESHOLD_LONG_KEY    = NOAH_KEYMAP_SAFE_RANGE + 0x15,
    TEST_PENDING_RELEASE_KEY   = NOAH_KEYMAP_SAFE_RANGE + 0x16,
    TEST_FINAL_TAP_ONLY_KEY    = NOAH_KEYMAP_SAFE_RANGE + 0x17,
    TEST_HELD_ACTION_KEY       = NOAH_KEYMAP_SAFE_RANGE + 0x18,
    TEST_THIRD_ACTION          = NOAH_KEYMAP_SAFE_RANGE + 0x19,
    TEST_TRANSPARENT_HOLD_KEY  = NOAH_KEYMAP_SAFE_RANGE + 0x1A,
};

#define TEST_PROJECTION_FEEDBACK_MARKER UINT16_MAX

static uint16_t                           fake_time;
static uint8_t                            fake_mods;
static uint8_t                            fake_weak_mods;
static uint8_t                            fake_oneshot_mods;
static uint8_t                            fake_oneshot_locked_mods;
static uint8_t                            send_keyboard_report_count;
static uint16_t                           last_emitted_action;
static uint16_t                           last_delayed_action;
static delayed_action_mods_t              last_delayed_mods;
static uint16_t                           delayed_action_count;
static uint16_t                           delayed_action_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY + KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY];
static delayed_action_mods_t              delayed_action_mods_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY + KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY];
static uint16_t                           projection_order_count;
static uint16_t                           projection_order_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY * 2u];
static uint16_t                           feedback_pulse_count;
static keypos_t                           feedback_pulse_key_pos_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
static key_feedback_pulse_kind_t          feedback_pulse_kind_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
static uint8_t                            feedback_pulse_tap_branch_log[KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY];
static bool                               delayed_action_enqueue_hook_armed;
static bool                               delayed_action_reenter_drain;
static keypos_t                           delayed_action_enqueue_key_pos;
static uint16_t                           delayed_action_enqueue_action;
static delayed_action_mods_t              delayed_action_enqueue_mods;
static uint8_t                            split_runtime_sync_count;
static key_feedback_branch_confirm_mode_t fake_branch_confirm_mode;
static uint16_t                           test_keymap[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS];

layer_state_t layer_state;

bool owned_keycode_is_supported(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool owned_keycode_acquire(uint16_t keycode, owned_keycode_lease_t *lease) {
    (void)keycode;
    (void)lease;
    return false;
}

bool owned_keycode_release(owned_keycode_lease_t *lease) {
    (void)lease;
    return false;
}

void owned_keycode_track_physical_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool owned_keycode_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

static bool test_keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static keypos_t test_keypos(uint8_t row, uint8_t col) {
    return (keypos_t){
        .row = row,
        .col = col,
    };
}

static runtime_event_t test_key_runtime_core_key_event(runtime_event_kind_t kind, uint16_t keycode, keypos_t key_pos) {
    return (runtime_event_t){
        .kind = kind,
        .data.key_event =
            {
                .keycode = keycode,
                .key_pos = key_pos,
            },
    };
}

// The production runtime arms pulses through the projection path, which always
// records the owning key position. This local helper mirrors that, rather than
// leaving a keypos-less arming function on the production surface with no
// production caller.
static void test_arm_feedback_pulse(keypos_t key_pos, key_feedback_pulse_kind_t kind) {
    key_runtime_core_state_t *state = key_runtime_core_state();

    state->feedback_pulse_timer           = timer_read();
    state->feedback_pulse_sequence        = key_runtime_core_state_next_feedback_sequence(state);
    state->feedback_pulse_active          = true;
    state->feedback_pulse_kind            = kind;
    state->feedback_pulse_key_pos         = key_pos;
    state->feedback_pulse_tap_branch      = 0u;
    state->feedback_pulse_queued          = false;
    state->feedback_pulse_queued_sequence = 0u;
}

static void test_key_runtime_core_apply_key_event(runtime_event_kind_t kind, uint16_t keycode, keypos_t key_pos, uint16_t event_time) {
    runtime_event_t event = test_key_runtime_core_key_event(kind, keycode, key_pos);

    key_runtime_core_apply_event(&event, event_time);
}

static const layer_ownership_binding_snapshot_t *test_find_layer_binding(const layer_ownership_debug_snapshot_t *snapshot, keypos_t key_pos, uint8_t layer) {
    if (!snapshot) {
        return NULL;
    }

    for (uint16_t index = 0; index < LAYER_OWNERSHIP_BINDING_CAPACITY; index++) {
        const layer_ownership_binding_snapshot_t *binding = &snapshot->bindings[index];
        if (binding->active && binding->layer == layer && test_keypos_equal(binding->key_pos, key_pos)) {
            return binding;
        }
    }

    return NULL;
}

static const held_action_binding_snapshot_t *test_find_held_action_binding(const held_action_debug_snapshot_t *snapshot, keypos_t key_pos, uint16_t action) {
    if (!snapshot) {
        return NULL;
    }

    for (uint16_t index = 0; index < HELD_ACTION_BINDING_CAPACITY; index++) {
        const held_action_binding_snapshot_t *binding = &snapshot->actions[index];
        if (binding->active && binding->action == action && test_keypos_equal(binding->key_pos, key_pos)) {
            return binding;
        }
    }

    return NULL;
}

static const held_repeat_binding_snapshot_t *test_find_held_repeat_binding(const held_repeat_debug_snapshot_t *snapshot, keypos_t key_pos, uint16_t action) {
    if (!snapshot) {
        return NULL;
    }

    for (uint16_t index = 0; index < HELD_REPEAT_BINDING_CAPACITY; index++) {
        const held_repeat_binding_snapshot_t *binding = &snapshot->bindings[index];
        if (binding->active && binding->action == action && test_keypos_equal(binding->key_pos, key_pos)) {
            return binding;
        }
    }

    return NULL;
}

static handled_key_resolution_t test_handled_key_resolution(uint16_t keycode, uint8_t tap_count) {
    uint16_t            flags = HANDLED_KEY_FLAG_HANDLED;
    key_behavior_step_t step  = {
        .tap = TAP_SENDS(keycode),
    };
    uint8_t layer         = UINT8_MAX;
    bool    has_more_taps = false;

    if (keycode == TEST_PENDING_MULTI_TAP_KEY) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
        if (tap_count == 1u) {
            step = (key_behavior_step_t){
                .tap = TAP_SENDS(TEST_ACTION),
            };
            has_more_taps = true;
        } else {
            step = (key_behavior_step_t){
                .tap       = TAP_SENDS(TEST_SECOND_ACTION),
                .hold      = TAP_AT_HOLD_THRESHOLD(TEST_ACTION),
                .long_hold = TAP_AT_HOLD_THRESHOLD(TEST_SECOND_ACTION),
            };
        }
    } else if (keycode == TEST_FINAL_TAP_ONLY_KEY) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
        if (tap_count == 1u) {
            step = (key_behavior_step_t){
                .tap = TAP_SENDS(TEST_ACTION),
            };
            has_more_taps = true;
        } else {
            step = (key_behavior_step_t){
                .tap = TAP_SENDS(TEST_SECOND_ACTION),
            };
        }
    } else if (keycode == KC_LEFT_GUI) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
        if (tap_count == 1u) {
            step          = key_behavior_step_none();
            has_more_taps = true;
        } else if (tap_count == 2u) {
            step = (key_behavior_step_t){
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_ALT),
            };
            has_more_taps = true;
        } else {
            step = (key_behavior_step_t){
                .tap = TAP_SENDS(OSM(MOD_LSFT)),
            };
        }
    } else if (keycode == KC_RIGHT_GUI) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
        if (tap_count == 1u) {
            step          = key_behavior_step_none();
            has_more_taps = true;
        } else {
            step = (key_behavior_step_t){
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RIGHT_ALT),
            };
        }
    } else if (keycode == TEST_INTERRUPTED_LAYER_KEY) {
        flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER | HANDLED_KEY_FLAG_LAYER_TAP;
        step = (key_behavior_step_t){
            .tap  = TAP_SENDS(KC_V),
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(2)),
        };
        layer = 2;
    } else if (keycode == TEST_RELEASE_PRIMARY_KEY) {
        step = (key_behavior_step_t){
            .hold = TAP_ON_RELEASE_AFTER_HOLD(TEST_ACTION),
        };
    } else if (keycode == TEST_THRESHOLD_LONG_KEY) {
        step = (key_behavior_step_t){
            .hold      = TAP_AT_HOLD_THRESHOLD(TEST_ACTION),
            .long_hold = TAP_ON_RELEASE_AFTER_HOLD(TEST_SECOND_ACTION),
        };
    } else if (keycode == TEST_PENDING_RELEASE_KEY) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
        step = (key_behavior_step_t){
            .hold = TAP_ON_RELEASE_AFTER_HOLD(TEST_ACTION),
        };
    } else if (keycode == TEST_HELD_ACTION_KEY) {
        step = (key_behavior_step_t){
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(TEST_ACTION),
        };
    } else if (keycode == TEST_TRANSPARENT_HOLD_KEY) {
        step = (key_behavior_step_t){
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_TRNS),
        };
    }

    return (handled_key_resolution_t){
        .keycode             = keycode,
        .tap_count           = tap_count,
        .step                = step,
        .tap_hold_term       = keycode == TEST_PENDING_MULTI_TAP_KEY ? 120 : CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term    = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term      = keycode == TEST_PENDING_MULTI_TAP_KEY ? 180 : CUSTOM_MULTI_TAP_TERM,
        .branch_confirm_term = CUSTOM_RGB_BRANCH_CONFIRM_TERM,
        .layer               = layer,
        .pd_mode             = pd_mode_for_keycode(keycode),
        .has_more_taps       = has_more_taps,
        .flags               = flags,
    };
}

static void test_reset_stubs(void) {
    fake_time                         = 1000;
    fake_mods                         = 0;
    fake_weak_mods                    = 0;
    fake_oneshot_mods                 = 0;
    fake_oneshot_locked_mods          = 0;
    send_keyboard_report_count        = 0;
    last_emitted_action               = KC_NO;
    last_delayed_action               = KC_NO;
    last_delayed_mods                 = (delayed_action_mods_t){0};
    delayed_action_count              = 0;
    projection_order_count            = 0;
    feedback_pulse_count              = 0;
    delayed_action_enqueue_hook_armed = false;
    delayed_action_reenter_drain      = false;
    delayed_action_enqueue_key_pos    = (keypos_t){0};
    delayed_action_enqueue_action     = KC_NO;
    delayed_action_enqueue_mods       = (delayed_action_mods_t){0};
    split_runtime_sync_count          = 0;
    fake_branch_confirm_mode          = KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS;
    layer_state                       = 0;

    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
        for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
                test_keymap[layer][row][col] = KC_TRNS;
            }
        }
    }
}

static void test_set_keymap_key(uint8_t layer, keypos_t key_pos, uint16_t keycode) {
    test_keymap[layer][key_pos.row][key_pos.col] = keycode;
}

key_feedback_branch_confirm_mode_t key_feedback_branch_confirm_mode(void) {
    return fake_branch_confirm_mode;
}

void key_feedback_pulse_observe(keypos_t key_pos, key_feedback_pulse_kind_t kind, uint8_t tap_branch) {
    CHECK(feedback_pulse_count < ARRAY_SIZE(feedback_pulse_key_pos_log));
    CHECK(projection_order_count < ARRAY_SIZE(projection_order_log));

    feedback_pulse_key_pos_log[feedback_pulse_count]    = key_pos;
    feedback_pulse_kind_log[feedback_pulse_count]       = kind;
    feedback_pulse_tap_branch_log[feedback_pulse_count] = tap_branch;
    feedback_pulse_count++;
    projection_order_log[projection_order_count++] = TEST_PROJECTION_FEEDBACK_MARKER;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

uint32_t timer_read32(void) {
    return fake_time;
}

uint32_t timer_elapsed32(uint32_t last) {
    return timer_read32() - last;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    return test_keymap[layer_num][row][column];
}

void layer_on(uint8_t layer) {
    layer_state |= (layer_state_t)1u << layer;
}

void layer_off(uint8_t layer) {
    layer_state &= (layer_state_t) ~((layer_state_t)1u << layer);
}

uint8_t get_mods(void) {
    return fake_mods;
}

uint8_t get_weak_mods(void) {
    return fake_weak_mods;
}

uint8_t get_oneshot_mods(void) {
    return fake_oneshot_mods;
}

uint8_t get_oneshot_locked_mods(void) {
    return fake_oneshot_locked_mods;
}

void set_mods(uint8_t mods) {
    fake_mods = mods;
}

void set_weak_mods(uint8_t mods) {
    fake_weak_mods = mods;
}

void set_oneshot_mods(uint8_t mods) {
    fake_oneshot_mods = mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    fake_oneshot_locked_mods = mods;
}

void clear_mods(void) {
    fake_mods = 0;
}

void clear_weak_mods(void) {
    fake_weak_mods = 0;
}

void clear_oneshot_mods(void) {
    fake_oneshot_mods = 0;
}

void clear_oneshot_locked_mods(void) {
    fake_oneshot_locked_mods = 0;
}

handled_key_resolution_t handled_key_lookup(uint16_t keycode) {
    return test_handled_key_resolution(keycode, 1);
}

handled_key_resolution_t handled_key_lookup_tap_count(uint16_t keycode, uint8_t tap_count) {
    return test_handled_key_resolution(keycode, tap_count);
}

void handled_key_lookup_into(uint16_t keycode, handled_key_resolution_t *out) {
    if (out) {
        *out = test_handled_key_resolution(keycode, 1u);
    }
}

void handled_key_lookup_tap_count_into(uint16_t keycode, uint8_t tap_count, handled_key_resolution_t *out) {
    if (out) {
        *out = test_handled_key_resolution(keycode, tap_count);
    }
}

bool key_behavior_has_more_taps(uint16_t keycode, uint8_t count) {
    if (keycode == TEST_PENDING_MULTI_TAP_KEY || keycode == TEST_FINAL_TAP_ONLY_KEY) {
        return count < 2u;
    }
    if (keycode == KC_LEFT_GUI) {
        return count < 3u;
    }
    if (keycode == KC_RIGHT_GUI) {
        return count < 2u;
    }

    return false;
}

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0);
}

void add_mods(uint8_t mods) {
    fake_mods |= mods;
}

void del_mods(uint8_t mods) {
    fake_mods &= (uint8_t)~mods;
}

void send_keyboard_report(void) {
    send_keyboard_report_count++;
}

static uint8_t test_pd_mode_index(pd_mode_mask_t mode) {
    switch (mode) {
        case PD_MODE_DRAGSCROLL:
            return PD_MODE_INDEX_DRAGSCROLL;
        case PD_MODE_VOLUME:
            return PD_MODE_INDEX_VOLUME;
        case PD_MODE_ARROW:
            return PD_MODE_INDEX_ARROW;
        default:
            return PD_MODE_COUNT;
    }
}

const pd_mode_def_t pd_modes[PD_MODE_COUNT] = {
    [PD_MODE_INDEX_DRAGSCROLL] =
        {
            .mode_flag   = PD_MODE_DRAGSCROLL,
            .keycode     = DRAGSCROLL,
            .lock_action = DRAGSCROLL_LOCK,
            .traits      = PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED | PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE,
        },
    [PD_MODE_INDEX_VOLUME] =
        {
            .mode_flag   = PD_MODE_VOLUME,
            .keycode     = VOLUME_MODE,
            .lock_action = VOLUME_MODE_LOCK,
            .traits      = PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED,
        },
    [PD_MODE_INDEX_ARROW] =
        {
            .mode_flag   = PD_MODE_ARROW,
            .keycode     = ARROW_MODE,
            .lock_action = ARROW_MODE_LOCK,
            .traits      = PD_MODE_TRAIT_PREFER_TYPING_LAYER,
        },
};

const pd_mode_def_t *pd_mode_lookup(pd_mode_mask_t mode) {
    uint8_t index = test_pd_mode_index(mode);
    return index < PD_MODE_COUNT ? &pd_modes[index] : NULL;
}

const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        if (pd_modes[index].lock_action == action) {
            return &pd_modes[index];
        }
    }

    return NULL;
}

uint8_t pd_mode_active_keyboard_event_masked_real_mods(void) {
    return 0;
}

bool is_pd_mode_lock_action(uint16_t action) {
    return pd_mode_lock_action_lookup(action) != NULL;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    const pd_mode_def_t *def = pd_mode_lookup(mode);
    return def != NULL && (def->traits & trait) == trait;
}

bool pd_any_active_mode_has_trait(pd_mode_traits_t trait) {
    return pd_mode_has_trait(pd_mode_local_active_snapshot(), trait);
}

bool is_keyboard_master(void) {
    return true;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    if (keycode == DRAGSCROLL) {
        return PD_MODE_DRAGSCROLL;
    }

    if (keycode == VOLUME_MODE) {
        return PD_MODE_VOLUME;
    }

    if (keycode == ARROW_MODE) {
        return PD_MODE_ARROW;
    }

    return 0;
}

void pd_mode_transition_activate(pd_mode_mask_t mode) {
    pd_mode_set(mode);
}

void pd_mode_transition_deactivate(pd_mode_mask_t mode) {
    pd_mode_clear(mode);
}

void pd_mode_transition_lock(pd_mode_mask_t mode) {
    pd_mode_set_locked(mode);
    pd_mode_set(mode);
}

void pd_mode_transition_unlock(pd_mode_mask_t mode) {
    pd_mode_clear_locked(mode);
    pd_mode_clear(mode);
}

void pd_mode_service_active_dpi_sync(void) {}

void noah_action_press(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void noah_action_release(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void action_dispatch(uint16_t action) {
    (void)action;
}

void noah_emit_action_tap(uint16_t action, noah_emit_policy_t policy) {
    last_emitted_action = action;
    (void)policy;
}

void noah_emit_action_tap_at(keypos_t key_pos, uint16_t action, noah_emit_policy_t policy) {
    (void)key_pos;
    noah_emit_action_tap(action, policy);
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    last_delayed_action = action;
    last_delayed_mods   = mods;
    if (delayed_action_count < ARRAY_SIZE(delayed_action_log)) {
        delayed_action_log[delayed_action_count]      = action;
        delayed_action_mods_log[delayed_action_count] = mods;
    }
    CHECK(projection_order_count < ARRAY_SIZE(projection_order_log));
    projection_order_log[projection_order_count++] = action;
    delayed_action_count++;

    if (delayed_action_enqueue_hook_armed) {
        delayed_action_enqueue_hook_armed = false;
        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(delayed_action_enqueue_key_pos, delayed_action_enqueue_action, delayed_action_enqueue_mods, false, 0u));
        if (delayed_action_reenter_drain) {
            key_runtime_deferred_release_drain_dispatches();
        }
    }
}

void dispatch_delayed_action_at(keypos_t key_pos, uint16_t action, delayed_action_mods_t mods) {
    (void)key_pos;
    dispatch_delayed_action(action, mods);
}

bool noah_synthetic_record_active(void) {
    return false;
}

bool macro_dispatch(uint16_t keycode) {
    (void)keycode;
    return false;
}

bool pd_mode_handle_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void split_runtime_sync(void) {
    split_runtime_sync_count++;
}

void split_runtime_sync_request(void) {
    split_runtime_sync();
}

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
                .key     = key_pos,
                .pressed = pressed,
            },
    };
}

static bool test_process_record(uint16_t keycode, keypos_t key_pos, bool pressed) {
    keyrecord_t record = test_record(key_pos, pressed);
    return noah_process_record_user(keycode, &record);
}

static void test_stage_active_slot(uint16_t keycode, keypos_t key_pos) {
    CHECK(!test_process_record(keycode, key_pos, true));
}

static void test_stage_pending_multi_tap(keypos_t key_pos) {
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, true));
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, false));
}

static void test_debug_reports_slot_phase_and_momentary_layer_interrupt_state(void) {
    keypos_t layer_key = test_keypos(0, 0);
    keypos_t other_key = test_keypos(0, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, layer_key, true));
    CHECK(noah_runtime_debug_slot_phase(layer_key) == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(!noah_runtime_debug_slot_momentary_tap_interrupted(layer_key));

    CHECK(!test_process_record(KC_V, other_key, true));
    CHECK(noah_runtime_debug_slot_phase(layer_key) == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(noah_runtime_debug_slot_momentary_tap_interrupted(layer_key));
}

static void test_snapshot_captures_cross_subsystem_runtime_state(void) {
    pd_mode_snapshot_t                        pd_snapshot;
    layer_ownership_debug_snapshot_t          layer_snapshot;
    held_action_debug_snapshot_t              held_action_snapshot;
    held_repeat_debug_snapshot_t              held_repeat_snapshot;
    keyboard_mod_ownership_debug_snapshot_t   keyboard_mod_snapshot;
    noah_runtime_trace_snapshot_t             trace_snapshot;
    const layer_ownership_binding_snapshot_t *layer_binding;
    const held_action_binding_snapshot_t     *held_action_binding;
    const held_repeat_binding_snapshot_t     *held_repeat_binding;
    keypos_t                                  key_pos;
    keypos_t                                  active_key  = test_keypos(0, 0);
    keypos_t                                  pending_key = test_keypos(0, 1);
    keypos_t                                  layer_key   = test_keypos(1, 2);
    keypos_t                                  action_key  = test_keypos(3, 4);
    keypos_t                                  repeat_key  = test_keypos(5, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_arm_feedback_pulse(test_keypos(0, 0), KEY_FEEDBACK_PULSE_HOLD);
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_ACTIVATE,
        .mode = PD_MODE_VOLUME,
    });
    pd_mode_apply_remote_snapshot(PD_MODE_ARROW, 0);
    test_stage_active_slot(KC_C, active_key);
    test_stage_pending_multi_tap(pending_key);
    noah_runtime_trace_reset();

    layer_ownership_set_lock_state(3, true);
    layer_ownership_momentary_press(layer_key, 2);
    noah_runtime_trace_emit(NOAH_TRACE_SPLIT_SYNC, NOAH_TRACE_SPLIT_SYNC_EVENT_SEND, PD_MODE_VOLUME, PD_MODE_ARROW);

    held_action_register(action_key, TEST_ACTION);
    held_repeat_start(repeat_key, TEST_ACTION, 25);

    keyboard_mod_ownership_register(KC_LEFT_SHIFT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_ALT);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    pd_snapshot = pd_mode_snapshot();
    layer_ownership_debug_snapshot(&layer_snapshot);
    held_action_debug_snapshot(&held_action_snapshot);
    held_repeat_debug_snapshot(&held_repeat_snapshot);
    keyboard_mod_ownership_debug_snapshot(&keyboard_mod_snapshot);
    noah_runtime_trace_snapshot(&trace_snapshot);
    layer_binding       = test_find_layer_binding(&layer_snapshot, layer_key, 2);
    held_action_binding = test_find_held_action_binding(&held_action_snapshot, action_key, TEST_ACTION);
    held_repeat_binding = test_find_held_repeat_binding(&held_repeat_snapshot, repeat_key, TEST_ACTION);

    CHECK(noah_runtime_debug_feedback_active());
    CHECK(pd_snapshot.local.active_mode == PD_MODE_VOLUME);
    CHECK(pd_snapshot.local.locked_mode == 0);
    CHECK(pd_snapshot.display.active_mode == PD_MODE_VOLUME);
    CHECK(noah_runtime_debug_slot_owner_keycode(active_key) == KC_C);
    CHECK(noah_runtime_debug_slot_tap_action(active_key) == KC_C);
    CHECK(!noah_runtime_debug_slot_momentary_tap_interrupted(active_key));
    CHECK(noah_runtime_debug_active_slot_count() == 1);
    CHECK(noah_runtime_debug_active_slot_key_pos(0, &key_pos));
    CHECK(test_keypos_equal(key_pos, active_key));
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 1);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_key_pos(0, &key_pos));
    CHECK(test_keypos_equal(key_pos, pending_key));
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&key_pos));
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&key_pos));

    CHECK(layer_snapshot.applied_layer_state == (((layer_state_t)1u << 2) | ((layer_state_t)1u << 3)));
    CHECK(layer_snapshot.locked_mask == ((layer_state_t)1u << 3));
    CHECK(layer_snapshot.momentary_refcounts[2] == 1);
    CHECK(layer_binding != NULL);

    CHECK(held_action_binding != NULL);
    CHECK(held_repeat_binding != NULL);
    CHECK(held_repeat_binding->interval_ms == 40);

    CHECK(keyboard_mod_snapshot.live_state.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(keyboard_mod_snapshot.live_state.weak == MOD_BIT(KC_RIGHT_ALT));
    CHECK(keyboard_mod_snapshot.live_state.oneshot == MOD_BIT(KC_LEFT_GUI));
    CHECK(keyboard_mod_snapshot.live_state.oneshot_locked == MOD_BIT(KC_RIGHT_GUI));
    CHECK(keyboard_mod_snapshot.managed_refcounts[1] == 1);

    CHECK(trace_snapshot.count == 3u);
    CHECK(!trace_snapshot.overflowed);
    CHECK(trace_snapshot.entries[0].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(trace_snapshot.entries[0].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_LOCK);
    CHECK(trace_snapshot.entries[1].kind == NOAH_TRACE_LAYER_OWNERSHIP);
    CHECK(trace_snapshot.entries[1].event == NOAH_TRACE_LAYER_OWNERSHIP_EVENT_MOMENTARY_PRESS);
    CHECK(trace_snapshot.entries[2].kind == NOAH_TRACE_SPLIT_SYNC);
    CHECK(trace_snapshot.entries[2].event == NOAH_TRACE_SPLIT_SYNC_EVENT_SEND);
    CHECK(trace_snapshot.entries[2].a == PD_MODE_VOLUME);
    CHECK(trace_snapshot.entries[2].b == PD_MODE_ARROW);
}

static void test_reset_clears_all_runtime_surfaces(void) {
    pd_mode_snapshot_t                      pd_snapshot;
    layer_ownership_debug_snapshot_t        layer_snapshot;
    held_action_debug_snapshot_t            held_action_snapshot;
    held_repeat_debug_snapshot_t            held_repeat_snapshot;
    keyboard_mod_ownership_debug_snapshot_t keyboard_mod_snapshot;
    noah_runtime_trace_snapshot_t           trace_snapshot;
    keypos_t                                active_key = test_keypos(0, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_arm_feedback_pulse(test_keypos(0, 0), KEY_FEEDBACK_PULSE_LONG_HOLD);
    (void)pd_mode_apply_command((pd_mode_command_t){
        .kind = PD_MODE_COMMAND_LOCK,
        .mode = PD_MODE_ARROW,
    });
    pd_mode_apply_remote_snapshot(PD_MODE_VOLUME, PD_MODE_VOLUME);
    test_stage_active_slot(KC_V, active_key);

    layer_ownership_set_lock_state(1, true);
    held_action_register(test_keypos(0, 1), TEST_ACTION);
    keyboard_mod_ownership_register(KC_LEFT_ALT);
    fake_weak_mods           = MOD_BIT(KC_RIGHT_CTRL);
    fake_oneshot_mods        = MOD_BIT(KC_LEFT_GUI);
    fake_oneshot_locked_mods = MOD_BIT(KC_RIGHT_GUI);

    noah_runtime_reset_for_test();
    pd_snapshot = pd_mode_snapshot();
    layer_ownership_debug_snapshot(&layer_snapshot);
    held_action_debug_snapshot(&held_action_snapshot);
    held_repeat_debug_snapshot(&held_repeat_snapshot);
    keyboard_mod_ownership_debug_snapshot(&keyboard_mod_snapshot);
    noah_runtime_trace_snapshot(&trace_snapshot);

    CHECK(!noah_runtime_debug_feedback_active());
    CHECK(pd_snapshot.local.active_mode == 0);
    CHECK(pd_snapshot.local.locked_mode == 0);
    CHECK(pd_snapshot.display.active_mode == 0);
    CHECK(pd_snapshot.display.locked_mode == 0);
    CHECK(noah_runtime_debug_slot_owner_keycode(active_key) == KC_NO);
    CHECK(noah_runtime_debug_slot_tap_action(active_key) == KC_NO);
    CHECK(noah_runtime_debug_slot_phase(active_key) == KEY_RUNTIME_SLOT_PHASE_IDLE);
    CHECK(!noah_runtime_debug_slot_momentary_tap_interrupted(active_key));
    CHECK(noah_runtime_debug_active_slot_count() == 0);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0);
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&active_key));
    CHECK(!noah_runtime_debug_pending_fallback_slot_key_pos(&active_key));

    CHECK(layer_snapshot.applied_layer_state == 0);
    CHECK(layer_snapshot.locked_mask == 0);
    CHECK(layer_snapshot.momentary_refcounts[1] == 0);
    CHECK(!layer_snapshot.bindings[0].active);

    CHECK(!held_action_snapshot.actions[0].active);
    CHECK(!held_repeat_snapshot.bindings[0].active);
    CHECK(held_action_snapshot.modifier_refcounts[2] == 0);

    CHECK(keyboard_mod_snapshot.live_state.real == 0);
    CHECK(keyboard_mod_snapshot.live_state.weak == 0);
    CHECK(keyboard_mod_snapshot.live_state.oneshot == 0);
    CHECK(keyboard_mod_snapshot.live_state.oneshot_locked == 0);
    CHECK(keyboard_mod_snapshot.managed_refcounts[2] == 0);
    CHECK(trace_snapshot.count == 0u);
    CHECK(!trace_snapshot.overflowed);
    CHECK(send_keyboard_report_count >= 2);
}

static void test_key_runtime_scan_skips_core_work_when_idle(void) {
    key_runtime_core_state_t *state;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    state = key_runtime_core_state();
    CHECK(state != NULL);
    CHECK(state->current_time == 0u);

    fake_time = (uint16_t)(fake_time + 42u);
    noah_key_runtime_scan();

    CHECK(state->current_time == 0u);
    CHECK(noah_runtime_debug_active_slot_count() == 0u);
    CHECK(noah_runtime_debug_pending_multi_tap_slot_count() == 0u);
    CHECK(delayed_action_count == 0u);
}

static void test_key_runtime_scan_drains_pending_release_without_core_work(void) {
    key_runtime_core_state_t *state;
    keypos_t                  key_pos = test_keypos(2, 5);
    keyboard_mod_state_t      mods    = {
        .real = MOD_BIT(KC_LEFT_SHIFT),
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    state = key_runtime_core_state();
    CHECK(state != NULL);
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_ACTION, mods, false, 0u));
    CHECK(key_runtime_core_pending_release_count() == 1u);

    fake_time = (uint16_t)(fake_time + 42u);
    noah_key_runtime_scan();

    CHECK(state->current_time == 0u);
    CHECK(key_runtime_core_pending_release_count() == 0u);
    CHECK(delayed_action_count == 1u);
    CHECK(last_delayed_action == TEST_ACTION);
    CHECK(last_delayed_mods.real == mods.real);
}

static void test_key_runtime_deferred_release_drain_is_bounded_and_fifo_at_full_capacity(void) {
    keypos_t              key_pos                  = test_keypos(2, 6);
    delayed_action_mods_t mods                     = {0};
    uint16_t              expected_projected_count = 0u;
    uint16_t              drain_count              = 0u;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, (uint16_t)(TEST_ACTION + index), mods, false, 0u));
    }
    CHECK(key_runtime_core_pending_release_count() == KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
    CHECK(!key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_THIRD_ACTION, mods, false, 0u));

    while (key_runtime_core_pending_release_count() != 0u) {
        uint8_t pending_before = key_runtime_core_pending_release_count();
        uint8_t expected_batch = pending_before < KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY ? pending_before : KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY;

        key_runtime_deferred_release_drain_dispatches();
        expected_projected_count = (uint16_t)(expected_projected_count + expected_batch);
        drain_count++;

        CHECK(delayed_action_count == expected_projected_count);
        CHECK(key_runtime_core_pending_release_count() == (uint8_t)(pending_before - expected_batch));
        CHECK(drain_count <= KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
    }

    CHECK(drain_count == (KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY + KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY - 1u) / KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY);
    CHECK(delayed_action_count == KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY; index++) {
        CHECK(delayed_action_log[index] == (uint16_t)(TEST_ACTION + index));
    }

    key_runtime_deferred_release_drain_dispatches();
    CHECK(delayed_action_count == KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY);
}

static void test_key_runtime_deferred_release_drain_defers_projection_enqueues_and_reentry(void) {
    keypos_t              key_pos = test_keypos(2, 7);
    delayed_action_mods_t mods    = {.real = MOD_LALT};

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_ACTION, mods, false, 0u));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_SECOND_ACTION, mods, false, 0u));

    delayed_action_enqueue_hook_armed = true;
    delayed_action_reenter_drain      = true;
    delayed_action_enqueue_key_pos    = key_pos;
    delayed_action_enqueue_action     = TEST_THIRD_ACTION;
    delayed_action_enqueue_mods       = mods;

    key_runtime_deferred_release_drain_dispatches();

    CHECK(delayed_action_count == 2u);
    CHECK(delayed_action_log[0] == TEST_ACTION);
    CHECK(delayed_action_log[1] == TEST_SECOND_ACTION);
    CHECK(key_runtime_core_pending_release_count() == 1u);

    key_runtime_deferred_release_drain_dispatches();
    CHECK(delayed_action_count == 3u);
    CHECK(delayed_action_log[2] == TEST_THIRD_ACTION);
    CHECK(key_runtime_core_pending_release_count() == 0u);

    key_runtime_deferred_release_drain_dispatches();
    CHECK(delayed_action_count == 3u);
}

static void test_key_runtime_scan_drains_bounded_batches_with_owner_mod_and_feedback_integrity(void) {
    enum {
        TEST_PENDING_COUNT = KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY + 2u,
    };
    key_runtime_core_state_t *state;
    const press_token_t      *first_token;
    const press_token_t      *second_token;
    keypos_t                  first_key  = test_keypos(3, 6);
    keypos_t                  second_key = test_keypos(3, 7);
    keyboard_mod_state_t      expected_mods[TEST_PENDING_COUNT];
    uint16_t                  first_owner;
    uint16_t                  second_owner;
    uint16_t                  state_time_before;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, second_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, second_key, fake_time);

    state        = key_runtime_core_state();
    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(state != NULL);
    CHECK(first_token != NULL);
    CHECK(second_token != NULL);
    CHECK(!first_token->active);
    CHECK(!second_token->active);
    CHECK(key_runtime_core_active_press_token_count() == 0u);
    CHECK(key_runtime_core_pending_multi_tap_count() == 0u);
    first_owner       = first_token->token_id;
    second_owner      = second_token->token_id;
    state_time_before = state->current_time;

    for (uint8_t index = 0u; index < TEST_PENDING_COUNT; index++) {
        keypos_t key_pos = (index & 1u) == 0u ? first_key : second_key;
        uint16_t owner   = (index & 1u) == 0u ? first_owner : second_owner;

        expected_mods[index] = (keyboard_mod_state_t){
            .real           = (uint8_t)(1u << index),
            .weak           = (uint8_t)(0x80u >> index),
            .oneshot        = (uint8_t)(0x10u + index),
            .oneshot_locked = (uint8_t)(0xA0u + index),
        };
        CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, (uint16_t)(TEST_ACTION + index), expected_mods[index], index == (KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY - 1u) || index == KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY, owner));
    }

    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(first_token->pending_release_emission);
    CHECK(second_token->pending_release_emission);
    CHECK(first_token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);
    CHECK(second_token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    noah_key_runtime_scan();

    CHECK(state->current_time == state_time_before);
    CHECK(delayed_action_count == KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY);
    CHECK(feedback_pulse_count == 1u);
    CHECK(key_runtime_core_pending_release_count() == 2u);
    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(first_token->pending_release_emission);
    CHECK(second_token->pending_release_emission);
    CHECK(first_token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);
    CHECK(second_token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    noah_key_runtime_scan();

    CHECK(state->current_time == state_time_before);
    CHECK(delayed_action_count == TEST_PENDING_COUNT);
    CHECK(feedback_pulse_count == 2u);
    CHECK(key_runtime_core_pending_release_count() == 0u);
    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(!first_token->pending_release_emission);
    CHECK(!second_token->pending_release_emission);
    CHECK(first_token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(second_token->phase == PRESS_TOKEN_PHASE_RELEASED);

    for (uint8_t index = 0u; index < TEST_PENDING_COUNT; index++) {
        CHECK(delayed_action_log[index] == (uint16_t)(TEST_ACTION + index));
        CHECK(delayed_action_mods_log[index].real == expected_mods[index].real);
        CHECK(delayed_action_mods_log[index].weak == expected_mods[index].weak);
        CHECK(delayed_action_mods_log[index].oneshot == expected_mods[index].oneshot);
        CHECK(delayed_action_mods_log[index].oneshot_locked == expected_mods[index].oneshot_locked);
    }

    CHECK(feedback_pulse_kind_log[0] == KEY_FEEDBACK_PULSE_TAP_COMMITTED);
    CHECK(feedback_pulse_kind_log[1] == KEY_FEEDBACK_PULSE_TAP_COMMITTED);
    CHECK(test_keypos_equal(feedback_pulse_key_pos_log[0], second_key));
    CHECK(test_keypos_equal(feedback_pulse_key_pos_log[1], first_key));
    CHECK(feedback_pulse_tap_branch_log[0] == 0u);
    CHECK(feedback_pulse_tap_branch_log[1] == 0u);
    CHECK(projection_order_count == TEST_PENDING_COUNT + 2u);
    CHECK(projection_order_log[0] == TEST_ACTION);
    CHECK(projection_order_log[1] == TEST_ACTION + 1u);
    CHECK(projection_order_log[2] == TEST_ACTION + 2u);
    CHECK(projection_order_log[3] == TEST_ACTION + 3u);
    CHECK(projection_order_log[4] == TEST_PROJECTION_FEEDBACK_MARKER);
    CHECK(projection_order_log[5] == TEST_ACTION + 4u);
    CHECK(projection_order_log[6] == TEST_PROJECTION_FEEDBACK_MARKER);
    CHECK(projection_order_log[7] == TEST_ACTION + 5u);

    noah_key_runtime_scan();
    CHECK(delayed_action_count == TEST_PENDING_COUNT);
    CHECK(feedback_pulse_count == 2u);
    CHECK(projection_order_count == TEST_PENDING_COUNT + 2u);
}

static bool test_effect_plan_contains(const key_runtime_core_effect_plan_t *plan, key_runtime_effect_kind_t kind) {
    if (!plan) {
        return false;
    }

    for (uint8_t index = 0u; index < plan->count; index++) {
        if (plan->items[index].kind == kind) {
            return true;
        }
    }

    return false;
}

static void test_key_runtime_core_unmatched_release_uses_shadow_inclusive_hold_context(void) {
    key_runtime_core_state_t      *state;
    key_runtime_core_effect_plan_t plan;
    handled_key_resolution_t       resolution = test_handled_key_resolution(TEST_TRANSPARENT_HOLD_KEY, 1u);
    keypos_t                       key_pos    = test_keypos(2, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    state = key_runtime_core_state();
    CHECK(state != NULL);
    layer_state                          = (layer_state_t)1u << 2;
    state->shadow_projection.layer_state = (layer_state_t)1u << 1;
    test_set_keymap_key(2u, key_pos, TEST_TRANSPARENT_HOLD_KEY);
    test_set_keymap_key(1u, key_pos, MO(3));

    key_runtime_core_effect_plan_init(&plan);
    CHECK(key_runtime_core_handle_handled_key_release(TEST_TRANSPARENT_HOLD_KEY, key_pos, &resolution, (keyboard_mod_state_t){0}, &plan));
    CHECK(plan.count == 2u);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);

    state->shadow_projection.layer_state = 0u;
    key_runtime_core_effect_plan_init(&plan);
    CHECK(key_runtime_core_handle_handled_key_release(TEST_TRANSPARENT_HOLD_KEY, key_pos, &resolution, (keyboard_mod_state_t){0}, &plan));
    CHECK(plan.count == 1u);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY);
}

static void test_key_runtime_core_active_release_uses_token_layer_contract_not_fallback_resolution(void) {
    key_runtime_core_effect_plan_t plan;
    handled_key_resolution_t       layer_resolution = test_handled_key_resolution(TEST_INTERRUPTED_LAYER_KEY, 1u);
    handled_key_resolution_t       plain_resolution = test_handled_key_resolution(KC_C, 1u);
    keypos_t                       key_pos          = test_keypos(2, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, key_pos, fake_time);
    key_runtime_core_effect_plan_init(&plan);
    CHECK(key_runtime_core_handle_handled_key_release(KC_C, key_pos, &layer_resolution, (keyboard_mod_state_t){0}, &plan));
    CHECK(!test_effect_plan_contains(&plan, KEY_RUNTIME_EFFECT_LAYER_RELEASE));

    test_reset_stubs();
    noah_runtime_reset_for_test();
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_INTERRUPTED_LAYER_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_INTERRUPTED_LAYER_KEY, key_pos, fake_time);
    key_runtime_core_effect_plan_init(&plan);
    CHECK(key_runtime_core_handle_handled_key_release(TEST_INTERRUPTED_LAYER_KEY, key_pos, &plain_resolution, (keyboard_mod_state_t){0}, &plan));
    CHECK(test_effect_plan_contains(&plan, KEY_RUNTIME_EFFECT_LAYER_RELEASE));
}

static void test_release_only_momentary_key_recovers_orphaned_layer_binding(void) {
    keypos_t key_pos = test_keypos(2, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    layer_ownership_momentary_press(key_pos, 2u);
    CHECK(layer_state_cmp(layer_state, 2u));
    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, key_pos, false));
    CHECK(!layer_state_cmp(layer_state, 2u));
    CHECK(noah_runtime_debug_slot_owner_keycode(key_pos) == KC_NO);
}

static void test_display_preview_bridges_momentary_layer_handoff_briefly(void) {
    keypos_t key_pos     = test_keypos(0, 0);
    keypos_t preview_pos = (keypos_t){0};

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, key_pos, true));
    CHECK(noah_runtime_debug_preview_owner_slot_key_pos(&preview_pos));
    CHECK(test_keypos_equal(preview_pos, key_pos));
    CHECK(key_feedback_preview_layer() == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(layer_state_cmp(layer_state, 2u));
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&preview_pos));
    CHECK(key_feedback_preview_layer() == 2u);

    fake_time = (uint16_t)(fake_time + (KEY_FEEDBACK_PREVIEW_DISPLAY_BRIDGE_MS - 1u));
    CHECK(key_feedback_preview_layer() == 2u);

    fake_time = (uint16_t)(fake_time + 1u);
    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_display_preview_bridge_clears_immediately_when_layer_releases(void) {
    keypos_t key_pos     = test_keypos(0, 0);
    keypos_t preview_pos = (keypos_t){0};

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, key_pos, true));
    CHECK(noah_runtime_debug_preview_owner_slot_key_pos(&preview_pos));
    CHECK(key_feedback_preview_layer() == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(layer_state_cmp(layer_state, 2u));
    CHECK(key_feedback_preview_layer() == 2u);

    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, key_pos, false));
    CHECK(!layer_state_cmp(layer_state, 2u));
    CHECK(!noah_runtime_debug_preview_owner_slot_key_pos(&preview_pos));
    CHECK(key_feedback_preview_layer() == UINT8_MAX);
}

static void test_key_runtime_core_release_tracks_press_by_position_despite_keycode_mismatch(void) {
    keypos_t              key_pos = test_keypos(2, 3);
    const press_token_t  *token;
    projection_snapshot_t snapshot;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->token_id == 1u);
    CHECK(token->physical_keycode == KC_C);
    CHECK(token->resolved_keycode == KC_C);
    CHECK(token->observed_release_keycode == KC_NO);

    fake_time = (uint16_t)(fake_time + 10u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, key_pos, fake_time);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(token->physical_keycode == KC_C);
    CHECK(token->resolved_keycode == KC_C);
    CHECK(token->observed_release_keycode == KC_V);
    CHECK(token->release_keycode_mismatched);
    CHECK(token->released_at == fake_time);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_press_token_count == 0u);
    CHECK(snapshot.core_release_keycode_mismatch_count == 1u);
    CHECK(snapshot.core_orphan_release_count == 0u);
}

static void test_key_runtime_core_timer_and_scan_do_not_rewrite_press_identity(void) {
    const uint16_t       layer_tap_keycode = LT(2, KC_V);
    keypos_t             key_pos           = test_keypos(4, 1);
    keypos_t             observed_key_pos;
    const press_token_t *token;
    runtime_event_t      advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
            },
    };
    runtime_event_t scan = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, layer_tap_keycode, key_pos, fake_time);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->hold_term_ms == TAPPING_TERM);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    key_runtime_core_apply_event(&scan, fake_time);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->token_id == 1u);
    CHECK(key_runtime_core_press_token_key_pos(token, &observed_key_pos));
    CHECK(test_keypos_equal(observed_key_pos, key_pos));
    CHECK(token->physical_keycode == layer_tap_keycode);
    CHECK(token->resolved_keycode == layer_tap_keycode);
    CHECK(token->hold_term_ms == TAPPING_TERM);
    CHECK(token->phase == PRESS_TOKEN_PHASE_HELD);
}

static void test_key_runtime_core_active_release_resolution_preserves_tap_window_without_scan(void) {
    key_runtime_core_active_release_resolution_t resolution;
    keypos_t                                     key_pos = test_keypos(4, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_RELEASE_PRIMARY_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_RELEASE_PRIMARY_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_active_release(key_pos, &resolution));
    CHECK(resolution.phase == KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW);
    CHECK(resolution.decision.outcome == KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION);
    CHECK(resolution.decision.action == TEST_ACTION);
    CHECK(!resolution.decision.release_owned_state);
}

static void test_key_runtime_core_active_release_resolution_tracks_threshold_hold_after_scan(void) {
    key_runtime_core_active_release_resolution_t resolution;
    keypos_t                                     key_pos = test_keypos(4, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_THRESHOLD_LONG_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_THRESHOLD_LONG_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_active_release(key_pos, &resolution));
    CHECK(resolution.phase == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE);
    CHECK(resolution.decision.outcome == KEY_RUNTIME_RELEASE_DECISION_OUTCOME_NONE);
    CHECK(!resolution.decision.release_owned_state);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_THRESHOLD_LONG_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_LONGER_HOLD_TERM);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_THRESHOLD_LONG_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_active_release(key_pos, &resolution));
    CHECK(resolution.phase == KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE);
    CHECK(resolution.decision.outcome == KEY_RUNTIME_RELEASE_DECISION_OUTCOME_ACTION);
    CHECK(resolution.decision.action == TEST_SECOND_ACTION);
}

static void test_key_runtime_core_active_release_effect_plan_buffers_multi_tap(void) {
    key_runtime_core_active_release_resolution_t resolution;
    key_runtime_core_release_effect_plan_t       plan;
    keypos_t                                     key_pos = test_keypos(4, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_active_release(key_pos, &resolution));
    CHECK(key_runtime_core_plan_active_release_effects(key_pos, TEST_PENDING_MULTI_TAP_KEY, &resolution, &plan));
    CHECK(plan.settlement == KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET);
    CHECK(plan.count == 0u);
    CHECK(plan.pending_multi_tap_seed.active);
    CHECK(plan.pending_multi_tap_seed.keycode == TEST_PENDING_MULTI_TAP_KEY);
    CHECK(test_keypos_equal(plan.pending_multi_tap_seed.key_pos, key_pos));
    CHECK(plan.pending_multi_tap_seed.tap_action == TEST_ACTION);
    CHECK(plan.pending_multi_tap_seed.tap_repeat_count == 1u);
    CHECK(plan.pending_multi_tap_seed.tap_hold_term == 120u);
    CHECK(plan.pending_multi_tap_seed.multi_tap_term == 180u);
    CHECK(plan.pending_multi_tap_seed.has_more_taps);
}

static void test_key_runtime_core_active_release_effect_plan_releases_layer_and_taps(void) {
    key_runtime_core_active_release_resolution_t resolution;
    key_runtime_core_release_effect_plan_t       plan;
    keypos_t                                     key_pos = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_INTERRUPTED_LAYER_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_INTERRUPTED_LAYER_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_active_release(key_pos, &resolution));
    CHECK(key_runtime_core_plan_active_release_effects(key_pos, TEST_INTERRUPTED_LAYER_KEY, &resolution, &plan));
    CHECK(plan.settlement == KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET);
    CHECK(!plan.pending_multi_tap_seed.active);
    CHECK(plan.count == 2u);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_LAYER_RELEASE);
    CHECK(test_keypos_equal(plan.items[0].data.key_pos, key_pos));
    CHECK(plan.items[1].kind == KEY_RUNTIME_EFFECT_DISPATCH_ACTION);
    CHECK(plan.items[1].data.action == KC_V);
}

static void test_key_runtime_core_direct_active_release_helper_seeds_pending_multi_tap(void) {
    key_runtime_core_effect_plan_t plan;
    const press_token_t           *token;
    const tap_series_t            *series;
    keypos_t                       key_pos = test_keypos(6, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_stage_active_slot(TEST_PENDING_MULTI_TAP_KEY, key_pos);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->resolved_keycode == TEST_PENDING_MULTI_TAP_KEY);

    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    key_runtime_core_effect_plan_init(&plan);
    handled_key_resolution_t resolution = test_handled_key_resolution(TEST_PENDING_MULTI_TAP_KEY, 1u);
    CHECK(key_runtime_core_handle_handled_key_release(TEST_PENDING_MULTI_TAP_KEY, key_pos, &resolution, (keyboard_mod_state_t){0}, &plan));
    CHECK(plan.count == 0u);
    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->keycode == TEST_PENDING_MULTI_TAP_KEY);
    CHECK(series->tap_count == 1u);
    CHECK(!series->pending_hold);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->observed_release_keycode == KC_NO);
}

static void test_key_runtime_core_pending_multi_tap_release_resolution_preserves_chain(void) {
    key_runtime_core_pending_multi_tap_release_resolution_t resolution;
    keypos_t                                                key_pos = test_keypos(4, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 60u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_release(key_pos, KC_NO, 0u, true, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_PRESERVE_CHAIN);
    CHECK(resolution.action == KC_NO);
    CHECK(resolution.repeat_count == 0u);
}

static void test_key_runtime_core_pending_multi_tap_release_effect_plan_preserves_chain(void) {
    key_runtime_core_pending_multi_tap_release_resolution_t resolution;
    key_runtime_core_release_effect_plan_t                  plan;
    keypos_t                                                key_pos = test_keypos(6, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 60u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_release(key_pos, KC_NO, 0u, true, &resolution));
    CHECK(key_runtime_core_plan_pending_multi_tap_release_effects(key_pos, false, &resolution, (delayed_action_mods_t){0}, &plan));
    CHECK(plan.settlement == KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_CLEAR_ACTIVE_PRESERVE_PENDING_MULTI_TAP);
    CHECK(plan.count == 0u);
}

static void test_key_runtime_core_pending_multi_tap_release_resolution_uses_hold_action_after_term(void) {
    key_runtime_core_pending_multi_tap_release_resolution_t resolution;
    keypos_t                                                key_pos = test_keypos(4, 5);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_RELEASE_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_RELEASE_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_release(key_pos, KC_NO, 1u, false, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_RELEASE_OUTCOME_DELAYED_ACTION);
    CHECK(resolution.action == TEST_ACTION);
    CHECK(resolution.repeat_count == 1u);
}

static void test_key_runtime_core_pending_multi_tap_release_effect_plan_delays_action(void) {
    key_runtime_core_pending_multi_tap_release_resolution_t resolution;
    key_runtime_core_release_effect_plan_t                  plan;
    delayed_action_mods_t                                   mods = {
        .real = MOD_BIT(KC_LEFT_SHIFT),
    };
    keypos_t key_pos = test_keypos(6, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_RELEASE_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_RELEASE_KEY, key_pos, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_release(key_pos, KC_NO, 1u, false, &resolution));
    CHECK(key_runtime_core_plan_pending_multi_tap_release_effects(key_pos, false, &resolution, mods, &plan));
    CHECK(plan.settlement == KEY_RUNTIME_CORE_RELEASE_SLOT_SETTLEMENT_RESET);
    CHECK(plan.count == 1u);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 1u);
    CHECK(plan.items[0].data.delayed_action.mods.real == mods.real);
}

static void test_key_runtime_core_direct_pending_multi_tap_release_helper_resets_slot(void) {
    key_runtime_core_pending_multi_tap_release_resolution_t resolution;
    key_runtime_core_release_effect_plan_t                  plan;
    const press_token_t                                    *token;
    const tap_series_t                                     *series;
    keypos_t                                                key_pos = test_keypos(6, 7);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, true));

    token  = key_runtime_core_press_token_at(key_pos);
    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->resolved_keycode == TEST_PENDING_MULTI_TAP_KEY);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->pending_hold);

    fake_time = (uint16_t)(fake_time + 120u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    CHECK(key_runtime_core_resolve_pending_multi_tap_release(key_pos, TEST_ACTION, 1u, true, &resolution));
    CHECK(key_runtime_core_plan_pending_multi_tap_release_effects(key_pos, false, &resolution, (delayed_action_mods_t){0}, &plan));
    CHECK(plan.count == 0u);
    CHECK(key_runtime_core_has_pending_multi_tap_at(key_pos));
    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->branch_confirming);
    CHECK(series->branch_confirm_tap_count == 2u);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->observed_release_keycode == TEST_PENDING_MULTI_TAP_KEY);
}

static void test_base_tap_multi_tap_flush_skips_branch_confirm(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 1u);
    CHECK(!series->branch_confirming);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);

    fake_time = (uint16_t)(fake_time + CUSTOM_MULTI_TAP_TERM + 1u);
    noah_key_runtime_scan();

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(!series->active);
    CHECK(last_delayed_action == TEST_ACTION);
    CHECK(delayed_action_count == 1u);
    CHECK(!key_runtime_core_has_pending_multi_tap_at(key_pos));

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);
}

static void test_inherited_non_base_modifier_tap_flush_keeps_branch_confirm_without_commit_feedback(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 2u);
    CHECK(series->tap_branch_has_authored_step);
    CHECK(!series->tap_branch_has_authored_tap);

    fake_time = (uint16_t)(fake_time + CUSTOM_MULTI_TAP_TERM + 1u);
    noah_key_runtime_scan();

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->branch_confirming);
    CHECK(series->branch_confirm_tap_count == 2u);
    CHECK(last_delayed_action == KC_NO);
    CHECK(delayed_action_count == 0u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_RGB_BRANCH_CONFIRM_TERM + 1u);
    noah_key_runtime_scan();

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(!series->active);
    CHECK(last_delayed_action == KC_LEFT_GUI);
    CHECK(delayed_action_count == 2u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);
}

static void test_inherited_terminal_modifier_tap_release_keeps_branch_confirm_without_commit_feedback(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(KC_RIGHT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_RIGHT_GUI, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(KC_RIGHT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_RIGHT_GUI, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->branch_confirming);
    CHECK(series->branch_confirm_tap_count == 2u);
    CHECK(series->tap_branch_has_authored_step);
    CHECK(!series->tap_branch_has_authored_tap);
    CHECK(last_delayed_action == KC_NO);
    CHECK(delayed_action_count == 0u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_RGB_BRANCH_CONFIRM_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(!key_runtime_core_has_pending_multi_tap_at(key_pos));
    CHECK(last_delayed_action == KC_RIGHT_GUI);
    CHECK(delayed_action_count == 2u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);
}

// A pending hold tier does not change the tap phase: the second tap has already
// named its branch, and that branch stays named until it is entered.
static void test_key_feedback_maps_name_the_branch_through_a_pending_hold(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 5);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(TEST_PENDING_MULTI_TAP_KEY, key_pos, true));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->pending_hold);
    CHECK(series->tap_count == 2u);
    CHECK(!series->authored_has_more_taps);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);

    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);
}

// A tap-only terminal branch names itself on the second tap and holds that name
// through every clock in between, until its action fires.
static void test_key_feedback_maps_name_final_tap_only_branch_until_the_action_fires(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, true));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->pending_hold);
    CHECK(series->tap_count == 2u);
    CHECK(!series->authored_has_more_taps);
    CHECK(last_emitted_action == KC_NO);
    CHECK(last_delayed_action == KC_NO);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);

    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_MULTI_TAP_TERM + 1u);
    noah_key_runtime_scan();

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 2u);
    CHECK(last_delayed_action == KC_NO);

    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 2u);
    CHECK(last_delayed_action == KC_NO);

    // Still the same branch. Neither the multi-tap term expiring nor the release
    // is a transition here: only entering the branch ends this phase.
    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);

    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_MULTI_TAP_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(last_delayed_action == KC_NO);
    CHECK(delayed_action_count == 0u);
    CHECK(key_runtime_core_has_pending_multi_tap_at(key_pos));

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + CUSTOM_RGB_BRANCH_CONFIRM_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(last_delayed_action == TEST_SECOND_ACTION);
    CHECK(delayed_action_count == 1u);
    CHECK(!key_runtime_core_has_pending_multi_tap_at(key_pos));

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);
}

// KC_LEFT_GUI authors branches at tap counts 2 and 3. Each tap past the base one
// renames the key to its own branch; only the base tap is quiet.
static void test_key_feedback_maps_rename_the_branch_on_every_tap(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 1u);
    CHECK(series->authored_has_more_taps);

    // Base tap: intent to enter a tap branch is not established yet.
    key_feedback_semantic_map(semantic_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_NONE);

    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 2u);
    CHECK(series->authored_has_more_taps);

    // Second tap names branch 2, with a deeper branch still reachable.
    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 3u);
    CHECK(!series->authored_has_more_taps);
    CHECK(!series->pending_hold);

    // Third tap renames to branch 3. Being the deepest authored branch does not
    // end the phase; only entering it does.
    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 3u);
}

// Regression: the branch name must not change between taps just because a clock
// expired. A tap held briefly and then released used to move the display and the
// engine onto different anchors, so the key changed color mid-gesture while the
// runtime would still accept another tap. There is no such clock now: only a new
// tap renames the branch, and only entering it ends the phase.
static void test_key_feedback_maps_hold_one_branch_name_across_a_slow_multi_tap(void) {
    uint8_t             semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t             tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    const tap_series_t *series;
    keypos_t            key_pos = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));

    // Second tap, deliberately held most of the tap-vs-hold window before release.
    // That release restamps the accept window, so the engine keeps taking taps
    // well past the point a press-anchored clock would have called this settled.
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM - 50u);
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, false));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 2u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    // Past where a clock anchored on the second press would have expired, and
    // still inside the accept window the release restarted. Same branch, and the
    // engine has resolved nothing.
    fake_time = (uint16_t)(fake_time + 60u);
    noah_key_runtime_scan();

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->branch_confirmed);
    CHECK(last_delayed_action == KC_NO);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 2u);

    // The third tap is the only thing that renames it.
    CHECK(!test_process_record(KC_LEFT_GUI, key_pos, true));

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->tap_count == 3u);

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 3u);
}

static void test_key_feedback_branch_confirm_mode_can_skip_branch_confirm_window(void) {
    uint8_t  semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t  tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    keypos_t key_pos = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    fake_branch_confirm_mode = KEY_FEEDBACK_BRANCH_CONFIRM_OFF;

    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, false));
    fake_time = (uint16_t)(fake_time + 20u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, true));
    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(TEST_FINAL_TAP_ONLY_KEY, key_pos, false));
    fake_time = (uint16_t)(fake_time + CUSTOM_MULTI_TAP_TERM + 1u);
    noah_key_runtime_scan();

    CHECK(last_delayed_action == TEST_SECOND_ACTION);
    CHECK(delayed_action_count == 1u);
    CHECK(!key_runtime_core_has_pending_multi_tap_at(key_pos));

    key_feedback_semantic_map(semantic_map);
    key_feedback_tap_branch_map(tap_branch_map);
    CHECK(key_feedback_semantic_map_get(semantic_map, key_pos) == KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED);
    CHECK(key_feedback_tap_branch_map_get(tap_branch_map, key_pos) == 0u);
}

static void test_key_feedback_flashing_visibility_tracks_each_owner_activation(void) {
    uint8_t  semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t  visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t first_key  = test_keypos(6, 2);
    keypos_t second_key = test_keypos(6, 3);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);
    key_runtime_core_observe_held_action_register(first_key, TEST_ACTION);

    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(key_feedback_semantic_map_get(semantic_map, first_key) == KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, first_key));

    fake_time = (uint16_t)(fake_time + KEY_FEEDBACK_FLASH_HALF_PERIOD_MS - 1u);
    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, first_key));

    fake_time = (uint16_t)(fake_time + 1u);
    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(key_feedback_semantic_map_get(semantic_map, first_key) == KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    CHECK(!key_origin_bitmap_has_keypos(visibility_bitmap, first_key));

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, second_key, fake_time);
    key_runtime_core_observe_held_action_register(second_key, TEST_ACTION);

    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(!key_origin_bitmap_has_keypos(visibility_bitmap, first_key));
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, second_key));

    fake_time = (uint16_t)(fake_time + KEY_FEEDBACK_FLASH_HALF_PERIOD_MS);
    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, first_key));
    CHECK(!key_origin_bitmap_has_keypos(visibility_bitmap, second_key));
}

static void test_key_feedback_broad_owner_follows_newest_activation_without_release_restart(void) {
    uint8_t  semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t  visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t  owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    keypos_t first_key  = test_keypos(6, 2);
    keypos_t second_key = test_keypos(6, 3);
    keypos_t owner;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);
    key_runtime_core_observe_held_action_register(first_key, TEST_ACTION);

    key_feedback_broad_owner_map(owner_map);
    owner = key_feedback_broad_owner_map_get(owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    CHECK(owner.row == first_key.row);
    CHECK(owner.col == first_key.col);

    fake_time = (uint16_t)(fake_time + KEY_FEEDBACK_FLASH_HALF_PERIOD_MS);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, second_key, fake_time);
    key_runtime_core_observe_held_action_register(second_key, TEST_ACTION);

    key_feedback_broad_owner_map(owner_map);
    owner = key_feedback_broad_owner_map_get(owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    CHECK(owner.row == second_key.row);
    CHECK(owner.col == second_key.col);

    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(!key_origin_bitmap_has_keypos(visibility_bitmap, first_key));
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, second_key));

    key_runtime_core_observe_held_action_unregister(second_key, TEST_ACTION);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_HELD_ACTION_KEY, second_key, fake_time);

    key_feedback_broad_owner_map(owner_map);
    owner = key_feedback_broad_owner_map_get(owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    CHECK(owner.row == first_key.row);
    CHECK(owner.col == first_key.col);

    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(!key_origin_bitmap_has_keypos(visibility_bitmap, first_key));

    fake_time = (uint16_t)(fake_time + KEY_FEEDBACK_FLASH_HALF_PERIOD_MS);
    key_feedback_semantic_map(semantic_map);
    key_feedback_flash_visibility_bitmap_for_semantic_map(semantic_map, visibility_bitmap);
    CHECK(key_origin_bitmap_has_keypos(visibility_bitmap, first_key));
}

static void test_key_feedback_broad_owner_uses_activation_sequence_across_timer_wrap(void) {
    uint8_t  owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    keypos_t first_key  = test_keypos(6, 2);
    keypos_t second_key = test_keypos(6, 3);
    keypos_t owner;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);
    key_runtime_core_observe_held_action_register(first_key, TEST_ACTION);

    fake_time = (uint16_t)(fake_time + 0x9000u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, second_key, fake_time);
    key_runtime_core_observe_held_action_register(second_key, TEST_ACTION);

    key_feedback_broad_owner_map(owner_map);
    owner = key_feedback_broad_owner_map_get(owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL);
    CHECK(owner.row == second_key.row);
    CHECK(owner.col == second_key.col);
}

static void test_key_runtime_core_pending_multi_tap_scan_resolution_promotes_hold_threshold(void) {
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    const tap_series_t                                  *series;
    keypos_t                                             key_pos = test_keypos(5, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 10u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->pending_hold);

    fake_time = (uint16_t)(fake_time + 120u);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_HOLD_THRESHOLD);
    CHECK(resolution.action == TEST_ACTION);
    CHECK(resolution.hold.action == TEST_ACTION);
    CHECK(resolution.completes_hold == false);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->branch_confirming);
}

static void test_key_runtime_core_pending_multi_tap_scan_resolution_promotes_long_hold(void) {
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    const tap_series_t                                  *series;
    keypos_t                                             key_pos = test_keypos(5, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 10u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    fake_time = (uint16_t)(fake_time + CUSTOM_LONGER_HOLD_TERM);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_LONG_HOLD);
    CHECK(resolution.action == TEST_SECOND_ACTION);
    CHECK(resolution.hold.action == TEST_SECOND_ACTION);
    CHECK(resolution.completes_hold);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->branch_confirming);
}

static void test_key_runtime_core_pending_multi_tap_scan_resolution_flushes_expired_chain(void) {
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    const tap_series_t                                  *series;
    keypos_t                                             key_pos = test_keypos(5, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 10u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 1u);
    CHECK(series->single_action == TEST_ACTION);
    CHECK(series->tap_term_ms == 180u);

    fake_time = (uint16_t)(fake_time + 181u);
    key_runtime_core_apply_event(&(runtime_event_t){.kind = RUNTIME_EVENT_KIND_SCAN}, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH);
    CHECK(resolution.action == TEST_ACTION);
    CHECK(resolution.repeat_count == 1u);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->branch_confirming);
}

static void test_key_runtime_core_transition_flush_foreign_multi_tap_clears_shadow_series(void) {
    key_runtime_transition_plan_t plan;
    const tap_series_t           *series;
    keypos_t                      pending_key = test_keypos(5, 3);
    keypos_t                      other_key   = test_keypos(5, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_stage_pending_multi_tap(pending_key);
    series = key_runtime_core_tap_series_at(pending_key);
    CHECK(series != NULL);
    CHECK(series->active);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_foreign_multi_tap(TEST_RELEASE_PRIMARY_KEY, other_key, &plan);

    CHECK(plan.count == 1u);
    CHECK(plan.items[0].kind == KEY_RUNTIME_EFFECT_DELAYED_ACTION);
    CHECK(plan.items[0].data.delayed_action.action == TEST_ACTION);
    CHECK(plan.items[0].data.delayed_action.repeat_count == 1u);

    series = key_runtime_core_tap_series_at(pending_key);
    CHECK(series != NULL);
    CHECK(!series->active);
}

static void test_key_runtime_transition_flush_multi_tap_chunks_without_dropping_effects(void) {
    enum {
        SERIES_COUNT = KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY + 5u,
    };

    key_runtime_transition_plan_t plan;
    key_runtime_core_state_t     *state;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(SERIES_COUNT < KEY_RUNTIME_CORE_TAP_SERIES_CAPACITY);
    state = key_runtime_core_state();
    CHECK(state != NULL);

    for (uint8_t index = 0; index < SERIES_COUNT; index++) {
        CHECK(index / MATRIX_COLS < MATRIX_ROWS);
        state->tap_series[index] = (tap_series_t){
            .active           = true,
            .keycode          = TEST_PENDING_MULTI_TAP_KEY,
            .tap_count        = 1u,
            .single_action    = TEST_ACTION,
            .tap_action       = TEST_ACTION,
            .tap_repeat_count = 1u,
            .saved_mod_state  = {0},
        };
        state->tap_series_count++;
    }

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_multi_tap(&plan);
    CHECK(!plan.overflowed);
    CHECK(state->tap_series_count == 0u);

    key_runtime_transition_execute_plan(&plan);
    CHECK(delayed_action_count == SERIES_COUNT);
    CHECK(last_delayed_action == TEST_ACTION);
}

static void test_key_runtime_core_transition_flush_active_keys_retires_shadow_token(void) {
    key_runtime_transition_plan_t plan;
    const press_token_t          *token;
    projection_snapshot_t         snapshot;
    keypos_t                      active_key = test_keypos(5, 5);
    keypos_t                      survivor   = test_keypos(5, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(TEST_RELEASE_PRIMARY_KEY, active_key, true));
    token = key_runtime_core_press_token_at(active_key);
    CHECK(token != NULL);
    CHECK(token->active);

    key_runtime_transition_plan_init(&plan);
    key_runtime_transition_flush_active_keys_except(survivor, &plan);

    token = key_runtime_core_press_token_at(active_key);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_CANCELLED);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_press_token_count == 0u);
}

static void test_key_runtime_core_tap_series_state_stays_independent_from_active_token_storage(void) {
    keypos_t                                             key_pos = test_keypos(6, 2);
    const press_token_t                                 *token;
    const tap_series_t                                  *series;
    projection_snapshot_t                                snapshot;
    key_runtime_core_pending_multi_tap_scan_resolution_t resolution;
    runtime_event_t                                      advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(CUSTOM_TAP_HOLD_TERM + 1u),
            },
    };
    runtime_event_t scan = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 10u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->keycode == TEST_PENDING_MULTI_TAP_KEY);
    CHECK(series->tap_count == 1u);
    CHECK(!series->pending_hold);

    fake_time = (uint16_t)(fake_time + 20u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);
    token  = key_runtime_core_press_token_at(key_pos);
    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->interaction.selection.tap_count == 2u);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 1u);
    CHECK(series->pending_hold);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    key_runtime_core_apply_event(&scan, fake_time);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_HELD);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_PENDING_MULTI_TAP_KEY, key_pos, fake_time);

    token  = key_runtime_core_press_token_at(key_pos);
    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->tap_count == 1u);
    CHECK(!series->pending_hold);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_press_token_count == 0u);
    CHECK(snapshot.core_tap_series_count == 1u);

    advance.data.timer_advance.advance_ms = 181u;
    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    key_runtime_core_apply_event(&scan, fake_time);

    CHECK(key_runtime_core_resolve_pending_multi_tap_scan(key_pos, &resolution));
    CHECK(resolution.outcome == KEY_RUNTIME_CORE_PENDING_MULTI_TAP_SCAN_OUTCOME_FLUSH);
    CHECK(resolution.action == TEST_ACTION);
    CHECK(resolution.repeat_count == 1u);

    series = key_runtime_core_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(!series->branch_confirming);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_press_token_count == 0u);
    CHECK(snapshot.core_tap_series_count == 1u);
}

static void test_key_runtime_core_layer_lock_observes_live_layer_ownership_state(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;
    keypos_t                                    key_pos = test_keypos(1, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(layer_ownership_set_lock_state(3, true));
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == ((layer_state_t)1u << 3));

    layer_ownership_momentary_press(key_pos, 2);

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == ((layer_state_t)1u << 3));

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.layer_state == (((layer_state_t)1u << 2) | ((layer_state_t)1u << 3)));
    CHECK(snapshot.core_shadow_locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(snapshot.core_shadow_layer_state == ((layer_state_t)1u << 3));
    CHECK(snapshot.core_lease_count == 0u);
    CHECK(snapshot.core_persistent_intent_count == 1u);

    CHECK(layer_ownership_momentary_release(key_pos));

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == ((layer_state_t)1u << 3));

    CHECK(layer_ownership_set_lock_state(3, false));
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->locked_layer_mask == 0u);
    CHECK(shadow->layer_state == 0u);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_locked_layer_mask == 0u);
    CHECK(snapshot.core_shadow_layer_state == 0u);
    CHECK(snapshot.locked_layer_mask == 0u);
    CHECK(snapshot.layer_state == 0u);
}

static void test_key_runtime_core_layer_tap_hold_creates_and_retires_layer_lease(void) {
    projection_snapshot_t snapshot;
    keypos_t              key_pos = test_keypos(2, 4);
    runtime_event_t       advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
            },
    };
    runtime_event_t scan = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(!test_process_record(LT(2, KC_V), key_pos, true));
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.layer_state == ((layer_state_t)1u << 2));
    CHECK(snapshot.core_shadow_layer_state == 0u);
    CHECK(snapshot.core_lease_count == 0u);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    key_runtime_core_apply_event(&scan, fake_time);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.layer_state == ((layer_state_t)1u << 2));
    CHECK(snapshot.core_shadow_layer_state == ((layer_state_t)1u << 2));
    CHECK(snapshot.core_lease_count == 1u);

    CHECK(!test_process_record(LT(2, KC_V), key_pos, false));
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.layer_state == 0u);
    CHECK(snapshot.core_shadow_layer_state == 0u);
    CHECK(snapshot.core_lease_count == 0u);
}

static void test_key_runtime_core_modifier_leases_keep_physical_and_managed_masks_separate(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    keypos_t                                    physical_pos = test_keypos(3, 3);
    keypos_t                                    mod_tap_pos  = test_keypos(3, 4);
    runtime_event_t                             advance      = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
            },
    };
    runtime_event_t scan = {
        .kind = RUNTIME_EVENT_KIND_SCAN,
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_RIGHT_SHIFT, physical_pos, fake_time);
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->keyboard_mod_state.real == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == 0u);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MT(MOD_LALT, KC_C), mod_tap_pos, fake_time);
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == 0u);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    key_runtime_core_apply_event(&scan, fake_time);

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == (uint8_t)(MOD_BIT(KC_RIGHT_SHIFT) | MOD_LALT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == MOD_LALT);
}

static void test_key_runtime_core_replacing_a_live_token_cleans_up_owned_leases(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;
    keypos_t                                    key_pos = test_keypos(5, 5);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MO(2), key_pos, fake_time);
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->layer_state == ((layer_state_t)1u << 2));

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->layer_state == 0u);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 0u);
    CHECK(snapshot.core_cancelled_press_count == 1u);
    CHECK(snapshot.core_press_token_count == 1u);
}

static void test_key_runtime_core_pd_mode_press_creates_active_mode_and_pointer_anchor(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;
    keypos_t                                    key_pos = test_keypos(1, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, VOLUME_MODE, key_pos, fake_time);

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(shadow->pd_mode_local_locked == 0);
    CHECK(shadow->pointer_anchor_active);
    CHECK(shadow->pointer_pd_mode_anchor_active);
    CHECK(!shadow->pointer_prefers_typing_layer);
    CHECK(!shadow->pointer_toggle_enabled);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(snapshot.core_shadow_pointer_anchor_active);
    CHECK(snapshot.core_shadow_pointer_pd_mode_anchor_active);
    CHECK(snapshot.core_lease_count == 2u);

    fake_time = (uint16_t)(fake_time + 10u);
    CHECK(!test_process_record(VOLUME_MODE, key_pos, false));
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->pd_mode_local_active == 0);
    CHECK(!shadow->pointer_anchor_active);
}

static void test_key_runtime_core_arrow_mode_prefers_typing_without_pointer_anchor(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;
    keypos_t                                    key_pos = test_keypos(1, 7);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, ARROW_MODE, key_pos, fake_time);

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(!shadow->pointer_pd_mode_anchor_active);
    CHECK(shadow->pointer_prefers_typing_layer);
    CHECK(!shadow->pointer_toggle_enabled);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_ARROW);
    CHECK(snapshot.core_shadow_pointer_prefers_typing_layer);
    CHECK(!snapshot.core_shadow_pointer_anchor_active);
    CHECK(snapshot.core_lease_count == 1u);
}

static void test_key_runtime_core_pd_mode_lock_observes_live_pd_mode_state(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(pd_mode_set_lock_state(PD_MODE_DRAGSCROLL, true));

    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_DRAGSCROLL);
    CHECK(shadow->pd_mode_local_locked == PD_MODE_DRAGSCROLL);
    CHECK(shadow->pointer_anchor_active);
    CHECK(shadow->pointer_pd_mode_anchor_active);
    CHECK(shadow->pointer_toggle_enabled);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_DRAGSCROLL);
    CHECK(snapshot.core_shadow_pd_mode_local_locked == PD_MODE_DRAGSCROLL);
    CHECK(snapshot.core_shadow_pointer_toggle_enabled);
    CHECK(snapshot.core_persistent_intent_count == 2u);
    CHECK(snapshot.pd_mode_local_active == PD_MODE_DRAGSCROLL);
    CHECK(snapshot.pd_mode_local_locked == PD_MODE_DRAGSCROLL);

    CHECK(pd_mode_set_lock_state(PD_MODE_DRAGSCROLL, false));
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->pd_mode_local_active == 0);
    CHECK(shadow->pd_mode_local_locked == 0);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(!shadow->pointer_toggle_enabled);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == 0);
    CHECK(snapshot.core_shadow_pd_mode_local_locked == 0);
    CHECK(!snapshot.core_shadow_pointer_anchor_active);
    CHECK(!snapshot.core_shadow_pointer_toggle_enabled);
    CHECK(snapshot.pd_mode_local_active == 0);
    CHECK(snapshot.pd_mode_local_locked == 0);
    CHECK(!snapshot.pointer_toggle_enabled);
}

static void test_key_runtime_core_activating_new_pd_mode_clears_foreign_mode_leases(void) {
    const key_runtime_core_shadow_projection_t *shadow;
    projection_snapshot_t                       snapshot;
    keypos_t                                    volume_pos = test_keypos(2, 6);
    keypos_t                                    arrow_pos  = test_keypos(2, 7);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, VOLUME_MODE, volume_pos, fake_time);
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(shadow->pointer_anchor_active);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, ARROW_MODE, arrow_pos, fake_time);
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(shadow->pointer_prefers_typing_layer);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_shadow_pd_mode_local_active == PD_MODE_ARROW);
    CHECK(snapshot.core_lease_count == 1u);

    fake_time = (uint16_t)(fake_time + 5u);
    CHECK(!test_process_record(VOLUME_MODE, volume_pos, false));
    shadow = key_runtime_core_shadow_projection();
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(shadow->pointer_prefers_typing_layer);
}

static void test_key_runtime_core_deferred_release_blocker_expires_after_tap_term(void) {
    projection_snapshot_t snapshot;
    keypos_t              key_pos = test_keypos(4, 0);
    runtime_event_t       advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(CUSTOM_TAP_HOLD_TERM + 1u),
            },
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_INTERRUPTED_LAYER_KEY, key_pos, fake_time);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 1u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 1u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(key_pos) == 1u);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 0u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 0u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(key_pos) == 0u);
}

static void test_key_runtime_core_production_hooks_feed_blocker_queries(void) {
    keypos_t key_pos = test_keypos(4, 5);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    CHECK(key_runtime_core_blocker_queries_authoritative());
    CHECK(!test_process_record(TEST_INTERRUPTED_LAYER_KEY, key_pos, true));
    CHECK(key_runtime_core_blocker_queries_authoritative());
    CHECK(key_runtime_core_has_any_deferred_release_blocker());
    CHECK(key_runtime_core_has_foreign_deferred_release_blocker_except(test_keypos(7, 7)));
    CHECK(!key_runtime_core_has_foreign_deferred_release_blocker_except(key_pos));

    fake_time = (uint16_t)(fake_time + CUSTOM_TAP_HOLD_TERM + 1u);
    key_runtime_core_observe_scan_cycle(fake_time);

    CHECK(key_runtime_core_blocker_queries_authoritative());
    CHECK(!key_runtime_core_has_any_deferred_release_blocker());
    CHECK(!key_runtime_core_has_foreign_deferred_release_blocker_except(test_keypos(7, 7)));
}

static void test_key_runtime_core_deferred_release_blocker_persists_after_tap_term_for_plain_tap(void) {
    projection_snapshot_t snapshot;
    keypos_t              key_pos = test_keypos(4, 2);
    runtime_event_t       advance = {
        .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
        .data.timer_advance =
            {
                .advance_ms = (uint16_t)(CUSTOM_TAP_HOLD_TERM + 1u),
            },
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, key_pos, fake_time);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 1u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 0u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(key_pos) == 1u);

    key_runtime_core_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 1u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 0u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(key_pos) == 1u);
}

static void test_key_runtime_core_other_press_interrupt_clears_momentary_layer_quick_tap_blocker(void) {
    projection_snapshot_t snapshot;
    keypos_t              layer_key = test_keypos(4, 3);
    keypos_t              other_key = test_keypos(4, 4);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_INTERRUPTED_LAYER_KEY, layer_key, fake_time);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 1u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 1u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(layer_key) == 1u);

    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, other_key, fake_time);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_deferred_release_blocker_count == 1u);
    CHECK(snapshot.core_deferred_release_timed_blocker_count == 0u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(layer_key) == 0u);
    CHECK(key_runtime_core_deferred_release_blocker_count_for_keypos(other_key) == 1u);
}

static void test_key_runtime_core_pending_release_state_survives_same_key_reuse(void) {
    projection_snapshot_t snapshot;
    const press_token_t  *token;
    keypos_t              key_pos = test_keypos(4, 1);
    keyboard_mod_state_t  mods    = {
        .real = MOD_LALT,
        .weak = MOD_BIT(KC_LEFT_SHIFT),
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, key_pos, fake_time);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);

    key_runtime_core_observe_release_dispatch_deferred(key_pos, TEST_ACTION, mods);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);
    CHECK(token->pending_release_emission);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_pending_release_count == 1u);
    CHECK(key_runtime_core_pending_release_count_for_keypos(key_pos) == 1u);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MO(2), key_pos, fake_time);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->resolved_keycode == MO(2));

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_pending_release_count == 1u);
    CHECK(key_runtime_core_pending_release_count_for_keypos(key_pos) == 1u);

    key_runtime_core_observe_release_dispatch_drained(key_pos, TEST_ACTION, mods);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_pending_release_count == 0u);
    CHECK(key_runtime_core_pending_release_count_for_keypos(key_pos) == 0u);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->resolved_keycode == MO(2));
}

static void test_key_runtime_core_take_pending_releases_preserves_order_and_clears_tokens(void) {
    pending_release_t     drained[2];
    projection_snapshot_t snapshot;
    const press_token_t  *first_token;
    const press_token_t  *second_token;
    keypos_t              first_key   = test_keypos(4, 6);
    keypos_t              second_key  = test_keypos(4, 7);
    keyboard_mod_state_t  first_mods  = {.real = MOD_LALT};
    keyboard_mod_state_t  second_mods = {.weak = MOD_BIT(KC_LEFT_SHIFT)};
    uint8_t               drained_count;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, first_key, fake_time);

    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, second_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, second_key, fake_time);

    key_runtime_core_observe_release_dispatch_deferred(first_key, TEST_ACTION, first_mods);
    key_runtime_core_observe_release_dispatch_deferred(second_key, TEST_SECOND_ACTION, second_mods);

    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(first_token != NULL);
    CHECK(second_token != NULL);
    CHECK(first_token->pending_release_emission);
    CHECK(second_token->pending_release_emission);

    drained_count = key_runtime_core_take_pending_release_dispatches(drained, ARRAY_SIZE(drained));
    CHECK(drained_count == 2u);
    CHECK(drained[0].action == TEST_ACTION);
    CHECK(key_runtime_core_pending_release_count_for_keypos(first_key) == 0u);
    CHECK(drained[1].action == TEST_SECOND_ACTION);
    CHECK(key_runtime_core_pending_release_count_for_keypos(second_key) == 0u);

    first_token  = key_runtime_core_press_token_at(first_key);
    second_token = key_runtime_core_press_token_at(second_key);
    CHECK(first_token != NULL);
    CHECK(second_token != NULL);
    CHECK(!first_token->pending_release_emission);
    CHECK(!second_token->pending_release_emission);
    CHECK(first_token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(second_token->phase == PRESS_TOKEN_PHASE_RELEASED);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_pending_release_count == 0u);
}

static void test_key_runtime_core_take_pending_releases_clears_owner_after_last_dispatch(void) {
    pending_release_t    drained;
    const press_token_t *token;
    keypos_t             key_pos = test_keypos(4, 5);
    keyboard_mod_state_t mods    = {.real = MOD_LALT};
    uint16_t             owner_token_id;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, key_pos, fake_time);

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    owner_token_id = token->token_id;
    CHECK(owner_token_id != 0u);

    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_ACTION, mods, false, owner_token_id));
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(key_pos, TEST_SECOND_ACTION, mods, false, owner_token_id));

    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token->pending_release_emission);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(drained.action == TEST_ACTION);
    CHECK(key_runtime_core_pending_release_count() == 1u);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token->pending_release_emission);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASE_PENDING);

    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(drained.action == TEST_SECOND_ACTION);
    CHECK(key_runtime_core_pending_release_count() == 0u);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(!token->pending_release_emission);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);
}

static void test_runtime_debug_deferred_release_view_follows_key_runtime_core_queue(void) {
    pending_release_t    pending;
    keypos_t             first_key  = test_keypos(5, 2);
    keypos_t             second_key = test_keypos(5, 3);
    keypos_t             debug_key_pos;
    keyboard_mod_state_t first_mods  = {.real = MOD_LALT};
    keyboard_mod_state_t second_mods = {.weak = MOD_BIT(KC_LEFT_SHIFT)};

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, first_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, first_key, fake_time);

    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, second_key, fake_time);
    fake_time = (uint16_t)(fake_time + 5u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, second_key, fake_time);

    CHECK(key_runtime_core_queue_pending_release_dispatch(first_key, TEST_ACTION, first_mods, false));
    CHECK(key_runtime_core_queue_pending_release_dispatch(second_key, TEST_SECOND_ACTION, second_mods, false));

    CHECK(noah_runtime_debug_deferred_release_count() == 2u);
    CHECK(noah_runtime_debug_deferred_release_key_pos(0u, &debug_key_pos));
    CHECK(debug_key_pos.row == first_key.row);
    CHECK(debug_key_pos.col == first_key.col);
    CHECK(noah_runtime_debug_deferred_release_action(0u) == TEST_ACTION);
    CHECK(noah_runtime_debug_deferred_release_key_pos(1u, &debug_key_pos));
    CHECK(debug_key_pos.row == second_key.row);
    CHECK(debug_key_pos.col == second_key.col);
    CHECK(noah_runtime_debug_deferred_release_action(1u) == TEST_SECOND_ACTION);

    CHECK(key_runtime_core_pending_release_at_order(0u, &pending));
    CHECK(pending.action == TEST_ACTION);
    CHECK(key_runtime_core_pending_release_at_order(1u, &pending));
    CHECK(pending.action == TEST_SECOND_ACTION);
}

static void test_key_runtime_core_runtime_owned_state_leases_track_and_clear(void) {
    projection_snapshot_t snapshot;
    keypos_t              key_pos = test_keypos(5, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    key_runtime_core_observe_held_action_register(key_pos, TEST_ACTION);
    key_runtime_core_observe_repeat_start(key_pos, TEST_SECOND_ACTION, 25u);

    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 2u);

    key_runtime_core_observe_held_action_unregister(key_pos, TEST_ACTION);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 1u);

    CHECK(key_runtime_core_release_owned_state_by_key(key_pos));
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 0u);
    CHECK(!key_runtime_core_release_owned_state_by_key(key_pos));
}

static void test_key_runtime_core_transition_execute_plan_updates_owned_state_leases(void) {
    projection_snapshot_t         snapshot;
    key_runtime_transition_plan_t plan = {
        .count = 2u,
        .items =
            {
                [0] =
                    {
                        .kind = KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER,
                        .data.held_action =
                            {
                                .key_pos = test_keypos(5, 1),
                                .action  = TEST_ACTION,
                            },
                    },
                [1] =
                    {
                        .kind = KEY_RUNTIME_EFFECT_REPEAT_START,
                        .data.repeat =
                            {
                                .key_pos   = test_keypos(5, 1),
                                .action    = TEST_SECOND_ACTION,
                                .repeat_hz = 25u,
                            },
                    },
            },
    };
    key_runtime_transition_plan_t release_plan = {
        .count = 1u,
        .items =
            {
                [0] =
                    {
                        .kind         = KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY,
                        .data.key_pos = test_keypos(5, 1),
                    },
            },
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    key_runtime_transition_execute_plan(&plan);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 2u);

    key_runtime_transition_execute_plan(&release_plan);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_lease_count == 0u);
}

static void test_key_runtime_core_projector_executes_effects_and_pending_dispatches(void) {
    delayed_action_mods_t mods = {
        .real = MOD_BIT(KC_LEFT_SHIFT),
    };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    key_runtime_core_project_effect(&(key_runtime_effect_t){
        .kind        = KEY_RUNTIME_EFFECT_DISPATCH_ACTION,
        .data.action = TEST_ACTION,
    });
    CHECK(last_emitted_action == TEST_ACTION);

    key_runtime_core_project_effect(&(key_runtime_effect_t){
        .kind                  = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP,
        .data.pd_mode_lock_tap = {.pd_mode = PD_MODE_ARROW, .key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS}},
    });
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);

    key_runtime_core_project_effect(&(key_runtime_effect_t){
        .kind = KEY_RUNTIME_EFFECT_PD_MODE_LOCK_STATE,
        .data.pd_mode_lock_state =
            {
                .pd_mode = PD_MODE_ARROW,
                .key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS},
                .locked  = false,
            },
    });
    CHECK(pd_mode_local_locked_snapshot() == 0);

    key_runtime_core_project_pending_release_dispatch(&(pending_release_t){
        .action = TEST_SECOND_ACTION,
        .mods   = mods,
    });
    CHECK(last_delayed_action == TEST_SECOND_ACTION);
    CHECK(last_delayed_mods.real == mods.real);
}

#if KEY_RUNTIME_CORE_TOKEN_ID_MAX == UINT16_MAX
static void test_key_runtime_core_token_ids_wrap_without_issuing_zero(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    const press_token_t      *token;
    keypos_t                  first_key  = test_keypos(0, 0);
    keypos_t                  second_key = test_keypos(0, 1);
    keypos_t                  third_key  = test_keypos(0, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    state->next_token_id = UINT16_MAX - 1u;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, first_key, fake_time);
    token = key_runtime_core_press_token_at(first_key);
    CHECK(token && token->token_id == UINT16_MAX - 1u);
    CHECK(state->next_token_id == UINT16_MAX);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, first_key, ++fake_time);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, second_key, ++fake_time);
    token = key_runtime_core_press_token_at(second_key);
    CHECK(token && token->token_id == UINT16_MAX);
    CHECK(state->next_token_id == 1u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, second_key, ++fake_time);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_B, third_key, ++fake_time);
    token = key_runtime_core_press_token_at(third_key);
    CHECK(token && token->token_id == 1u);
    CHECK(state->next_token_id == 2u);
    CHECK(state->token_allocation_failure_count == 0u);
}

static void test_key_runtime_core_token_allocator_skips_every_live_owner_store(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    const press_token_t      *allocated;
    keypos_t                  allocated_key = test_keypos(0, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    state->press_tokens[0] = (press_token_t){
        .active   = true,
        .token_id = 1u,
        .phase    = PRESS_TOKEN_PHASE_PRESSED,
    };
    state->press_tokens[1] = (press_token_t){
        .token_id                 = 2u,
        .phase                    = PRESS_TOKEN_PHASE_RELEASE_PENDING,
        .pending_release_emission = true,
    };
    state->press_token_count = 1u;
    state->leases[0]         = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_HELD_ACTION,
        .owner_token_id = 3u,
    };
    state->leases[1] = (lease_t){
        .active         = true,
        .kind           = LEASE_KIND_REPEAT,
        .owner_token_id = 4u,
    };
    state->lease_count         = 2u;
    state->pending_releases[0] = (pending_release_slot_t){
        .owner_token_id = 5u,
        .flags          = KEY_RUNTIME_PENDING_RELEASE_FLAG_ACTIVE,
    };
    state->pending_release_count = 1u;
    state->next_token_id         = 1u;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, allocated_key, fake_time);
    allocated = key_runtime_core_press_token_at(allocated_key);
    CHECK(allocated && allocated->active);
    CHECK(allocated->token_id == 6u);
    CHECK(state->next_token_id == 7u);
    CHECK(state->token_allocation_failure_count == 0u);
}

static void test_key_runtime_core_token_allocator_skips_live_low_id_after_wrap(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    const press_token_t      *wrapped;
    keypos_t                  live_key    = test_keypos(0, 0);
    keypos_t                  max_key     = test_keypos(0, 1);
    keypos_t                  wrapped_key = test_keypos(0, 2);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    state->press_tokens[0] = (press_token_t){
        .active   = true,
        .token_id = 1u,
        .phase    = PRESS_TOKEN_PHASE_PRESSED,
    };
    state->press_token_count = 1u;
    state->next_token_id     = UINT16_MAX;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, max_key, fake_time);
    CHECK(key_runtime_core_press_token_at(max_key)->token_id == UINT16_MAX);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, wrapped_key, ++fake_time);
    wrapped = key_runtime_core_press_token_at(wrapped_key);
    CHECK(wrapped && wrapped->token_id == 2u);
    CHECK(wrapped->token_id != key_runtime_core_press_token_at(live_key)->token_id);
    CHECK(state->next_token_id == 3u);
}

static void test_key_runtime_core_wrapped_owner_releases_held_and_repeat_leases(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    const press_token_t      *token;
    keypos_t                  key_pos = test_keypos(1, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    state->next_token_id = UINT16_MAX;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, key_pos, fake_time);
    token = key_runtime_core_press_token_at(key_pos);
    CHECK(token && token->token_id == UINT16_MAX);
    key_runtime_core_observe_held_action_register(key_pos, TEST_ACTION);
    key_runtime_core_observe_repeat_start(key_pos, TEST_SECOND_ACTION, 20u);
    CHECK(key_runtime_core_owner_has_lease_kind(state, UINT16_MAX, LEASE_KIND_HELD_ACTION));
    CHECK(key_runtime_core_owner_has_lease_kind(state, UINT16_MAX, LEASE_KIND_REPEAT));

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, TEST_HELD_ACTION_KEY, key_pos, ++fake_time);
    CHECK(key_runtime_core_release_owned_state_by_key(key_pos));
    CHECK(!key_runtime_core_owner_has_lease_kind(state, UINT16_MAX, LEASE_KIND_HELD_ACTION));
    CHECK(!key_runtime_core_owner_has_lease_kind(state, UINT16_MAX, LEASE_KIND_REPEAT));
    CHECK(state->lease_count == 0u);
}

static void test_key_runtime_core_pending_owner_survives_token_wrap_and_reuse(void) {
    key_runtime_core_state_t *state = key_runtime_core_state();
    pending_release_t         drained;
    const press_token_t      *old_token;
    const press_token_t      *new_token;
    keypos_t                  old_key = test_keypos(1, 0);
    keypos_t                  new_key = test_keypos(1, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    state->next_token_id = UINT16_MAX;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, old_key, fake_time);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, old_key, ++fake_time);
    old_token = key_runtime_core_press_token_at(old_key);
    CHECK(old_token && old_token->token_id == UINT16_MAX);
    CHECK(key_runtime_core_queue_pending_release_dispatch_for_owner(old_key, TEST_ACTION, (keyboard_mod_state_t){0}, false, UINT16_MAX));
    CHECK(key_runtime_core_press_token_at(old_key)->pending_release_emission);

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_V, new_key, ++fake_time);
    new_token = key_runtime_core_press_token_at(new_key);
    CHECK(new_token && new_token->token_id == 1u);
    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, new_key, ++fake_time);

    CHECK(key_runtime_core_take_pending_release_dispatches(&drained, 1u) == 1u);
    CHECK(drained.owner_token_id == UINT16_MAX);
    CHECK(!key_runtime_core_press_token_at(old_key)->pending_release_emission);
    CHECK(key_runtime_core_press_token_at(old_key)->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(key_runtime_core_press_token_at(new_key)->token_id == 1u);
}
#endif

#if KEY_RUNTIME_CORE_TOKEN_ID_MAX == 3u
static void test_key_runtime_core_token_allocator_exhaustion_fails_closed(void) {
    key_runtime_core_state_t      *state = key_runtime_core_state();
    key_runtime_core_effect_plan_t plan;
    projection_snapshot_t          snapshot;
    keypos_t                       key_pos = test_keypos(0, 0);

    test_reset_stubs();
    noah_runtime_reset_for_test();
    for (uint16_t index = 0u; index < KEY_RUNTIME_CORE_TOKEN_ID_MAX; index++) {
        state->leases[index] = (lease_t){
            .active         = true,
            .kind           = LEASE_KIND_HELD_ACTION,
            .owner_token_id = (uint16_t)(index + 1u),
        };
    }
    state->lease_count   = KEY_RUNTIME_CORE_TOKEN_ID_MAX;
    state->next_token_id = 3u;

    test_key_runtime_core_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, TEST_HELD_ACTION_KEY, key_pos, fake_time);
    CHECK(state->press_token_count == 0u);
    CHECK(state->lease_count == KEY_RUNTIME_CORE_TOKEN_ID_MAX);
    CHECK(state->next_token_id == 3u);
    CHECK(state->token_allocation_failure_count == 1u);
    CHECK(key_runtime_core_press_token_at(key_pos)->token_id == 0u);

    key_runtime_core_effect_plan_init(&plan);
    CHECK(key_runtime_core_handle_handled_key_press(TEST_HELD_ACTION_KEY, key_pos, &plan));
    CHECK(plan.count == 0u);
    CHECK(state->token_allocation_failed_packed_key_pos == KEY_RUNTIME_PACKED_KEYPOS_NONE);
    snapshot = key_runtime_core_projection_snapshot_capture();
    CHECK(snapshot.core_token_allocation_failure_count == 1u);
}
#endif

int main(void) {
#if KEY_RUNTIME_CORE_TOKEN_ID_MAX == UINT16_MAX
    test_debug_reports_slot_phase_and_momentary_layer_interrupt_state();
    test_snapshot_captures_cross_subsystem_runtime_state();
    test_reset_clears_all_runtime_surfaces();
    test_key_runtime_scan_skips_core_work_when_idle();
    test_key_runtime_scan_drains_pending_release_without_core_work();
    test_key_runtime_deferred_release_drain_is_bounded_and_fifo_at_full_capacity();
    test_key_runtime_deferred_release_drain_defers_projection_enqueues_and_reentry();
    test_key_runtime_scan_drains_bounded_batches_with_owner_mod_and_feedback_integrity();
    test_key_runtime_core_unmatched_release_uses_shadow_inclusive_hold_context();
    test_key_runtime_core_active_release_uses_token_layer_contract_not_fallback_resolution();
    test_release_only_momentary_key_recovers_orphaned_layer_binding();
    test_display_preview_bridges_momentary_layer_handoff_briefly();
    test_display_preview_bridge_clears_immediately_when_layer_releases();
    test_key_runtime_core_release_tracks_press_by_position_despite_keycode_mismatch();
    test_key_runtime_core_timer_and_scan_do_not_rewrite_press_identity();
    test_key_runtime_core_active_release_resolution_preserves_tap_window_without_scan();
    test_key_runtime_core_active_release_resolution_tracks_threshold_hold_after_scan();
    test_key_runtime_core_active_release_effect_plan_buffers_multi_tap();
    test_key_runtime_core_active_release_effect_plan_releases_layer_and_taps();
    test_key_runtime_core_direct_active_release_helper_seeds_pending_multi_tap();
    test_key_runtime_core_pending_multi_tap_release_resolution_preserves_chain();
    test_key_runtime_core_pending_multi_tap_release_effect_plan_preserves_chain();
    test_key_runtime_core_pending_multi_tap_release_resolution_uses_hold_action_after_term();
    test_key_runtime_core_pending_multi_tap_release_effect_plan_delays_action();
    test_key_runtime_core_direct_pending_multi_tap_release_helper_resets_slot();
    test_base_tap_multi_tap_flush_skips_branch_confirm();
    test_inherited_non_base_modifier_tap_flush_keeps_branch_confirm_without_commit_feedback();
    test_inherited_terminal_modifier_tap_release_keeps_branch_confirm_without_commit_feedback();
    test_key_feedback_maps_name_the_branch_through_a_pending_hold();
    test_key_feedback_maps_name_final_tap_only_branch_until_the_action_fires();
    test_key_feedback_maps_rename_the_branch_on_every_tap();
    test_key_feedback_maps_hold_one_branch_name_across_a_slow_multi_tap();
    test_key_feedback_branch_confirm_mode_can_skip_branch_confirm_window();
    test_key_feedback_flashing_visibility_tracks_each_owner_activation();
    test_key_feedback_broad_owner_follows_newest_activation_without_release_restart();
    test_key_feedback_broad_owner_uses_activation_sequence_across_timer_wrap();
    test_key_runtime_core_pending_multi_tap_scan_resolution_promotes_hold_threshold();
    test_key_runtime_core_pending_multi_tap_scan_resolution_promotes_long_hold();
    test_key_runtime_core_pending_multi_tap_scan_resolution_flushes_expired_chain();
    test_key_runtime_core_transition_flush_foreign_multi_tap_clears_shadow_series();
    test_key_runtime_transition_flush_multi_tap_chunks_without_dropping_effects();
    test_key_runtime_core_transition_flush_active_keys_retires_shadow_token();
    test_key_runtime_core_tap_series_state_stays_independent_from_active_token_storage();
    test_key_runtime_core_layer_lock_observes_live_layer_ownership_state();
    test_key_runtime_core_layer_tap_hold_creates_and_retires_layer_lease();
    test_key_runtime_core_modifier_leases_keep_physical_and_managed_masks_separate();
    test_key_runtime_core_replacing_a_live_token_cleans_up_owned_leases();
    test_key_runtime_core_pd_mode_press_creates_active_mode_and_pointer_anchor();
    test_key_runtime_core_arrow_mode_prefers_typing_without_pointer_anchor();
    test_key_runtime_core_pd_mode_lock_observes_live_pd_mode_state();
    test_key_runtime_core_activating_new_pd_mode_clears_foreign_mode_leases();
    test_key_runtime_core_deferred_release_blocker_expires_after_tap_term();
    test_key_runtime_core_production_hooks_feed_blocker_queries();
    test_key_runtime_core_deferred_release_blocker_persists_after_tap_term_for_plain_tap();
    test_key_runtime_core_other_press_interrupt_clears_momentary_layer_quick_tap_blocker();
    test_key_runtime_core_pending_release_state_survives_same_key_reuse();
    test_key_runtime_core_take_pending_releases_preserves_order_and_clears_tokens();
    test_key_runtime_core_take_pending_releases_clears_owner_after_last_dispatch();
    test_runtime_debug_deferred_release_view_follows_key_runtime_core_queue();
    test_key_runtime_core_runtime_owned_state_leases_track_and_clear();
    test_key_runtime_core_transition_execute_plan_updates_owned_state_leases();
    test_key_runtime_core_projector_executes_effects_and_pending_dispatches();
    test_key_runtime_core_token_ids_wrap_without_issuing_zero();
    test_key_runtime_core_token_allocator_skips_every_live_owner_store();
    test_key_runtime_core_token_allocator_skips_live_low_id_after_wrap();
    test_key_runtime_core_wrapped_owner_releases_held_and_repeat_leases();
    test_key_runtime_core_pending_owner_survives_token_wrap_and_reuse();
#elif KEY_RUNTIME_CORE_TOKEN_ID_MAX == 3u
    test_key_runtime_core_token_allocator_exhaustion_fails_closed();
#else
#    error "runtime_debug_test requires the production or exhaustion token-ID domain"
#endif

    puts("runtime_debug host tests passed");
    return 0;
}

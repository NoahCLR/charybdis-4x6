#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/action/action_lifecycle.h"
#include "users/noah/lib/key/interaction/handled_key.h"
#include "users/noah/lib/key/ownership/held_action.h"
#include "users/noah/lib/key/ownership/held_repeat.h"
#include "users/noah/lib/key/runtime/delayed_action.h"
#include "users/noah/lib/key/runtime/key_runtime_feedback.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/runtime/pd_mode_internal.h"
#include "users/noah/lib/runtime_v2/runtime_v2.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/ownership/layer_ownership.h"
#include "users/noah/lib/state/runtime/runtime_debug.h"
#include "users/noah/lib/state/runtime/runtime_reset.h"
#include "users/noah/lib/state/runtime/runtime_trace.h"
#include "users/noah/noah_runtime.h"

enum {
    TEST_ACTION                = SAFE_RANGE + 0x10,
    TEST_PENDING_MULTI_TAP_KEY = SAFE_RANGE + 0x11,
    TEST_INTERRUPTED_LAYER_KEY = SAFE_RANGE + 0x12,
};

static uint16_t fake_time;
static uint8_t  fake_mods;
static uint8_t  fake_weak_mods;
static uint8_t  fake_oneshot_mods;
static uint8_t  fake_oneshot_locked_mods;
static uint8_t  send_keyboard_report_count;

layer_state_t layer_state;

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

static runtime_event_t test_runtime_v2_key_event(runtime_event_kind_t kind, uint16_t keycode, keypos_t key_pos) {
    return (runtime_event_t){
        .kind = kind,
        .data.key_event =
            {
                .keycode = keycode,
                .key_pos = key_pos,
            },
    };
}

static void test_runtime_v2_apply_key_event(runtime_event_kind_t kind, uint16_t keycode, keypos_t key_pos, uint16_t event_time) {
    runtime_event_t event = test_runtime_v2_key_event(kind, keycode, key_pos);

    runtime_v2_apply_event(&event, event_time);
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
    uint16_t flags = HANDLED_KEY_FLAG_HANDLED;

    if (keycode == TEST_PENDING_MULTI_TAP_KEY) {
        flags |= HANDLED_KEY_FLAG_MULTI_TAP;
    } else if (keycode == TEST_INTERRUPTED_LAYER_KEY) {
        flags |= HANDLED_KEY_FLAG_MOMENTARY_LAYER | HANDLED_KEY_FLAG_LAYER_TAP;
    }

    return (handled_key_resolution_t){
        .keycode          = keycode,
        .tap_count        = tap_count,
        .step             = keycode == TEST_INTERRUPTED_LAYER_KEY ? (key_behavior_step_t){
            .tap  = TAP_SENDS(KC_V),
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(2)),
        } : (key_behavior_step_t){
            .tap = TAP_SENDS(keycode),
        },
        .tap_hold_term    = keycode == TEST_PENDING_MULTI_TAP_KEY ? 120 : CUSTOM_TAP_HOLD_TERM,
        .longer_hold_term = CUSTOM_LONGER_HOLD_TERM,
        .multi_tap_term   = keycode == TEST_PENDING_MULTI_TAP_KEY ? 180 : CUSTOM_MULTI_TAP_TERM,
        .layer            = keycode == TEST_INTERRUPTED_LAYER_KEY ? 2 : UINT8_MAX,
        .pd_mode          = 0,
        .has_more_taps    = false,
        .flags            = flags,
    };
}

static void test_reset_stubs(void) {
    fake_time                  = 1000;
    fake_mods                  = 0;
    fake_weak_mods             = 0;
    fake_oneshot_mods          = 0;
    fake_oneshot_locked_mods   = 0;
    send_keyboard_report_count = 0;
    layer_state                = 0;
}

uint16_t timer_read(void) {
    return fake_time;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(fake_time - last);
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return layer < LAYER_COUNT && (state & ((layer_state_t)1u << layer)) != 0;
}

uint16_t keycode_at_keymap_location(uint8_t layer_num, uint8_t row, uint8_t column) {
    (void)layer_num;
    (void)row;
    (void)column;
    return KC_TRNS;
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

handled_key_resolution_ctx_t handled_key_resolution_ctx_live(keypos_t key_pos) {
    return handled_key_resolution_ctx_make(key_pos, (layer_state_t)1u << 0);
}

delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
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
    (void)action;
    (void)policy;
}

void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    (void)action;
    (void)mods;
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

void split_runtime_sync(void) {}

static keyrecord_t test_record(keypos_t key_pos, bool pressed) {
    return (keyrecord_t){
        .event =
            {
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

    key_feedback_pulse_arm(false);
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

    key_feedback_pulse_arm(true);
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

static void test_runtime_v2_release_tracks_press_by_position_despite_keycode_mismatch(void) {
    keypos_t                key_pos = test_keypos(2, 3);
    const press_token_t    *token;
    projection_snapshot_t   snapshot;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    token = runtime_v2_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->token_id == 1u);
    CHECK(token->physical_keycode == KC_C);
    CHECK(token->resolved_keycode == KC_C);
    CHECK(token->observed_release_keycode == KC_NO);

    fake_time = (uint16_t)(fake_time + 10u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_V, key_pos, fake_time);

    token = runtime_v2_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(token->physical_keycode == KC_C);
    CHECK(token->resolved_keycode == KC_C);
    CHECK(token->observed_release_keycode == KC_V);
    CHECK(token->release_keycode_mismatched);
    CHECK(token->released_at == fake_time);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_press_token_count == 0u);
    CHECK(snapshot.v2_release_keycode_mismatch_count == 1u);
    CHECK(snapshot.v2_orphan_release_count == 0u);
}

static void test_runtime_v2_timer_and_scan_do_not_rewrite_press_identity(void) {
    const uint16_t          layer_tap_keycode = LT(2, KC_V);
    keypos_t                key_pos           = test_keypos(4, 1);
    const press_token_t    *token;
    press_token_t           original;
    runtime_event_t         advance = {
                .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                .data.timer_advance =
                    {
                        .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
                    },
            };
    runtime_event_t         scan = {
                .kind = RUNTIME_EVENT_KIND_SCAN,
            };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, layer_tap_keycode, key_pos, fake_time);
    token = runtime_v2_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->hold_term_ms == TAPPING_TERM);
    original = *token;

    runtime_v2_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    runtime_v2_apply_event(&scan, fake_time);

    token = runtime_v2_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->token_id == original.token_id);
    CHECK(test_keypos_equal(token->key_pos, original.key_pos));
    CHECK(token->physical_keycode == layer_tap_keycode);
    CHECK(token->resolved_keycode == layer_tap_keycode);
    CHECK(token->hold_term_ms == original.hold_term_ms);
    CHECK(token->phase == PRESS_TOKEN_PHASE_HELD);
}

static void test_runtime_v2_tap_series_state_stays_independent_from_active_token_storage(void) {
    keypos_t                key_pos = test_keypos(6, 2);
    const press_token_t    *token;
    const tap_series_t     *series;
    projection_snapshot_t   snapshot;
    runtime_event_t         advance = {
                .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                .data.timer_advance =
                    {
                        .advance_ms = (uint16_t)(CUSTOM_TAP_HOLD_TERM + 1u),
                    },
            };
    runtime_event_t         scan = {
                .kind = RUNTIME_EVENT_KIND_SCAN,
            };
    uint16_t                tap_series_expire_ms;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    fake_time = (uint16_t)(fake_time + 10u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, key_pos, fake_time);

    series = runtime_v2_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->keycode == KC_C);
    CHECK(series->tap_count == 1u);
    CHECK(!series->pending_hold);

    fake_time = (uint16_t)(fake_time + 20u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);
    token = runtime_v2_press_token_at(key_pos);
    series = runtime_v2_tap_series_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->pending_hold);

    runtime_v2_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    runtime_v2_apply_event(&scan, fake_time);

    token = runtime_v2_press_token_at(key_pos);
    CHECK(token != NULL);
    CHECK(token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_HELD);

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_C, key_pos, fake_time);

    token = runtime_v2_press_token_at(key_pos);
    series = runtime_v2_tap_series_at(key_pos);
    CHECK(token != NULL);
    CHECK(!token->active);
    CHECK(token->phase == PRESS_TOKEN_PHASE_RELEASED);
    CHECK(series != NULL);
    CHECK(series->active);
    CHECK(series->keycode == KC_C);
    CHECK(series->tap_count == 1u);
    CHECK(!series->pending_hold);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_press_token_count == 0u);
    CHECK(snapshot.v2_tap_series_count == 1u);

    tap_series_expire_ms = (uint16_t)(series->tap_term_ms + 1u);
    runtime_v2_apply_event(&(runtime_event_t){
                               .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                               .data.timer_advance =
                                   {
                                       .advance_ms = tap_series_expire_ms,
                                   },
                           },
                           fake_time);
    fake_time = (uint16_t)(fake_time + tap_series_expire_ms);
    runtime_v2_apply_event(&scan, fake_time);

    series = runtime_v2_tap_series_at(key_pos);
    CHECK(series != NULL);
    CHECK(!series->active);
    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_tap_series_count == 0u);
}

static void test_runtime_v2_layer_leases_and_layer_lock_recompute_from_owned_state(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              key_pos = test_keypos(1, 1);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    runtime_v2_layer_lock_set(3, true);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == ((layer_state_t)1u << 3));

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MO(2), key_pos, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == (((layer_state_t)1u << 2) | ((layer_state_t)1u << 3)));

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(snapshot.v2_shadow_layer_state == (((layer_state_t)1u << 2) | ((layer_state_t)1u << 3)));
    CHECK(snapshot.v2_lease_count == 1u);
    CHECK(snapshot.v2_persistent_intent_count == 1u);

    fake_time = (uint16_t)(fake_time + 5u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, MO(2), key_pos, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->locked_layer_mask == ((layer_state_t)1u << 3));
    CHECK(shadow->layer_state == ((layer_state_t)1u << 3));

    runtime_v2_layer_lock_set(3, false);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->locked_layer_mask == 0u);
    CHECK(shadow->layer_state == 0u);
}

static void test_runtime_v2_layer_tap_hold_creates_and_retires_layer_lease(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              key_pos = test_keypos(2, 4);
    runtime_event_t                       advance = {
                    .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                    .data.timer_advance =
                        {
                            .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
                        },
                };
    runtime_event_t                       scan = {
                    .kind = RUNTIME_EVENT_KIND_SCAN,
                };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, LT(2, KC_V), key_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->layer_state == 0u);

    runtime_v2_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    runtime_v2_apply_event(&scan, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->layer_state == ((layer_state_t)1u << 2));
    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_layer_state == ((layer_state_t)1u << 2));
    CHECK(snapshot.v2_lease_count == 1u);

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, LT(2, KC_V), key_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->layer_state == 0u);
    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_layer_state == 0u);
    CHECK(snapshot.v2_lease_count == 0u);
}

static void test_runtime_v2_modifier_leases_keep_physical_and_managed_masks_separate(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              physical_pos = test_keypos(3, 3);
    keypos_t                              mod_tap_pos  = test_keypos(3, 4);
    runtime_event_t                       advance = {
                    .kind = RUNTIME_EVENT_KIND_TIMER_ADVANCE,
                    .data.timer_advance =
                        {
                            .advance_ms = (uint16_t)(TAPPING_TERM + 1u),
                        },
                };
    runtime_event_t                       scan = {
                    .kind = RUNTIME_EVENT_KIND_SCAN,
                };

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_LEFT_SHIFT, physical_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->keyboard_mod_state.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == 0u);

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MT(MOD_LALT, KC_C), mod_tap_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == 0u);

    runtime_v2_apply_event(&advance, fake_time);
    fake_time = (uint16_t)(fake_time + advance.data.timer_advance.advance_ms);
    runtime_v2_apply_event(&scan, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == (uint8_t)(MOD_BIT(KC_LEFT_SHIFT) | MOD_LALT));
    CHECK(shadow->keyboard_physical_mod_mask == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(shadow->keyboard_managed_mod_mask == MOD_LALT);

    fake_time = (uint16_t)(fake_time + 5u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, KC_LEFT_SHIFT, physical_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == MOD_LALT);
    CHECK(shadow->keyboard_physical_mod_mask == 0u);
    CHECK(shadow->keyboard_managed_mod_mask == MOD_LALT);

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, MT(MOD_LALT, KC_C), mod_tap_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->keyboard_mod_state.real == 0u);
    CHECK(shadow->keyboard_physical_mod_mask == 0u);
    CHECK(shadow->keyboard_managed_mod_mask == 0u);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_keyboard_mod_state.real == 0u);
    CHECK(snapshot.v2_shadow_keyboard_physical_mod_mask == 0u);
    CHECK(snapshot.v2_shadow_keyboard_managed_mod_mask == 0u);
}

static void test_runtime_v2_replacing_a_live_token_cleans_up_owned_leases(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              key_pos = test_keypos(5, 5);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, MO(2), key_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->layer_state == ((layer_state_t)1u << 2));

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, KC_C, key_pos, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->layer_state == 0u);
    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_lease_count == 0u);
    CHECK(snapshot.v2_cancelled_press_count == 1u);
    CHECK(snapshot.v2_press_token_count == 1u);
}

static void test_runtime_v2_pd_mode_press_creates_active_mode_and_pointer_anchor(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              key_pos = test_keypos(1, 6);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, VOLUME_MODE, key_pos, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(shadow->pd_mode_local_locked == 0);
    CHECK(shadow->pointer_anchor_active);
    CHECK(shadow->pointer_pd_mode_anchor_active);
    CHECK(!shadow->pointer_prefers_typing_layer);
    CHECK(!shadow->pointer_toggle_enabled);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(snapshot.v2_shadow_pointer_anchor_active);
    CHECK(snapshot.v2_shadow_pointer_pd_mode_anchor_active);
    CHECK(snapshot.v2_lease_count == 2u);

    fake_time = (uint16_t)(fake_time + 10u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, VOLUME_MODE, key_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->pd_mode_local_active == 0);
    CHECK(!shadow->pointer_anchor_active);
}

static void test_runtime_v2_arrow_mode_prefers_typing_without_pointer_anchor(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              key_pos = test_keypos(1, 7);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, ARROW_MODE, key_pos, fake_time);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(!shadow->pointer_pd_mode_anchor_active);
    CHECK(shadow->pointer_prefers_typing_layer);
    CHECK(!shadow->pointer_toggle_enabled);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_pd_mode_local_active == PD_MODE_ARROW);
    CHECK(snapshot.v2_shadow_pointer_prefers_typing_layer);
    CHECK(!snapshot.v2_shadow_pointer_anchor_active);
    CHECK(snapshot.v2_lease_count == 1u);
}

static void test_runtime_v2_pd_mode_lock_owns_pointer_toggle_intent(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;

    test_reset_stubs();
    noah_runtime_reset_for_test();

    runtime_v2_pd_mode_lock_set(PD_MODE_DRAGSCROLL, true);

    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_DRAGSCROLL);
    CHECK(shadow->pd_mode_local_locked == PD_MODE_DRAGSCROLL);
    CHECK(shadow->pointer_anchor_active);
    CHECK(shadow->pointer_pd_mode_anchor_active);
    CHECK(shadow->pointer_toggle_enabled);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_pd_mode_local_active == PD_MODE_DRAGSCROLL);
    CHECK(snapshot.v2_shadow_pd_mode_local_locked == PD_MODE_DRAGSCROLL);
    CHECK(snapshot.v2_shadow_pointer_toggle_enabled);
    CHECK(snapshot.v2_persistent_intent_count == 2u);

    runtime_v2_pd_mode_lock_set(PD_MODE_DRAGSCROLL, false);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->pd_mode_local_active == 0);
    CHECK(shadow->pd_mode_local_locked == 0);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(!shadow->pointer_toggle_enabled);
}

static void test_runtime_v2_activating_new_pd_mode_clears_foreign_mode_leases(void) {
    const runtime_v2_shadow_projection_t *shadow;
    projection_snapshot_t                 snapshot;
    keypos_t                              volume_pos = test_keypos(2, 6);
    keypos_t                              arrow_pos  = test_keypos(2, 7);

    test_reset_stubs();
    noah_runtime_reset_for_test();

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, VOLUME_MODE, volume_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow != NULL);
    CHECK(shadow->pd_mode_local_active == PD_MODE_VOLUME);
    CHECK(shadow->pointer_anchor_active);

    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_DOWN, ARROW_MODE, arrow_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(!shadow->pointer_anchor_active);
    CHECK(shadow->pointer_prefers_typing_layer);

    snapshot = runtime_v2_projection_snapshot_capture();
    CHECK(snapshot.v2_shadow_pd_mode_local_active == PD_MODE_ARROW);
    CHECK(snapshot.v2_lease_count == 1u);

    fake_time = (uint16_t)(fake_time + 5u);
    test_runtime_v2_apply_key_event(RUNTIME_EVENT_KIND_KEY_UP, VOLUME_MODE, volume_pos, fake_time);
    shadow = runtime_v2_shadow_projection();
    CHECK(shadow->pd_mode_local_active == PD_MODE_ARROW);
    CHECK(shadow->pointer_prefers_typing_layer);
}

int main(void) {
    test_debug_reports_slot_phase_and_momentary_layer_interrupt_state();
    test_snapshot_captures_cross_subsystem_runtime_state();
    test_reset_clears_all_runtime_surfaces();
    test_runtime_v2_release_tracks_press_by_position_despite_keycode_mismatch();
    test_runtime_v2_timer_and_scan_do_not_rewrite_press_identity();
    test_runtime_v2_tap_series_state_stays_independent_from_active_token_storage();
    test_runtime_v2_layer_leases_and_layer_lock_recompute_from_owned_state();
    test_runtime_v2_layer_tap_hold_creates_and_retires_layer_lease();
    test_runtime_v2_modifier_leases_keep_physical_and_managed_masks_separate();
    test_runtime_v2_replacing_a_live_token_cleans_up_owned_leases();
    test_runtime_v2_pd_mode_press_creates_active_mode_and_pointer_anchor();
    test_runtime_v2_arrow_mode_prefers_typing_without_pointer_anchor();
    test_runtime_v2_pd_mode_lock_owns_pointer_toggle_intent();
    test_runtime_v2_activating_new_pd_mode_clears_foreign_mode_leases();

    puts("runtime_debug host tests passed");
    return 0;
}

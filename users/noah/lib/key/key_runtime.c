// ────────────────────────────────────────────────────────────────────────────
// Key Runtime
// ────────────────────────────────────────────────────────────────────────────
//
// Custom key processing, tap/hold resolution, and matrix-scan hold logic.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "noah.h"
#include "action_router.h"
#include "key_behavior.h"
#include "multi_tap_engine.h"
#include "../pointing/pd_mode_key_runtime.h"
#include "../pointing/pointing_device_modes.h"

// ─── Tracked Press State ─────────────────────────────────────────────────────
//
// The key runtime tracks the currently active key-behavior key. Pointing-
// device mode keys use their own runtime module so the typing FSM does not
// also need to own that press state.
//
// Invariant: held_action_keycode != KC_NO implies this active key registered a
// hold action that must be released before the state is discarded.

typedef struct {
    uint16_t        timer;
    uint16_t        keycode;
    keypos_t        key_pos;
    bool            hold_fired;
    uint16_t        held_action_keycode;
    uint16_t        tap_action;
    uint16_t        tap_hold_term;
    uint16_t        longer_hold_term;
    uint16_t        multi_tap_term;
    bool            hold_one_shot_fired;
    bool            layer_interrupted;
    hold_behavior_t hold;
    hold_behavior_t long_hold;
} active_key_state_t;

typedef struct {
    uint8_t real;
    uint8_t weak;
    uint8_t oneshot;
    uint8_t oneshot_locked;
} delayed_action_mods_t;

typedef struct {
    uint8_t real;
    uint8_t weak;
    uint8_t oneshot;
    uint8_t oneshot_locked;
} key_runtime_saved_mods_t;

// A held pure modifier is owned by the physical switch that started it, not by
// whichever custom key the tap/hold FSM is currently resolving.
typedef struct {
    bool     active;
    keypos_t key_pos;
    uint16_t action;
} held_modifier_binding_t;

#define ACTIVE_KEY_STATE_INIT                           \
    {                                                   \
        .keycode             = KC_NO,                   \
        .held_action_keycode = KC_NO,                   \
        .tap_hold_term       = CUSTOM_TAP_HOLD_TERM,    \
        .longer_hold_term    = CUSTOM_LONGER_HOLD_TERM, \
        .multi_tap_term      = CUSTOM_MULTI_TAP_TERM,   \
    }

static active_key_state_t active_key = ACTIVE_KEY_STATE_INIT;
// Pure modifiers must stay held even after the tap/hold FSM moves on to another custom key.
static held_modifier_binding_t held_modifiers[8] = {0};
static uint8_t                 held_modifier_refcounts[8] = {0};

static inline uint8_t behavior_get_layer(uint16_t keycode) {
    return IS_QK_LAYER_TAP(keycode) ? QK_LAYER_TAP_GET_LAYER(keycode) : QK_MOMENTARY_GET_LAYER(keycode);
}

static inline bool is_layer_key(uint16_t keycode) {
    return IS_QK_MOMENTARY(keycode) || IS_QK_LAYER_TAP(keycode);
}

static inline bool keypos_equal(keypos_t lhs, keypos_t rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

static inline bool active_key_matches(uint16_t keycode, keypos_t key_pos) {
    return active_key.keycode == keycode && keypos_equal(active_key.key_pos, key_pos);
}

static bool action_is_pure_modifier_keycode(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
        case KC_LEFT_SHIFT:
        case KC_LEFT_ALT:
        case KC_LEFT_GUI:
        case KC_RIGHT_CTRL:
        case KC_RIGHT_SHIFT:
        case KC_RIGHT_ALT:
        case KC_RIGHT_GUI:
            return true;
        default:
            return false;
    }
}

static int8_t held_modifier_index_for_action(uint16_t action) {
    switch (action) {
        case KC_LEFT_CTRL:
            return 0;
        case KC_LEFT_SHIFT:
            return 1;
        case KC_LEFT_ALT:
            return 2;
        case KC_LEFT_GUI:
            return 3;
        case KC_RIGHT_CTRL:
            return 4;
        case KC_RIGHT_SHIFT:
            return 5;
        case KC_RIGHT_ALT:
            return 6;
        case KC_RIGHT_GUI:
            return 7;
        default:
            return -1;
    }
}

static int8_t held_modifier_find_slot_for_key(keypos_t key_pos) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_modifiers); i++)
        if (held_modifiers[i].active && keypos_equal(held_modifiers[i].key_pos, key_pos)) return (int8_t)i;

    return -1;
}

static int8_t held_modifier_find_free_slot(void) {
    for (uint8_t i = 0; i < ARRAY_SIZE(held_modifiers); i++)
        if (!held_modifiers[i].active) return (int8_t)i;

    return -1;
}

static void held_modifier_remove_slot(uint8_t slot) {
    uint16_t action = held_modifiers[slot].action;
    int8_t   index  = held_modifier_index_for_action(action);

    held_modifiers[slot].active = false;
    held_modifiers[slot].action = KC_NO;

    if (index < 0 || held_modifier_refcounts[index] == 0) {
        return;
    }

    held_modifier_refcounts[index]--;
    if (held_modifier_refcounts[index] == 0) {
        del_mods(MOD_BIT(action));
        send_keyboard_report();
    }
}

static void held_modifier_register(keypos_t key_pos, uint16_t action) {
    int8_t slot  = held_modifier_find_slot_for_key(key_pos);
    int8_t index = held_modifier_index_for_action(action);

    if (index < 0) {
        return;
    }

    if (slot >= 0) {
        if (held_modifiers[slot].action == action) {
            return;
        }
        held_modifier_remove_slot((uint8_t)slot);
    } else {
        slot = held_modifier_find_free_slot();
        if (slot < 0) {
            return;
        }
    }

    held_modifiers[slot] = (held_modifier_binding_t){
        .active  = true,
        .key_pos = key_pos,
        .action  = action,
    };

    if (held_modifier_refcounts[index]++ == 0) {
        add_mods(MOD_BIT(action));
        send_keyboard_report();
    }
}

static bool held_modifier_unregister(keypos_t key_pos) {
    int8_t slot = held_modifier_find_slot_for_key(key_pos);

    if (slot < 0) {
        return false;
    }

    held_modifier_remove_slot((uint8_t)slot);
    return true;
}

// ─── Key Behavior State ──────────────────────────────────────────────────────
static multi_tap_t multi_tap = {0};

static inline void active_key_reset(void) {
    active_key = (active_key_state_t)ACTIVE_KEY_STATE_INIT;
}

static inline delayed_action_mods_t delayed_action_mods_from_multi_tap(const multi_tap_t *mt) {
    return (delayed_action_mods_t){
        .real           = mt->saved_mods,
        .weak           = mt->saved_weak_mods,
        .oneshot        = mt->saved_oneshot_mods,
        .oneshot_locked = mt->saved_oneshot_locked_mods,
    };
}

static key_runtime_saved_mods_t key_runtime_suspend_mods(void) {
    key_runtime_saved_mods_t saved = {
        .real           = get_mods(),
        .weak           = get_weak_mods(),
        .oneshot        = get_oneshot_mods(),
        .oneshot_locked = get_oneshot_locked_mods(),
    };

    clear_mods();
    clear_weak_mods();
    clear_oneshot_mods();
    clear_oneshot_locked_mods();
    send_keyboard_report();

    return saved;
}

static void key_runtime_restore_mods(key_runtime_saved_mods_t saved) {
    set_mods(saved.real);
    set_weak_mods(saved.weak);
    set_oneshot_mods(saved.oneshot);
    set_oneshot_locked_mods(saved.oneshot_locked);
    send_keyboard_report();
}

static void dispatch_delayed_action(uint16_t action, delayed_action_mods_t mods) {
    key_runtime_saved_mods_t saved = key_runtime_suspend_mods();

    set_mods(mods.real);
    set_weak_mods(mods.weak);
    set_oneshot_mods(mods.oneshot);
    set_oneshot_locked_mods(mods.oneshot_locked);
    send_keyboard_report();

    action_router_dispatch(action);

    key_runtime_restore_mods(saved);
}

static void dispatch_multi_tap_action(uint16_t action, const multi_tap_t *mt) {
    dispatch_delayed_action(action, delayed_action_mods_from_multi_tap(mt));
}

static inline void register_held_action(keypos_t key_pos, uint16_t action) {
    if (action_is_pure_modifier_keycode(action)) {
        held_modifier_register(key_pos, action);
        return;
    }

    register_code16(action);
}

static inline void unregister_held_action(keypos_t key_pos, uint16_t action) {
    if (action_is_pure_modifier_keycode(action)) {
        held_modifier_unregister(key_pos);
        return;
    }

    unregister_code16(action);
}

static inline bool held_action_survives_flush(uint16_t action) {
    return action_is_pure_modifier_keycode(action);
}

static inline void active_key_track(uint16_t keycode, keypos_t key_pos, uint16_t tap_action, hold_behavior_t hold, hold_behavior_t long_hold, uint16_t tap_hold_term, uint16_t longer_hold_term, uint16_t multi_tap_term, bool hold_fired) {
    active_key = (active_key_state_t){
        .timer               = timer_read(),
        .keycode             = keycode,
        .key_pos             = key_pos,
        .hold_fired          = hold_fired,
        .held_action_keycode = KC_NO,
        .tap_action          = tap_action,
        .tap_hold_term       = tap_hold_term,
        .longer_hold_term    = longer_hold_term,
        .multi_tap_term      = multi_tap_term,
        .hold                = hold,
        .long_hold           = long_hold,
    };
}

static inline void deactivate_momentary_layer_if_unlocked(uint16_t keycode) {
    uint8_t layer = behavior_get_layer(keycode);
    if (!action_router_layer_is_locked(layer)) {
        layer_off(layer);
    }
}

typedef struct {
    key_behavior_view_t behavior;
    uint8_t             pd_mode;
} handled_key_view_t;

static inline handled_key_view_t handled_key_lookup(uint16_t keycode) {
    return (handled_key_view_t){
        .behavior = key_behavior_lookup(keycode),
        .pd_mode  = pd_mode_for_keycode(keycode),
    };
}

static inline uint16_t handled_key_tap_action(handled_key_view_t key, keyrecord_t *record) {
    if (key.behavior.single.tap.present) return key.behavior.single.tap.action;
    if (key.pd_mode) return keymap_key_to_keycode(LAYER_BASE, record->event.key);
    if (key.behavior.is_layer_tap) return QK_LAYER_TAP_GET_TAP_KEYCODE(key.behavior.keycode);
    if (key.behavior.is_momentary_layer) return KC_NO;
    if (key.behavior.keycode >= SAFE_RANGE) return keymap_key_to_keycode(LAYER_BASE, record->event.key);
    return key.behavior.keycode;
}

static inline bool handled_key_multi_tap_repress(handled_key_view_t key, uint16_t keycode) {
    return multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && key.behavior.has_multi_tap;
}

static inline uint16_t handled_key_advance_multi_tap(uint16_t keycode) {
    return multi_tap_advance(&multi_tap, keycode, key_behavior_step_lookup, key_behavior_has_more_taps);
}

static inline void handled_key_dispatch_tap_or_begin_multi_tap(uint16_t keycode, handled_key_view_t key, keyrecord_t *record) {
    uint16_t tap_action = handled_key_tap_action(key, record);
    if (key.behavior.has_multi_tap) {
        multi_tap_begin(&multi_tap, keycode, tap_action, key.behavior.tap_hold_term, key.behavior.multi_tap_term);
    } else if (tap_action != KC_NO) {
        action_router_dispatch(tap_action);
    }
}

static uint16_t pd_mode_key_runtime_advance_multi_tap(void *context, uint16_t keycode) {
    (void)context;
    return handled_key_advance_multi_tap(keycode);
}

static void pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap(void *context, uint16_t keycode, keyrecord_t *record) {
    handled_key_view_t *key = context;
    handled_key_dispatch_tap_or_begin_multi_tap(keycode, *key, record);
}

static void pd_mode_key_runtime_dispatch_action(void *context, uint16_t action) {
    (void)context;
    action_router_dispatch(action);
}

static bool pd_mode_key_runtime_capture_pending_multi_tap_hold(void *context, uint16_t *tap_action, uint16_t *hold_action) {
    (void)context;

    if (!multi_tap_pending_hold(&multi_tap)) {
        return false;
    }

    *tap_action  = multi_tap.tap_action;
    *hold_action = multi_tap.hold.action;
    multi_tap_reset(&multi_tap);
    return true;
}

static uint16_t select_release_hold_action(uint16_t elapsed, uint16_t hold_action, hold_behavior_t long_hold, uint16_t longer_hold_term) {
    if (hold_sends_on_release(long_hold) && elapsed >= longer_hold_term) {
        return long_hold.action;
    }
    return hold_action;
}

static void fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold) {
    if (action_router_is_layer_lock_action(hold.action) || !hold_registers_while_held(hold)) {
        if (active_key.held_action_keycode != KC_NO) {
            unregister_held_action(active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
        action_router_dispatch(hold.action);
        if (long_hold.present) {
            active_key.hold_fired          = false;
            active_key.hold_one_shot_fired = true;
        } else {
            active_key.hold_fired          = true;
            active_key.hold_one_shot_fired = false;
        }
        return;
    }

    register_held_action(active_key.key_pos, hold.action);
    active_key.held_action_keycode = hold.action;
    active_key.hold_fired          = !long_hold.present;
    active_key.hold_one_shot_fired = false;
}

static void promote_to_long_hold(hold_behavior_t long_hold) {
    if (active_key.held_action_keycode != KC_NO) {
        unregister_held_action(active_key.key_pos, active_key.held_action_keycode);
        active_key.held_action_keycode = KC_NO;
    }

    if (action_router_is_layer_lock_action(long_hold.action) || !hold_registers_while_held(long_hold)) {
        action_router_dispatch(long_hold.action);
        active_key.hold_fired = true;
        return;
    }

    register_held_action(active_key.key_pos, long_hold.action);
    active_key.held_action_keycode = long_hold.action;
    active_key.hold_fired          = true;
}

static inline void deactivate_pending_multi_tap_layer_before_lock(uint16_t action) {
    if (is_layer_key(active_key.keycode) && action_router_is_layer_lock_action(action)) {
        layer_off(behavior_get_layer(active_key.keycode));
    }
}

static void flush_active_key(void) {
    if (active_key.keycode == KC_NO) return;

    if (active_key.hold_fired || active_key.held_action_keycode != KC_NO) {
        active_key.hold_fired = false;
        if (active_key.held_action_keycode != KC_NO && !held_action_survives_flush(active_key.held_action_keycode)) {
            unregister_held_action(active_key.key_pos, active_key.held_action_keycode);
            active_key.held_action_keycode = KC_NO;
        }
    } else if (!is_layer_key(active_key.keycode) && active_key.tap_action != KC_NO) {
        action_router_dispatch(active_key.tap_action);
    }

    active_key_reset();
}

static bool process_pd_mode_key(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    const pd_mode_key_runtime_hooks_t hooks = {
        .context                         = &key,
        .advance_multi_tap               = pd_mode_key_runtime_advance_multi_tap,
        .dispatch_tap_or_begin_multi_tap = pd_mode_key_runtime_dispatch_tap_or_begin_multi_tap,
        .dispatch_action                 = pd_mode_key_runtime_dispatch_action,
        .capture_pending_multi_tap_hold  = pd_mode_key_runtime_capture_pending_multi_tap_hold,
    };

    return pd_mode_key_runtime_process(keycode, record, key.pd_mode, key.behavior.tap_hold_term, handled_key_multi_tap_repress(key, keycode), &hooks);
}

static bool process_key_behavior_press(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    key_behavior_view_t behavior = key.behavior;

    if (handled_key_multi_tap_repress(key, keycode)) {
        uint16_t action = handled_key_advance_multi_tap(keycode);
        if (action != KC_NO) {
            action_router_dispatch(action);
        }

        if (behavior.is_momentary_layer) {
            layer_on(behavior_get_layer(keycode));
        }

        bool pending_hold = multi_tap_pending_hold(&multi_tap);
        if (pending_hold || behavior.is_momentary_layer) {
            active_key_track(keycode, record->event.key, KC_NO, hold_behavior_none(), hold_behavior_none(), behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, !pending_hold);
        }
        return true;
    }

    if (behavior.is_momentary_layer) {
        layer_on(behavior_get_layer(keycode));
    }

    flush_active_key();
    active_key_track(keycode, record->event.key, handled_key_tap_action(key, record), behavior.single.hold, behavior.single.long_hold, behavior.tap_hold_term, behavior.longer_hold_term, behavior.multi_tap_term, false);
    return true;
}

static bool process_key_behavior_release_pending_multi_tap_hold(uint16_t keycode, key_behavior_view_t behavior) {
    if (!(multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode)) {
        return false;
    }

    delayed_action_mods_t cached_mods      = delayed_action_mods_from_multi_tap(&multi_tap);
    bool                  was_release_hold = hold_sends_on_release(multi_tap.hold);
    hold_behavior_t       cached_hold      = multi_tap.hold;
    hold_behavior_t       cached_long_hold = multi_tap.long_hold;
    uint8_t               repeat_count     = 0;
    uint16_t              action           = multi_tap_resolve_hold(&multi_tap, keycode, key_behavior_has_more_taps, &repeat_count);

    if (was_release_hold && cached_hold.present && repeat_count == 1 && action == cached_hold.action) {
        action = select_release_hold_action(timer_elapsed(active_key.timer), cached_hold.action, cached_long_hold, active_key.longer_hold_term);
    }

    for (uint8_t i = 0; i < repeat_count; i++) {
        if (action != KC_NO) {
            dispatch_delayed_action(action, cached_mods);
        }
    }

    if (behavior.is_momentary_layer) {
        deactivate_momentary_layer_if_unlocked(keycode);
    }
    active_key_reset();
    return true;
}

static bool process_key_behavior_release_active_key(uint16_t keycode, keyrecord_t *record, key_behavior_view_t behavior) {
    if (behavior.is_momentary_layer) {
        deactivate_momentary_layer_if_unlocked(keycode);
    }

    if (!active_key_matches(keycode, record->event.key)) {
        // active_key may already belong to a newer custom press; release any
        // pure modifier still owned by this physical key.
        held_modifier_unregister(record->event.key);
        return true;
    }

    active_key_state_t released_key = active_key;
    active_key_reset();

    if (released_key.hold_fired || released_key.held_action_keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(released_key.timer);

        if (released_key.held_action_keycode != KC_NO) {
            unregister_held_action(released_key.key_pos, released_key.held_action_keycode);
        }

        if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
            action_router_dispatch(released_key.long_hold.action);
        }
        return true;
    }

    uint16_t elapsed = timer_elapsed(released_key.timer);

    if (elapsed < released_key.tap_hold_term && !(behavior.is_momentary_layer && released_key.layer_interrupted)) {
        if (behavior.has_multi_tap) {
            multi_tap_begin(&multi_tap, keycode, released_key.tap_action, released_key.tap_hold_term, released_key.multi_tap_term);
        } else if (released_key.tap_action != KC_NO) {
            action_router_dispatch(released_key.tap_action);
        }
        return true;
    }

    if (hold_sends_on_release(released_key.hold)) {
        action_router_dispatch(select_release_hold_action(elapsed, released_key.hold.action, released_key.long_hold, released_key.longer_hold_term));
    } else if (hold_sends_on_release(released_key.long_hold) && elapsed >= released_key.longer_hold_term) {
        action_router_dispatch(released_key.long_hold.action);
    } else if (!released_key.hold_one_shot_fired && !behavior.is_momentary_layer && released_key.tap_action != KC_NO) {
        action_router_dispatch(released_key.tap_action);
    }

    return true;
}

static bool process_key_behavior_release(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    if (process_key_behavior_release_pending_multi_tap_hold(keycode, key.behavior)) {
        return true;
    }

    return process_key_behavior_release_active_key(keycode, record, key.behavior);
}

static bool process_key_behavior(uint16_t keycode, keyrecord_t *record, handled_key_view_t key) {
    if (!key.behavior.handled) {
        return false;
    }

    if (record->event.pressed) {
        return process_key_behavior_press(keycode, record, key);
    }

    return process_key_behavior_release(keycode, record, key);
}

// ─── Tap/Hold Behavior ─────────────────────────────────────────────────────

bool noah_get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    (void)record;

    switch (keycode) {
        case MT(MOD_LSFT, KC_CAPS):
            return true;
        default:
            return false;
    }
}

// ─── Process Record ────────────────────────────────────────────────────────

bool noah_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed && active_key.keycode != KC_NO && !active_key_matches(keycode, record->event.key) && is_layer_key(active_key.keycode)) {
        active_key.layer_interrupted = true;
    }

    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_multi_tap_action);
    }

    if (pd_mode_handle_key_event(keycode, record)) {
        return false;
    }

    handled_key_view_t key = handled_key_lookup(keycode);

    if (process_pd_mode_key(keycode, record, key)) {
        return false;
    }

    if (process_key_behavior(keycode, record, key)) {
        return false;
    }

    if (!record->event.pressed) {
        return true;
    }

    if (macro_dispatch(keycode)) return false;
    return true;
}

void noah_matrix_scan_user(void) {
    pd_mode_key_runtime_scan();

    if (active_key.keycode != KC_NO && !active_key.hold_fired) {
        uint16_t elapsed = timer_elapsed(active_key.timer);
        if (hold_fires_at_threshold(active_key.long_hold) && elapsed >= active_key.longer_hold_term) {
            promote_to_long_hold(active_key.long_hold);
        } else if (hold_fires_at_threshold(active_key.hold) && elapsed >= active_key.tap_hold_term) {
            fire_hold_at_threshold(active_key.hold, active_key.long_hold);
        }
    }

    if (multi_tap_pending_hold(&multi_tap) && active_key.keycode != KC_NO) {
        uint16_t elapsed = timer_elapsed(multi_tap.timer);

        if (hold_fires_at_threshold(multi_tap.long_hold) && elapsed >= active_key.longer_hold_term) {
            deactivate_pending_multi_tap_layer_before_lock(multi_tap.long_hold.action);
            active_key.long_hold = multi_tap.long_hold;
            promote_to_long_hold(active_key.long_hold);
            multi_tap_reset(&multi_tap);
        } else if (multi_tap_hold_elapsed(&multi_tap)) {
            deactivate_pending_multi_tap_layer_before_lock(multi_tap.hold.action);
            active_key.long_hold = multi_tap.long_hold;
            fire_hold_at_threshold(multi_tap.hold, active_key.long_hold);
            multi_tap_reset(&multi_tap);
        }
    }

    if (multi_tap_expired(&multi_tap)) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_multi_tap_action);
    }
}

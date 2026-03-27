// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap
// ────────────────────────────────────────────────────────────────────────────
//
// This is a QMK firmware keymap for the Bastard Keyboards Charybdis 4x6,
// a split ergonomic keyboard with a built-in trackball on the right half.
//
// Architecture overview:
//
//   keymap.c (this file)
//     Processing logic: key event handlers, pointing device integration,
//     and RGB layer indicators.  Ties together the helper headers below.
//
//   key_config.h
//     All key behavior configuration: enums (layers, custom keycodes),
//     unified key_behaviors[] table, combos, macro definitions, and
//     LAYOUT arrays.  Edit that file to change what keys do.
//
//   lib/key_behavior.h
//     Unified key behavior schema plus lookup helpers for the runtime view.
//     Keeps key_config.h focused on data.
//
//   pointing_device_modes.h
//     Bitfield-based mode system that changes what the trackball does.
//     Mode flags, structs, state helpers, and the pd_modes[] config table.
//
//   pointing_device_mode_handlers.h
//     Handler implementations for each trackball mode (volume, brightness,
//     zoom, arrow).  Included by pointing_device_modes.h.
//
//   split_sync.h
//     Syncs pointing-device state (mode flags + auto-mouse elapsed time)
//     from the master half to the slave half over QMK's split RPC
//     transport.  Both values travel in a single 3-byte packet.
//
//   rgb_helpers.h
//     Split-safe helper functions for rgb_matrix_set_color() and struct
//     typedefs for RGB config tables.  Handles LED chunk boundaries so
//     callers can use global LED indices (0–57).
//
//   rgb_config.h
//     All RGB color definitions: layer indicators, mode overlays, LED
//     group highlights, and auto-mouse gradient endpoints.
//
//   rgb_automouse.h
//     Renders the auto-mouse countdown gradient (white → red) based on
//     how much of the timeout has elapsed.
//
// Key concepts for newcomers:
//
//   - "Auto-mouse layer":  QMK can automatically activate a layer when
//     it detects trackball movement, and deactivate it after a timeout.
//     We use this for LAYER_POINTER.
//
//   - "Pointing device modes":  We define custom modes that change the
//     behavior of the pointing device (e.g. volume control, scroll) and
//     tie them to keys in the keymap.  Modes are activated by holding
//     the key and deactivated on release.
//
//   - "Key behaviors":  key_config.h authors one behavior row per handled
//     key in key_behaviors[].  Processing logic here reads that unified
//     config and applies tap, hold, long-hold, and multi-tap rules.
//     Combos are also defined in key_config.h but handled by QMK before
//     process_record_user runs.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

// Lock a layer on from any tap or hold tier in key_behaviors[].
// Example:
//   { .keycode = MO(LAYER_LOWER), .tap_counts = { [1] = { .tap = TAP_SENDS(KC_MPLY), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(LOCK_LAYER(LAYER_NUM)) } } }
#define LOCK_LAYER(layer) (LAYER_LOCK_BASE + (layer))

#include "key_config.h"

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update config.h");
#endif

#include "lib/key_behavior.h"
#include "lib/multi_tap.h"
#include "lib/pointing_device_modes.h"
#include "lib/split_sync.h"
#include "lib/rgb_helpers.h"
#include "rgb_config.h"
#include "lib/rgb_automouse.h"

#define PD_MODE_COLOR_COUNT (sizeof(pd_mode_colors) / sizeof(pd_mode_colors[0]))
#define LAYER_LED_GROUP_COUNT (sizeof(layer_led_groups) / sizeof(layer_led_groups[0]))
#define PD_MODE_LED_GROUP_COUNT (sizeof(pd_mode_led_groups) / sizeof(pd_mode_led_groups[0]))

// Force master/slave role at compile time.  Needed when both halves have
// their own USB connection (e.g. for full LED brightness on each side
// when using a long cable) so they don't both detect USB and fight over
// who is master.  This overrides role detection only — handedness (left/right)
// is still determined by MASTER_RIGHT in the keyboard-level config in the qmk repo.
// Build with: -e FORCE_MASTER=yes or -e FORCE_SLAVE=yes
#if defined(FORCE_MASTER)
bool is_keyboard_master_impl(void) {
    return true;
}
#elif defined(FORCE_SLAVE)
#    include "usb_util.h" // QMK
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif

// ─── Pointing Device Mode Timer ──────────────────────────────────────────────
// Separate from key_timer so PD mode keys and key behavior keys don't
// stomp each other's timers if pressed in quick succession.
static uint16_t pd_mode_timer;
static uint16_t pd_mode_timer_keycode = KC_NO;

static inline uint16_t key_behavior_single_tap_action(key_behavior_view_t behavior) {
    if (behavior.single.tap.present) return behavior.single.tap.action;
    return behavior.is_momentary_layer ? KC_NO : behavior.keycode;
}

static inline uint16_t pd_mode_single_tap_action(keyrecord_t *record, key_behavior_view_t behavior) {
    if (behavior.single.tap.present) return behavior.single.tap.action;
    return keymap_key_to_keycode(LAYER_BASE, record->event.key);
}

// ─── Key Behavior State ─────────────────────────────────────────────────────
//
// Thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM      — tap vs hold boundary
//   CUSTOM_LONGER_HOLD_TERM   — hold vs long-hold boundary
//   CUSTOM_MULTI_TAP_TERM     — max gap between consecutive taps
//   COMBO_TERM                — max gap between keys for combos

// Active key tracking — only one key behavior key tracked at a time.
static uint16_t key_timer;
static uint16_t key_active     = KC_NO;
static bool     key_hold_fired = false;
static uint16_t active_tap_action = KC_NO;

// Cached lookups for the currently active key (populated on press, cleared on release).
static hold_behavior_t active_hold;
static hold_behavior_t active_long_hold;

// Multi-tap state — see lib/multi_tap.h for the state machine.
static multi_tap_t multi_tap = {0};

// Layer lock — which layer (if any) is locked on via multi-tap hold.
// 0 = no layer locked.
static uint8_t locked_layer = 0;

// Hold-after-multi-tap: the keycode currently registered (held down) via
// the hold path.  KC_NO = nothing held.  Unregistered on key release.
static uint16_t held_action_keycode = KC_NO;

static inline bool is_layer_lock_action(uint16_t action) {
    return action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT;
}

// Dispatch an action keycode.  Handles LOCK_LAYER(n) dynamically;
// everything else falls through to tap_code16.
static void dispatch_action(uint16_t action) {
    if (is_layer_lock_action(action)) {
        uint8_t layer = action - LAYER_LOCK_BASE;
        if (locked_layer == layer) {
            layer_off(layer);
            locked_layer = 0;
        } else {
            if (locked_layer) layer_off(locked_layer);
            layer_on(layer);
            locked_layer = layer;
        }
        return;
    }
    tap_code16(action);
}

// Release-based tier selection shared by single-press holds and release-based
// multi-tap holds.  If a long-hold exists and the key stayed down long
// enough, prefer it; otherwise keep the base hold action.
static uint16_t select_release_hold_action(uint16_t elapsed, uint16_t hold_action,
                                           hold_behavior_t long_hold) {
    if (hold_sends_on_release(long_hold) && elapsed >= CUSTOM_LONGER_HOLD_TERM) {
        return long_hold.action;
    }
    return hold_action;
}

// Fire a threshold-based hold tier. PRESS_AND_HOLD_UNTIL_RELEASE registers a
// real key until release; TAP_AT_HOLD_THRESHOLD sends once immediately. Only
// threshold-fired long_hold tiers can promote later.
static void fire_hold_at_threshold(hold_behavior_t hold, hold_behavior_t long_hold) {
    if (is_layer_lock_action(hold.action) || !hold_registers_while_held(hold)) {
        if (held_action_keycode != KC_NO) {
            unregister_code16(held_action_keycode);
            held_action_keycode = KC_NO;
        }
        dispatch_action(hold.action);
        key_hold_fired = true;
        return;
    }

    register_code16(hold.action);
    held_action_keycode = hold.action;
    key_hold_fired      = !hold_fires_at_threshold(long_hold);
}

// Promote the currently active hold into the long-hold tier.
static void promote_to_long_hold(hold_behavior_t long_hold) {
    if (held_action_keycode != KC_NO) {
        unregister_code16(held_action_keycode);
        held_action_keycode = KC_NO;
    }

    if (is_layer_lock_action(long_hold.action) || !hold_registers_while_held(long_hold)) {
        dispatch_action(long_hold.action);
        key_hold_fired = true;
        return;
    }

    register_code16(long_hold.action);
    held_action_keycode = long_hold.action;
    key_hold_fired      = true;
}

// Flush the currently tracked key when a new key overwrites key_active.
// Without this, pressing a second hold-key before releasing the first
// would silently drop the first key (no tap, no hold — just lost).
static void flush_active_key(void) {
    if (key_active == KC_NO) return;

    if (key_hold_fired || held_action_keycode != KC_NO) {
        // A hold tier is currently registered — unregister it before the
        // active key is overwritten.  This also covers the promotion window
        // where held_action_keycode is live but key_hold_fired is still false.
        key_hold_fired = false;
        if (held_action_keycode != KC_NO) {
            unregister_code16(held_action_keycode);
            held_action_keycode = KC_NO;
        }
    } else if (!IS_QK_MOMENTARY(key_active) && active_tap_action != KC_NO) {
        // Still in tap window for a regular key — resolve using the
        // authored single-tap action rather than assuming keycode == tap.
        dispatch_action(active_tap_action);
    }
    // MO-only or multi-tap-only without hold behavior: layer_off
    // happens on release (independent of key_active), so no action needed.

    key_active       = KC_NO;
    active_tap_action = KC_NO;
    active_hold      = hold_behavior_none();
    active_long_hold = hold_behavior_none();
}

// ─── RGB Color Cache ─────────────────────────────────────────────────────────
// Pre-computed HSV → RGB conversions, populated once in keyboard_post_init_user.
#ifdef RGB_MATRIX_ENABLE
static rgb_t layer_rgb[LAYER_COUNT];
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static rgb_t led_group_rgb[LAYER_LED_GROUP_COUNT];
static rgb_t pd_mode_led_group_rgb[PD_MODE_LED_GROUP_COUNT];
#endif

// Simulate a full press+release of a Charybdis firmware keycode (e.g.
// DRAGSCROLL_MODE_TOGGLE).  Uses process_record_kb (not _user) to avoid
// infinite recursion back into our own handler.
static void tap_custom_bk_keycode(uint16_t kc) {
    keyrecord_t rec = {0};

    rec.event.pressed = true;
    process_record_kb(kc, &rec);

    rec.event.pressed = false;
    process_record_kb(kc, &rec);
}

// ─── Key Event Processing ───────────────────────────────────────────────────
//
// QMK calls this function for every key press and release.  Returning false
// tells QMK "I handled it, don't process further."  Returning true means
// "pass it along to the next handler."
//
// The logic is organized in stages:
//   1) Flush pending multi-tap on any different key press
//   2) Pointing device mode keys (press + release)
//   3) Normalized key behavior (press + release)
//   4) Early return for releases (everything below is press-only)
//   5) Macros (press-only)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 1) Flush pending multi-tap if a different key is pressed ---
    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_action);
    }

    // --- 2) Pointing device mode keys (react on both press and release) ---

    // 2a) Generic mode keys: tap sends the base-layer key (or override),
    //     hold activates the pointing-device mode, and key_behaviors[]
    //     can add multi-tap behavior.
    {
        uint8_t mode = pd_mode_for_keycode(keycode);
        if (mode) {
            key_behavior_view_t behavior = key_behavior_lookup(keycode);

            if (record->event.pressed) {
                // Multi-tap detection: same key pressed again while pending.
                if (multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && behavior.has_multi_tap) {
                    uint16_t action = multi_tap_advance(&multi_tap, keycode, key_behavior_step_lookup,
                                                        key_behavior_has_more_taps);
                    if (action != KC_NO) dispatch_action(action);
                    return false;
                }

                pd_mode_timer         = timer_read();
                pd_mode_timer_keycode = keycode;
                pd_mode_update(mode, true);
            } else {
                pd_mode_update(mode, false);
                if (pd_mode_timer_keycode == keycode && timer_elapsed(pd_mode_timer) < CUSTOM_TAP_HOLD_TERM) {
                    // Tap — check for multi-tap deferral first.
                    uint16_t tap_key = pd_mode_single_tap_action(record, behavior);
                    if (behavior.has_multi_tap) {
                        multi_tap_begin(&multi_tap, keycode, tap_key);
                    } else {
                        dispatch_action(tap_key);
                    }
                }
            }
            pd_state_sync();
            return false;
        }
    }

    // 2b) Special dragscroll keys (need custom press/release logic).
    switch (keycode) {
        case DRAGSCROLL_MODE: {
            static bool dragscroll_was_locked = false;
            if (record->event.pressed) {
                pd_mode_timer         = timer_read();
                pd_mode_timer_keycode = keycode;
                dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();
                pd_mode_update(PD_MODE_DRAGSCROLL, true);
                pd_state_sync();
                return true;
            } else {
                if (dragscroll_was_locked) {
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                    return false;
                }
                pd_mode_update(PD_MODE_DRAGSCROLL, false);
                pd_state_sync();
                if (pd_mode_timer_keycode == keycode && timer_elapsed(pd_mode_timer) < CUSTOM_TAP_HOLD_TERM) {
                    charybdis_set_pointer_dragscroll_enabled(false);
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                    return false;
                }
                return true;
            }
        }

        case DRG_TOG_ON_HOLD:
            if (record->event.pressed) {
                pd_mode_timer         = timer_read();
                pd_mode_timer_keycode = keycode;
            } else {
                bool dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();

                if (dragscroll_was_locked) {
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else if (pd_mode_timer_keycode == keycode && timer_elapsed(pd_mode_timer) > CUSTOM_TAP_HOLD_TERM) {
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else {
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                }
            }
            return false;
    }

    // --- 3) Key behavior view (react on both press and release) ---
    //
    // key_behavior_lookup() exposes the authored key_behaviors[] row plus
    // runtime facts such as "is this an MO() key?".
    //
    // MO() keycodes that appear in the behavior view are intercepted here
    // so we can add multi-tap behavior.  We handle layer_on/layer_off
    // ourselves and return false to prevent QMK from doing it twice.
    {
        key_behavior_view_t behavior = key_behavior_lookup(keycode);

        // If this keycode is covered by the normalized behavior view, we handle it.
        if (behavior.handled) {
            if (record->event.pressed) {
                // Multi-tap detection: same key pressed again while pending.
                if (multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && behavior.has_multi_tap) {
                    uint16_t action = multi_tap_advance(&multi_tap, keycode, key_behavior_step_lookup,
                                                        key_behavior_has_more_taps);
                    if (action != KC_NO) dispatch_action(action);
                    // Activate MO() layer so it's on while held.
                    if (behavior.is_momentary_layer) layer_on(QK_MOMENTARY_GET_LAYER(keycode));
                    // Track key for release handling when pending hold or MO layer.
                    if (multi_tap_pending_hold(&multi_tap) || behavior.is_momentary_layer) {
                        key_active       = keycode;
                        key_timer        = timer_read();
                        // If pending_hold, keep key_hold_fired false so release runs.
                        key_hold_fired   = !multi_tap_pending_hold(&multi_tap);
                        active_tap_action = KC_NO;
                        active_hold      = hold_behavior_none();
                        active_long_hold = hold_behavior_none();
                    }
                    return false;
                }

                // MO() layer activation — always turn on immediately.
                if (behavior.is_momentary_layer) {
                    layer_on(QK_MOMENTARY_GET_LAYER(keycode));
                }

                // If another behavior key is still active (overlapping
                // keypresses), flush it so it isn't silently dropped.
                flush_active_key();

                // Normal press — start tracking + cache lookups.
                key_timer        = timer_read();
                key_active       = keycode;
                key_hold_fired   = false;
                active_tap_action = key_behavior_single_tap_action(behavior);
                active_hold      = behavior.single.hold;
                active_long_hold = behavior.single.long_hold;
            } else {
                // Release — resolve pending hold-after-multi-tap first.
                if (multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode) {
                    bool            was_release_hold = hold_sends_on_release(multi_tap.hold);
                    hold_behavior_t cached_hold = multi_tap.hold;
                    hold_behavior_t cached_long_hold = multi_tap.long_hold;
                    uint8_t         repeat_count = 0;
                    uint16_t action = multi_tap_resolve_hold(&multi_tap, keycode, key_behavior_has_more_taps,
                                                             &repeat_count);
                    if (was_release_hold && cached_hold.present && repeat_count == 1 &&
                        action == cached_hold.action) {
                        action = select_release_hold_action(timer_elapsed(key_timer), cached_hold.action,
                                                            cached_long_hold);
                    }
                    for (uint8_t i = 0; i < repeat_count; i++) {
                        if (action != KC_NO) dispatch_action(action);
                    }
                    // Deactivate MO() layer unless it was just locked.
                    if (behavior.is_momentary_layer) {
                        uint8_t layer = QK_MOMENTARY_GET_LAYER(keycode);
                        if (locked_layer != layer) layer_off(layer);
                    }
                    key_active       = KC_NO;
                    active_tap_action = KC_NO;
                    active_hold      = hold_behavior_none();
                    active_long_hold = hold_behavior_none();
                    return false;
                }

                // MO() layer deactivates on release (unless locked).
                if (behavior.is_momentary_layer) {
                    uint8_t layer = QK_MOMENTARY_GET_LAYER(keycode);
                    if (locked_layer != layer) layer_off(layer);
                }

                // Only process if this matches the key we're tracking.
                if (key_active != keycode) return false;

                // Snapshot cached lookups before clearing.
                uint16_t        rel_tap_action = active_tap_action;
                hold_behavior_t rel_hold       = active_hold;
                hold_behavior_t rel_long_hold  = active_long_hold;

                key_active       = KC_NO;
                active_tap_action = KC_NO;
                active_hold      = hold_behavior_none();
                active_long_hold = hold_behavior_none();

                // If any keycode is registered (immediate hold, multi-tap hold,
                // or long-hold), unregister it and we're done.
                if (key_hold_fired || held_action_keycode != KC_NO) {
                    key_hold_fired = false;
                    if (held_action_keycode != KC_NO) {
                        unregister_code16(held_action_keycode);
                        held_action_keycode = KC_NO;
                    }
                    return false;
                }

                // Determine action based on elapsed time.
                uint16_t elapsed = timer_elapsed(key_timer);

                if (elapsed < CUSTOM_TAP_HOLD_TERM) {
                    // TAP while any layer is locked → unlock immediately.
                    if (behavior.is_momentary_layer && locked_layer) {
                        layer_off(locked_layer);
                        locked_layer = 0;
                    } else if (behavior.has_multi_tap) {
                        multi_tap_begin(&multi_tap, keycode, rel_tap_action);
                    } else if (rel_tap_action != KC_NO) {
                        dispatch_action(rel_tap_action);
                    }
                    // MO()-only with no multi-tap: tap does nothing (layer already toggled).
                } else {
                    if (hold_sends_on_release(rel_hold)) {
                        dispatch_action(select_release_hold_action(elapsed, rel_hold.action, rel_long_hold));
                    } else if (!behavior.is_momentary_layer && rel_tap_action != KC_NO) {
                        // No hold tier was configured, so press duration does not
                        // change the result for regular keys.
                        dispatch_action(rel_tap_action);
                    }
                    // MO()-only held: layer was already active, nothing extra to send.
                }
            }
            return false;
        }
    }

    // --- 4) Everything below is press-only — let releases pass through. ---
    if (!record->event.pressed) {
        return true;
    }

    // --- 5) Macros (fire once on key-down, defined in key_config.h) ---
    if (macro_dispatch(keycode)) return false;
    return true;
}

// ─── Matrix Scan: Hold Detection + Multi-Tap Flush ──────────────────────────
//
// Called every matrix scan cycle (~1ms).
//   1. Threshold-fired hold/long-hold: fires as soon as threshold is reached.
//      Also handles the long-hold tier after multi-tap hold when
//      section 2 promotes a multi-tap hold into the shared active state.
//   2. Threshold-fired hold-after-multi-tap: fires the current step's hold tier
//      once CUSTOM_TAP_HOLD_TERM is reached.  If that hold tier has an
//      threshold-fired long-hold, keep tracking so section 1 can promote it at
//      CUSTOM_LONGER_HOLD_TERM. TAP_ON_RELEASE_AFTER_HOLD multi-tap holds skip
//      this section and are resolved on release in process_record_user.
//   3. Flushes pending multi-taps when the window expires.
void matrix_scan_user(void) {
    // 1. Threshold-based hold detection.
    if (key_active != KC_NO && !key_hold_fired) {
        uint16_t elapsed = timer_elapsed(key_timer);
        // Threshold-fired long hold fires at CUSTOM_LONGER_HOLD_TERM.
        if (hold_fires_at_threshold(active_long_hold) && elapsed >= CUSTOM_LONGER_HOLD_TERM) {
            promote_to_long_hold(active_long_hold);
        }
        // Threshold-fired hold fires at CUSTOM_TAP_HOLD_TERM.
        else if (hold_fires_at_threshold(active_hold) && elapsed >= CUSTOM_TAP_HOLD_TERM) {
            fire_hold_at_threshold(active_hold, active_long_hold);
        }
    }

    // 2. Hold-after-multi-tap: fire the hold tier once threshold is reached.
    if (multi_tap_hold_elapsed(&multi_tap)) {
        // Drop the MO layer so a threshold-fired layer lock is immediately visible
        // while the key is still held. The release path will layer_off again,
        // but turning off an already-off layer is a no-op.
        if (IS_QK_MOMENTARY(key_active) && is_layer_lock_action(multi_tap.hold.action)) {
            layer_off(QK_MOMENTARY_GET_LAYER(key_active));
        }
        active_long_hold = multi_tap.long_hold;
        fire_hold_at_threshold(multi_tap.hold, active_long_hold);
        multi_tap_reset(&multi_tap);
    }

    // 3. Flush pending multi-tap when the window expires.
    if (multi_tap_expired(&multi_tap)) {
        multi_tap_flush(&multi_tap, key_behavior_step_lookup, dispatch_action);
    }
}

// ─── Initialization ─────────────────────────────────────────────────────────

void keyboard_post_init_user(void) {
#ifdef RGB_MATRIX_ENABLE
    // Pre-compute HSV → RGB conversions for all color tables so the
    // per-frame render callback doesn't need to do it.
    for (uint8_t i = 0; i < LAYER_COUNT; i++)
        layer_rgb[i] = hsv_to_rgb(layer_colors[i]);
    // Map each mode's color by matching mode_flag from pd_modes[] to pd_mode_colors[].
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        for (uint8_t c = 0; c < PD_MODE_COLOR_COUNT; c++) {
            if (pd_mode_colors[c].mode_flag == pd_modes[i].mode_flag) {
                pd_mode_rgb[i] = hsv_to_rgb(pd_mode_colors[c].color);
                break;
            }
        }
    }
    for (uint8_t i = 0; i < LAYER_LED_GROUP_COUNT; i++)
        led_group_rgb[i] = hsv_to_rgb(layer_led_groups[i].color);
    for (uint8_t i = 0; i < PD_MODE_LED_GROUP_COUNT; i++)
        pd_mode_led_group_rgb[i] = hsv_to_rgb(pd_mode_led_groups[i].color);
#endif
}

// ─── Pointing Device Integration ────────────────────────────────────────────
#ifdef POINTING_DEVICE_ENABLE

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER);
    set_auto_mouse_enable(true);
    split_sync_init();
}

bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode) return true;
    }
    switch (keycode) {
        case SNIPING_MODE:
        case SNIPING_MODE_TOGGLE:
        case DRAGSCROLL_MODE:
        case DRAGSCROLL_MODE_TOGGLE:
        case DPI_MOD:
        case DPI_RMOD:
        case S_D_MOD:
        case S_D_RMOD:
        case DRG_TOG_ON_HOLD:
            return true;
    }
    return false;
}
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag) && pd_modes[i].handler) {
            return pd_modes[i].handler(mouse_report);
        }
    }
    return mouse_report;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, LAYER_RAISE));

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (layer_state_cmp(state, LAYER_POINTER) && layer_state_cmp(state, LAYER_RAISE)) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    } else if (layer_state_cmp(state, LAYER_POINTER) && (state & ~((layer_state_t)1 << LAYER_POINTER)) != 0 && !get_auto_mouse_toggle() && get_auto_mouse_key_tracker() == 0 && !charybdis_get_pointer_dragscroll_enabled()) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    }
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

    return state;
}

#endif // POINTING_DEVICE_ENABLE

#ifdef RGB_MATRIX_ENABLE

// ─── LED Index Map ──────────────────────────────────────────────────────────
//
// ╭────────────────────────╮                 ╭────────────────────────╮
//    0   7   8  15  16  20                     49  45  44  37  36  29
// ├────────────────────────┤                 ├────────────────────────┤
//    1   6   9  14  17  21                     50  46  43  38  35  30
// ├────────────────────────┤                 ├────────────────────────┤
//    2   5  10  13  18  22                     51  47  42  39  34  31
// ├────────────────────────┤                 ├────────────────────────┤
//    3   4  11  12  19  23                     52  48  41  40  33  32
// ╰────────────────────────╯                 ╰────────────────────────╯
//                       26  27  28     53  54  XX
//                           25  24     55  XX
//                     ╰────────────╯ ╰────────────╯

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool layer_painted = false;
    for (int8_t i = LAYER_COUNT - 1; i > 0; i--) {
        if (!layer_state_cmp(layer_state, i)) continue;
        if (layer_colors[i].s == 0 && layer_colors[i].v == 0) continue;
        rgb_set_both_halves(layer_rgb[i], led_min, led_max);
        layer_painted = true;
        break;
    }
#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    if (!layer_painted && layer_state_cmp(layer_state, get_auto_mouse_layer())) {
        automouse_rgb_render(led_min, led_max, automouse_color_start, automouse_color_end);
        layer_painted = true;
    }
#    endif

    for (uint8_t g = 0; g < LAYER_LED_GROUP_COUNT; g++) {
        if (layer_led_groups[g].layers & layer_state) {
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count, led_min, led_max, led_group_rgb[g]);
        }
    }

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag)) {
            rgb_set_right_half(pd_mode_rgb[i], led_min, led_max);
            break;
        }
    }

    for (uint8_t g = 0; g < PD_MODE_LED_GROUP_COUNT; g++) {
        if (pd_mode_active(pd_mode_led_groups[g].mode_flag)) {
            rgb_set_led_group(pd_mode_led_groups[g].leds, pd_mode_led_groups[g].count, led_min, led_max, pd_mode_led_group_rgb[g]);
        }
    }

    return layer_painted;
}

#endif // RGB_MATRIX_ENABLE

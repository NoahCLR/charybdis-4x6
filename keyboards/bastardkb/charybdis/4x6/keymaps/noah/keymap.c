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
//     per-feature config tables, combos, macro definitions, and LAYOUT
//     arrays.  Edit that file to change what keys do.
//
//   lib/key_types.h
//     Struct typedefs for the key behavior tables (hold_key_t, etc.).
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
//   - "Key behavior tables":  Per-feature config tables in key_config.h.
//     A key can appear in multiple tables to combine behaviors (e.g.
//     KC_6 is in hold_keys AND tap_actions).  Processing logic here
//     looks up each table independently.  Combos are also defined in
//     key_config.h but handled by QMK before process_record_user runs.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // QMK

#include "key_config.h"

#ifdef VIA_ENABLE
_Static_assert(LAYER_COUNT == DYNAMIC_KEYMAP_LAYER_COUNT, "LAYER_COUNT and DYNAMIC_KEYMAP_LAYER_COUNT are out of sync — update config.h");
#endif

#define HOLD_KEY_COUNT (sizeof(hold_keys) / sizeof(hold_keys[0]))
#define LONGER_HOLD_KEY_COUNT (sizeof(longer_hold_keys) / sizeof(longer_hold_keys[0]))
#define TAP_ACTION_COUNT (sizeof(tap_actions) / sizeof(tap_actions[0]))
#define MODE_TAP_OVERRIDE_COUNT (sizeof(mode_tap_overrides) / sizeof(mode_tap_overrides[0]))

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

// ─── Key Behavior Lookups ───────────────────────────────────────────────────
//
// Each lookup scans a small config table from key_config.h.
// Returns NULL if the keycode isn't in that table.

static const hold_key_t *hold_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < HOLD_KEY_COUNT; i++)
        if (hold_keys[i].keycode == keycode) return &hold_keys[i];
    return NULL;
}

static const longer_hold_key_t *longer_hold_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < LONGER_HOLD_KEY_COUNT; i++)
        if (longer_hold_keys[i].keycode == keycode) return &longer_hold_keys[i];
    return NULL;
}

// Find the tap_action entry for (keycode, tap_count), or NULL.
static const tap_action_t *tap_action_lookup(uint16_t keycode, uint8_t tap_count) {
    for (uint8_t i = 0; i < TAP_ACTION_COUNT; i++)
        if (tap_actions[i].keycode == keycode && tap_actions[i].tap_count == tap_count)
            return &tap_actions[i];
    return NULL;
}

// Does this key have any tap_action entry with tap_count > count?
static bool has_more_taps(uint16_t keycode, uint8_t count) {
    for (uint8_t i = 0; i < TAP_ACTION_COUNT; i++)
        if (tap_actions[i].keycode == keycode && tap_actions[i].tap_count > count)
            return true;
    return false;
}

// Does this key appear in tap_actions[] at all?
static bool is_multi_tap_key(uint16_t keycode) {
    for (uint8_t i = 0; i < TAP_ACTION_COUNT; i++)
        if (tap_actions[i].keycode == keycode) return true;
    return false;
}

static const mode_tap_override_t *mode_tap_override_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < MODE_TAP_OVERRIDE_COUNT; i++)
        if (mode_tap_overrides[i].keycode == keycode) return &mode_tap_overrides[i];
    return NULL;
}

// ─── Key Behavior State ─────────────────────────────────────────────────────
//
// Thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM      — tap vs hold boundary
//   CUSTOM_LONGER_HOLD_TERM   — hold vs longer-hold boundary
//   CUSTOM_MULTI_TAP_TERM     — max gap between consecutive taps
//   COMBO_TERM                — max gap between keys for combos

// Active key tracking — only one key behavior key tracked at a time.
static uint16_t key_timer;
static uint16_t key_active     = KC_NO;
static bool     key_hold_fired = false;

// Cached lookups for the currently active key (populated on press, cleared on release).
static const hold_key_t        *active_hold   = NULL;
static const longer_hold_key_t *active_longer = NULL;

// Multi-tap state — see lib/multi_tap.h for the state machine.
static multi_tap_t multi_tap = {0};

// Layer lock — which layer (if any) is locked on via multi-tap hold.
// 0 = no layer locked.
static uint8_t locked_layer = 0;

// Hold-after-multi-tap: the keycode currently registered (held down) via
// the hold path.  KC_NO = nothing held.  Unregistered on key release.
static uint16_t held_action_keycode = KC_NO;

// Dispatch an action keycode.  Handles LAYER_LOCK(n) dynamically;
// everything else falls through to tap_code16.
static void dispatch_action(uint16_t action) {
    if (action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT) {
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
//   3) Key behavior tables (press + release)
//   4) Early return for releases (everything below is press-only)
//   5) Macros (press-only)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 1) Flush pending multi-tap if a different key is pressed ---
    if (multi_tap_active(&multi_tap) && record->event.pressed && keycode != multi_tap.keycode) {
        multi_tap_flush(&multi_tap, tap_action_lookup, dispatch_action);
    }

    // --- 2) Pointing device mode keys (react on both press and release) ---

    // 2a) Generic mode keys: tap sends base-layer key (or override), hold activates mode.
    //     Also supports multi-tap via tap_actions[].
    {
        uint8_t mode = pd_mode_for_keycode(keycode);
        if (mode) {
            if (record->event.pressed) {
                // Multi-tap detection: same key pressed again while pending.
                if (multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && is_multi_tap_key(keycode)) {
                    uint16_t action = multi_tap_advance(&multi_tap, keycode, tap_action_lookup, has_more_taps);
                    if (action != KC_NO) dispatch_action(action);
                    return false;
                }

                pd_mode_timer = timer_read();
                pd_mode_update(mode, true);
            } else {
                pd_mode_update(mode, false);
                if (timer_elapsed(pd_mode_timer) < CUSTOM_TAP_HOLD_TERM) {
                    // Tap — check for multi-tap deferral first.
                    if (is_multi_tap_key(keycode)) {
                        // Resolve tap key: tap_count=1 > override > base-layer fallback.
                        const tap_action_t *single = tap_action_lookup(keycode, 1);
                        uint16_t tap_key;
                        if (single) {
                            tap_key = single->action;
                        } else {
                            const mode_tap_override_t *ovr = mode_tap_override_lookup(keycode);
                            tap_key = ovr ? ovr->tap : keymap_key_to_keycode(LAYER_BASE, record->event.key);
                        }
                        multi_tap_begin(&multi_tap, keycode, tap_key);
                    } else {
                        const mode_tap_override_t *ovr = mode_tap_override_lookup(keycode);
                        if (ovr) {
                            tap_code16(ovr->tap);
                        } else {
                            uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                            tap_code16(fallback_key);
                        }
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
                if (timer_elapsed(pd_mode_timer) < CUSTOM_TAP_HOLD_TERM) {
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
                pd_mode_timer = timer_read();
            } else {
                bool dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();

                if (dragscroll_was_locked) {
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else if (timer_elapsed(pd_mode_timer) > CUSTOM_TAP_HOLD_TERM) {
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

    // --- 3) Key behavior tables (react on both press and release) ---
    //
    // A keycode can appear in multiple tables.  We look up each table
    // independently and combine the results.
    //
    // MO() keycodes that appear in tap_actions[] are intercepted here
    // so we can add multi-tap behavior.  We handle layer_on/layer_off
    // ourselves and return false to prevent QMK from doing it twice.
    {
        const hold_key_t *hold    = hold_lookup(keycode);
        bool              mt_key  = is_multi_tap_key(keycode);
        bool              is_mo   = IS_QK_MOMENTARY(keycode);

        // If the keycode is in any behavior table, we handle it.
        if (hold || mt_key || is_mo) {
            if (record->event.pressed) {
                // Multi-tap detection: same key pressed again while pending.
                if (multi_tap_active(&multi_tap) && multi_tap.keycode == keycode && mt_key) {
                    uint16_t action = multi_tap_advance(&multi_tap, keycode, tap_action_lookup, has_more_taps);
                    if (action != KC_NO) dispatch_action(action);
                    // Activate MO() layer so it's on while held.
                    if (is_mo) layer_on(QK_MOMENTARY_GET_LAYER(keycode));
                    // Track key for release handling when pending hold or MO layer.
                    if (multi_tap_pending_hold(&multi_tap) || is_mo) {
                        key_active     = keycode;
                        key_timer      = timer_read();
                        // If pending_hold, keep key_hold_fired false so release runs.
                        key_hold_fired = !multi_tap_pending_hold(&multi_tap);
                        active_hold    = NULL;
                        active_longer  = NULL;
                    }
                    return false;
                }

                // MO() layer activation — always turn on immediately.
                if (is_mo) {
                    layer_on(QK_MOMENTARY_GET_LAYER(keycode));
                }

                // Normal press — start tracking + cache lookups.
                key_timer      = timer_read();
                key_active     = keycode;
                key_hold_fired = false;
                active_hold    = hold;
                active_longer  = longer_hold_lookup(keycode);
            } else {
                // Release — resolve pending hold-after-multi-tap first.
                if (multi_tap_pending_hold(&multi_tap) && multi_tap.keycode == keycode) {
                    uint16_t action = multi_tap_resolve_hold(&multi_tap, keycode, has_more_taps);
                    if (action != KC_NO) dispatch_action(action);
                    // Deactivate MO() layer unless it was just locked.
                    if (is_mo) {
                        uint8_t layer = QK_MOMENTARY_GET_LAYER(keycode);
                        if (locked_layer != layer) layer_off(layer);
                    }
                    key_active    = KC_NO;
                    active_hold   = NULL;
                    active_longer = NULL;
                    return false;
                }

                // MO() layer deactivates on release (unless locked).
                if (is_mo) {
                    uint8_t layer = QK_MOMENTARY_GET_LAYER(keycode);
                    if (locked_layer != layer) layer_off(layer);
                }

                // Only process if this matches the key we're tracking.
                if (key_active != keycode) return false;

                // Snapshot cached lookups before clearing.
                const hold_key_t        *rel_hold   = active_hold;
                const longer_hold_key_t *rel_longer = active_longer;

                key_active    = KC_NO;
                active_hold   = NULL;
                active_longer = NULL;

                // If hold already fired (immediate mode or hold-after-multi-tap),
                // unregister any held keycode and we're done.
                if (key_hold_fired) {
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
                    if (is_mo && locked_layer) {
                        layer_off(locked_layer);
                        locked_layer = 0;
                    } else if (mt_key) {
                        // tap_count=1 overrides single-tap action (e.g. give MO keys a tap action).
                        // Default: regular keys send themselves, MO keys send nothing.
                        const tap_action_t *single = tap_action_lookup(keycode, 1);
                        uint16_t single_act = single ? single->action : (is_mo ? KC_NO : keycode);
                        multi_tap_begin(&multi_tap, keycode, single_act);
                    } else if (rel_hold) {
                        tap_code16(keycode);
                    }
                    // MO()-only with no multi-tap: tap does nothing (layer already toggled).
                } else if (rel_longer && elapsed > CUSTOM_LONGER_HOLD_TERM) {
                    tap_code16(rel_longer->longer_hold);
                } else {
                    if (rel_hold) tap_code16(rel_hold->hold);
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
//   1. For keys with immediate hold or immediate longer-hold:
//      fires the action as soon as the threshold is reached.
//   2. Hold-after-multi-tap: fires hold action once CUSTOM_TAP_HOLD_TERM is reached.
//   3. Flushes pending multi-taps when the window expires.
void matrix_scan_user(void) {
    // 1. Threshold-based hold detection.
    if (key_active != KC_NO && !key_hold_fired) {
        uint16_t elapsed = timer_elapsed(key_timer);
        // Immediate longer hold fires at CUSTOM_LONGER_HOLD_TERM.
        // Registered (not tapped) so the OS can auto-repeat while held.
        if (active_longer && active_longer->immediate && elapsed >= CUSTOM_LONGER_HOLD_TERM) {
            register_code16(active_longer->longer_hold);
            held_action_keycode = active_longer->longer_hold;
            key_hold_fired = true;
        }
        // Immediate hold fires at CUSTOM_TAP_HOLD_TERM.
        // Registered (not tapped) so the OS can auto-repeat while held.
        else if (active_hold && active_hold->immediate && elapsed >= CUSTOM_TAP_HOLD_TERM) {
            register_code16(active_hold->hold);
            held_action_keycode = active_hold->hold;
            key_hold_fired = true;
        }
    }

    // 2. Hold-after-multi-tap: fire hold action once threshold is reached.
    //    Custom actions (layer lock) dispatch once; regular keycodes are
    //    registered (held) and unregistered when the physical key is released.
    if (multi_tap_hold_elapsed(&multi_tap)) {
        uint16_t action = multi_tap.hold_action;
        if (action >= LAYER_LOCK_BASE && action < LAYER_LOCK_BASE + LAYER_COUNT) {
            dispatch_action(action);
            // Drop the MO layer so the locked layer is immediately visible
            // while the key is still held.  The release path will layer_off
            // again, but turning off an already-off layer is a no-op.
            if (IS_QK_MOMENTARY(key_active)) {
                layer_off(QK_MOMENTARY_GET_LAYER(key_active));
            }
        } else {
            register_code16(action);
            held_action_keycode = action;
        }
        key_hold_fired = true;
        multi_tap_reset(&multi_tap);
    }

    // 3. Flush pending multi-tap when the window expires.
    if (multi_tap_expired(&multi_tap)) {
        multi_tap_flush(&multi_tap, tap_action_lookup, dispatch_action);
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

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
//     All key behavior configuration: enums (layers, custom keycodes,
//     tap dances), tap dance config table, tap/hold mapping table,
//     macro definitions, and LAYOUT arrays.  Edit this file to change
//     what keys do.
//
//   pointing_device_modes.h
//     Bitfield-based mode system that changes what the trackball does.
//     Modes: Volume (Y-axis → volume), Brightness (Y-axis → brightness),
//     Zoom (Y-axis → GUI+Plus/Minus), Arrow (motion → arrow keys),
//     Dragscroll (firmware-native scroll mode).
//
//   split_sync.h
//     Syncs pointing-device state (mode flags + auto-mouse elapsed time)
//     from the master half to the slave half over QMK's split RPC
//     transport.  Both values travel in a single 3-byte packet.
//
//   rgb_automouse.h
//     Renders the auto-mouse countdown gradient (white → red) based on
//     how much of the timeout has elapsed.
//
//   rgb_config.h
//     All RGB color definitions (layer indicators, mode overlays, auto-mouse
//     gradient) and split-safe helper functions for rgb_matrix_set_color().
//
// Key concepts for newcomers:
//
//   - "Split keyboard":  Each physical half has its own MCU.  The right
//     half (master) runs the keymap logic and sends state to the left
//     half (slave) so it can update its own LEDs.
//
//   - "Auto-mouse layer":  QMK can automatically activate a layer when
//     it detects trackball movement, and deactivate it after a timeout.
//     We use this for LAYER_POINTER.
//
//   - "Pointing device modes":  We define custom modes that change the behavior of the
//     pointing device (e.g. volume control, scroll, mouse movement) and tie
//     them to keys in the keymap.  These modes are implemented by intercepting
//     the relevant keycodes in process_record_user() and setting flags that
//     the pointing device code checks to decide what to do with pointing device movement.
//
//   - "Tap dance":  Some keys (6, 7, 8, Lower, Raise) use QMK's tap
//     dance for double-tap actions (media controls).  Config is data-
//     driven via td_config[].
//
//   - "Tap vs Hold":  Remaining number/punctuation keys use a custom
//     three-tier system: tap (<150ms), hold (150–400ms), longer hold
//     (>400ms).  Hold fires immediately via matrix_scan_user() for
//     most keys; arrow keys keep release-based timing for three tiers.
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H

#include "key_config.h"
#include "pointing_device_modes.h"
#include "split_sync.h"
#include "rgb_automouse.h"
#include "rgb_config.h"

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
#    include "usb_util.h"
bool is_keyboard_master_impl(void) {
    usb_disconnect();
    return false;
}
#endif

// ─── Tap Dance Callbacks ────────────────────────────────────────────────────
// Config (enums, types, tables) is in key_config.h.  Only the callbacks
// live here; tap_dance_actions[] is populated from td_config[] at init.

static uint8_t td_hold_layer_active = 0;

static void td_finished(tap_dance_state_t *state, void *user_data) {
    const td_config_t *cfg = (const td_config_t *)user_data;

    if (state->count == 1) {
        if (state->pressed) {
            if (cfg->hold_layer) {
                layer_on(cfg->hold_layer);
                td_hold_layer_active = cfg->hold_layer;
            } else {
                tap_code16(cfg->hold);
            }
        } else {
            if (cfg->tap != KC_NO) tap_code16(cfg->tap);
        }
    } else if (state->count == 2 && !state->pressed) {
        tap_code16(cfg->double_tap);
    }
}

static void td_reset(tap_dance_state_t *state, void *user_data) {
    if (td_hold_layer_active) {
        layer_off(td_hold_layer_active);
        td_hold_layer_active = 0;
    }
}

// QMK requires this array to exist.  Entries are populated at init time
// from td_config[] so adding a new tap dance only requires one line in
// key_config.h — no changes needed here.
tap_dance_action_t tap_dance_actions[TD_COUNT];

// ─── Custom Tap / Hold / Longer-Hold System ─────────────────────────────────
//
// The mapping table lives in key_config.h (tap_hold_config[]).
// This section contains only the processing state and lookup logic.
//
// Thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM      = 150ms
//   CUSTOM_LONGER_HOLD_TERM   = 400ms
static uint16_t tap_hold_timer;
static uint16_t tap_hold_keycode = KC_NO;
static bool     tap_hold_fired   = false;
static const tap_hold_config_t *tap_hold_active_cfg = NULL;

// Look up a keycode in the tap_hold_config table.  Returns NULL if not found.
static const tap_hold_config_t *tap_hold_lookup(uint16_t keycode) {
    for (uint8_t i = 0; i < TAP_HOLD_CONFIG_COUNT; i++) {
        if (tap_hold_config[i].tap == keycode) return &tap_hold_config[i];
    }
    return NULL;
}

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
//   1) Pointing device mode keys (press + release)
//   2) Tap/hold/longer-hold keys (press + release)
//   3) Early return for releases (everything below is press-only)
//   4) Macros (press-only)

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // --- 1) Pointing device mode keys (react on both press and release) ---

    // 1a) Generic mode keys: tap sends base-layer key, hold activates mode.
    // Keycodes are looked up from pd_modes[] — no hardcoded cases needed.
    {
        uint8_t mode = pd_mode_for_keycode(keycode);
        if (mode) {
            if (record->event.pressed) {
                tap_hold_timer = timer_read();
                pd_mode_update(mode, true);
            } else {
                pd_mode_update(mode, false);
                if (timer_elapsed(tap_hold_timer) < CUSTOM_TAP_HOLD_TERM) {
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                }
            }
            pd_state_sync();
            return false;
        }
    }

    // 1b) Special dragscroll keys (need custom press/release logic).
    switch (keycode) {
        // DRAGSCROLL_MODE (Charybdis firmware keycode, not in our enum):
        // Dual-purpose like the other mode keys: tap sends the base-layer
        // key, hold activates drag-scroll.  Also adds unlock behavior:
        // if drag-scroll was already *toggled on* (locked), pressing and
        // releasing the momentary key will unlock it instead.
        case DRAGSCROLL_MODE: {
            static bool dragscroll_was_locked = false;
            if (record->event.pressed) {
                tap_hold_timer        = timer_read();
                dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();
                pd_mode_update(PD_MODE_DRAGSCROLL, true);
                pd_state_sync();
                return true;
            } else {
                if (dragscroll_was_locked) {
                    // It was locked — turn off the toggle and clear mode flag.
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                    return false;
                }
                pd_mode_update(PD_MODE_DRAGSCROLL, false);
                pd_state_sync();
                if (timer_elapsed(tap_hold_timer) < CUSTOM_TAP_HOLD_TERM) {
                    // Tap: disable dragscroll if it was enabled, then send the base-layer key.
                    charybdis_set_pointer_dragscroll_enabled(false);
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                    return false;
                }
                return true;
            }
        }

        // DRG_TOG_ON_HOLD: Dual-purpose key for the auto-mouse layer.
        //   Tap  → sends whatever key is at this position on LAYER_BASE.
        //   Hold → enables drag-scroll lock.
        //   When already locked, any press (tap or hold) unlocks.
        case DRG_TOG_ON_HOLD:
            if (record->event.pressed) {
                tap_hold_timer = timer_read();
            } else {
                bool dragscroll_was_locked = charybdis_get_pointer_dragscroll_enabled();

                if (dragscroll_was_locked) {
                    // Already locked — any release (tap or hold) unlocks.
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else if (timer_elapsed(tap_hold_timer) > CUSTOM_TAP_HOLD_TERM) {
                    // HOLD: toggle drag-scroll lock on
                    tap_custom_bk_keycode(DRAGSCROLL_MODE_TOGGLE);
                    pd_mode_update(PD_MODE_DRAGSCROLL, charybdis_get_pointer_dragscroll_enabled());
                    pd_state_sync();
                } else {
                    // TAP: look up and send the base-layer key at this matrix position
                    uint16_t fallback_key = keymap_key_to_keycode(LAYER_BASE, record->event.key);
                    tap_code16(fallback_key);
                }
            }
            return false;
    }

    // --- 2) Custom tap/hold/longer-hold keys (react on both press and release) ---
    // Config table is in key_config.h.  Keys with immediate=true fire their
    // hold variant at the threshold via matrix_scan_user (no release needed).
    {
        const tap_hold_config_t *cfg = tap_hold_lookup(keycode);
        if (cfg) {
            if (record->event.pressed) {
                tap_hold_timer      = timer_read();
                tap_hold_keycode    = keycode;
                tap_hold_active_cfg = cfg;
                tap_hold_fired      = false;
            } else {
                tap_hold_keycode    = KC_NO;
                tap_hold_active_cfg = NULL;
                if (tap_hold_fired) {
                    tap_hold_fired = false;
                } else {
                    uint16_t elapsed = timer_elapsed(tap_hold_timer);

                    if (elapsed < CUSTOM_TAP_HOLD_TERM) {
                        tap_code16(cfg->tap);
                    } else if (elapsed > CUSTOM_LONGER_HOLD_TERM && cfg->longer_hold != KC_NO) {
                        tap_code16(cfg->longer_hold);
                    } else {
                        tap_code16(cfg->hold);
                    }
                }
            }
            return false;
        }
    }

    // --- 3) Everything below is press-only — let releases pass through. ---
    if (!record->event.pressed) {
        return true;
    }

    // --- 4) Macros (fire once on key-down, defined in key_config.h) ---
    MACRO_DISPATCH(keycode);
    return false;
}

// ─── Immediate Hold Detection ────────────────────────────────────────────────
//
// Called every matrix scan cycle (~1ms).  For keys with immediate=true in
// tap_hold_config[], fires the hold variant as soon as CUSTOM_TAP_HOLD_TERM
// is reached — no release needed.  Keys with immediate=false (e.g. arrows)
// are handled entirely on release in process_record_user().
void matrix_scan_user(void) {
    if (tap_hold_active_cfg && !tap_hold_fired) {
        if (!tap_hold_active_cfg->immediate) return;
        if (timer_elapsed(tap_hold_timer) >= CUSTOM_TAP_HOLD_TERM) {
            tap_code16(tap_hold_active_cfg->hold);
            tap_hold_fired = true;
        }
    }
}

// ─── Initialization ─────────────────────────────────────────────────────────

void keyboard_post_init_user(void) {
    // Populate tap_dance_actions[] from td_config[] so adding a new
    // tap dance only requires one line in key_config.h.
    for (uint8_t i = 0; i < TD_COUNT; i++) {
        tap_dance_actions[i].fn.on_each_tap       = NULL;
        tap_dance_actions[i].fn.on_dance_finished = td_finished;
        tap_dance_actions[i].fn.on_reset          = td_reset;
        tap_dance_actions[i].fn.on_each_release   = NULL;
        tap_dance_actions[i].user_data            = (void *)&td_config[i];
    }
}

// ─── Pointing Device Integration ────────────────────────────────────────────
//
// The Charybdis has a trackball on the right half.  QMK's pointing
// device subsystem calls these hooks to let us customize behavior.
//
// Key features:
//   - Auto-mouse: LAYER_POINTER activates when the trackball moves and
//     deactivates after AUTO_MOUSE_TIME (1200ms) of no movement.
//   - Sniping: Automatically enabled on LAYER_RAISE for precision work
//     (lower DPI while that layer is active).
//   - Mode interception: Volume, Brightness, Zoom, and Arrow modes intercept
//     trackball reports before they reach the OS (see pointing_device_modes.h).
#ifdef POINTING_DEVICE_ENABLE

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
void pointing_device_init_user(void) {
    set_auto_mouse_layer(LAYER_POINTER);
    set_auto_mouse_enable(true);
    split_sync_init(); // register split RPC handler for state sync
}
// Tell QMK which custom keycodes count as "mouse activity" so that pressing
// them keeps the auto-mouse layer alive (prevents it from timing out while
// you're actively using trackball features).
bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    // Mode keycodes from pd_modes[] — auto-discovered, no manual list needed.
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_modes[i].keycode != KC_NO && pd_modes[i].keycode == keycode)
            return true;
    }
    // Charybdis firmware keycodes + special custom keys (not in pd_modes[]).
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

// Called every scan cycle with the trackball's motion report.
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (!pd_mode_active(pd_modes[i].mode_flag)) continue;
        switch (pd_modes[i].mode_flag) {
            case PD_MODE_VOLUME:
                return handle_volume_mode(mouse_report);
            case PD_MODE_BRIGHTNESS:
                return handle_brightness_mode(mouse_report);
            case PD_MODE_ZOOM:
                return handle_zoom_mode(mouse_report);
            case PD_MODE_ARROW:
                return handle_arrow_mode(mouse_report);
            default:
                break; // PD_MODE_DRAGSCROLL: handled by charybdis firmware
        }
    }
    return mouse_report;
}

// Called whenever the active layer set changes.
// We use this to:
//   1. Prevent the auto-mouse pointer layer from stacking on top of other
//      active layers (which would cause flickering / stuck states).
//   2. Enable sniping (lower DPI) automatically on LAYER_RAISE.
//   3. Disable auto-mouse while LAYER_RAISE is active, because sniping
//      and auto-mouse fight over pointer behavior.
layer_state_t layer_state_set_user(layer_state_t state) {
    // Enable sniping (lower DPI) automatically on LAYER_RAISE.
    charybdis_set_pointer_sniping_enabled(layer_state_cmp(state, LAYER_RAISE));

#    ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    // Strip LAYER_POINTER when LAYER_RAISE is active to avoid key
    // conflicts (both layers define different keys at the same positions).
    // Auto-mouse stays enabled — it may re-activate POINTER on trackball
    // movement, but the strip will remove it again on the next layer
    // state update.  This avoids calling set_auto_mouse_enable() which
    // destructively resets all auto-mouse state (key tracker, toggle,
    // timers), breaking drag-scroll lock and momentary drag-scroll.
    if (layer_state_cmp(state, LAYER_POINTER) && layer_state_cmp(state, LAYER_RAISE)) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    }
    // For other non-base layers, strip POINTER only if nothing is holding
    // it active (no toggle, no held mouse key, no drag scroll).
    else if (layer_state_cmp(state, LAYER_POINTER) && (state & ~((layer_state_t)1 << LAYER_POINTER)) != 0 && !get_auto_mouse_toggle() && get_auto_mouse_key_tracker() == 0 && !charybdis_get_pointer_dragscroll_enabled()) {
        state &= ~((layer_state_t)1 << LAYER_POINTER);
    }
#    endif // POINTING_DEVICE_AUTO_MOUSE_ENABLE

    return state;
}

#endif // POINTING_DEVICE_ENABLE

#ifdef RGB_MATRIX_ENABLE

// ─── RGB Matrix Layer Indicators ────────────────────────────────────────────
//
// Each layer gets a distinct LED color so you always know which layer is
// active at a glance.  Pointing device modes overlay a color on the right
// half (the trackball side) so you can tell which mode the trackball is in.
//
// Colors are defined in rgb_config.h.  This section is rendering logic only.
// See rgb_config.h for the split-safe helper API and all color values.

// ─── LED Index Map ──────────────────────────────────────────────────────────
//
// Physical LED positions for reference when targeting specific LEDs.
// Numbers are the LED index passed to rgb_matrix_set_color().
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
//
// 0–28  → left half (29 LEDs)
// 29–57 → right half (29 LEDs, 2 are dummy/unused on pointer side)

// Pre-computed RGB cache.  hsv_to_rgb() is called once at init instead of
// every frame (~30fps × 2 chunks = ~60 calls/sec saved).
// HSV source arrays live in rgb_config.h (layer_colors[], pd_mode_colors[]).
static rgb_t layer_rgb[LAYER_COUNT];
static rgb_t pd_mode_rgb[PD_MODE_COUNT];
static rgb_t led_group_rgb[LAYER_LED_GROUP_COUNT];
static bool  rgb_colors_init = false;

// QMK calls this in batches (led_min to led_max) — potentially multiple
// times per frame, once per "chunk" of LEDs.  On a split keyboard, each
// half only processes its own LEDs.
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    // One-time initialization: convert all HSV colors to RGB.
    if (!rgb_colors_init) {
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
        rgb_colors_init = true;
    }

    // Paint all LEDs with the highest active layer's color.
    // Layers with {0,0,0} in layer_colors[] are skipped (BASE uses the
    // default RGB effect, POINTER uses the auto-mouse gradient below).
    // The layer-stripping logic in layer_state_set_user ensures that
    // explicit layers take priority over the auto-activated pointer layer.
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

    // Paint per-layer LED group highlights (e.g. modifier keys on specific layers).
    for (uint8_t g = 0; g < LAYER_LED_GROUP_COUNT; g++) {
        if (layer_state_cmp(layer_state, layer_led_groups[g].layer)) {
            rgb_set_led_group(layer_led_groups[g].leds, layer_led_groups[g].count,
                              led_min, led_max, led_group_rgb[g]);
        }
    }

    // If a pointing device mode is active, override the right half with the
    // mode's color.  This provides instant visual feedback for which mode
    // the trackball is in.  Priority order comes from pd_modes[].
    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        if (pd_mode_active(pd_modes[i].mode_flag)) {
            rgb_set_right_half(pd_mode_rgb[i], led_min, led_max);
            break;
        }
    }

    return layer_painted;
}

#endif // RGB_MATRIX_ENABLE

// ────────────────────────────────────────────────────────────────────────────
// Key Configuration — what each key does
// ────────────────────────────────────────────────────────────────────────────
//
// This file holds all the behavioral data for the keymap: the unified
// key_behaviors[] table, combos, macro definitions, and LAYOUT arrays.
// The processing logic that uses this data lives in runtime modules under lib/.
//
// Want to change what a key does?  Edit this file.
// Want to change how keys are processed?  Edit the runtime modules under lib/.
//
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "quantum_keycodes.h"
#include QMK_KEYBOARD_H // QMK

#include "lib/key_behavior.h"

// ─── Layers ─────────────────────────────────────────────────────────────────
//
// If you add or remove a layer, also update DYNAMIC_KEYMAP_LAYER_COUNT in config.h.
enum charybdis_keymap_layers {
    LAYER_BASE = 0, // Default QWERTY typing layer
    LAYER_NUM,      // Numpad on the right half
    LAYER_LOWER,    // Symbols and DPI controls
    LAYER_RAISE,    // Navigation, media, and mouse buttons (sniping enabled)
    LAYER_POINTER,  // Auto-mouse layer: activates on trackball movement, deactivates after timeout
    LAYER_COUNT,
};

// ─── Custom Keycodes ────────────────────────────────────────────────────────
//
// Custom keycodes are assigned values starting from SAFE_RANGE so they don't
// collide with any built-in QMK or Charybdis keycodes.
//
// MACRO_0–15 are generic macro slots (some used, rest reserved for VIA).
// VOLUME_MODE / BRIGHTNESS_MODE / ARROW_MODE / ZOOM_MODE activate pointing
// device modes while held.
// DRAGSCROLL: tap = base-layer key, double-tap = lock dragscroll, hold = momentary.
// The hold path is handled by the pd_mode key system (section 2 in process_record_user).
// Pointing-device modes can dispatch LOCK_PD_MODE(mode_keycode) via the
// key_behaviors[] multi-tap system.
// PD_MODE_LOCK_BASE reserves one lock/toggle action per pd-mode keycode.
// LAYER_LOCK_BASE reserves LAYER_COUNT keycodes for layer locking via
// actions authored in key_behaviors[].  Use the LOCK_LAYER(n) macro there.

enum custom_keycodes {
    MACRO_0 = SAFE_RANGE,
    MACRO_1,
    MACRO_2,
    MACRO_3,
    MACRO_4,
    MACRO_5,
    MACRO_6,  // reserved for VIA
    MACRO_7,  // reserved for VIA
    MACRO_8,  // reserved for VIA
    MACRO_9,  // reserved for VIA
    MACRO_10, // reserved for VIA
    MACRO_11, // reserved for VIA
    MACRO_12, // reserved for VIA
    MACRO_13, // reserved for VIA
    MACRO_14, // reserved for VIA
    MACRO_15, // reserved for VIA
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRAGSCROLL,
    PD_MODE_LOCK_BASE, // reserves one lock/toggle action per pd-mode keycode — use LOCK_PD_MODE(mode_keycode)
    LAYER_LOCK_BASE = PD_MODE_LOCK_BASE + (DRAGSCROLL - VOLUME_MODE + 1), // reserves LAYER_COUNT keycodes — use LOCK_LAYER(n) macro
    CUSTOM_KEYCODES_END = LAYER_LOCK_BASE + LAYER_COUNT,
};

#define LOCK_PD_MODE(mode_keycode_) (PD_MODE_LOCK_BASE + ((mode_keycode_) - VOLUME_MODE))
#define LOCK_LAYER(layer_)          (LAYER_LOCK_BASE + (layer_))

bool macro_dispatch(uint16_t keycode);
extern const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS];

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// key_behaviors[] is the single authored behavior table for keys handled by
// the custom state machine.  One row describes one physical key.
//
// Each row contains up to KEY_BEHAVIOR_MAX_TAP_COUNT tap counts:
//   tap_counts[0] = single press
//   tap_counts[1] = double tap
//   tap_counts[2] = triple tap
//
// Tap-count entries are exact matches. If a count has no entry, quick taps
// fall back to the key's normal tap behavior repeated that many times.
//
// Within each step:
//   tap       = what happens on a quick release
//               omit it to keep the key's normal tap behavior
//   hold      = what happens after CUSTOM_TAP_HOLD_TERM
//   long_hold = what happens after CUSTOM_LONGER_HOLD_TERM
//
// This keeps all behavior for one key in one place.
//
// Timing thresholds are defined in config.h:
//   CUSTOM_TAP_HOLD_TERM    — tap vs hold boundary (150ms)
//   CUSTOM_LONGER_HOLD_TERM — hold vs long-hold boundary (400ms)
//   CUSTOM_MULTI_TAP_TERM   — max gap between consecutive taps (150ms)
//   COMBO_TERM              — max gap between keys for combos (50ms)
//
// Per-key timing overrides (all optional — omit to inherit the global default):
//   .tap_hold_term   = <ms>  — overrides CUSTOM_TAP_HOLD_TERM for this key
//                              LT() keys default to TAPPING_TERM (200ms) instead
//   .longer_hold_term = <ms> — overrides CUSTOM_LONGER_HOLD_TERM for this key
//   .multi_tap_term  = <ms>  — overrides CUSTOM_MULTI_TAP_TERM for this key
//
// Authoring terms:
//   .tap = TAP_SENDS(x)   = tapping sends x
//   omit .tap             = tapping keeps the key's normal tap behavior
//   .hold = PRESS_AND_HOLD_UNTIL_RELEASE(x)
//                         = holding presses x until you release
//   .hold = TAP_AT_HOLD_THRESHOLD(x)
//                         = holding sends x once when the hold threshold is reached
//   .hold = TAP_ON_RELEASE_AFTER_HOLD(x)
//                         = holding sends x once when you release
//   .long_hold            = second hold tier after the longer threshold
//
// The small authoring DSL used below also lives in lib/key_behavior.h, so this
// file stays focused on configuration data.
//
// Full example — every field populated to show the complete surface area:
//
//   {
//       .keycode          = KC_X,
//       .tap_hold_term    = 180,  // optional: tap vs hold for this key
//       .longer_hold_term = 500,  // optional: hold vs long-hold for this key
//       .multi_tap_term   = 200,  // optional: max gap between consecutive taps
//       .tap_counts = {
//           [0] = {                                                    // single tap/hold
//               .tap       = TAP_SENDS(KC_X),                         // quick release → X
//               .hold      = TAP_ON_RELEASE_AFTER_HOLD(KC_LSFT),      // short hold → Shift sent on release
//               .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCTL),   // long hold → Ctrl registered while held
//           },                                                         //   ↑ escalating commitment: one-shot → registered
//           [1] = {                                                    // double tap/hold
//               .tap       = TAP_SENDS(G(KC_X)),                      // double-tap → GUI+X
//               .hold      = TAP_ON_RELEASE_AFTER_HOLD(KC_MUTE),      // double-tap then hold → Mute on release
//               .long_hold = TAP_ON_RELEASE_AFTER_HOLD(KC_VOLD),      // held longer → Vol Down on release
//           },                                                         //   ↑ both release-based: elapsed time picks
//           [2] = {                                                    // triple tap/hold
//               .tap  = TAP_SENDS(LOCK_LAYER(LAYER_LOWER)),           // triple-tap → lock LAYER_LOWER
//               .hold = TAP_AT_HOLD_THRESHOLD(KC_MNXT),               // triple-tap then hold → Next track once
//           },                                                         //   ↑ TAP_AT_THRESHOLD alone, no long_hold needed
//       },
//   },
//
// Hold mode combinations — all valid and invalid pairs:
//
//   hold only:
//     PRESS_AND_HOLD_UNTIL_RELEASE          — registered while held, unregistered on release
//     TAP_AT_HOLD_THRESHOLD                 — fires once at tap_hold_term, done
//     TAP_ON_RELEASE_AFTER_HOLD             — fires on release if held past tap_hold_term
//
//   long_hold only (no hold):
//     PRESS_AND_HOLD_UNTIL_RELEASE          — fires at longer_hold_term; short holds act as tap
//     TAP_AT_HOLD_THRESHOLD                 — fires once at longer_hold_term; short holds act as tap
//     TAP_ON_RELEASE_AFTER_HOLD             — fires on release if held past longer_hold_term; short holds act as tap
//
//   hold + long_hold pairs:
//     TAP_ON_RELEASE_AFTER_HOLD + TAP_ON_RELEASE_AFTER_HOLD    — both resolve on release; elapsed time picks which
//     TAP_ON_RELEASE_AFTER_HOLD + PRESS_AND_HOLD_UNTIL_RELEASE — release one-shot on short hold, registered key on long hold
//     TAP_ON_RELEASE_AFTER_HOLD + TAP_AT_HOLD_THRESHOLD        — release one-shot on short hold, immediate one-shot at longer threshold
//
//     TAP_AT_HOLD_THRESHOLD     + TAP_ON_RELEASE_AFTER_HOLD    — immediate one-shot at threshold, release one-shot if held longer
//     TAP_AT_HOLD_THRESHOLD     + TAP_AT_HOLD_THRESHOLD        — immediate one-shot at tap_hold_term, second one-shot at longer_hold_term
//     TAP_AT_HOLD_THRESHOLD     + PRESS_AND_HOLD_UNTIL_RELEASE — immediate one-shot at threshold, registered key if held longer
//
//     PRESS_AND_HOLD_UNTIL_RELEASE + TAP_ON_RELEASE_AFTER_HOLD    — key A registered while held; on release: unregister A then send B at release
//     PRESS_AND_HOLD_UNTIL_RELEASE + TAP_AT_HOLD_THRESHOLD         — key A registered; at longer_hold_term: unregister A, fire B immediately
//     PRESS_AND_HOLD_UNTIL_RELEASE + PRESS_AND_HOLD_UNTIL_RELEASE  — key A registered; at longer_hold_term: unregister A, register B until release

#ifdef KEY_CONFIG_DEFINE_BEHAVIORS

// keycode                single / double / triple / N behavior
const key_behavior_t
    key_behaviors[] =
        {
            // Number row — shifted symbols on hold, media on double-tap
            {.keycode = KC_1, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXLM)}}},
            {.keycode = KC_2, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AT)}}},
            {.keycode = KC_3, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_HASH)}}},
            {.keycode = KC_4, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DLR)}}},
            {.keycode = KC_5, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PERC)}}},
            {.keycode = KC_6, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_CIRC)}, [1] = {.tap = TAP_SENDS(KC_MPLY)}}},
            {.keycode = KC_7, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AMPR)}, [1] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)}}},
            {.keycode = KC_8, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR)}, [1] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)}}},
            {.keycode = KC_9, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LPRN)}}},
            {.keycode = KC_0, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RPRN)}}},

            // Punctuation → shifted variants
            {.keycode = KC_MINS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)}}},
            {.keycode = KC_EQL, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PLUS)}}},
            {.keycode = KC_LBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCBR)}}},
            {.keycode = KC_RBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RCBR)}}},
            {.keycode = KC_BSLS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PIPE)}}},
            {.keycode = KC_GRV, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_TILD)}}},
            {.keycode = KC_SCLN, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_COLN)}}},
            {.keycode = KC_QUOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)}}},
            {.keycode = KC_COMM, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)}}},
            {.keycode = KC_DOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)}}},

            // Escape → Force Quit on hold,tilde on tap
            {.keycode = KC_ESC, .tap_counts = {[0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))}, [1] = {.tap = TAP_SENDS(S(KC_GRV))}}},

            // Enter → Shift+Enter (new line without send in chat apps)
            {.keycode = KC_ENT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}}},

            // Arrows — release-based hold plus immediate long hold
            {.keycode = KC_LEFT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))}}},
            {.keycode = KC_RIGHT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))}}},

            // Layer keys — tap override on single tap, media on multi-tap, layer lock or repeat on hold
            {
                .keycode = MO(LAYER_LOWER),
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_LOWER))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            {
                .keycode = MO(LAYER_RAISE),
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_RAISE))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            // Home-row layer-tap key — double-tap locks LAYER_RAISE (same toggle as the thumb MO key).
            // tap_hold_term inherits the LT() default (TAPPING_TERM) so typing feel is unchanged.
            {
                .keycode = LT(LAYER_RAISE, KC_SLSH),
                .tap_counts =
                    {
                        [1] = {.hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_RAISE))},
                    },
            },

            // Pointing device mode keys.
            // Single tap defaults to the base-layer key at that position unless [0]
            // overrides it here.
            {.keycode = VOLUME_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(KC_MUTE)}}},
            {.keycode = ARROW_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))}}},

            // Dragscroll: tap = base-layer key, double-tap = lock, hold = momentary (via pd_mode).
            {.keycode = DRAGSCROLL, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(DRAGSCROLL))}}},
};

const uint8_t key_behavior_count = sizeof(key_behaviors) / sizeof(key_behaviors[0]);

#endif // KEY_CONFIG_DEFINE_BEHAVIORS

// ─── Macros ─────────────────────────────────────────────────────────────────
//
// Each macro maps a custom keycode to a SEND_STRING expression.
// Returns true if the keycode was handled, false otherwise.
//
// To add a macro: add a case below, and make sure the keycode exists in the
// custom_keycodes enum above.

#ifdef KEY_CONFIG_DEFINE_MACROS

bool macro_dispatch(uint16_t keycode) {
    switch (keycode) {
        case MACRO_0: // Spotlight search (macOS): GUI + Space
            SEND_STRING(SS_LGUI(SS_TAP(X_SPACE)));
            return true;
        case MACRO_1: // Claude: Alt + Space
            SEND_STRING(SS_LALT(SS_TAP(X_SPACE)));
            return true;
        case MACRO_2: // Terminal: Alt + GUI + Space
            SEND_STRING(SS_LALT(SS_LGUI(SS_TAP(X_SPACE))));
            return true;
        case MACRO_3: // OCR text copy (macOS): Ctrl + Alt + GUI + C
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("c"))));
            return true;
        case MACRO_4: // Screenshot (macOS): Ctrl + Alt + GUI + X
            SEND_STRING(SS_LCTL(SS_LALT(SS_LGUI("x"))));
            return true;
        case MACRO_5: // Emoji picker (macOS): Ctrl + GUI + Space
            SEND_STRING(SS_LCTL(SS_LGUI(SS_TAP(X_SPACE))));
            return true;
        default:
            return false;
    }
}

#endif // KEY_CONFIG_DEFINE_MACROS

// Combo definitions and physical keymap layouts live in keymap.c so that the
// file users open actually shows the board layout, and because QMK's keymap
// introspection expects combo definitions in that translation unit.

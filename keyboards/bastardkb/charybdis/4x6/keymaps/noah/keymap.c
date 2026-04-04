// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the authored keymap data:
//   - VIA_MACROS(MACRO)
//   - HARDCODED_MACROS(MACRO)
//   - COMBOS(COMBO)
//   - key_behaviors[]
//   - keymaps[][]
//
// Shared layer declarations and userspace-owned keycodes live in
// users/noah/noah_keymap.h. Keymap-local custom keycodes live below.
// Default standard QMK hooks live in users/noah/hooks.c; shared runtime
// processing lives in the userspace runtime modules under users/noah/lib/.
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h"

// ─── Keymap-Local Custom Keycodes ──────────────────────────────────────────
//
// Add keymap-local custom keycodes here. The VIA -> QMK converter reads this
// enum and maps later CUSTOM(n) tokens back to these symbolic names.
//
// Keep the sentinel, then add real keycodes below it. The first real keycode
// will naturally start at NOAH_KEYMAP_SAFE_RANGE. Those keycodes can be
// handled in process_record_user() and used inside key_behaviors[] actions
// such as TAP_SENDS(...), TAP_AT_HOLD_THRESHOLD(...), or
// PRESS_AND_HOLD_UNTIL_RELEASE(...).
//
enum keymap_custom_keycodes {
    KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
    RIGHT_THUMB,
    LEFT_THUMB
    // MY_CUSTOM_KEY,
    // MY_OTHER_KEY,
};

// ─── VIA Macros ─────────────────────────────────────────────────────────────
// VIA_MACRO_0–15 are the authored aliases for VIA's dynamic macro slots.
// All 16 slots are listed here so the slot limit stays visible in keymap.c.
// Use an empty string for an unused slot. The converter script can update this
// block from a VIA export, and the firmware seeds VIA's dynamic macro EEPROM
// defaults from it on init/reset.
// Workflow:
//   - author defaults here -> compile/flash -> reset EEPROM to re-seed them
//   - or run via_to_qmk_layout.py --write -> import the selected VIA export,
//     rewrite this block and keymaps[][], then compile/flash -> reset EEPROM
//     to re-seed those defaults
//
// If you do not run the converter, the firmware compiles exactly from the
// values authored here.
// Syntax:
//   - text {hello} sends "hello" one key at a time
//   - {KC_A} tap one key, {KC_LGUI,KC_SPC} tap a chord
//   - {+KC_LGUI} key down, {-KC_LGUI} key up
//   - {250} wait 250 ms before the next macro step
//     e.g. {KC_A}{250}{KC_B} pauses between A and B; other keys pressed
//     during the delay are queued
#define VIA_MACROS(MACRO)                                \
    MACRO(VIA_MACRO_0, "{KC_LGUI,KC_SPC}")               \
    MACRO(VIA_MACRO_1, "{KC_LALT,KC_SPC}")               \
    MACRO(VIA_MACRO_2, "{KC_LALT,KC_LGUI,KC_SPC}")       \
    MACRO(VIA_MACRO_3, "{KC_LCTL,KC_LALT,KC_LGUI,KC_C}") \
    MACRO(VIA_MACRO_4, "{KC_LCTL,KC_LALT,KC_LGUI,KC_X}") \
    MACRO(VIA_MACRO_5, "{KC_LCTL,KC_LGUI,KC_SPC}")       \
    MACRO(VIA_MACRO_6, "{KC_LALT,KC_LGUI,KC_8}")         \
    MACRO(VIA_MACRO_7, "")                               \
    MACRO(VIA_MACRO_8, "")                               \
    MACRO(VIA_MACRO_9, "")                               \
    MACRO(VIA_MACRO_10, "")                              \
    MACRO(VIA_MACRO_11, "")                              \
    MACRO(VIA_MACRO_12, "")                              \
    MACRO(VIA_MACRO_13, "")                              \
    MACRO(VIA_MACRO_14, "")                              \
    MACRO(VIA_MACRO_15, "")

// ─── Hardcoded Macros ───────────────────────────────────────────────────────
// HARDCODED_MACROS(MACRO) is the authored table for hardcoded custom macro
// keycodes used by key_behaviors[] and authored layers.
// All 16 slots are listed here so the slot limit stays visible in keymap.c.
// Use an empty string for an unused slot.
// VIA's dynamic macro payloads live in VIA_MACROS(MACRO) above.
// Use VIA_MACRO_n when you want a QMK/VIA dynamic macro keycode instead of
// one of these hardcoded custom macros.
// In the VIA export JSON, these hardcoded custom macros remain CUSTOM(64+n),
// while VIA's dynamic macro keycodes appear as MACRO(n).
// Hardcoded macros use the same "{...}" payload language as VIA macros, but
// they stay native to the firmware side and are interpreted at runtime by
// users/noah/lib/macro/macro_payload.c.
//
// The via_to_qmk_layout.py converter never rewrites this block; it only knows the MACRO_n names
// when converting VIA layout tokens.
// If you need more hardcoded slots than MACRO_15, extend enum custom_keycodes
// in users/noah/noah_keymap.h.
#define HARDCODED_MACROS(MACRO) \
    MACRO(MACRO_0, "")          \
    MACRO(MACRO_1, "")          \
    MACRO(MACRO_2, "")          \
    MACRO(MACRO_3, "")          \
    MACRO(MACRO_4, "")          \
    MACRO(MACRO_5, "")          \
    MACRO(MACRO_6, "")          \
    MACRO(MACRO_7, "")          \
    MACRO(MACRO_8, "")          \
    MACRO(MACRO_9, "")          \
    MACRO(MACRO_10, "")         \
    MACRO(MACRO_11, "")         \
    MACRO(MACRO_12, "")         \
    MACRO(MACRO_13, "")         \
    MACRO(MACRO_14, "")         \
    MACRO(MACRO_15, "")

// ─── Combos ─────────────────────────────────────────────────────────────────
//
// COMBOS(COMBO) is the authored combo table.
// Each row is:
//   COMBO(output_keycode, (key_1, key_2, ...))
// Group the key list in parentheses so 2-key and 3+-key combos read the same way.
//
// Valid combo outputs include plain keycodes, hardcoded macros (MACRO_n),
// VIA macros (VIA_MACRO_n), LOCK_LAYER(...), LOCK_PD_MODE(...), and keycodes
// that also have rows in key_behaviors[].
//
// If a combo emits a keycode that also has a row in key_behaviors[],
// that emitted key can reuse the same custom behavior handling.
//
// Combo timing is tuned in config.h via COMBO_TERM.
// Current default: COMBO_TERM = 50 ms.
#define COMBOS(COMBO)                                      \
    COMBO(KC_TAB, (KC_D, LT(LAYER_NAV, KC_F)))             \
    /* COMBO(MACRO_0, (KC_Q, KC_W)) */                     \
    /* COMBO(VIA_MACRO_0, (KC_U, KC_I)) */                 \
    /* COMBO(LOCK_LAYER(LAYER_NAV), (KC_J, KC_K)) */       \
    /* COMBO(LOCK_PD_MODE(ARROW_MODE), (KC_M, KC_COMM)) */ \
    /* COMBO(..., (...)) */                                \
    /* ... */

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// key_behaviors[] is the single authored behavior table for keys handled by
// the custom state machine. One row describes one physical key.
//
// tap_counts[0] = single press
// tap_counts[1] = double tap
// tap_counts[2] = triple tap
// tap_counts[3] = quadruple tap
// tap_counts[4] = quintuple tap
//
// timing defaults currently set in config.h:
//   - TAPPING_TERM = 200 ms
//     used by built-in QMK LT()/MT() keys and by LT() rows here when
//     .tap_hold_term is omitted
//   - CUSTOM_TAP_HOLD_TERM = 150 ms
//     default first hold threshold for custom key_behavior rows
//   - CUSTOM_LONGER_HOLD_TERM = 400 ms
//     default longer-hold threshold
//   - CUSTOM_MULTI_TAP_TERM = 150 ms
//     max gap allowed between taps in a multi-tap sequence
//   - KEY_BEHAVIOR_MAX_TAP_COUNT = 5
//     current runtime limit for tap_counts[] entries
//
// per-key timing overrides:
//   - .tap_hold_term overrides the first hold threshold for that row
//   - .longer_hold_term overrides the longer-hold threshold
//   - .multi_tap_term overrides the max gap between taps
//   - omit them (or leave them 0) to use the defaults above
//   - for LT() rows, omitted .tap_hold_term falls back to TAPPING_TERM;
//     all other rows fall back to CUSTOM_TAP_HOLD_TERM
//
// multi-tap behavior:
//   - if a key has higher tap_counts[] entries, lower tap counts wait one
//     multi-tap window before firing so the engine can see whether more taps
//     follow
//   - this means single taps on multi-tap keys are delayed by .multi_tap_term
//
// omit .tap to keep the key's normal tap behavior for that step
// .hold and .long_hold are independent: define either one by itself, or use
// both together for a two-stage hold
//
// action can be a plain keycode, a modded keycode, a macro, a layer lock,
// a pointer-mode lock, or a QMK behavior keycode such as TG()/TO()/TT()/OSL()
// and LT()/MT() when you want their tap or hold semantics explicitly
//   - Use LOCK_LAYER(layer) to toggle a layer lock.
//     Locking the same layer again turns it off; locking a different layer
//     switches the lock to that layer.
//   - Use LOCK_PD_MODE(mode_keycode) to toggle a pointer-mode lock.
//     Activating the same mode again unlocks it; pressing, holding or locking
//     any other pointer-mode key also clears the previous lock.
//   - Add hardcoded custom macros to HARDCODED_MACROS(MACRO) above, then use
//     MACRO_n as the action in any helper: TAP_SENDS(MACRO_n),
//     TAP_AT_HOLD_THRESHOLD(MACRO_n), TAP_ON_RELEASE_AFTER_HOLD(MACRO_n), or
//     PRESS_AND_HOLD_UNTIL_RELEASE(MACRO_n).
//   - Use VIA_MACRO_n for a QMK/VIA dynamic macro keycode whose default
//     contents come from VIA_MACROS(MACRO) above.
//   - Modded keycodes like G(KC_RIGHT), A(KC_LEFT), or S(KC_1) let one action
//     send GUI, Alt, Shift, and similar variants without adding separate keys.
//
// tap accepts one helper:
//   TAP_SENDS(action)
//     send action on a quick release instead of the key's normal tap
//     use this for alternate tap output, media keys, macros, layer locks,
//     and pointer-mode locks
//
// hold and long_hold accept the same three helpers:
//   PRESS_AND_HOLD_UNTIL_RELEASE(action)
//     normal keys/modifiers:
//       cross .tap_hold_term -> press/register action
//       release the key -> unregister action
//     macros:
//       cross .tap_hold_term -> send action once immediately
//       release the key -> do nothing
//     use this for modifiers or keys you want to stay down while held;
//     macros use the same helper for a held-triggered one-shot
//
//   TAP_AT_HOLD_THRESHOLD(action)
//     cross .tap_hold_term -> send action once immediately
//     use this for one-shot actions such as layer lock, pointer lock,
//     media controls, or macros
//
//   TAP_ON_RELEASE_AFTER_HOLD(action)
//     cross .tap_hold_term -> qualify the hold, but do nothing yet
//     release the key -> send action once
//     use this when the key should stay quiet while held, or when a longer
//     hold should still be able to replace the shorter hold action
//     with .long_hold configured, this creates a clean middle tier:
//       release after .tap_hold_term but before .longer_hold_term = .hold action
//       keep holding past .longer_hold_term = .long_hold action instead
//
// illustrative example row with custom timings and mixed tap/hold combinations:
// {
//     .keycode = KC_EXAMPLE,
//     .tap_hold_term = 150, // overrides the default tap-vs-hold threshold for this key
//     .longer_hold_term = 400, // overrides the default longer hold threshold for this key
//     .multi_tap_term = 150, // overrides the default multi-tap threshold for this key
//     .tap_counts =
//         {
//             [0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_EXAMPLE))}, // normal tap, exclusive long hold
//             [1] = {.tap = TAP_SENDS(S(KC_EXAMPLE))}, // alternate tap output
//             [2] = {.tap = TAP_SENDS(KC_EXAMPLE), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXAMPLE)}, // tap once, or keep held
//             [3] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_EXAMPLE))}, // normal tap, shifted hold
//             [4] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_EXAMPLE)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_EXAMPLE))}, // middle hold on release, longer hold upgrades
//             [5] = {.tap = TAP_SENDS(MACRO_n), .hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_EXAMPLE))}, // macro on tap, layer lock on hold
//             [6] = {.tap = TAP_SENDS(LOCK_PD_MODE(PD_MODE_EXAMPLE))}, // pointer-mode lock on tap
//         },
// }

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

            // Escape → Force Quit on hold, tilde on double-tap
            {.keycode = KC_ESC, .tap_counts = {[0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))}, [1] = {.tap = TAP_SENDS(S(KC_GRV))}}},

            // Enter → Shift+Enter (new line without send in chat apps)
            {.keycode = KC_ENT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}}},

            // Arrows — release-based hold plus immediate long hold
            {.keycode = KC_LEFT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))}}},
            {.keycode = KC_RIGHT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))}}},

            // Layer keys — tap override on single tap, media on multi-tap, layer lock or repeat on hold
            {
                .keycode = LEFT_THUMB,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_SYM)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            {
                .keycode = RIGHT_THUMB,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            // layer-tap key — double-tap hold locks LAYER_NAV.
            // tap_hold_term inherits the LT() default (TAPPING_TERM) so typing feel is unchanged.
            {
                .keycode = LT(LAYER_NAV, KC_SLSH),
                .tap_counts =
                    {
                        [1] = {.hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))},
                    },
            },

            // Pointing device mode keys.
            // Single tap defaults to the base-layer key at that position unless [0]
            // overrides it here.
            {.keycode = VOLUME_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(KC_MUTE)}}},
            {.keycode = ARROW_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))}}},
            {
                .keycode = PINCH_MODE,
                .tap_counts =
                    {
                        [1] = {.tap = TAP_SENDS(VIA_MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
                    },
            },

            // Dragscroll: tap = base-layer key, double-tap = lock, hold = momentary (via pd_mode).
            {.keycode = DRAGSCROLL, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(DRAGSCROLL))}}},
};

// ─── Keymap Layouts ─────────────────────────────────────────────────────────

// QMK key prefixes quick reference:
//   KC_     = plain keycode              MT() = mod on hold, key on tap
//   LT(l,k) = layer l on hold, k on tap  MO() = momentary layer while held
//   G()     = GUI + key                   A()  = Alt + key
//   S()     = Shift + key                 LCAG() = Ctrl+Alt+GUI + key
//   LSG()   = Left Shift+GUI + key        LAG()  = Left Alt+GUI + key
//
//   - XXXXXXX = key does nothing on this layer.
//     _______ = transparent, falls through to the layer below.
//
// The number row (KC_1–KC_0) and punctuation keys use the key behavior
// tables defined above — they are NOT using QMK's built-in mod-tap.
// See users/noah/lib/key/key_runtime.c for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),            KC_A,            KC_S,             KC_D,  LT(LAYER_NAV,KC_F),             KC_G,                KC_H,  LT(LAYER_SYM,KC_J),             KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_CTRL,  LT(LAYER_SYM,KC_Z),             KC_X,              KC_C,              KC_V,              KC_B,                KC_N,             KC_M,          KC_COMM,           KC_DOT,  LT(LAYER_NAV,KC_SLSH),     KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,     LEFT_THUMB,        RIGHT_THUMB,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NUM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P7,             KC_P8,             KC_P9,           XXXXXXX,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,     MO(LAYER_NAV),           XXXXXXX,              XXXXXXX,             KC_P4,             KC_P5,             KC_P6,           XXXXXXX,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           _______,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,             KC_P1,             KC_P2,             KC_P3,           KC_COMM,            KC_DOT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,             KC_P0,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_SYM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,           XXXXXXX,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LPRN,           KC_RPRN,           KC_QUOT,           KC_PPLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           XXXXXXX,           XXXXXXX,           XXXXXXX,            KC_ESC,           XXXXXXX,              XXXXXXX,           XXXXXXX,           KC_LBRC,           KC_RBRC,           KC_DQUO,           KC_PEQL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,           _______,        LCAG(KC_X),        LCAG(KC_C),         LSG(KC_V),         LSG(KC_P),              XXXXXXX,           XXXXXXX,           KC_LCBR,           KC_RCBR,           _______,      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NAV] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
              LAG(KC_ESC),           XXXXXXX,           XXXXXXX,        LCAG(KC_V),           XXXXXXX,           XXXXXXX,              KC_MPLY,           KC_MNXT,           KC_MPRV,           KC_MUTE,           KC_VOLD,           KC_VOLU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  XXXXXXX,           G(KC_Q),           G(KC_W),           G(KC_A),           XXXXXXX,           XXXXXXX,          VIA_MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           KC_BRID,           KC_BRIU,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
         MT(MOD_LSFT,KC_CAPS),        LSG(KC_Z),          XXXXXXX,          G(KC_C),          XXXXXXX,           XXXXXXX,          VIA_MACRO_1,           KC_LEFT,           KC_DOWN,           KC_RGHT,            KC_ESC,           XXXXXXX,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
              VIA_MACRO_5,           G(KC_Z),           G(KC_X),           G(KC_V),           XXXXXXX,           XXXXXXX,          VIA_MACRO_0,           MS_BTN1,           MS_BTN2,        DRAGSCROLL,           _______,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           _______,           _______,           _______,           _______,           _______,      BRIGHTNESS_MODE,        PINCH_MODE,           MS_BTN3,        ARROW_MODE,           _______,      KC_RIGHT_GUI,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,          VOLUME_MODE,           MS_BTN1,           MS_BTN2,        DRAGSCROLL,           _______,        ARROW_MODE,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                     KC_LEFT_GUI,  LT(LAYER_NUM,KC_SPC),         _______,              _______,            KC_ENT,
                                                                                               KC_DEL,           KC_BSPC,              KC_BSPC
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),
    // clang-format on
};

// Expand the authored macro tables, combo table, and derived counts above
// into the runtime symbols expected by the userspace runtime and QMK.
MATERIALIZE_KEYMAP_DATA();

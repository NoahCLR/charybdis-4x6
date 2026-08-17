// ────────────────────────────────────────────────────────────────────────────
// {{PROFILE_TITLE}} Charybdis 4x6 keymap data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the authored keymap data:
//   - VIA_MACROS(MACRO)
//   - HARDCODED_MACROS(MACRO)
//   - COMBOS(COMBO)
//   - key_behaviors[]
//   - keymaps[][]
//
// Userspace-owned keycodes live in users/noah/noah_keymap_ids.h.
// Keymap-local custom keycodes live below.
// Default standard QMK hooks live in users/noah/hooks.c; shared runtime
// processing lives in the userspace runtime modules under users/noah/lib/.
// ────────────────────────────────────────────────────────────────────────────

// Starter tables intentionally contain no active combo or key-behavior rows.
// Profile Studio removes these flags when you add the first real row.
#define NOAH_KEYMAP_EMPTY_COMBOS
#define NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS

#include "noah_keymap.h"

// ─── Keymap-Local Custom Keycodes ──────────────────────────────────────────
//
// Add keymap-local custom keycodes here. The VIA -> QMK converter reads this
// enum and maps later CUSTOM(n) tokens back to these symbolic names.
//
// Keep the sentinel, then add real keycodes below it. The first real keycode
// will naturally start at NOAH_KEYMAP_SAFE_RANGE. Those keycodes can be
// handled in process_record_user() and used inside key_behaviors[] actions
// such as TAP_SENDS(...), TAP_AT_HOLD_THRESHOLD(...),
// TAP_ON_RELEASE_AFTER_HOLD(...), REPEAT_WHILE_HELD(...),
// or PRESS_AND_HOLD_UNTIL_RELEASE(...).
//
enum keymap_custom_keycodes {
    KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
    // MY_CUSTOM_KEY,
};

// ─── VIA Macros ─────────────────────────────────────────────────────────────
// VIA_MACRO_0–63 are the authored aliases for VIA's dynamic macro slots.
// All 64 slots are listed here so the slot limit stays visible in keymap.c.
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
#define VIA_MACROS(MACRO)   \
    MACRO(VIA_MACRO_0, "")  \
    MACRO(VIA_MACRO_1, "")  \
    MACRO(VIA_MACRO_2, "")  \
    MACRO(VIA_MACRO_3, "")  \
    MACRO(VIA_MACRO_4, "")  \
    MACRO(VIA_MACRO_5, "")  \
    MACRO(VIA_MACRO_6, "")  \
    MACRO(VIA_MACRO_7, "")  \
    MACRO(VIA_MACRO_8, "")  \
    MACRO(VIA_MACRO_9, "")  \
    MACRO(VIA_MACRO_10, "") \
    MACRO(VIA_MACRO_11, "") \
    MACRO(VIA_MACRO_12, "") \
    MACRO(VIA_MACRO_13, "") \
    MACRO(VIA_MACRO_14, "") \
    MACRO(VIA_MACRO_15, "") \
    MACRO(VIA_MACRO_16, "") \
    MACRO(VIA_MACRO_17, "") \
    MACRO(VIA_MACRO_18, "") \
    MACRO(VIA_MACRO_19, "") \
    MACRO(VIA_MACRO_20, "") \
    MACRO(VIA_MACRO_21, "") \
    MACRO(VIA_MACRO_22, "") \
    MACRO(VIA_MACRO_23, "") \
    MACRO(VIA_MACRO_24, "") \
    MACRO(VIA_MACRO_25, "") \
    MACRO(VIA_MACRO_26, "") \
    MACRO(VIA_MACRO_27, "") \
    MACRO(VIA_MACRO_28, "") \
    MACRO(VIA_MACRO_29, "") \
    MACRO(VIA_MACRO_30, "") \
    MACRO(VIA_MACRO_31, "") \
    MACRO(VIA_MACRO_32, "") \
    MACRO(VIA_MACRO_33, "") \
    MACRO(VIA_MACRO_34, "") \
    MACRO(VIA_MACRO_35, "") \
    MACRO(VIA_MACRO_36, "") \
    MACRO(VIA_MACRO_37, "") \
    MACRO(VIA_MACRO_38, "") \
    MACRO(VIA_MACRO_39, "") \
    MACRO(VIA_MACRO_40, "") \
    MACRO(VIA_MACRO_41, "") \
    MACRO(VIA_MACRO_42, "") \
    MACRO(VIA_MACRO_43, "") \
    MACRO(VIA_MACRO_44, "") \
    MACRO(VIA_MACRO_45, "") \
    MACRO(VIA_MACRO_46, "") \
    MACRO(VIA_MACRO_47, "") \
    MACRO(VIA_MACRO_48, "") \
    MACRO(VIA_MACRO_49, "") \
    MACRO(VIA_MACRO_50, "") \
    MACRO(VIA_MACRO_51, "") \
    MACRO(VIA_MACRO_52, "") \
    MACRO(VIA_MACRO_53, "") \
    MACRO(VIA_MACRO_54, "") \
    MACRO(VIA_MACRO_55, "") \
    MACRO(VIA_MACRO_56, "") \
    MACRO(VIA_MACRO_57, "") \
    MACRO(VIA_MACRO_58, "") \
    MACRO(VIA_MACRO_59, "") \
    MACRO(VIA_MACRO_60, "") \
    MACRO(VIA_MACRO_61, "") \
    MACRO(VIA_MACRO_62, "") \
    MACRO(VIA_MACRO_63, "")

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
// in users/noah/noah_keymap_ids.h.
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
// VIA macros (VIA_MACRO_n), LOCK_LAYER(...), explicit pd-mode lock keycodes
// such as ARROW_MODE_LOCK, and keycodes that also have rows in key_behaviors[].
//
// If a combo emits a keycode that also has a row in key_behaviors[],
// that emitted key can reuse the same custom behavior handling.
//
// Runtime ownership still keeps one representative combo owner key for release
// matching, but RGB feedback and PD key-local rendering use the full
// physical combo footprint. Cross-half combos therefore broaden locality to
// both halves instead of guessing one side.
//
// Combo origin tracking follows the live resolved keycodes QMK sees, so VIA /
// dynamic keymap changes stay authoritative. Keep each combo row's member
// keycodes unique so that footprint tracking can disambiguate the chord.
//
// Combo timing is tuned in config.h via COMBO_TERM.
// Current default: COMBO_TERM = 50 ms.
// clang-format off
// The body is comments until the first authored row, and clang-format would
// collapse a comment-only macro body onto one line and drop the continuations.
// Profile Studio appends rows under the #define line, so the trailing
// backslashes have to survive formatting or the appended row lands outside the
// macro.
#define COMBOS(COMBO)                                \
    /* COMBO(KC_TAB, (KC_D, KC_F)) */                \
    /* COMBO(MACRO_0, (KC_Q, KC_W)) */               \
    /* COMBO(VIA_MACRO_0, (KC_U, KC_I)) */           \
    /* COMBO(LOCK_LAYER(LAYER_NAV), (KC_J, KC_K)) */ \
    /* COMBO(ARROW_MODE_LOCK, (KC_M, KC_COMM)) */    \
    /* COMBO(..., (...)) */                          \
    /* ... */
// clang-format on

// ─── Key Behavior Tables ────────────────────────────────────────────────────
//
// key_behaviors[] is the single authored behavior table for keys handled by
// the custom state machine. One row describes one physical key.
//
// tap_counts[0] = tap index 0 (single press)
// tap_counts[1] = tap index 1 (double press)
// tap_counts[2] = tap index 2 (triple press)
// tap_counts[3] = tap index 3 (quadruple press)
// tap_counts[4] = tap index 4 (quintuple press)
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
//   - this means base single taps on multi-tap keys are delayed by
//     .multi_tap_term, but skip the branch-confirm window
//   - double-tap and higher terminal tap branches that author .tap use the
//     same pending window, then the branch-confirm window, so RGB can show the
//     selected branch before the tap-commit pulse
//   - if a non-base branch omits .tap, a quick tap keeps the key's normal tap
//     behavior; branch-confirm still shows when that branch authors a hold or
//     long-hold tier, but tap-commit feedback is skipped
//
// within one tap index:
//   - .tap is the tap tier
//   - .hold is the normal hold tier
//   - .long_hold is the longer-hold tier
// omit .tap to keep the key's normal tap behavior for that tap index
// if .tap is set but hold/long_hold are omitted, keys that already have a
// default held path keep using it. Stacked pd-mode rows are contained: if a
// first-tap override can branch into another pd mode on a later hold, the
// first pd mode waits until the hold threshold instead of activating
// immediately. Other keycodes keep the tap override and send it on release
// .hold and .long_hold are independent: define either one by itself, or use
// both together for a two-stage hold
//
// RGB feedback follows authored tiers on the current tap index:
//   - multi-tap windows show a neutral pending color while the engine is still
//     resolving which tap index wins
//   - when a tap index commits, it can briefly show the authored branch color
//     before action feedback takes over
//   - an authored .hold tier can show hold-tier feedback while pending/active
//   - an authored .long_hold tier can show longer-hold-tier feedback when it
//     commits or stays active
//   - a missing .hold tier stays visually quiet even if .long_hold exists
//   - primary momentary layer access uses the layer color itself rather than a
//     separate hold pulse
//
// action can be a plain keycode, a modded keycode, a macro, a layer lock,
// a pointer-mode lock, or a supported QMK behavior keycode such as MT()/OSM()
//   - Use LOCK_LAYER(layer) to toggle a layer lock.
//     Locking the same layer again turns it off; different layers toggle
//     independently and can stay active together.
//   - Use PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for a momentary layer hold
//     owned by the custom runtime.
//   - Raw TG()/TO()/TT()/OSL()/LM()/LT() actions inside key_behaviors[] are
//     intentionally unsupported because they bypass layer ownership. LT()
//     remains supported as the keycode of a key_behavior row itself.
//   - Use the generated *_LOCK keycode to toggle a pointer-mode lock.
//     Activating the same mode again unlocks it; pressing, holding or locking
//     any other pointer-mode key also clears the previous lock.
//   - Add hardcoded custom macros to HARDCODED_MACROS(MACRO) above, then use
//     MACRO_n as the action in any helper: TAP_SENDS(MACRO_n),
//     TAP_AT_HOLD_THRESHOLD(MACRO_n), TAP_ON_RELEASE_AFTER_HOLD(MACRO_n),
//     PRESS_AND_HOLD_UNTIL_RELEASE(MACRO_n).
//   - Use VIA_MACRO_n for a QMK/VIA dynamic macro keycode whose default
//     contents come from VIA_MACROS(MACRO) above.
//   - Modded keycodes like G(KC_RIGHT), A(KC_LEFT), or S(KC_1) let one action
//     send GUI, Alt, Shift, and similar variants without adding separate keys.
//     Custom held actions decompose those into owned real mods plus the base
//     key so overlap stays safe; ordinary QMK tap paths still keep stock
//     tap_code16()/send_string semantics.
//
// tap-tier actions accept one helper:
//   TAP_SENDS(action)
//     send action on a quick release instead of the key's normal tap
//     use this for alternate tap output, media keys, macros, layer locks,
//     and pointer-mode locks
//   TAP_SENDS(KC_TRNS)
//     keep the lower active layer's tap output at this physical key while the
//     authored row still owns this tap count's timing and branching
//
// hold-tier and long-hold-tier actions accept the same three helpers:
//   PRESS_AND_HOLD_UNTIL_RELEASE(action)
//     normal keys/modifiers:
//       cross .tap_hold_term -> press/register action
//       release the key -> unregister action
//     macros:
//       cross .tap_hold_term -> send action once immediately
//       release the key -> do nothing
//     use this for modifiers or keys you want to stay down while held;
//     macros use the same helper for a held-triggered one-shot
//     PRESS_AND_HOLD_UNTIL_RELEASE(KC_TRNS) uses the lower active layer's
//     actual hold-tier behavior at the same key; if that lower key is a
//     momentary layer or pd mode, its normal hold ownership comes through
//
//   REPEAT_WHILE_HELD(action, hz)
//     cross .tap_hold_term -> send action once immediately
//     keep holding -> keep sending action at the authored frequency
//     release the key -> stop repeating immediately
//     supported authored range: 1..100 Hz
//     use this for click spam, repeated navigation, or other rapid tap
//     actions that should stay declarative inside key_behaviors[]
//     REPEAT_WHILE_HELD(KC_TRNS, hz) uses the lower active layer's same-tier
//     behavior; lower mode-owned hold surfaces still behave like themselves
//
//   TAP_AT_HOLD_THRESHOLD(action)
//     cross .tap_hold_term -> send action once immediately
//     use this for one-shot actions such as layer lock, pointer lock,
//     media controls, or macros
//     TAP_AT_HOLD_THRESHOLD(KC_TRNS) uses the lower active layer's same-tier
//     behavior when this threshold fires
//
//   TAP_ON_RELEASE_AFTER_HOLD(action)
//     cross .tap_hold_term -> qualify the hold, but do nothing yet
//     release the key -> send action once
//     use this when the key should stay quiet while held, or when a longer
//     hold should still be able to replace the shorter hold action
//     TAP_ON_RELEASE_AFTER_HOLD(KC_TRNS) releases into the lower active
//     layer's same-tier behavior at this physical key
//     with .long_hold configured, this creates a clean middle tier:
//       release after .tap_hold_term but before .longer_hold_term = .hold action
//       keep holding past .longer_hold_term = .long_hold action instead
//
// illustrative example row with custom timings and mixed tap indexes:
// {
//     .keycode = KC_EXAMPLE,
//     .tap_hold_term = 150, // overrides the default tap-vs-hold threshold for this key
//     .longer_hold_term = 400, // overrides the default longer hold threshold for this key
//     .multi_tap_term = 150, // overrides the default multi-tap threshold for this key
//     .tap_counts =
//         {
//             [0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_EXAMPLE))}, // tap index 0: normal tap, long-hold threshold action only
//             [1] = {.tap = TAP_SENDS(S(KC_EXAMPLE))}, // tap index 1: alternate tap output
//             [2] = {.tap = TAP_SENDS(KC_EXAMPLE), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXAMPLE)}, // tap index 2: tap once, or keep a held action active
//             [3] = {.tap = TAP_SENDS(EXAMPLE_MODE_LOCK), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_EXAMPLE))}, // tap index 3: pointer-mode lock on tap, shifted held action
//             [4] = {.tap = TAP_SENDS(MACRO_n), .hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_EXAMPLE)), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_EXAMPLE))}, // tap index 4: macro on tap, release action on hold, layer-lock threshold action on long hold
//         },
// }

const key_behavior_t key_behaviors[] = {
    {0},
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
// The number row (KC_1-KC_0) and punctuation keys are good candidates for
// custom rows in key_behaviors[] when you want alternate taps or holds.
// See the key runtime modules under users/noah/lib/key/ for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                   KC_ESC,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,           KC_MINS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,           KC_BSLS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,              KC_A,              KC_S,              KC_D,              KC_F,              KC_G,                 KC_H,              KC_J,              KC_K,              KC_L,           KC_SCLN,           KC_QUOT,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_LEFT_CTRL,              KC_Z,              KC_X,              KC_C,              KC_V,              KC_B,                 KC_N,              KC_M,           KC_COMM,            KC_DOT,           KC_SLSH,      KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,            KC_SPC,           KC_BSPC,              KC_ENT,             KC_NO,
                                                                                               KC_DEL,           KC_BSPC,              KC_NO
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NUM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_SYM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NAV] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_POINTER] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),
    // clang-format on
};

// Expand the authored macro tables, combo table, and derived counts above
// into the runtime symbols expected by the userspace runtime and QMK.
MATERIALIZE_KEYMAP_DATA();

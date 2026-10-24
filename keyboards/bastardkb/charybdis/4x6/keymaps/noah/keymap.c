// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the authored keymap data:
//   - CUSTOM_KEYS(KEY, SHORTCUT), each custom key's name and shortcut
//   - MACROS(MACRO), with each macro's name
//   - COMBOS(COMBO, COMBO_WINDOW)
//   - key_behaviors[]
//   - layer_names[]
//   - keymaps[][]
//
// Userspace-owned keycodes live in users/noah/noah_keymap_ids.h.
// Default standard QMK hooks live in users/noah/hooks.c; shared runtime
// processing lives in the userspace runtime modules under users/noah/lib/.
// ────────────────────────────────────────────────────────────────────────────

#include "noah_keymap.h"

// ─── Custom Keys ────────────────────────────────────────────────────────────
// CUSTOM_KEY_0–127 are named keys: place one on a layer or emit it from a
// combo. A custom key's shortcut is its own action, the way an ordinary key's
// keycode is: held while the key is held, tapped when it is tapped. Its
// key_behaviors[] row, if any, works on top of that. Without either it does
// nothing. A behaviour step on another key may send a custom key; it sends
// only its shortcut. All 128 slots are listed here so the slot limit stays
// visible in keymap.c.
// Each row is one of:
//   KEY(CUSTOM_KEY_n, "name")                       no shortcut
//   SHORTCUT(CUSTOM_KEY_n, "name", mods, key)       a shortcut
// The name is what Charybdis Ark shows for the key: at most 32 bytes of
// UTF-8. Use an empty string for an unused slot. A shortcut is one key plus
// any modifiers, left and right mixed: `mods` is SHORTCUT_MODS() of modifier
// keys (SHORTCUT_NO_MODS for none; never a QMK MOD_* constant) and `key` a
// basic key (KC_A–KC_EXSEL, no modifier keys) or a mouse button
// (MS_BTN1–MS_BTN8), or KC_NO for modifiers alone, e.g.
//   SHORTCUT(CUSTOM_KEY_4, "Hyper K", SHORTCUT_MODS(KC_LCTL, KC_RALT), KC_K)
// A profile saved from Charybdis Ark keeps its own names and shortcuts.
#define CUSTOM_KEYS(KEY, SHORTCUT)                                              \
    KEY(CUSTOM_KEY_0, "Right Thumb")                                            \
    KEY(CUSTOM_KEY_1, "Left Thumb")                                             \
    KEY(CUSTOM_KEY_2, "Click Spam")                                             \
    KEY(CUSTOM_KEY_3, "Vorssaint Radial")                                       \
    SHORTCUT(CUSTOM_KEY_4, "VoiceInk", SHORTCUT_MODS(KC_LCTL, KC_LALT), KC_SPC) \
    KEY(CUSTOM_KEY_5, "")                                                       \
    KEY(CUSTOM_KEY_6, "")                                                       \
    KEY(CUSTOM_KEY_7, "")                                                       \
    KEY(CUSTOM_KEY_8, "")                                                       \
    KEY(CUSTOM_KEY_9, "")                                                       \
    KEY(CUSTOM_KEY_10, "")                                                      \
    KEY(CUSTOM_KEY_11, "")                                                      \
    KEY(CUSTOM_KEY_12, "")                                                      \
    KEY(CUSTOM_KEY_13, "")                                                      \
    KEY(CUSTOM_KEY_14, "")                                                      \
    KEY(CUSTOM_KEY_15, "")                                                      \
    KEY(CUSTOM_KEY_16, "")                                                      \
    KEY(CUSTOM_KEY_17, "")                                                      \
    KEY(CUSTOM_KEY_18, "")                                                      \
    KEY(CUSTOM_KEY_19, "")                                                      \
    KEY(CUSTOM_KEY_20, "")                                                      \
    KEY(CUSTOM_KEY_21, "")                                                      \
    KEY(CUSTOM_KEY_22, "")                                                      \
    KEY(CUSTOM_KEY_23, "")                                                      \
    KEY(CUSTOM_KEY_24, "")                                                      \
    KEY(CUSTOM_KEY_25, "")                                                      \
    KEY(CUSTOM_KEY_26, "")                                                      \
    KEY(CUSTOM_KEY_27, "")                                                      \
    KEY(CUSTOM_KEY_28, "")                                                      \
    KEY(CUSTOM_KEY_29, "")                                                      \
    KEY(CUSTOM_KEY_30, "")                                                      \
    KEY(CUSTOM_KEY_31, "")                                                      \
    KEY(CUSTOM_KEY_32, "")                                                      \
    KEY(CUSTOM_KEY_33, "")                                                      \
    KEY(CUSTOM_KEY_34, "")                                                      \
    KEY(CUSTOM_KEY_35, "")                                                      \
    KEY(CUSTOM_KEY_36, "")                                                      \
    KEY(CUSTOM_KEY_37, "")                                                      \
    KEY(CUSTOM_KEY_38, "")                                                      \
    KEY(CUSTOM_KEY_39, "")                                                      \
    KEY(CUSTOM_KEY_40, "")                                                      \
    KEY(CUSTOM_KEY_41, "")                                                      \
    KEY(CUSTOM_KEY_42, "")                                                      \
    KEY(CUSTOM_KEY_43, "")                                                      \
    KEY(CUSTOM_KEY_44, "")                                                      \
    KEY(CUSTOM_KEY_45, "")                                                      \
    KEY(CUSTOM_KEY_46, "")                                                      \
    KEY(CUSTOM_KEY_47, "")                                                      \
    KEY(CUSTOM_KEY_48, "")                                                      \
    KEY(CUSTOM_KEY_49, "")                                                      \
    KEY(CUSTOM_KEY_50, "")                                                      \
    KEY(CUSTOM_KEY_51, "")                                                      \
    KEY(CUSTOM_KEY_52, "")                                                      \
    KEY(CUSTOM_KEY_53, "")                                                      \
    KEY(CUSTOM_KEY_54, "")                                                      \
    KEY(CUSTOM_KEY_55, "")                                                      \
    KEY(CUSTOM_KEY_56, "")                                                      \
    KEY(CUSTOM_KEY_57, "")                                                      \
    KEY(CUSTOM_KEY_58, "")                                                      \
    KEY(CUSTOM_KEY_59, "")                                                      \
    KEY(CUSTOM_KEY_60, "")                                                      \
    KEY(CUSTOM_KEY_61, "")                                                      \
    KEY(CUSTOM_KEY_62, "")                                                      \
    KEY(CUSTOM_KEY_63, "")                                                      \
    KEY(CUSTOM_KEY_64, "")                                                      \
    KEY(CUSTOM_KEY_65, "")                                                      \
    KEY(CUSTOM_KEY_66, "")                                                      \
    KEY(CUSTOM_KEY_67, "")                                                      \
    KEY(CUSTOM_KEY_68, "")                                                      \
    KEY(CUSTOM_KEY_69, "")                                                      \
    KEY(CUSTOM_KEY_70, "")                                                      \
    KEY(CUSTOM_KEY_71, "")                                                      \
    KEY(CUSTOM_KEY_72, "")                                                      \
    KEY(CUSTOM_KEY_73, "")                                                      \
    KEY(CUSTOM_KEY_74, "")                                                      \
    KEY(CUSTOM_KEY_75, "")                                                      \
    KEY(CUSTOM_KEY_76, "")                                                      \
    KEY(CUSTOM_KEY_77, "")                                                      \
    KEY(CUSTOM_KEY_78, "")                                                      \
    KEY(CUSTOM_KEY_79, "")                                                      \
    KEY(CUSTOM_KEY_80, "")                                                      \
    KEY(CUSTOM_KEY_81, "")                                                      \
    KEY(CUSTOM_KEY_82, "")                                                      \
    KEY(CUSTOM_KEY_83, "")                                                      \
    KEY(CUSTOM_KEY_84, "")                                                      \
    KEY(CUSTOM_KEY_85, "")                                                      \
    KEY(CUSTOM_KEY_86, "")                                                      \
    KEY(CUSTOM_KEY_87, "")                                                      \
    KEY(CUSTOM_KEY_88, "")                                                      \
    KEY(CUSTOM_KEY_89, "")                                                      \
    KEY(CUSTOM_KEY_90, "")                                                      \
    KEY(CUSTOM_KEY_91, "")                                                      \
    KEY(CUSTOM_KEY_92, "")                                                      \
    KEY(CUSTOM_KEY_93, "")                                                      \
    KEY(CUSTOM_KEY_94, "")                                                      \
    KEY(CUSTOM_KEY_95, "")                                                      \
    KEY(CUSTOM_KEY_96, "")                                                      \
    KEY(CUSTOM_KEY_97, "")                                                      \
    KEY(CUSTOM_KEY_98, "")                                                      \
    KEY(CUSTOM_KEY_99, "")                                                      \
    KEY(CUSTOM_KEY_100, "")                                                     \
    KEY(CUSTOM_KEY_101, "")                                                     \
    KEY(CUSTOM_KEY_102, "")                                                     \
    KEY(CUSTOM_KEY_103, "")                                                     \
    KEY(CUSTOM_KEY_104, "")                                                     \
    KEY(CUSTOM_KEY_105, "")                                                     \
    KEY(CUSTOM_KEY_106, "")                                                     \
    KEY(CUSTOM_KEY_107, "")                                                     \
    KEY(CUSTOM_KEY_108, "")                                                     \
    KEY(CUSTOM_KEY_109, "")                                                     \
    KEY(CUSTOM_KEY_110, "")                                                     \
    KEY(CUSTOM_KEY_111, "")                                                     \
    KEY(CUSTOM_KEY_112, "")                                                     \
    KEY(CUSTOM_KEY_113, "")                                                     \
    KEY(CUSTOM_KEY_114, "")                                                     \
    KEY(CUSTOM_KEY_115, "")                                                     \
    KEY(CUSTOM_KEY_116, "")                                                     \
    KEY(CUSTOM_KEY_117, "")                                                     \
    KEY(CUSTOM_KEY_118, "")                                                     \
    KEY(CUSTOM_KEY_119, "")                                                     \
    KEY(CUSTOM_KEY_120, "")                                                     \
    KEY(CUSTOM_KEY_121, "")                                                     \
    KEY(CUSTOM_KEY_122, "")                                                     \
    KEY(CUSTOM_KEY_123, "")                                                     \
    KEY(CUSTOM_KEY_124, "")                                                     \
    KEY(CUSTOM_KEY_125, "")                                                     \
    KEY(CUSTOM_KEY_126, "")                                                     \
    KEY(CUSTOM_KEY_127, "")

// ─── Macros ─────────────────────────────────────────────────────────────────
// MACRO_0–127 are the authored aliases for the 128 macro slots.
// All 128 slots are listed here so the slot limit stays visible in keymap.c.
// Each row is:
//   MACRO(MACRO_n, "name", "payload")
// The name is what Charybdis Ark shows for the macro: at most 32 bytes of
// UTF-8. Use empty strings for an unused slot. The payloads are the macro
// domain of the compiled default profile, which a keyboard with no saved
// profile plays. A profile saved from Charybdis Ark keeps its own names and
// payloads.
// Payload syntax:
//   - bare text such as hello sends "hello" one key at a time
//   - {KC_A} tap one key, {KC_LGUI,KC_SPC} tap a chord
//   - {+KC_LGUI} key down, {-KC_LGUI} key up
//   - {250} wait 250 ms before the next macro step
//     e.g. {KC_A}{250}{KC_B} pauses between A and B; other keys pressed
//     during the delay are queued
#define MACROS(MACRO)                                                              \
    MACRO(MACRO_0, "Spotlight", "{KC_LGUI,KC_SPC}")                                \
    MACRO(MACRO_1, "AI Chat", "{KC_LALT,KC_SPC}")                                  \
    MACRO(MACRO_2, "Warp Terminal", "{KC_LALT,KC_LGUI,KC_SPC}")                    \
    MACRO(MACRO_3, "OCR Copy", "{KC_LCTL,KC_LALT,KC_LGUI,KC_C}")                   \
    MACRO(MACRO_4, "Drag Screenshot", "{KC_LCTL,KC_LALT,KC_LGUI,KC_X}")            \
    MACRO(MACRO_5, "Emoji", "{KC_LCTL,KC_LGUI,KC_SPC}")                            \
    MACRO(MACRO_6, "Zoom Screen", "{KC_LALT,KC_LGUI,KC_8}")                        \
    MACRO(MACRO_7, "Clipboard History", "{KC_LCTL,KC_LALT,KC_LGUI,KC_V}")          \
    MACRO(MACRO_8, "VS Code Preview MD", "{KC_LSFT,KC_LGUI,KC_V}")                 \
    MACRO(MACRO_9, "VS Code Run Task", "{KC_LSFT,KC_LGUI,KC_P}")                   \
    MACRO(MACRO_10, "Select All + Copy", "{KC_LGUI,KC_A}{50}{KC_LGUI,KC_C}")       \
    MACRO(MACRO_11, "Select All", "{KC_LGUI,KC_A}")                                \
    MACRO(MACRO_12, "Copy", "{KC_LGUI,KC_C}")                                      \
    MACRO(MACRO_13, "Paste", "{KC_LGUI,KC_V}")                                     \
    MACRO(MACRO_14, "Cut", "{KC_LGUI,KC_X}")                                       \
    MACRO(MACRO_15, "Undo", "{KC_LGUI,KC_Z}")                                      \
    MACRO(MACRO_16, "Redo", "{KC_LSFT,KC_LGUI,KC_Z}")                              \
    MACRO(MACRO_17, "", "")                                                        \
    MACRO(MACRO_18, "", "")                                                        \
    MACRO(MACRO_19, "T3 Code", "{+KC_LGUI}{KC_TAB}{120}t3 code{-KC_LGUI}")         \
    MACRO(MACRO_20, "", "")                                                        \
    MACRO(MACRO_21, "", "")                                                        \
    MACRO(MACRO_22, "", "")                                                        \
    MACRO(MACRO_23, "", "")                                                        \
    MACRO(MACRO_24, "", "")                                                        \
    MACRO(MACRO_25, "", "")                                                        \
    MACRO(MACRO_26, "", "")                                                        \
    MACRO(MACRO_27, "", "")                                                        \
    MACRO(MACRO_28, "", "")                                                        \
    MACRO(MACRO_29, "", "")                                                        \
    MACRO(MACRO_30, "", "")                                                        \
    MACRO(MACRO_31, "", "")                                                        \
    MACRO(MACRO_32, "", "")                                                        \
    MACRO(MACRO_33, "", "")                                                        \
    MACRO(MACRO_34, "", "")                                                        \
    MACRO(MACRO_35, "", "")                                                        \
    MACRO(MACRO_36, "", "")                                                        \
    MACRO(MACRO_37, "", "")                                                        \
    MACRO(MACRO_38, "", "")                                                        \
    MACRO(MACRO_39, "", "")                                                        \
    MACRO(MACRO_40, "", "")                                                        \
    MACRO(MACRO_41, "", "")                                                        \
    MACRO(MACRO_42, "", "")                                                        \
    MACRO(MACRO_43, "", "")                                                        \
    MACRO(MACRO_44, "", "")                                                        \
    MACRO(MACRO_45, "", "")                                                        \
    MACRO(MACRO_46, "", "")                                                        \
    MACRO(MACRO_47, "", "")                                                        \
    MACRO(MACRO_48, "", "")                                                        \
    MACRO(MACRO_49, "", "")                                                        \
    MACRO(MACRO_50, "", "")                                                        \
    MACRO(MACRO_51, "", "")                                                        \
    MACRO(MACRO_52, "", "")                                                        \
    MACRO(MACRO_53, "", "")                                                        \
    MACRO(MACRO_54, "", "")                                                        \
    MACRO(MACRO_55, "", "")                                                        \
    MACRO(MACRO_56, "", "")                                                        \
    MACRO(MACRO_57, "", "")                                                        \
    MACRO(MACRO_58, "", "")                                                        \
    MACRO(MACRO_59, "", "")                                                        \
    MACRO(MACRO_60, "", "")                                                        \
    MACRO(MACRO_61, "", "")                                                        \
    MACRO(MACRO_62, "", "")                                                        \
    MACRO(MACRO_63, "", "")                                                        \
    MACRO(MACRO_64, "", "")                                                        \
    MACRO(MACRO_65, "", "")                                                        \
    MACRO(MACRO_66, "", "")                                                        \
    MACRO(MACRO_67, "", "")                                                        \
    MACRO(MACRO_68, "", "")                                                        \
    MACRO(MACRO_69, "", "")                                                        \
    MACRO(MACRO_70, "", "")                                                        \
    MACRO(MACRO_71, "", "")                                                        \
    MACRO(MACRO_72, "", "")                                                        \
    MACRO(MACRO_73, "", "")                                                        \
    MACRO(MACRO_74, "", "")                                                        \
    MACRO(MACRO_75, "", "")                                                        \
    MACRO(MACRO_76, "", "")                                                        \
    MACRO(MACRO_77, "", "")                                                        \
    MACRO(MACRO_78, "", "")                                                        \
    MACRO(MACRO_79, "", "")                                                        \
    MACRO(MACRO_80, "", "")                                                        \
    MACRO(MACRO_81, "", "")                                                        \
    MACRO(MACRO_82, "", "")                                                        \
    MACRO(MACRO_83, "", "")                                                        \
    MACRO(MACRO_84, "", "")                                                        \
    MACRO(MACRO_85, "", "")                                                        \
    MACRO(MACRO_86, "", "")                                                        \
    MACRO(MACRO_87, "", "")                                                        \
    MACRO(MACRO_88, "", "")                                                        \
    MACRO(MACRO_89, "", "")                                                        \
    MACRO(MACRO_90, "", "")                                                        \
    MACRO(MACRO_91, "", "")                                                        \
    MACRO(MACRO_92, "", "")                                                        \
    MACRO(MACRO_93, "", "")                                                        \
    MACRO(MACRO_94, "", "")                                                        \
    MACRO(MACRO_95, "", "")                                                        \
    MACRO(MACRO_96, "", "")                                                        \
    MACRO(MACRO_97, "", "")                                                        \
    MACRO(MACRO_98, "", "")                                                        \
    MACRO(MACRO_99, "", "")                                                        \
    MACRO(MACRO_100, "", "")                                                       \
    MACRO(MACRO_101, "", "")                                                       \
    MACRO(MACRO_102, "", "")                                                       \
    MACRO(MACRO_103, "", "")                                                       \
    MACRO(MACRO_104, "", "")                                                       \
    MACRO(MACRO_105, "", "")                                                       \
    MACRO(MACRO_106, "", "")                                                       \
    MACRO(MACRO_107, "", "")                                                       \
    MACRO(MACRO_108, "", "")                                                       \
    MACRO(MACRO_109, "", "")                                                       \
    MACRO(MACRO_110, "", "")                                                       \
    MACRO(MACRO_111, "", "")                                                       \
    MACRO(MACRO_112, "", "")                                                       \
    MACRO(MACRO_113, "", "")                                                       \
    MACRO(MACRO_114, "", "")                                                       \
    MACRO(MACRO_115, "", "")                                                       \
    MACRO(MACRO_116, "", "")                                                       \
    MACRO(MACRO_117, "", "")                                                       \
    MACRO(MACRO_118, "", "")                                                       \
    MACRO(MACRO_119, "", "")                                                       \
    MACRO(MACRO_120, "", "")                                                       \
    MACRO(MACRO_121, "", "")                                                       \
    MACRO(MACRO_122, "", "")                                                       \
    MACRO(MACRO_123, "", "")                                                       \
    MACRO(MACRO_124, "", "")                                                       \
    MACRO(MACRO_125, "", "")                                                       \
    MACRO(MACRO_126, "", "")                                                       \
    MACRO(MACRO_127, "", "")

// ─── Combos ─────────────────────────────────────────────────────────────────
//
// COMBOS(COMBO, COMBO_WINDOW) is the authored combo table.
// Each row is one of:
//   COMBO(output_keycode, (key_1, key_2, ...))
//   COMBO_WINDOW(output_keycode, (key_1, key_2, ...), window_ms)
// Group the key list in parentheses so 2-key and 3+-key combos read the same way.
// COMBO follows COMBO_TERM; COMBO_WINDOW gives that combo its own window.
//
// Valid combo outputs include plain keycodes, custom keys (CUSTOM_KEY_n),
// macros (MACRO_n), MO(layer) (the layer stays on while the combo is
// held), LOCK_LAYER(...), explicit pointing-mode lock keycodes such as
// PD_SLOT_4_LOCK, and keycodes that also have rows in key_behaviors[].
//
// If a combo emits a keycode that also has a row in key_behaviors[],
// that emitted key can reuse the same custom behavior handling.
//
// Runtime ownership still keeps one representative combo owner key for release
// matching, but RGB feedback and PD key-local rendering use the full
// physical combo footprint. Cross-half combos therefore broaden locality to
// both halves instead of guessing one side.
//
// Combo origin tracking follows the live resolved keycodes QMK sees, so the
// active profile's keymap stays authoritative. Keep each combo row's member
// keycodes unique so that footprint tracking can disambiguate the chord.
// Overlap-suppressed candidates are retired from feedback at the next compat
// reconciliation boundary; delayed legitimate outputs retain their exact
// physical footprint through QMK's final legal buffered-output opportunity.
//
// The default window is COMBO_TERM in config.h (currently 50 ms); the hold
// threshold is TAPPING_TERM. A live profile stores its own default window and
// hold threshold, set in Charybdis Ark.
#define COMBOS(COMBO, COMBO_WINDOW)                                                                       \
    COMBO(KC_TAB, (KC_D, LT(LAYER_NAV, KC_F)))                                                            \
    COMBO(CUSTOM_KEY_2, (PD_SLOT_5, MS_BTN3))                                                             \
    COMBO(KC_LGUI, (KC_N, KC_M))                                                                          \
    COMBO(G(KC_N), (KC_M, KC_COMM, KC_DOT, LT(LAYER_NAV, KC_SLSH)))                                       \
    COMBO(G(KC_N), (MS_BTN1, MS_BTN2, PD_SLOT_0, LT(LAYER_NAV, KC_SLSH)))                                 \
    COMBO(KC_LGUI, (MS_BTN1, PD_SLOT_1))                                                                  \
    COMBO(G(KC_A), (G(KC_C), G(KC_V)))                                                                    \
    COMBO(G(KC_T), (MS_BTN1, MS_BTN2, PD_SLOT_0))                                                         \
    COMBO(G(KC_T), (KC_M, KC_COMM, KC_DOT))                                                               \
    COMBO(MS_BTN6, (MS_BTN1, MS_BTN2))                                                                    \
    COMBO(MO(LAYER_EXTRA_1), (CUSTOM_KEY_1, KC_BSPC))                                                     \
    COMBO(MO(LAYER_NUM), (KC_SPC, KC_DEL))                                                                \
    COMBO(KC_ESC, (LT(LAYER_SYM, KC_Z), KC_X, KC_C, KC_V, KC_M, KC_COMM, KC_DOT, LT(LAYER_NAV, KC_SLSH))) \
    COMBO(MO(LAYER_EXTRA_1), (CUSTOM_KEY_0, KC_BSPC))                                                     \
    COMBO(CUSTOM_KEY_4, (KC_BSPC, KC_ENT))

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
//     the hold time of every dual-role key (LT()/MT()/OSM()/TT()/OSL()), with
//     or without a row here, when .tap_hold_term is omitted; the runtime
//     decides all of them (D-F27), and the live profile's dual-role setting
//     replaces it once live
//   - CUSTOM_TAP_HOLD_TERM = 150 ms
//     default first hold threshold for every other key_behavior row
//   - CUSTOM_LONGER_HOLD_TERM = 400 ms
//     default long hold threshold
//   - CUSTOM_MULTI_TAP_TERM = 150 ms
//     max gap allowed between taps in a multi-tap sequence
//   - KEY_BEHAVIOR_MAX_TAP_COUNT = 5
//     current runtime limit for tap_counts[] entries
//
// per-key timing overrides:
//   - .tap_hold_term overrides the first hold threshold for that row
//   - .longer_hold_term overrides the long hold threshold
//   - .multi_tap_term overrides the max gap between taps
//   - omit them (or leave them 0) to use the defaults above
//   - for LT()/MT()/OSM()/TT()/OSL() rows, omitted .tap_hold_term falls back
//     to TAPPING_TERM (or the live dual-role setting);
//     all other rows fall back to CUSTOM_TAP_HOLD_TERM
//
// per-key policy flags, omit them (or leave them false) for ordinary keys:
//   - .keeps_auto_mouse_anchored counts the row's keycode as a mouse record, so
//     pressing it keeps the pointer layer up instead of letting auto mouse
//     reset on it. Only keys that drive the mouse through authored actions need
//     it: mouse keycodes and pointing-mode keys already anchor on their own.
//     Without it, a TAP_SENDS(KC_TRNS) tier on the pointer layer finds no lower
//     layer to fall through to, because the press already took that layer down
//
// multi-tap behavior:
//   - if a key has higher tap_counts[] entries, lower tap counts wait one
//     multi-tap window before firing so the engine can see whether more taps
//     follow
//   - this means base single taps on multi-tap keys are delayed by
//     .multi_tap_term
//   - terminal tap branches wait that same window rather than firing on the
//     release, so every authored depth shows its branch color for the same
//     length of time before its action lands
//   - a quick run of taps past the deepest branch is typing: no branch fires,
//     and every press in the run sends the first tap once (D-F33). A held press
//     in it takes tap_counts[0]'s hold
//   - if a non-base branch omits .tap, a quick tap repeats the first tap, once
//     per press; the branch color still shows, but tap-commit feedback is
//     skipped because no .tap was authored on that branch
//
// within one tap index:
//   - .tap is the tap tier
//   - .hold is the normal hold tier
//   - .long_hold is the long-hold tier
// omit .tap to keep the key's normal tap behavior for that tap index
// if .tap is set but hold/long_hold are omitted, keys that already have a
// default held path keep using it. Stacked pointing-mode rows are contained: if
// a first-tap override can branch into another pointing mode on a later hold,
// the first pointing mode waits until the hold threshold instead of activating
// immediately. Other keycodes keep the tap override and send it on release
// .hold and .long_hold are independent: define either one by itself, or use
// both together for a two-stage hold
//
// RGB feedback follows authored tiers on the current tap index:
//   - every tap past the base one shows that branch's authored color for as long
//     as nothing more specific applies; another tap just renames it. Single taps
//     stay quiet. Any action state replaces it: a pending hold tier, the tap a
//     release would send, an active hold. So a branch with no .hold shows its tap
//     color once .multi_tap_term has run out, and a branch authoring no .tap keeps
//     the branch color until its hold threshold because it has nothing else to name
//   - an authored .hold tier can show hold-tier feedback while pending/active
//   - an authored .long_hold tier can show long-hold-tier feedback when it
//     commits or stays active
//   - a missing .hold tier stays visually quiet even if .long_hold exists
//   - primary momentary layer access uses the layer color itself rather than a
//     separate hold pulse
//
// action can be a plain keycode, a modded keycode, a macro, a layer lock,
// a pointing-mode lock, or a supported QMK behavior keycode such as MT()/OSM()
//   - Use LOCK_LAYER(layer) to toggle a layer lock.
//     Locking the same layer again turns it off; different layers toggle
//     independently and can stay active together.
//   - Use PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer)) for a momentary layer hold
//     owned by the custom runtime.
//   - TG(), TO() and OSL() work as taps, and TT() as
//     PRESS_AND_HOLD_UNTIL_RELEASE(TT(layer)). LM() and LT() are refused as
//     actions inside key_behaviors[]; LT() remains supported as the keycode
//     of a key_behavior row itself.
//   - Use the generated *_LOCK keycode to toggle a pointing-mode lock.
//     Activating the same mode again unlocks it; pressing, holding or locking
//     any other pointing-mode key also clears the previous lock.
//   - Use MACRO_n for a macro, as a key or as the action in any helper:
//     TAP_SENDS(MACRO_n), PRESS_AND_HOLD_UNTIL_RELEASE(MACRO_n).
//     Its default contents come from MACROS(MACRO) above.
//   - A custom key (CUSTOM_KEY_n) as an action sends only its shortcut, never
//     its own behaviour (D-F26). As a key it is the keycode of its own row,
//     placed on a layer or emitted by a combo.
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
//     and pointing-mode locks
//   TAP_SENDS(KC_TRNS)
//     keep the lower active layer's tap output at this physical key while the
//     authored row still owns this tap count's timing and branching
//
// hold-tier and long-hold-tier actions take one of four hold styles:
//   PRESS_AND_HOLD_UNTIL_RELEASE(action)
//     normal keys/modifiers:
//       cross .tap_hold_term -> press/register action
//       release the key -> unregister action
//     macros:
//       cross .tap_hold_term -> send action once immediately
//       release the key -> do nothing
//     use this for modifiers or keys you want to stay down while held;
//     macros use the same hold style for a held-triggered one-shot
//     PRESS_AND_HOLD_UNTIL_RELEASE(KC_TRNS) uses the lower active layer's
//     actual hold-tier behavior at the same key; if that lower key is a
//     momentary layer or pointing mode, its normal hold ownership comes through
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
//     use this when the key should stay quiet while held, or when a long
//     hold should still be able to replace the hold action
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
//     .longer_hold_term = 400, // overrides the default long hold threshold for this key
//     .multi_tap_term = 150, // overrides the default multi-tap threshold for this key
//     .tap_counts =
//         {
//             [0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_EXAMPLE))}, // tap index 0: normal tap, long-hold threshold action only
//             [1] = {.tap = TAP_SENDS(S(KC_EXAMPLE))}, // tap index 1: alternate tap output
//             [2] = {.tap = TAP_SENDS(KC_EXAMPLE), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXAMPLE)}, // tap index 2: tap once, or keep a held action active
//             [3] = {.tap = TAP_SENDS(EXAMPLE_MODE_LOCK), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_EXAMPLE))}, // tap index 3: pointing-mode lock on tap, shifted held action
//             [4] = {.tap = TAP_SENDS(MACRO_n), .hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_EXAMPLE)), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_EXAMPLE))}, // tap index 4: macro on tap, release action on hold, layer-lock threshold action on long hold
//         },
// }

const key_behavior_t
    key_behaviors[] =
        {
            // ─── Typing Keys ─────────────────────────────────────────────────────────────────
            {.keycode = KC_1, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXLM)}}},
            {.keycode = KC_2, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AT)}}},
            {.keycode = KC_3, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_HASH)}}},
            {.keycode = KC_4, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DLR)}}},
            {.keycode = KC_5, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PERC)}}},
            {.keycode = KC_6, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_CIRC)}}},
            {.keycode = KC_7, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_AMPR)}}},
            {.keycode = KC_8, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR)}}},
            {.keycode = KC_9, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LPRN)}}},
            {.keycode = KC_0, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RPRN)}}},
            {.keycode = KC_MINS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)}}},
            {.keycode = KC_BSLS, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_PIPE)}}},
            {.keycode = KC_SCLN, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_COLN)}}},
            {.keycode = KC_QUOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)}}},
            {.keycode = KC_COMM, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)}}},
            {.keycode = KC_DOT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)}}},
            {.keycode = KC_LBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCBR)}}},
            {.keycode = KC_RBRC, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_RCBR)}}},

            // ─── Editing / System Keys ───────────────────────────────────────────────────────
            {.keycode = KC_ESC, .tap_counts = {[0] = {.long_hold = TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))}, [1] = {.tap = TAP_SENDS(S(KC_GRV))}}},
            {.keycode = KC_LEFT_SHIFT, .tap_counts = {[0] = {.tap = TAP_SENDS(KC_CAPS)}}},
            {.keycode = KC_RIGHT_ALT, .tap_counts = {[0] = {.tap = TAP_SENDS(PD_SLOT_4_LOCK)}, [1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_6)}}},
            {.keycode = KC_ENT, .tap_counts = {[0] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))}}},
            {.keycode = KC_LEFT_GUI, .tap_counts = {[1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_ALT)}, [2] = {.tap = TAP_SENDS(OSM(MOD_LSFT))}}},

            // ─── Layer-Tap Keys ──────────────────────────────────────────────────────────────
            {.keycode = LT(LAYER_NAV, KC_F)},
            {.keycode = LT(LAYER_NAV, KC_SLSH), .tap_hold_term = 150, .multi_tap_term = 100},

            // ─── Navigation-Layer Overrides ─────────────────────────────────────────────────
            {.keycode = KC_LEFT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))}}},
            {.keycode = KC_RIGHT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))}}},
            {.keycode = G(KC_C), .tap_counts = {[1] = {.tap = TAP_SENDS(MACRO_10)}}},
            {.keycode = G(KC_V), .tap_counts = {[1] = {.tap = TAP_SENDS(MACRO_7)}}},

            // ─── Mouse-Button Keys ──────────────────────────────────────────────────────────
            {.keycode = MS_BTN3, .multi_tap_term = 100, .tap_hold_term = 100, .tap_counts = {[1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}}},

            // ─── Pointing-Mode Keys ──────────────────────────────────────────────────────────
            {.keycode = PD_SLOT_2, .tap_counts = {[0] = {.tap = TAP_SENDS(KC_TRNS)}}},
            {
                .keycode = PD_SLOT_5,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(KC_TRNS)},
                        [1] = {.tap = TAP_SENDS(MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_3)},
                    },
            },
            {.keycode = PD_SLOT_1, .tap_counts = {[0] = {.tap = TAP_SENDS(KC_TRNS)}, [1] = {.tap = TAP_SENDS(KC_MUTE)}}},

            // Dragscroll: single tap '.', hold = momentary, double-tap hold = lock.
            {
                .keycode = PD_SLOT_0,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(KC_TRNS)},
                        [1] = {.hold = TAP_AT_HOLD_THRESHOLD(PD_SLOT_0_LOCK)},
                    },
            },

            // ─── Custom Keys ─────────────────────────────────────────────────────────────────
            // Left Thumb
            {
                .keycode = CUSTOM_KEY_1,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_SYM)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))},
                        [1] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NUM)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_EXTRA_1)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_EXTRA_1))},
                        [3] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_EXTRA_2)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_EXTRA_2))},
                        [4] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_POINTER)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_POINTER))},
                    },
            },

            // Right Thumb
            {
                .keycode = CUSTOM_KEY_0,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            // Vorssaint Radial
            {
                .keycode                   = CUSTOM_KEY_3,
                .keeps_auto_mouse_anchored = true,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(KC_TRNS), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(LCAG(KC_SPC))},
                    },
            },

            // Click Spam
            {
                .keycode                   = CUSTOM_KEY_2,
                .tap_hold_term             = 1,
                .keeps_auto_mouse_anchored = true,
                .tap_counts =
                    {
                        [0] = {.hold = REPEAT_WHILE_HELD(MS_BTN1, 100)},
                    },
            },

};

// ─── Layer Names ────────────────────────────────────────────────────────────
//
// What Charybdis Ark calls each layer: at most 32 bytes of UTF-8 text. An
// empty name leaves the layer unnamed. A profile saved from Charybdis Ark
// keeps its own names.
const char layer_names[LAYER_COUNT][NOAH_LAYER_NAME_SIZE] = {
    [LAYER_BASE]    = "Base",
    [LAYER_NUM]     = "Number",
    [LAYER_SYM]     = "Symbol",
    [LAYER_NAV]     = "Navigation",
    [LAYER_POINTER] = "Pointing",
    [LAYER_EXTRA_1] = "Function",
    [LAYER_EXTRA_2] = "Game",
    [LAYER_EXTRA_3] = "Extra",
    [LAYER_EXTRA_4] = "",
    [LAYER_EXTRA_5] = "",
    [LAYER_EXTRA_6] = "",
    [LAYER_EXTRA_7] = "",
    [LAYER_EXTRA_8] = "",
    [LAYER_EXTRA_9] = "",
    [LAYER_EXTRA_10] = "",
    [LAYER_EXTRA_11] = "",
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
// The number row (KC_1-KC_0) and punctuation keys use the key behavior
// tables defined above; they are not using QMK's built-in mod-tap.
// See the key runtime modules under users/noah/lib/key/ for details.
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    // clang-format off
    [LAYER_BASE] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                KC_ESCAPE,              KC_1,              KC_2,              KC_3,              KC_4,              KC_5,                 KC_6,              KC_7,              KC_8,              KC_9,              KC_0,          KC_MINUS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                   KC_TAB,              KC_Q,              KC_W,              KC_E,              KC_R,              KC_T,                 KC_Y,              KC_U,              KC_I,              KC_O,              KC_P,      KC_BACKSLASH,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
           KC_LEFT_SHIFT,             KC_A,              KC_S,              KC_D,  LT(LAYER_NAV,KC_F),              KC_G,                KC_H,  LT(LAYER_SYM,KC_J),             KC_K,              KC_L,      KC_SEMICOLON,          KC_QUOTE,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_CTRL,  LT(LAYER_SYM,KC_Z),             KC_X,              KC_C,              KC_V,              KC_B,                KC_N,             KC_M,          KC_COMM,           KC_DOT,  LT(LAYER_NAV,KC_SLSH),     KC_RIGHT_ALT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                       KC_LEFT_GUI,          KC_SPACE,      CUSTOM_KEY_1,         CUSTOM_KEY_0,          KC_ENTER,
                                                                                            KC_DELETE,      KC_BACKSPACE,         KC_BACKSPACE
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NUM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                KC_ESCAPE,             KC_F1,             KC_F2,             KC_F3,             KC_F4,             KC_F5,                KC_F6,             KC_F7,             KC_F8,             KC_F9,            KC_F10,          KC_MINUS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           KC_KP_7,           KC_KP_8,           KC_KP_9,           _______,        KC_KP_PLUS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           _______,           _______,           _______,     MO(LAYER_NAV),           _______,              _______,           KC_KP_4,           KC_KP_5,           KC_KP_6,           _______,       KC_KP_EQUAL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
             KC_RIGHT_ALT,           _______,           _______,           _______,           _______,           _______,              _______,           KC_KP_1,           KC_KP_2,           KC_KP_3,           KC_COMM,            KC_DOT,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           KC_KP_0,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_SYM] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                KC_ESCAPE,           _______,           DPI_MOD,          DPI_RMOD,           S_D_MOD,          S_D_RMOD,              _______,           _______,           _______,           _______,           _______,          KC_MINUS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           KC_LPRN,           KC_RPRN,          KC_QUOTE,        KC_KP_PLUS,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,           _______,           _______,           _______,         KC_ESCAPE,           _______,              _______,           _______,   KC_LEFT_BRACKET,  KC_RIGHT_BRACKET,           KC_DQUO,       KC_KP_EQUAL,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
              MACRO_5,           _______,       MACRO_4,       MACRO_3,       MACRO_8,       MACRO_9,              _______,           _______,           KC_LCBR,           KC_RCBR,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NAV] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,       MACRO_7,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           G(KC_Q),           G(KC_W),      MACRO_11,           _______,           _______,          MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,      MACRO_16,           _______,      MACRO_12,           _______,           _______,          MACRO_1,           KC_LEFT,           KC_DOWN,          KC_RIGHT,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
              KC_LEFT_ALT,      MACRO_15,      MACRO_14,      MACRO_13,           _______,           _______,          MACRO_0,           MS_BTN1,           MS_BTN2,         PD_SLOT_0,           _______,           _______,
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
                  _______,           _______,           _______,           _______,           _______,           _______,            PD_SLOT_2,         PD_SLOT_5,           MS_BTN3,      CUSTOM_KEY_3,         PD_SLOT_7,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,            PD_SLOT_1,           MS_BTN1,           MS_BTN2,         PD_SLOT_0,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_EXTRA_1] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,             KC_F1,             KC_F2,             KC_F3,             KC_F4,             KC_F5,                KC_F6,             KC_F7,             KC_F8,             KC_F9,            KC_F10,            KC_F11,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,          MACRO_2,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              G(KC_U),           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,         MACRO_19,           G(KC_1),           G(KC_2),           G(KC_3),           G(KC_4),           G(KC_5),
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_EXTRA_2] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,              KC_Q,              KC_W,              KC_E,              KC_R,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,              KC_A,              KC_S,              KC_D,              KC_F,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_EXTRA_3] = LAYOUT(
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

    [LAYER_EXTRA_4] = LAYOUT(
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

    [LAYER_EXTRA_5] = LAYOUT(
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

    [LAYER_EXTRA_6] = LAYOUT(
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

    [LAYER_EXTRA_7] = LAYOUT(
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

    [LAYER_EXTRA_8] = LAYOUT(
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

    [LAYER_EXTRA_9] = LAYOUT(
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

    [LAYER_EXTRA_10] = LAYOUT(
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

    [LAYER_EXTRA_11] = LAYOUT(
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

// MACROS() above in the bank encoding. Regenerate it with
// NOAH_WRITE_AUTHORED_MACRO_BANK=1 sh tests/host/run_real_profile_validation_tests.sh.
#include "authored_macro_bank.h"

// Expand the authored macro tables, combo table, and derived counts above
// into the runtime symbols expected by the userspace runtime and QMK.
MATERIALIZE_KEYMAP_DATA();

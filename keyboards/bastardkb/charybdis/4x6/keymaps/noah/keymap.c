// ────────────────────────────────────────────────────────────────────────────
// Noah's Charybdis 4x6 keymap data
// ────────────────────────────────────────────────────────────────────────────
//
// This translation unit owns the authored keymap data:
//   - CUSTOM_KEYS(KEY), each custom key's name
//   - VIA_MACROS(MACRO), with each macro's name
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
// CUSTOM_KEY_0–63 are named keys that do only what their key_behaviors[] row
// says: place one on a layer or emit it from a combo. Without a row it does
// nothing, and a behaviour step cannot send one. All 64 slots are listed here
// so the slot limit stays visible in keymap.c.
// Each row is:
//   KEY(CUSTOM_KEY_n, "name")
// The name is what Charybdis Ark shows for the key: at most 20 printable
// ASCII characters. Use an empty string for an unused slot. A profile saved
// from Charybdis Ark keeps its own names.
#define CUSTOM_KEYS(KEY)             \
    KEY(CUSTOM_KEY_0, "Right Thumb") \
    KEY(CUSTOM_KEY_1, "Left Thumb")  \
    KEY(CUSTOM_KEY_2, "Click Spam")  \
    KEY(CUSTOM_KEY_3, "Drag Window") \
    KEY(CUSTOM_KEY_4, "")            \
    KEY(CUSTOM_KEY_5, "")            \
    KEY(CUSTOM_KEY_6, "")            \
    KEY(CUSTOM_KEY_7, "")            \
    KEY(CUSTOM_KEY_8, "")            \
    KEY(CUSTOM_KEY_9, "")            \
    KEY(CUSTOM_KEY_10, "")           \
    KEY(CUSTOM_KEY_11, "")           \
    KEY(CUSTOM_KEY_12, "")           \
    KEY(CUSTOM_KEY_13, "")           \
    KEY(CUSTOM_KEY_14, "")           \
    KEY(CUSTOM_KEY_15, "")           \
    KEY(CUSTOM_KEY_16, "")           \
    KEY(CUSTOM_KEY_17, "")           \
    KEY(CUSTOM_KEY_18, "")           \
    KEY(CUSTOM_KEY_19, "")           \
    KEY(CUSTOM_KEY_20, "")           \
    KEY(CUSTOM_KEY_21, "")           \
    KEY(CUSTOM_KEY_22, "")           \
    KEY(CUSTOM_KEY_23, "")           \
    KEY(CUSTOM_KEY_24, "")           \
    KEY(CUSTOM_KEY_25, "")           \
    KEY(CUSTOM_KEY_26, "")           \
    KEY(CUSTOM_KEY_27, "")           \
    KEY(CUSTOM_KEY_28, "")           \
    KEY(CUSTOM_KEY_29, "")           \
    KEY(CUSTOM_KEY_30, "")           \
    KEY(CUSTOM_KEY_31, "")           \
    KEY(CUSTOM_KEY_32, "")           \
    KEY(CUSTOM_KEY_33, "")           \
    KEY(CUSTOM_KEY_34, "")           \
    KEY(CUSTOM_KEY_35, "")           \
    KEY(CUSTOM_KEY_36, "")           \
    KEY(CUSTOM_KEY_37, "")           \
    KEY(CUSTOM_KEY_38, "")           \
    KEY(CUSTOM_KEY_39, "")           \
    KEY(CUSTOM_KEY_40, "")           \
    KEY(CUSTOM_KEY_41, "")           \
    KEY(CUSTOM_KEY_42, "")           \
    KEY(CUSTOM_KEY_43, "")           \
    KEY(CUSTOM_KEY_44, "")           \
    KEY(CUSTOM_KEY_45, "")           \
    KEY(CUSTOM_KEY_46, "")           \
    KEY(CUSTOM_KEY_47, "")           \
    KEY(CUSTOM_KEY_48, "")           \
    KEY(CUSTOM_KEY_49, "")           \
    KEY(CUSTOM_KEY_50, "")           \
    KEY(CUSTOM_KEY_51, "")           \
    KEY(CUSTOM_KEY_52, "")           \
    KEY(CUSTOM_KEY_53, "")           \
    KEY(CUSTOM_KEY_54, "")           \
    KEY(CUSTOM_KEY_55, "")           \
    KEY(CUSTOM_KEY_56, "")           \
    KEY(CUSTOM_KEY_57, "")           \
    KEY(CUSTOM_KEY_58, "")           \
    KEY(CUSTOM_KEY_59, "")           \
    KEY(CUSTOM_KEY_60, "")           \
    KEY(CUSTOM_KEY_61, "")           \
    KEY(CUSTOM_KEY_62, "")           \
    KEY(CUSTOM_KEY_63, "")

// ─── VIA Macros ─────────────────────────────────────────────────────────────
// VIA_MACRO_0–63 are the authored aliases for VIA's dynamic macro slots.
// All 64 slots are listed here so the slot limit stays visible in keymap.c.
// Each row is:
//   MACRO(VIA_MACRO_n, "name", "payload")
// The name is what Charybdis Ark shows for the macro: at most 20 printable
// ASCII characters. Use empty strings for an unused slot. The firmware seeds
// VIA's dynamic macro EEPROM defaults from the payloads on init/reset.
// Workflow: author defaults here -> compile/flash -> reset EEPROM to re-seed
// them. A profile saved from Charybdis Ark keeps its own names and payloads.
// Payload syntax:
//   - text {hello} sends "hello" one key at a time
//   - {KC_A} tap one key, {KC_LGUI,KC_SPC} tap a chord
//   - {+KC_LGUI} key down, {-KC_LGUI} key up
//   - {250} wait 250 ms before the next macro step
//     e.g. {KC_A}{250}{KC_B} pauses between A and B; other keys pressed
//     during the delay are queued
#define VIA_MACROS(MACRO)                                                        \
    MACRO(VIA_MACRO_0, "Spotlight", "{KC_LGUI,KC_SPC}")                          \
    MACRO(VIA_MACRO_1, "AI Chat", "{KC_LALT,KC_SPC}")                            \
    MACRO(VIA_MACRO_2, "Warp Terminal", "{KC_LALT,KC_LGUI,KC_SPC}")              \
    MACRO(VIA_MACRO_3, "OCR Copy", "{KC_LCTL,KC_LALT,KC_LGUI,KC_C}")             \
    MACRO(VIA_MACRO_4, "Drag Screenshot", "{KC_LCTL,KC_LALT,KC_LGUI,KC_X}")      \
    MACRO(VIA_MACRO_5, "Emoji", "{KC_LCTL,KC_LGUI,KC_SPC}")                      \
    MACRO(VIA_MACRO_6, "Zoom Screen", "{KC_LALT,KC_LGUI,KC_8}")                  \
    MACRO(VIA_MACRO_7, "Clipboard History", "{KC_LCTL,KC_LALT,KC_LGUI,KC_V}")    \
    MACRO(VIA_MACRO_8, "VS Code Preview MD", "{KC_LSFT,KC_LGUI,KC_V}")           \
    MACRO(VIA_MACRO_9, "VS Code Run Task", "{KC_LSFT,KC_LGUI,KC_P}")             \
    MACRO(VIA_MACRO_10, "Select All + Copy", "{KC_LGUI,KC_A}{50}{KC_LGUI,KC_C}") \
    MACRO(VIA_MACRO_11, "", "")                                                  \
    MACRO(VIA_MACRO_12, "", "")                                                  \
    MACRO(VIA_MACRO_13, "", "")                                                  \
    MACRO(VIA_MACRO_14, "", "")                                                  \
    MACRO(VIA_MACRO_15, "", "")                                                  \
    MACRO(VIA_MACRO_16, "", "")                                                  \
    MACRO(VIA_MACRO_17, "", "")                                                  \
    MACRO(VIA_MACRO_18, "", "")                                                  \
    MACRO(VIA_MACRO_19, "", "")                                                  \
    MACRO(VIA_MACRO_20, "", "")                                                  \
    MACRO(VIA_MACRO_21, "", "")                                                  \
    MACRO(VIA_MACRO_22, "", "")                                                  \
    MACRO(VIA_MACRO_23, "", "")                                                  \
    MACRO(VIA_MACRO_24, "", "")                                                  \
    MACRO(VIA_MACRO_25, "", "")                                                  \
    MACRO(VIA_MACRO_26, "", "")                                                  \
    MACRO(VIA_MACRO_27, "", "")                                                  \
    MACRO(VIA_MACRO_28, "", "")                                                  \
    MACRO(VIA_MACRO_29, "", "")                                                  \
    MACRO(VIA_MACRO_30, "", "")                                                  \
    MACRO(VIA_MACRO_31, "", "")                                                  \
    MACRO(VIA_MACRO_32, "", "")                                                  \
    MACRO(VIA_MACRO_33, "", "")                                                  \
    MACRO(VIA_MACRO_34, "", "")                                                  \
    MACRO(VIA_MACRO_35, "", "")                                                  \
    MACRO(VIA_MACRO_36, "", "")                                                  \
    MACRO(VIA_MACRO_37, "", "")                                                  \
    MACRO(VIA_MACRO_38, "", "")                                                  \
    MACRO(VIA_MACRO_39, "", "")                                                  \
    MACRO(VIA_MACRO_40, "", "")                                                  \
    MACRO(VIA_MACRO_41, "", "")                                                  \
    MACRO(VIA_MACRO_42, "", "")                                                  \
    MACRO(VIA_MACRO_43, "", "")                                                  \
    MACRO(VIA_MACRO_44, "", "")                                                  \
    MACRO(VIA_MACRO_45, "", "")                                                  \
    MACRO(VIA_MACRO_46, "", "")                                                  \
    MACRO(VIA_MACRO_47, "", "")                                                  \
    MACRO(VIA_MACRO_48, "", "")                                                  \
    MACRO(VIA_MACRO_49, "", "")                                                  \
    MACRO(VIA_MACRO_50, "", "")                                                  \
    MACRO(VIA_MACRO_51, "", "")                                                  \
    MACRO(VIA_MACRO_52, "", "")                                                  \
    MACRO(VIA_MACRO_53, "", "")                                                  \
    MACRO(VIA_MACRO_54, "", "")                                                  \
    MACRO(VIA_MACRO_55, "", "")                                                  \
    MACRO(VIA_MACRO_56, "", "")                                                  \
    MACRO(VIA_MACRO_57, "", "")                                                  \
    MACRO(VIA_MACRO_58, "", "")                                                  \
    MACRO(VIA_MACRO_59, "", "")                                                  \
    MACRO(VIA_MACRO_60, "", "")                                                  \
    MACRO(VIA_MACRO_61, "", "")                                                  \
    MACRO(VIA_MACRO_62, "", "")                                                  \
    MACRO(VIA_MACRO_63, "", "")

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
// VIA macros (VIA_MACRO_n), LOCK_LAYER(...), explicit pd-mode lock keycodes
// such as PD_SLOT_4_LOCK, and keycodes that also have rows in key_behaviors[].
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
// Overlap-suppressed candidates are retired from feedback at the next compat
// reconciliation boundary; delayed legitimate outputs retain their exact
// physical footprint through QMK's final legal buffered-output opportunity.
//
// The default window is COMBO_TERM in config.h (currently 50 ms); the hold
// threshold is TAPPING_TERM. A live profile stores its own default window and
// hold threshold, set in Charybdis Ark.
#define COMBOS(COMBO, COMBO_WINDOW)                                                   \
    COMBO(KC_TAB, (KC_D, LT(LAYER_NAV, KC_F)))                                        \
    COMBO(CUSTOM_KEY_2, (PD_SLOT_5, MS_BTN3))                                         \
    COMBO(KC_LGUI, (KC_N, KC_M))                                                      \
    COMBO_WINDOW(G(KC_N), (KC_M, KC_COMM, KC_DOT, LT(LAYER_NAV, KC_SLSH)), 100)       \
    COMBO_WINDOW(G(KC_N), (MS_BTN1, MS_BTN2, PD_SLOT_0, LT(LAYER_NAV, KC_SLSH)), 100) \
    COMBO(KC_LGUI, (MS_BTN1, PD_SLOT_1))                                              \
    COMBO(G(KC_A), (G(KC_C), G(KC_V)))                                                \
    COMBO(G(KC_T), (MS_BTN1, MS_BTN2, PD_SLOT_0))                                     \
    COMBO(G(KC_T), (KC_M, KC_COMM, KC_DOT))                                           \
    COMBO(MS_BTN6, (MS_BTN1, MS_BTN2))

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
//     used by built-in QMK LT()/MT()/TT()/OSL()/OSM() keys and by LT() rows
//     here when .tap_hold_term is omitted; the live profile's dual-role
//     setting replaces it once live
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
//   - for LT() rows, omitted .tap_hold_term falls back to TAPPING_TERM (or
//     the live dual-role setting);
//     all other rows fall back to CUSTOM_TAP_HOLD_TERM
//
// per-key policy flags, omit them (or leave them false) for ordinary keys:
//   - .keeps_auto_mouse_anchored counts the row's keycode as a mouse record, so
//     pressing it keeps the pointer layer up instead of letting auto mouse
//     reset on it. Only keys that drive the mouse through authored actions need
//     it: mouse keycodes and pointer-mode keys already anchor on their own.
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
//   - the tap index cycles through the authored branches: a four-branch row
//     answers a fifth tap with branch one again, a sixth with branch two, so any
//     run of taps resolves to exactly one action instead of a whole gesture plus
//     a partial one. Tapping cannot repeat an action inside one window
//   - if a non-base branch omits .tap, a quick tap keeps the key's normal tap
//     behavior; the branch color still shows, but tap-commit feedback is
//     skipped because no .tap was authored on that branch
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
//   - every tap past the base one shows that branch's authored color for as long
//     as nothing more specific applies; another tap just renames it. Single taps
//     stay quiet. Any action state replaces it: a pending hold tier, the tap a
//     release would send, an active hold. So a branch with no .hold shows its tap
//     color once .multi_tap_term has run out, and a branch authoring no .tap keeps
//     the branch color until its hold threshold because it has nothing else to name
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
//   - Use VIA_MACRO_n for a macro, as a key or as the action in any helper:
//     TAP_SENDS(VIA_MACRO_n), PRESS_AND_HOLD_UNTIL_RELEASE(VIA_MACRO_n).
//     Its default contents come from VIA_MACROS(MACRO) above.
//   - A custom key (CUSTOM_KEY_n) is not an action: it is the keycode of its
//     own row, placed on a layer or emitted by a combo.
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
            {.keycode = LT(LAYER_NAV, KC_SLSH), .tap_hold_term = 100, .tap_counts = {[1] = {.hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))}}},

            // ─── Navigation-Layer Overrides ─────────────────────────────────────────────────
            {.keycode = KC_LEFT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))}}},
            {.keycode = KC_RIGHT, .tap_counts = {[0] = {.hold = TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT)), .long_hold = TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))}}},
            {.keycode = G(KC_C), .tap_counts = {[1] = {.tap = TAP_SENDS(VIA_MACRO_10)}}},
            {.keycode = G(KC_V), .tap_counts = {[1] = {.tap = TAP_SENDS(VIA_MACRO_7)}}},

            // ─── Mouse-Button Keys ──────────────────────────────────────────────────────────
            {.keycode = MS_BTN3, .multi_tap_term = 100, .tap_hold_term = 100, .tap_counts = {[1] = {.hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN7)}}},

            // ─── Pointer-Mode Keys ──────────────────────────────────────────────────────────
            {.keycode = PD_SLOT_2, .tap_counts = {[0] = {.tap = TAP_SENDS(KC_TRNS)}}},
            {
                .keycode = PD_SLOT_5,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(KC_TRNS)},
                        [1] = {.tap = TAP_SENDS(VIA_MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(PD_SLOT_3)},
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
                .keycode       = CUSTOM_KEY_1,
                .tap_hold_term = 150,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_SYM)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))},
                        [1] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NUM)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_EXTRA_1)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_EXTRA_1))},
                        [3] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_EXTRA_2)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_EXTRA_2))},
                    },
            },

            // Right Thumb
            {
                .keycode       = CUSTOM_KEY_0,
                .tap_hold_term = 150,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV)), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))},
                        [1] = {.tap = TAP_SENDS(KC_MPLY), .hold = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE), .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))},
                        [2] = {.tap = TAP_SENDS(KC_MNXT), .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)},
                        [3] = {.tap = TAP_SENDS(KC_MPRV), .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)},
                    },
            },

            // Drag Window
            {
                .keycode                   = CUSTOM_KEY_3,
                .multi_tap_term            = 100,
                .tap_hold_term             = 100,
                .keeps_auto_mouse_anchored = true,
                .tap_counts =
                    {
                        [0] = {.tap = TAP_SENDS(KC_TRNS), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MS_BTN6)},
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
// What Charybdis Ark calls each layer: at most 23 bytes of UTF-8 text. A
// profile saved from Charybdis Ark keeps its own names.
const char layer_names[LAYER_COUNT][NOAH_LAYER_NAME_SIZE] = {
    [LAYER_BASE]    = "Base",
    [LAYER_NUM]     = "Number",
    [LAYER_SYM]     = "Symbol",
    [LAYER_NAV]     = "Navigation",
    [LAYER_POINTER] = "Pointing",
    [LAYER_EXTRA_1] = "Function",
    [LAYER_EXTRA_2] = "Game",
    [LAYER_EXTRA_3] = "Extra",
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
              VIA_MACRO_5,           _______,       VIA_MACRO_4,       VIA_MACRO_3,       VIA_MACRO_8,       VIA_MACRO_9,              _______,           _______,           KC_LCBR,           KC_RCBR,           _______,           _______,
  // ╰───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╯
                                                                           _______,           _______,           _______,              _______,           _______,
                                                                                              _______,           _______,              _______
  //                                                                ╰────────────────────────────────────────────────────╯ ╰────────────────────────────────────────────────────╯
    ),

    [LAYER_NAV] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,       VIA_MACRO_7,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           G(KC_Q),           G(KC_W),           G(KC_A),           _______,           _______,          VIA_MACRO_2,           G(KC_C),             KC_UP,           G(KC_V),           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
            KC_LEFT_SHIFT,        S(G(KC_Z)),           _______,           G(KC_C),           _______,           _______,          VIA_MACRO_1,           KC_LEFT,           KC_DOWN,          KC_RIGHT,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
              KC_LEFT_ALT,           G(KC_Z),           G(KC_X),           G(KC_V),           _______,           _______,          VIA_MACRO_0,           MS_BTN1,           MS_BTN2,         PD_SLOT_0,           _______,           _______,
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
                  _______,           _______,           _______,           _______,           _______,           _______,            PD_SLOT_2,         PD_SLOT_5,           MS_BTN3,      CUSTOM_KEY_3,           _______,           _______,
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

    [LAYER_EXTRA_2] = LAYOUT(
  // ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮ ╭───────────────────────────────────────────────────────────────────────────────────────────────────────────────────╮
                  _______,           _______,           _______,           _______,           _______,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,              KC_Q,              KC_W,              KC_E,              KC_R,              _______,           _______,           _______,           _______,           _______,           _______,
  // ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤ ├───────────────────────────────────────────────────────────────────────────────────────────────────────────────────┤
                  _______,           _______,              KC_A,              KC_S,              KC_D,           _______,              _______,           _______,           _______,           _______,           _______,           _______,
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
    // clang-format on
};

// Expand the authored macro tables, combo table, and derived counts above
// into the runtime symbols expected by the userspace runtime and QMK.
MATERIALIZE_KEYMAP_DATA();

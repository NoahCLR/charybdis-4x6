#pragma once

// Expand the authored data tables in keymap.c into the runtime symbols
// consumed by the userspace runtime and QMK combo engine.

#define _KEYMAP_CONCAT_INNER(a_, b_) a_##b_
#define _KEYMAP_CONCAT(a_, b_) _KEYMAP_CONCAT_INNER(a_, b_)
#define _KEYMAP_VIA_MACRO_PAYLOAD_ENTRY(keycode_, payload_) [((keycode_) - VIA_MACRO_0)] = (payload_),
#define _KEYMAP_HARDCODED_MACRO_PAYLOAD_ENTRY(keycode_, payload_) [((keycode_) - MACRO_0)] = (payload_),
#define _KEYMAP_COMBO_OUTPUT_ENTRY(result_, keys_) (result_),
#define _KEYMAP_STRIP_PARENS(...) __VA_ARGS__
#define _KEYMAP_COMBO_BIND_DEF(result_, keys_) COMBO(((const uint16_t[]){_KEYMAP_STRIP_PARENS keys_, COMBO_END}), result_),

#ifdef COMBO_ENABLE
#    define _KEYMAP_COMBO_DATA() combo_t key_combos[] = {COMBOS(_KEYMAP_COMBO_BIND_DEF)};
#    define _KEYMAP_COMBO_OUTPUT_DATA()                                                                                         \
        static const uint16_t noah_combo_output_keycodes_data[] = {COMBOS(_KEYMAP_COMBO_OUTPUT_ENTRY) KC_NO};                 \
        const uint16_t *const noah_combo_output_keycodes        = noah_combo_output_keycodes_data;                            \
        const uint8_t        noah_combo_output_count            = (uint8_t)((sizeof(noah_combo_output_keycodes_data) /        \
                                                                  sizeof(noah_combo_output_keycodes_data[0])) - 1u)
#else
#    define _KEYMAP_COMBO_DATA()
#    define _KEYMAP_COMBO_OUTPUT_DATA()          \
        const uint16_t *const noah_combo_output_keycodes = 0; \
        const uint8_t        noah_combo_output_count     = 0
#endif

#define MATERIALIZE_KEYMAP_DATA()                                                                                                       \
    const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT]             = {VIA_MACROS(_KEYMAP_VIA_MACRO_PAYLOAD_ENTRY)};             \
    const char *const hardcoded_macro_payloads[HARDCODED_MACRO_SLOT_COUNT] = {HARDCODED_MACROS(_KEYMAP_HARDCODED_MACRO_PAYLOAD_ENTRY)}; \
    _KEYMAP_COMBO_DATA()                                                                                                                \
    _KEYMAP_COMBO_OUTPUT_DATA();                                                                                                       \
    const uint8_t key_behavior_count = sizeof(key_behaviors) / sizeof(key_behaviors[0])

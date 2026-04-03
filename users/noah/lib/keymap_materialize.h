#pragma once

// Expand the authored data tables in keymap.c into the runtime symbols
// consumed by the userspace runtime and QMK combo engine.

#define _KEYMAP_CONCAT_INNER(a_, b_) a_##b_
#define _KEYMAP_CONCAT(a_, b_) _KEYMAP_CONCAT_INNER(a_, b_)
#define _KEYMAP_COMBO_KEYS_NAME(line_) _KEYMAP_CONCAT(combo_keys_, line_)
#define _KEYMAP_VIA_MACRO_PAYLOAD_ENTRY(keycode_, payload_) [((keycode_) - VIA_MACRO_0)] = (payload_),
#define _KEYMAP_HARDCODED_MACRO_PAYLOAD_ENTRY(keycode_, payload_) [((keycode_) - MACRO_0)] = (payload_),
#define _KEYMAP_STRIP_PARENS(...) __VA_ARGS__
#define _KEYMAP_COMBO_KEYS_DEF(result_, keys_) const uint16_t PROGMEM _KEYMAP_COMBO_KEYS_NAME(__LINE__)[] = {_KEYMAP_STRIP_PARENS keys_, COMBO_END};
#define _KEYMAP_COMBO_BIND_DEF(result_, keys_) COMBO(_KEYMAP_COMBO_KEYS_NAME(__LINE__), result_),

#define MATERIALIZE_KEYMAP_DATA()                                                \
    const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT] = {               \
        VIA_MACROS(_KEYMAP_VIA_MACRO_PAYLOAD_ENTRY)                              \
    };                                                                           \
    const char *const hardcoded_macro_payloads[HARDCODED_MACRO_SLOT_COUNT] = {   \
        HARDCODED_MACROS(_KEYMAP_HARDCODED_MACRO_PAYLOAD_ENTRY)                  \
    };                                                                           \
    COMBOS(_KEYMAP_COMBO_KEYS_DEF)                                               \
    combo_t key_combos[] = {                                                     \
        COMBOS(_KEYMAP_COMBO_BIND_DEF)                                           \
    };                                                                           \
    const uint8_t key_behavior_count = sizeof(key_behaviors) / sizeof(key_behaviors[0])

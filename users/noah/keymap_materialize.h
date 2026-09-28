#pragma once

// Expand the authored data tables in keymap.c into the runtime symbols
// consumed by the userspace runtime and QMK combo engine.

#define _KEYMAP_CONCAT_INNER(a_, b_) a_##b_
#define _KEYMAP_CONCAT(a_, b_) _KEYMAP_CONCAT_INNER(a_, b_)
#define _KEYMAP_VIA_MACRO_PAYLOAD_ENTRY(keycode_, name_, payload_) [((keycode_) - VIA_MACRO_0)] = (payload_),
#define _KEYMAP_VIA_MACRO_NAME_ENTRY(keycode_, name_, payload_) [((keycode_) - VIA_MACRO_0)] = name_,
#define _KEYMAP_VIA_MACRO_NAME_CHECK(keycode_, name_, payload_) _Static_assert(sizeof(name_) <= NOAH_MACRO_NAME_SIZE, #keycode_ ": a macro name holds at most 20 characters");
#define _KEYMAP_CUSTOM_KEY_NAME_ENTRY(keycode_, name_) [((keycode_) - CUSTOM_KEY_0)] = name_,
#define _KEYMAP_CUSTOM_KEY_NAME_CHECK(keycode_, name_)                                                                         \
    _Static_assert(NOAH_KEYCODE_IS_CUSTOM_KEY(keycode_), #keycode_ ": CUSTOM_KEYS rows name custom keys");                      \
    _Static_assert(sizeof(name_) <= NOAH_MACRO_NAME_SIZE, #keycode_ ": a custom key name holds at most 20 characters");
#define _KEYMAP_STRIP_PARENS(...) __VA_ARGS__
// COMBO(result, keys) follows COMBO_TERM; COMBO_WINDOW(result, keys, ms) has
// its own window.
#define _KEYMAP_COMBO_BIND_DEF(result_, keys_) COMBO(((const uint16_t[]){_KEYMAP_STRIP_PARENS keys_, COMBO_END}), result_),
#define _KEYMAP_COMBO_WINDOW_BIND_DEF(result_, keys_, term_ms_) _KEYMAP_COMBO_BIND_DEF(result_, keys_)
#define _KEYMAP_COMBO_OUTPUT_ENTRY(result_, keys_) (result_),
#define _KEYMAP_COMBO_WINDOW_OUTPUT_ENTRY(result_, keys_, term_ms_) (result_),
#define _KEYMAP_COMBO_TERM_ENTRY(result_, keys_) 0u,
#define _KEYMAP_COMBO_WINDOW_TERM_ENTRY(result_, keys_, term_ms_) (term_ms_),
#define _KEYMAP_COMBO_NO_CHECK(result_, keys_)
#define _KEYMAP_COMBO_WINDOW_CHECK(result_, keys_, term_ms_) _Static_assert((term_ms_) > 0 && (term_ms_) <= UINT16_MAX, "COMBO_WINDOW takes its window in ms; COMBO follows COMBO_TERM");

#if defined(COMBO_ENABLE) && defined(NOAH_KEYMAP_EMPTY_COMBOS)
#    if __INCLUDE_LEVEL__ == 0
#        define _KEYMAP_EMPTY_COMBO_COUNT_OVERRIDE() \
            uint16_t combo_count(void) {             \
                return 0;                            \
            }
#    else
#        define _KEYMAP_EMPTY_COMBO_COUNT_OVERRIDE()
#    endif
#    define _KEYMAP_COMBO_DATA()                     \
        combo_t        key_combos[1]       = {{0}}; \
        const uint16_t noah_combo_terms[1] = {0};   \
        _KEYMAP_EMPTY_COMBO_COUNT_OVERRIDE()
#    define _KEYMAP_COMBO_COUNT_DATA() const uint8_t noah_combo_count = 0
#    define _KEYMAP_COMBO_OUTPUT_DATA()                       \
        const uint16_t *const noah_combo_output_keycodes = 0; \
        const uint8_t         noah_combo_output_count    = 0
#elif defined(COMBO_ENABLE)
#    define _KEYMAP_COMBO_DATA()                                                                                 \
        COMBOS(_KEYMAP_COMBO_NO_CHECK, _KEYMAP_COMBO_WINDOW_CHECK)                                               \
        combo_t        key_combos[]       = {COMBOS(_KEYMAP_COMBO_BIND_DEF, _KEYMAP_COMBO_WINDOW_BIND_DEF)};     \
        const uint16_t noah_combo_terms[] = {COMBOS(_KEYMAP_COMBO_TERM_ENTRY, _KEYMAP_COMBO_WINDOW_TERM_ENTRY)};
#    define _KEYMAP_COMBO_COUNT_DATA() const uint8_t noah_combo_count = (uint8_t)(sizeof(key_combos) / sizeof(key_combos[0]))
#    define _KEYMAP_COMBO_OUTPUT_DATA()                                                                                                          \
        static const uint16_t noah_combo_output_keycodes_data[] = {COMBOS(_KEYMAP_COMBO_OUTPUT_ENTRY, _KEYMAP_COMBO_WINDOW_OUTPUT_ENTRY) KC_NO}; \
        const uint16_t *const noah_combo_output_keycodes        = noah_combo_output_keycodes_data;                                               \
        const uint8_t         noah_combo_output_count           = (uint8_t)((sizeof(noah_combo_output_keycodes_data) / sizeof(noah_combo_output_keycodes_data[0])) - 1u)
#else
#    define _KEYMAP_COMBO_DATA()
#    define _KEYMAP_COMBO_COUNT_DATA() const uint8_t noah_combo_count = 0
#    define _KEYMAP_COMBO_OUTPUT_DATA()                       \
        const uint16_t *const noah_combo_output_keycodes = 0; \
        const uint8_t         noah_combo_output_count    = 0
#endif

#ifdef NOAH_KEYMAP_EMPTY_KEY_BEHAVIORS
#    define _KEYMAP_KEY_BEHAVIOR_COUNT_DATA() const uint8_t key_behavior_count = 0
#else
#    define _KEYMAP_KEY_BEHAVIOR_COUNT_DATA() const uint8_t key_behavior_count = sizeof(key_behaviors) / sizeof(key_behaviors[0])
#endif

#define MATERIALIZE_KEYMAP_DATA()                                                                                               \
    CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_CHECK)                                                                                  \
    const char custom_key_names[CUSTOM_KEY_SLOT_COUNT][NOAH_MACRO_NAME_SIZE] = {CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_ENTRY)};     \
    VIA_MACROS(_KEYMAP_VIA_MACRO_NAME_CHECK)                                                                                    \
    const char *const via_macro_payloads[VIA_MACRO_SLOT_COUNT]                 = {VIA_MACROS(_KEYMAP_VIA_MACRO_PAYLOAD_ENTRY)}; \
    const char        via_macro_names[VIA_MACRO_SLOT_COUNT][NOAH_MACRO_NAME_SIZE] = {VIA_MACROS(_KEYMAP_VIA_MACRO_NAME_ENTRY)}; \
    _KEYMAP_COMBO_DATA()                                                                                                        \
    _KEYMAP_COMBO_COUNT_DATA();                                                                                                 \
    _KEYMAP_COMBO_OUTPUT_DATA();                                                                                                \
    _KEYMAP_KEY_BEHAVIOR_COUNT_DATA()

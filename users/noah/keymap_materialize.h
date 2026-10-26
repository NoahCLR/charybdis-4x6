#pragma once

// Expand the authored data tables in keymap.c into the runtime symbols
// consumed by the userspace runtime and QMK combo engine.

#define _KEYMAP_CONCAT_INNER(a_, b_) a_##b_
#define _KEYMAP_CONCAT(a_, b_) _KEYMAP_CONCAT_INNER(a_, b_)
#define _KEYMAP_MACRO_PAYLOAD_ENTRY(keycode_, name_, payload_) [((keycode_) - MACRO_0)] = (payload_),
#define _KEYMAP_MACRO_NAME_ENTRY(keycode_, name_, payload_) [((keycode_) - MACRO_0)] = name_,
#define _KEYMAP_MACRO_NAME_CHECK(keycode_, name_, payload_) _Static_assert(sizeof(name_) <= NOAH_MACRO_NAME_SIZE, #keycode_ ": a macro name holds at most 32 bytes of UTF-8");
#define _KEYMAP_CUSTOM_KEY_NAME_ENTRY(keycode_, name_) [((keycode_) - CUSTOM_KEY_0)] = name_,
#define _KEYMAP_CUSTOM_KEY_NAME_CHECK(keycode_, name_)                                                                         \
    _Static_assert(NOAH_KEYCODE_IS_CUSTOM_KEY(keycode_), #keycode_ ": CUSTOM_KEYS rows name custom keys");                      \
    _Static_assert(sizeof(name_) <= NOAH_MACRO_NAME_SIZE, #keycode_ ": a custom key name holds at most 32 bytes of UTF-8");
// SHORTCUT(keycode, name, mods, key, layers) also gives the key its shortcut
// (D-F26): modifiers written as SHORTCUT_MODS(...) or SHORTCUT_NO_MODS, a key
// from the shortcut vocabulary, then the layers it turns on while held,
// written as SHORTCUT_LAYERS(...) or SHORTCUT_NO_LAYERS. SHORTCUT_MODS names
// modifier keys, left and right mixed, and builds the 8-bit report mask from
// them. A bare number or a QMK MOD_* constant is refused: QMK's right-hand
// MOD_RALT (0x14) reads as Right Ctrl + Left Alt in a report mask, so it would
// compile to the wrong keys. SHORTCUT_LAYERS names layers 1-15 of the bank's
// sixteen; the base layer is always on, and so is refused, as is a bare mask.
// It holds them as MO() does; SHORTCUT_LOCK_LAYERS, SHORTCUT_MOVE_TO_LAYERS,
// SHORTCUT_TAP_TOGGLE_LAYERS and SHORTCUT_ONESHOT_LAYERS name them the same way
// and act on them as TG(), TO(), TT() and OSL() do.
#define _KEYMAP_SHORTCUT_MODS_TAG 0x100
#define _KEYMAP_SHORTCUT_NOT_A_MOD 0x200
#define _KEYMAP_SHORTCUT_MOD_BIT(key_) (IS_MODIFIER_KEYCODE(key_) ? MOD_BIT(key_) : _KEYMAP_SHORTCUT_NOT_A_MOD)
#define _KEYMAP_SHORTCUT_MODS_1(a_) _KEYMAP_SHORTCUT_MOD_BIT(a_)
#define _KEYMAP_SHORTCUT_MODS_2(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_1(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_3(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_2(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_4(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_3(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_5(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_4(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_6(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_5(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_7(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_6(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_8(a_, ...) (_KEYMAP_SHORTCUT_MOD_BIT(a_) | _KEYMAP_SHORTCUT_MODS_7(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_MODS_PICK(a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, count_, ...) _KEYMAP_SHORTCUT_MODS_##count_
#define SHORTCUT_MODS(...) (_KEYMAP_SHORTCUT_MODS_TAG | _KEYMAP_SHORTCUT_MODS_PICK(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__))
#define SHORTCUT_NO_MODS _KEYMAP_SHORTCUT_MODS_TAG
#define _KEYMAP_SHORTCUT_LAYERS_TAG 0x10000L
#define _KEYMAP_SHORTCUT_NOT_A_LAYER 0x20000L
#define _KEYMAP_SHORTCUT_LAYER_BIT(layer_) ((layer_) > 0 && (layer_) < NOAH_CUSTOM_KEY_SHORTCUT_LAYERS ? (1L << ((layer_) & (NOAH_CUSTOM_KEY_SHORTCUT_LAYERS - 1))) : _KEYMAP_SHORTCUT_NOT_A_LAYER)
#define _KEYMAP_SHORTCUT_LAYERS_1(a_) _KEYMAP_SHORTCUT_LAYER_BIT(a_)
#define _KEYMAP_SHORTCUT_LAYERS_2(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_1(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_3(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_2(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_4(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_3(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_5(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_4(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_6(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_5(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_7(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_6(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_8(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_7(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_9(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_8(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_10(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_9(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_11(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_10(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_12(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_11(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_13(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_12(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_14(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_13(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_15(a_, ...) (_KEYMAP_SHORTCUT_LAYER_BIT(a_) | _KEYMAP_SHORTCUT_LAYERS_14(__VA_ARGS__))
#define _KEYMAP_SHORTCUT_LAYERS_PICK(a1_, a2_, a3_, a4_, a5_, a6_, a7_, a8_, a9_, a10_, a11_, a12_, a13_, a14_, a15_, count_, ...) _KEYMAP_SHORTCUT_LAYERS_##count_
// The layer kind rides above the tag, in bits 20-22, and its complement in
// bits 24-26, so two forms OR'ed together, or a bare mask, never read as one.
#define _KEYMAP_SHORTCUT_KIND_SHIFT 20
#define _KEYMAP_SHORTCUT_KIND_CHECK_SHIFT 24
#define _KEYMAP_SHORTCUT_KIND_BITS(kind_) (((long)(kind_) << _KEYMAP_SHORTCUT_KIND_SHIFT) | ((long)(~(kind_) & 0x7) << _KEYMAP_SHORTCUT_KIND_CHECK_SHIFT))
#define _KEYMAP_SHORTCUT_KIND_LAYERS(kind_, ...) (_KEYMAP_SHORTCUT_LAYERS_TAG | _KEYMAP_SHORTCUT_KIND_BITS(kind_) | _KEYMAP_SHORTCUT_LAYERS_PICK(__VA_ARGS__, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)(__VA_ARGS__))
#define SHORTCUT_LAYERS(...) _KEYMAP_SHORTCUT_KIND_LAYERS(NOAH_SHORTCUT_LAYERS_HOLD, __VA_ARGS__)
#define SHORTCUT_LOCK_LAYERS(...) _KEYMAP_SHORTCUT_KIND_LAYERS(NOAH_SHORTCUT_LAYERS_LOCK, __VA_ARGS__)
#define SHORTCUT_MOVE_TO_LAYERS(...) _KEYMAP_SHORTCUT_KIND_LAYERS(NOAH_SHORTCUT_LAYERS_MOVE, __VA_ARGS__)
#define SHORTCUT_TAP_TOGGLE_LAYERS(...) _KEYMAP_SHORTCUT_KIND_LAYERS(NOAH_SHORTCUT_LAYERS_TAP_TOGGLE, __VA_ARGS__)
#define SHORTCUT_ONESHOT_LAYERS(...) _KEYMAP_SHORTCUT_KIND_LAYERS(NOAH_SHORTCUT_LAYERS_ONESHOT, __VA_ARGS__)
#define SHORTCUT_NO_LAYERS (_KEYMAP_SHORTCUT_LAYERS_TAG | _KEYMAP_SHORTCUT_KIND_BITS(NOAH_SHORTCUT_LAYERS_HOLD))
#define _KEYMAP_SHORTCUT_KIND_OF(layers_) ((uint8_t)(((layers_) >> _KEYMAP_SHORTCUT_KIND_SHIFT) & 0x7))
#define _KEYMAP_SHORTCUT_KIND_WELL_FORMED(layers_) ((((layers_) >> _KEYMAP_SHORTCUT_KIND_CHECK_SHIFT) & 0x7) == (~((layers_) >> _KEYMAP_SHORTCUT_KIND_SHIFT) & 0x7))
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_NAME_ENTRY(keycode_, name_, mods_, key_, layers_) _KEYMAP_CUSTOM_KEY_NAME_ENTRY(keycode_, name_)
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_CHECK(keycode_, name_, mods_, key_, layers_)                                                \
    _KEYMAP_CUSTOM_KEY_NAME_CHECK(keycode_, name_)                                                                              \
    _Static_assert(((mods_) & ~0xff) == _KEYMAP_SHORTCUT_MODS_TAG,                                                              \
                   #keycode_ ": write a shortcut's modifiers as SHORTCUT_MODS(KC_LCTL, ...) of modifier keys, or SHORTCUT_NO_MODS"); \
    _Static_assert(NOAH_CUSTOM_KEY_SHORTCUT_KEY_VALID(key_), #keycode_ ": a shortcut's key is a basic key or a mouse button");   \
    _Static_assert(((layers_) & ~(0xffffL | (0x7L << _KEYMAP_SHORTCUT_KIND_SHIFT) | (0x7L << _KEYMAP_SHORTCUT_KIND_CHECK_SHIFT))) == _KEYMAP_SHORTCUT_LAYERS_TAG && _KEYMAP_SHORTCUT_KIND_WELL_FORMED(layers_) && _KEYMAP_SHORTCUT_KIND_OF(layers_) < NOAH_SHORTCUT_LAYER_KIND_COUNT, \
                   #keycode_ ": write a shortcut's layers as SHORTCUT_LAYERS(...) of layers 1-15 (or the LOCK, MOVE_TO, TAP_TOGGLE or ONESHOT form), or SHORTCUT_NO_LAYERS"); \
    _Static_assert(((mods_) & 0xff) != 0 || (key_) != KC_NO || ((layers_) & 0xffff) != 0, #keycode_ ": a SHORTCUT row holds a shortcut; use KEY for none"); \
    _Static_assert(_KEYMAP_SHORTCUT_KIND_OF(layers_) == NOAH_SHORTCUT_LAYERS_HOLD || ((layers_) & 0xffff) != 0, #keycode_ ": a layer kind needs layers");
#define _KEYMAP_CUSTOM_KEY_NO_SHORTCUT(keycode_, name_) [((keycode_) - CUSTOM_KEY_0)] = {0u},
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_ENTRY(keycode_, name_, mods_, key_, layers_) [((keycode_) - CUSTOM_KEY_0)] = {(uint8_t)(mods_), (uint8_t)(key_), (uint8_t)(layers_), (uint8_t)((layers_) >> 8), _KEYMAP_SHORTCUT_KIND_OF(layers_)},
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
    _Static_assert(sizeof(keymaps) / sizeof(keymaps[0]) == LAYER_COUNT, "keymaps[] lists every layer of the bank, transparent or not"); \
    CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_CHECK, _KEYMAP_CUSTOM_KEY_SHORTCUT_CHECK)                                                \
    const char custom_key_names[CUSTOM_KEY_SLOT_COUNT][NOAH_MACRO_NAME_SIZE] = {CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NAME_ENTRY, _KEYMAP_CUSTOM_KEY_SHORTCUT_NAME_ENTRY)}; \
    const uint8_t custom_key_shortcuts[CUSTOM_KEY_SLOT_COUNT][NOAH_CUSTOM_KEY_SHORTCUT_SIZE] = {CUSTOM_KEYS(_KEYMAP_CUSTOM_KEY_NO_SHORTCUT, _KEYMAP_CUSTOM_KEY_SHORTCUT_ENTRY)}; \
    MACROS(_KEYMAP_MACRO_NAME_CHECK)                                                                                            \
    const char *const macro_payloads[MACRO_SLOT_COUNT]                      = {MACROS(_KEYMAP_MACRO_PAYLOAD_ENTRY)};            \
    const char        macro_names[MACRO_SLOT_COUNT][NOAH_MACRO_NAME_SIZE]   = {MACROS(_KEYMAP_MACRO_NAME_ENTRY)};               \
    const uint8_t     authored_macro_bank[]                                 = {NOAH_AUTHORED_MACRO_BANK};        \
    const uint16_t    authored_macro_bank_size                              = sizeof(authored_macro_bank);      \
    _KEYMAP_COMBO_DATA()                                                                                                        \
    _KEYMAP_COMBO_COUNT_DATA();                                                                                                 \
    _KEYMAP_COMBO_OUTPUT_DATA();                                                                                                \
    _KEYMAP_KEY_BEHAVIOR_COUNT_DATA()

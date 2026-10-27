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
// SHORTCUT(keycode, name, mods, keys, layers) also gives the key its shortcut
// (D-F26): modifiers written as SHORTCUT_MODS(...) or SHORTCUT_NO_MODS, up to
// six keys from the shortcut vocabulary written as SHORTCUT_KEYS(...) in the
// order they are pressed, or SHORTCUT_NO_KEYS, then the layers it turns on
// while held, written as SHORTCUT_LAYERS(...) or SHORTCUT_NO_LAYERS. SHORTCUT_MODS names
// modifier keys, left and right mixed, and builds the 8-bit report mask from
// them. A bare number or a QMK MOD_* constant is refused: QMK's right-hand
// MOD_RALT (0x14) reads as Right Ctrl + Left Alt in a report mask, so it would
// compile to the wrong keys. SHORTCUT_LAYERS names layers 1-15 of the bank's
// sixteen; the base layer is always on, and so is refused, as is a bare mask.
// It holds them as MO() does; SHORTCUT_LOCK_LAYERS, SHORTCUT_MOVE_TO_LAYERS,
// SHORTCUT_TAP_TOGGLE_LAYERS and SHORTCUT_ONESHOT_LAYERS name them the same way
// and act on them as TG(), TO(), TT() and OSL() do. SHORTCUT_KEYS refuses a
// bare key, KC_NO, a seventh key, the same key twice, a second system key, a
// second media key, and two of its forms combined.
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
// The keys ride one byte each in bits 0-47, below the tag; a code that is no
// shortcut key sets a bit the check refuses. Bits 49-56 seal the form with the
// complement of its keys' sum, so two forms OR'ed together, or a bare number,
// never read as one: OR'ing two seals sets bits the complement of the OR'ed
// keys' sum lacks.
#define _KEYMAP_SHORTCUT_KEYS_TAG (1ULL << 60)
#define _KEYMAP_SHORTCUT_NOT_A_KEY (1ULL << 57)
#define _KEYMAP_SHORTCUT_KEYS_SEAL_SHIFT 49
#define _KEYMAP_SHORTCUT_KEYS_SEAL(sum_) ((unsigned long long)(~(unsigned)(sum_) & 0xffu) << _KEYMAP_SHORTCUT_KEYS_SEAL_SHIFT)
#define _KEYMAP_SHORTCUT_KEY_AT(key_, index_) ((unsigned)(key_) <= 0xff && NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_KEY((unsigned)(key_)) ? (unsigned long long)(key_) << (8 * (index_)) : _KEYMAP_SHORTCUT_NOT_A_KEY)
#define _KEYMAP_SHORTCUT_KEYS_1(a_) _KEYMAP_SHORTCUT_KEY_AT(a_, 0)
#define _KEYMAP_SHORTCUT_KEYS_2(a_, b_) (_KEYMAP_SHORTCUT_KEYS_1(a_) | _KEYMAP_SHORTCUT_KEY_AT(b_, 1))
#define _KEYMAP_SHORTCUT_KEYS_3(a_, b_, c_) (_KEYMAP_SHORTCUT_KEYS_2(a_, b_) | _KEYMAP_SHORTCUT_KEY_AT(c_, 2))
#define _KEYMAP_SHORTCUT_KEYS_4(a_, b_, c_, d_) (_KEYMAP_SHORTCUT_KEYS_3(a_, b_, c_) | _KEYMAP_SHORTCUT_KEY_AT(d_, 3))
#define _KEYMAP_SHORTCUT_KEYS_5(a_, b_, c_, d_, e_) (_KEYMAP_SHORTCUT_KEYS_4(a_, b_, c_, d_) | _KEYMAP_SHORTCUT_KEY_AT(e_, 4))
#define _KEYMAP_SHORTCUT_KEYS_6(a_, b_, c_, d_, e_, f_) (_KEYMAP_SHORTCUT_KEYS_5(a_, b_, c_, d_, e_) | _KEYMAP_SHORTCUT_KEY_AT(f_, 5))
#define _KEYMAP_SHORTCUT_KEYS_7(...) _KEYMAP_SHORTCUT_NOT_A_KEY
#define _KEYMAP_SHORTCUT_KEYS_SUM_1(a_) ((unsigned)(a_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_2(a_, b_) (_KEYMAP_SHORTCUT_KEYS_SUM_1(a_) + (unsigned)(b_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_3(a_, b_, c_) (_KEYMAP_SHORTCUT_KEYS_SUM_2(a_, b_) + (unsigned)(c_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_4(a_, b_, c_, d_) (_KEYMAP_SHORTCUT_KEYS_SUM_3(a_, b_, c_) + (unsigned)(d_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_5(a_, b_, c_, d_, e_) (_KEYMAP_SHORTCUT_KEYS_SUM_4(a_, b_, c_, d_) + (unsigned)(e_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_6(a_, b_, c_, d_, e_, f_) (_KEYMAP_SHORTCUT_KEYS_SUM_5(a_, b_, c_, d_, e_) + (unsigned)(f_))
#define _KEYMAP_SHORTCUT_KEYS_SUM_7(...) 0u
#define _KEYMAP_SHORTCUT_KEYS_COUNT(a1_, a2_, a3_, a4_, a5_, a6_, a7_, count_, ...) count_
#define _KEYMAP_SHORTCUT_KEYS_FORM_(count_, ...) (_KEYMAP_SHORTCUT_KEYS_TAG | _KEYMAP_SHORTCUT_KEYS_##count_(__VA_ARGS__) | _KEYMAP_SHORTCUT_KEYS_SEAL(_KEYMAP_SHORTCUT_KEYS_SUM_##count_(__VA_ARGS__)))
#define _KEYMAP_SHORTCUT_KEYS_FORM(count_, ...) _KEYMAP_SHORTCUT_KEYS_FORM_(count_, __VA_ARGS__)
#define SHORTCUT_KEYS(...) _KEYMAP_SHORTCUT_KEYS_FORM(_KEYMAP_SHORTCUT_KEYS_COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0), __VA_ARGS__)
#define SHORTCUT_NO_KEYS (_KEYMAP_SHORTCUT_KEYS_TAG | _KEYMAP_SHORTCUT_KEYS_SEAL(0u))
#define _KEYMAP_SHORTCUT_KEY(keys_, index_) ((uint8_t)((keys_) >> (8 * (index_))))
// What the checks read back from a form: each of its six bytes a shortcut key
// or zero, packed from the first, and its seal the complement of their sum.
#define _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, i_) (_KEYMAP_SHORTCUT_KEY(keys_, i_) == 0 || NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_KEY(_KEYMAP_SHORTCUT_KEY(keys_, i_)))
#define _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, i_) (_KEYMAP_SHORTCUT_KEY(keys_, i_) == 0 || _KEYMAP_SHORTCUT_KEY(keys_, (i_) - 1) != 0)
#define _KEYMAP_SHORTCUT_KEYS_BYTES_SUM(keys_)                                                                                         \
    ((unsigned)_KEYMAP_SHORTCUT_KEY(keys_, 0) + _KEYMAP_SHORTCUT_KEY(keys_, 1) + _KEYMAP_SHORTCUT_KEY(keys_, 2) + _KEYMAP_SHORTCUT_KEY(keys_, 3) + \
     _KEYMAP_SHORTCUT_KEY(keys_, 4) + _KEYMAP_SHORTCUT_KEY(keys_, 5))
#define _KEYMAP_SHORTCUT_KEYS_WELL_FORMED(keys_)                                                                                                                       \
    (_KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 0) && _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 1) && _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 2) &&                        \
     _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 3) && _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 4) && _KEYMAP_SHORTCUT_KEY_BYTE_VALID(keys_, 5) &&                        \
     _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, 1) && _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, 2) && _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, 3) &&                      \
     _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, 4) && _KEYMAP_SHORTCUT_KEY_BYTE_PACKED(keys_, 5) &&                                                                   \
     (((keys_) >> _KEYMAP_SHORTCUT_KEYS_SEAL_SHIFT) & 0xffu) == (~_KEYMAP_SHORTCUT_KEYS_BYTES_SUM(keys_) & 0xffu))
#define _KEYMAP_SHORTCUT_SAME(keys_, i_, j_) (_KEYMAP_SHORTCUT_KEY(keys_, i_) != 0 && _KEYMAP_SHORTCUT_KEY(keys_, i_) == _KEYMAP_SHORTCUT_KEY(keys_, j_))
#define _KEYMAP_SHORTCUT_KEYS_DISTINCT(keys_)                                                                                                    \
    !(_KEYMAP_SHORTCUT_SAME(keys_, 0, 1) || _KEYMAP_SHORTCUT_SAME(keys_, 0, 2) || _KEYMAP_SHORTCUT_SAME(keys_, 0, 3) || _KEYMAP_SHORTCUT_SAME(keys_, 0, 4) || \
      _KEYMAP_SHORTCUT_SAME(keys_, 0, 5) || _KEYMAP_SHORTCUT_SAME(keys_, 1, 2) || _KEYMAP_SHORTCUT_SAME(keys_, 1, 3) || _KEYMAP_SHORTCUT_SAME(keys_, 1, 4) || \
      _KEYMAP_SHORTCUT_SAME(keys_, 1, 5) || _KEYMAP_SHORTCUT_SAME(keys_, 2, 3) || _KEYMAP_SHORTCUT_SAME(keys_, 2, 4) || _KEYMAP_SHORTCUT_SAME(keys_, 2, 5) || \
      _KEYMAP_SHORTCUT_SAME(keys_, 3, 4) || _KEYMAP_SHORTCUT_SAME(keys_, 3, 5) || _KEYMAP_SHORTCUT_SAME(keys_, 4, 5))
#define _KEYMAP_SHORTCUT_KEYS_COUNT_OF(keys_, is_)                                                                                    \
    ((is_(_KEYMAP_SHORTCUT_KEY(keys_, 0)) ? 1 : 0) + (is_(_KEYMAP_SHORTCUT_KEY(keys_, 1)) ? 1 : 0) + (is_(_KEYMAP_SHORTCUT_KEY(keys_, 2)) ? 1 : 0) + \
     (is_(_KEYMAP_SHORTCUT_KEY(keys_, 3)) ? 1 : 0) + (is_(_KEYMAP_SHORTCUT_KEY(keys_, 4)) ? 1 : 0) + (is_(_KEYMAP_SHORTCUT_KEY(keys_, 5)) ? 1 : 0))
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
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_NAME_ENTRY(keycode_, name_, mods_, keys_, layers_) _KEYMAP_CUSTOM_KEY_NAME_ENTRY(keycode_, name_)
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_CHECK(keycode_, name_, mods_, keys_, layers_)                                               \
    _KEYMAP_CUSTOM_KEY_NAME_CHECK(keycode_, name_)                                                                              \
    _Static_assert(((mods_) & ~0xff) == _KEYMAP_SHORTCUT_MODS_TAG,                                                              \
                   #keycode_ ": write a shortcut's modifiers as SHORTCUT_MODS(KC_LCTL, ...) of modifier keys, or SHORTCUT_NO_MODS"); \
    _Static_assert(((keys_) & ~(0xffffffffffffULL | (0xffULL << _KEYMAP_SHORTCUT_KEYS_SEAL_SHIFT))) == _KEYMAP_SHORTCUT_KEYS_TAG,  \
                   #keycode_ ": write a shortcut's keys as SHORTCUT_KEYS(...) of at most six keyboard, media, system or mouse keys, or SHORTCUT_NO_KEYS"); \
    _Static_assert(_KEYMAP_SHORTCUT_KEYS_WELL_FORMED(keys_), #keycode_ ": write a shortcut's keys as one SHORTCUT_KEYS(...), never two combined or a bare number"); \
    _Static_assert(_KEYMAP_SHORTCUT_KEYS_DISTINCT(keys_), #keycode_ ": a shortcut names each key once");                       \
    _Static_assert(_KEYMAP_SHORTCUT_KEYS_COUNT_OF(keys_, NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_SYSTEM) <= 1 && _KEYMAP_SHORTCUT_KEYS_COUNT_OF(keys_, NOAH_CUSTOM_KEY_SHORTCUT_KEY_IS_CONSUMER) <= 1, \
                   #keycode_ ": a shortcut holds at most one system key and one media key");                                \
    _Static_assert(((layers_) & ~(0xffffL | (0x7L << _KEYMAP_SHORTCUT_KIND_SHIFT) | (0x7L << _KEYMAP_SHORTCUT_KIND_CHECK_SHIFT))) == _KEYMAP_SHORTCUT_LAYERS_TAG && _KEYMAP_SHORTCUT_KIND_WELL_FORMED(layers_) && _KEYMAP_SHORTCUT_KIND_OF(layers_) < NOAH_SHORTCUT_LAYER_KIND_COUNT, \
                   #keycode_ ": write a shortcut's layers as SHORTCUT_LAYERS(...) of layers 1-15 (or the LOCK, MOVE_TO, TAP_TOGGLE or ONESHOT form), or SHORTCUT_NO_LAYERS"); \
    _Static_assert(((mods_) & 0xff) != 0 || ((keys_) & 0xff) != 0 || ((layers_) & 0xffff) != 0, #keycode_ ": a SHORTCUT row holds a shortcut; use KEY for none"); \
    _Static_assert(_KEYMAP_SHORTCUT_KIND_OF(layers_) == NOAH_SHORTCUT_LAYERS_HOLD || ((layers_) & 0xffff) != 0, #keycode_ ": a layer kind needs layers");
#define _KEYMAP_CUSTOM_KEY_NO_SHORTCUT(keycode_, name_) [((keycode_) - CUSTOM_KEY_0)] = {0u},
#define _KEYMAP_CUSTOM_KEY_SHORTCUT_ENTRY(keycode_, name_, mods_, keys_, layers_)                                                                                       \
    [((keycode_) - CUSTOM_KEY_0)] = {(uint8_t)(mods_), _KEYMAP_SHORTCUT_KEY(keys_, 0), _KEYMAP_SHORTCUT_KEY(keys_, 1), _KEYMAP_SHORTCUT_KEY(keys_, 2), _KEYMAP_SHORTCUT_KEY(keys_, 3), \
                                     _KEYMAP_SHORTCUT_KEY(keys_, 4), _KEYMAP_SHORTCUT_KEY(keys_, 5), (uint8_t)(layers_), (uint8_t)((layers_) >> 8), _KEYMAP_SHORTCUT_KIND_OF(layers_)},
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

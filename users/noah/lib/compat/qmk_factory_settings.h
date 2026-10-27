#pragma once
#include <stdint.h>
#ifdef RGB_MATRIX_ENABLE
#    include "rgb_matrix.h"
#endif
// These fallback definitions mirror the pinned QMK factory initialization.
// Keymap overrides remain authoritative. No mutable getters belong here.
#ifndef AUTO_MOUSE_DEBOUNCE
#    define AUTO_MOUSE_DEBOUNCE 25
#endif
#ifndef AUTO_MOUSE_DEFAULT_LAYER
#    define AUTO_MOUSE_DEFAULT_LAYER 1
#endif
#ifndef AUTO_MOUSE_TIME
#    define AUTO_MOUSE_TIME 650
#endif
#ifndef CHARYBDIS_MINIMUM_DEFAULT_DPI
#    define CHARYBDIS_MINIMUM_DEFAULT_DPI 400
#endif
#ifndef CHARYBDIS_MINIMUM_SNIPING_DPI
#    define CHARYBDIS_MINIMUM_SNIPING_DPI 200
#endif
#ifndef RGB_MATRIX_DEFAULT_ON
#    define RGB_MATRIX_DEFAULT_ON 1
#endif
#ifndef RGB_MATRIX_DEFAULT_MODE
#    define RGB_MATRIX_DEFAULT_MODE 1
#endif
#ifndef RGB_MATRIX_DEFAULT_SPD
#    define RGB_MATRIX_DEFAULT_SPD 127
#endif
#ifndef RGB_MATRIX_DEFAULT_FLAGS
#    define RGB_MATRIX_DEFAULT_FLAGS 0xff
#endif
#ifndef RGB_MATRIX_DEFAULT_HUE
#    define RGB_MATRIX_DEFAULT_HUE 0
#endif
#ifndef RGB_MATRIX_DEFAULT_SAT
#    define RGB_MATRIX_DEFAULT_SAT 255
#endif
#ifndef RGB_MATRIX_DEFAULT_VAL
#    define RGB_MATRIX_DEFAULT_VAL 255
#endif
#define NOAH_QMK_FACTORY_RGB_MODE ((uint32_t)RGB_MATRIX_DEFAULT_ON | (uint32_t)RGB_MATRIX_DEFAULT_MODE << 8 | (uint32_t)RGB_MATRIX_DEFAULT_SPD << 16 | (uint32_t)RGB_MATRIX_DEFAULT_FLAGS << 24)
#define NOAH_QMK_FACTORY_RGB_COLOR ((uint32_t)RGB_MATRIX_DEFAULT_HUE | (uint32_t)RGB_MATRIX_DEFAULT_SAT << 8 | (uint32_t)RGB_MATRIX_DEFAULT_VAL << 16)

#ifndef NKRO_DEFAULT_ON
#    define NKRO_DEFAULT_ON 0
#endif
// quantum/keycode_config.h: NKRO bit 7, oneshot bit 10, autocorrect bit 12.
// quantum/eeconfig.c enables oneshot and autocorrect at factory reset.
#define NOAH_QMK_FACTORY_KEYMAP_OPTIONS ((1u << 10) | (1u << 12) | (NKRO_DEFAULT_ON ? 1u << 7 : 0u))

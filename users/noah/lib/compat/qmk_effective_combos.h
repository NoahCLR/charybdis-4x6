#pragma once
#include QMK_KEYBOARD_H
#include "noah_keymap_ids.h"
#ifdef COMBO_ENABLE
#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#    include "../profile/runtime/effective_combo_runtime.h"
#    define noah_qmk_combo_count noah_effective_combo_count
#    define noah_qmk_combo_get noah_effective_combo_get
#else
static inline uint16_t noah_qmk_combo_count(void) {return noah_combo_count;}
static inline combo_t *noah_qmk_combo_get(uint16_t index) {return index < noah_combo_count ? &key_combos[index] : NULL;}
#endif

#endif

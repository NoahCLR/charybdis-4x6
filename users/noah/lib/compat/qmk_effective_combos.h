#pragma once
#include QMK_KEYBOARD_H
#include "noah_keymap_ids.h"
#ifdef COMBO_ENABLE
#    ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#        include "../profile/runtime/effective_combo_runtime.h"
#        define noah_qmk_combo_count noah_effective_combo_count
#        define noah_qmk_combo_get noah_effective_combo_get
// Participation fields of a combo definition (participation-policy.md).
#        define noah_qmk_combo_allowed_layers noah_effective_combo_allowed_layers
#        define noah_qmk_combo_enabled noah_effective_combo_enabled
#    else
static inline uint16_t noah_qmk_combo_count(void) {
    return noah_combo_count;
}
static inline combo_t *noah_qmk_combo_get(uint16_t index) {
    return index < noah_combo_count ? &key_combos[index] : NULL;
}
// Without the live owner the compiled table allows every layer of the bank
// and enables every combo.
static inline uint32_t noah_qmk_combo_allowed_layers(uint16_t index) {
    (void)index;
    return (uint32_t)((UINT64_C(1) << LAYER_COUNT) - 1u);
}
static inline bool noah_qmk_combo_enabled(uint16_t index) {
    (void)index;
    return true;
}
#    endif

#endif

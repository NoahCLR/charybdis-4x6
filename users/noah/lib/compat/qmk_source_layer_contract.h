// ────────────────────────────────────────────────────────────────────────────
// QMK Source-Layer Contract
// ────────────────────────────────────────────────────────────────────────────
//
// The layer QMK resolved a physical key's keycode from. pre_process_record_
// quantum() stores it in the source-layer cache before any hook, the tapping
// engine or process_combo() sees the press, and a release reads the entry its
// press stored. Participation decides against it (participation-policy.md).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(STRICT_LAYER_RELEASE) || defined(NO_ACTION_LAYER)
#    error "participation needs QMK's source-layer cache"
#endif

static inline uint8_t noah_qmk_contract_source_layer(keypos_t key) {
    return read_source_layers_cache(key);
}

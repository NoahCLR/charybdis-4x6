// ────────────────────────────────────────────────────────────────────────────
// QMK Source-Layer Contract
// ────────────────────────────────────────────────────────────────────────────
//
// The layer QMK resolved a physical key's keycode from. pre_process_record_
// quantum() stores it in the source-layer cache before any hook, the tapping
// engine or process_combo() sees the press; a press that waited in a queue is
// resolved again, and stored again, when QMK processes it. A release reads the
// entry its press stored. Participation decides against it
// (participation-policy.md).
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#if defined(STRICT_LAYER_RELEASE) || defined(NO_ACTION_LAYER)
#    error "participation needs QMK's source-layer cache"
#endif

static inline uint8_t noah_qmk_contract_source_layer(keypos_t key) {
    return read_source_layers_cache(key);
}

// The layer a press of key resolves from now (QMK's layer_switch_get_layer()),
// before QMK processes it and stores that in the cache.
static inline uint8_t noah_qmk_contract_resolve_source_layer(keypos_t key) {
    return layer_switch_get_layer(key);
}

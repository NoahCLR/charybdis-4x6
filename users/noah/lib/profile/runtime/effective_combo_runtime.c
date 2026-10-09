#include "effective_combo_runtime.h"
#ifdef COMBO_ENABLE
#    include "profile_action_runtime_v1.h"
#    include "noah_keymap_ids.h"
#    include <string.h>

static noah_effective_combo_runtime_t *installed;
void                                   noah_effective_combo_runtime_init(noah_effective_combo_runtime_t *runtime) {
    if (runtime) {
        memset(runtime, 0, sizeof(*runtime));
        runtime->valid = true;
    }
}
bool noah_effective_combo_runtime_install(noah_effective_combo_runtime_t *runtime) {
    if (!runtime || (installed && installed != runtime)) return false;
    installed = runtime;
    return true;
}
void noah_effective_combo_runtime_uninstall(noah_effective_combo_runtime_t *runtime) {
    if (installed == runtime) installed = NULL;
}
void noah_effective_combo_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    noah_effective_combo_runtime_t *runtime = context;
    (void)publication_count;
    (void)previous;
    (void)active;
    if (!runtime) return;
    const noah_effective_profile_snapshot_t *compiled = runtime->compiled_defaults;
    memset(runtime, 0, sizeof(*runtime));
    runtime->compiled_defaults = compiled;
    if (view && !(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS) && compiled) view = compiled;
    if (!view) return;
    if (!(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS)) {
        runtime->valid = true;
        return;
    }
    runtime->live = true;
    noah_profile_domain_range_t        combos;
    noah_profile_combo_v1_header_t     header = {0};
    noah_profile_combo_v1_validation_t walk   = {0};
    noah_profile_action_v1_limits_t    limits = noah_profile_action_v1_default_limits();
    noah_profile_combo_v1_row_t        row;
    if (!noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_COMBOS, &combos) || view->base_offset > SIZE_MAX - combos.offset) return;
    for (;;) {
        noah_profile_combo_v1_iteration_t step  = noah_profile_combo_v1_iteration_step(&walk, &view->reader, view->base_offset + combos.offset, combos.length, &limits, &header, &row, NULL);
        uint8_t                           index = (uint8_t)(walk.row_index - 1u);
        if (step == NOAH_PROFILE_COMBO_V1_HEADER) {
            runtime->default_term = header.default_term_ms;
            runtime->hold_term    = header.hold_term_ms;
            continue;
        }
        if (step == NOAH_PROFILE_COMBO_V1_ITERATING) continue;
        if (step == NOAH_PROFILE_COMBO_V1_COMPLETE) break;
        if (step != NOAH_PROFILE_COMBO_V1_ROW) return;
        bool follows = row.term_ms == 0u;
        if (follows) runtime->follows_default[index / 8u] |= (uint8_t)(1u << (index % 8u));
        runtime->terms[index]          = follows ? runtime->default_term : row.term_ms;
        runtime->flags[index]          = row.flags;
        runtime->allowed_layers[index] = row.allowed_layers;
        runtime->rows[index].keys      = runtime->inputs[index];
        if (noah_profile_action_runtime_v1_to_native(&row.output, &runtime->rows[index].keycode) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK || !runtime->rows[index].keycode) return;
        for (uint8_t member = 0; member < row.input_count; member++) {
            uint16_t *key = &runtime->inputs[index][member];
            if (noah_profile_action_runtime_v1_to_native(&row.inputs[member], key) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK || *key <= 1u) return;
            for (uint8_t prior = 0; prior < member; prior++)
                if (runtime->inputs[index][prior] == *key) return;
        }
    }
    runtime->count = header.row_count;
    runtime->valid = true;
}
bool noah_effective_combo_valid(void) {
    return !installed || installed->valid;
}
uint16_t noah_effective_combo_count(void) {
    if (installed && !installed->valid) return 0;
    return installed && installed->live ? installed->count : noah_combo_count;
}
combo_t *noah_effective_combo_get(uint16_t index) {
    if (index >= noah_effective_combo_count()) return NULL;
    return installed && installed->live ? &installed->rows[index] : &key_combos[index];
}
// Without a stored table the keymap's combos run with their authored windows:
// COMBO follows COMBO_TERM, COMBO_WINDOW carries its own.
uint16_t noah_effective_combo_term(uint16_t index) {
    if (installed && installed->valid && installed->live) return index < installed->count ? installed->terms[index] : COMBO_TERM;
    return index < noah_combo_count && noah_combo_terms[index] ? noah_combo_terms[index] : COMBO_TERM;
}
uint16_t noah_effective_combo_default_term(void) {
    return installed && installed->valid && installed->live ? installed->default_term : COMBO_TERM;
}
bool noah_effective_combo_follows_default(uint16_t index) {
    if (installed && installed->valid && installed->live) return index < installed->count && (installed->follows_default[index / 8u] >> (index % 8u) & 1u);
    return index < noah_effective_combo_count() && !noah_combo_terms[index];
}
// A current table stores the threshold even with no rows.
uint16_t noah_effective_combo_hold_term(void) {
    return installed && installed->valid && installed->live ? installed->hold_term : TAPPING_TERM;
}
uint32_t noah_effective_combo_allowed_layers(uint16_t index) {
    if (installed && installed->valid && installed->live) return index < installed->count ? installed->allowed_layers[index] : 0u;
    return (uint32_t)((UINT64_C(1) << LAYER_COUNT) - 1u);
}
bool noah_effective_combo_enabled(uint16_t index) {
    if (installed && installed->valid && installed->live) return index < installed->count && !(installed->flags[index] & NOAH_PROFILE_COMBO_V3_FLAG_DISABLED);
    return index < noah_effective_combo_count();
}
uint8_t noah_effective_combo_flags(uint16_t index) {
    if (installed && installed->valid && installed->live && index < installed->count) return installed->flags[index];
    uint8_t flags = 0;
#    ifdef COMBO_MUST_PRESS_IN_ORDER
    flags |= 4u;
#    endif
#    ifdef COMBO_MUST_HOLD_MODS
    combo_t *combo = noah_effective_combo_get(index);
    uint16_t key   = combo ? combo->keycode : 0u;
    if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) flags |= 1u;
#    endif
    return flags;
}

#endif

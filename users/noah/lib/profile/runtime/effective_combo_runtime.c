#include "effective_combo_runtime.h"
#ifdef COMBO_ENABLE
#include "profile_action_runtime_v1.h"
#include "noah_keymap_ids.h"
#include <string.h>

static noah_effective_combo_runtime_t *installed;
void noah_effective_combo_runtime_init(noah_effective_combo_runtime_t *runtime) {
    if (runtime) {memset(runtime, 0, sizeof(*runtime)); runtime->valid = true;}
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
    (void)publication_count; (void)previous; (void)active;
    if (!runtime) return;
    memset(runtime, 0, sizeof(*runtime));
    if (!view) return;
    if (!(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS)) {runtime->valid = true; return;}
    runtime->live = true;
    if (view->profile.combos.row_count > 32u) return;
    for (uint8_t index = 0; index < view->profile.combos.row_count; index++) {
        noah_profile_combo_v1_row_t row;
        if (!noah_profile_combo_v1_read_row(&view->reader, view->base_offset, &view->profile.combos, index, &row)) return;
        if (index && row.hold_term_ms != runtime->hold_term) return;
        runtime->hold_term = row.hold_term_ms;
        runtime->terms[index] = row.term_ms;
        runtime->flags[index] = row.flags;
        runtime->rows[index].keys = runtime->inputs[index];
        if (noah_profile_action_runtime_v1_to_native(&row.output, &runtime->rows[index].keycode) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK || !runtime->rows[index].keycode) return;
        for (uint8_t member = 0; member < row.input_count; member++) {
            uint16_t *key = &runtime->inputs[index][member];
            if (noah_profile_action_runtime_v1_to_native(&row.inputs[member], key) != NOAH_PROFILE_ACTION_RUNTIME_V1_OK || *key <= 1u) return;
            for (uint8_t prior = 0; prior < member; prior++) if (runtime->inputs[index][prior] == *key) return;
        }
    }
    runtime->count = view->profile.combos.row_count;
    runtime->valid = true;
}
bool noah_effective_combo_valid(void) {return !installed || installed->valid;}
uint16_t noah_effective_combo_count(void) {
    if (installed && !installed->valid) return 0;
    return installed && installed->live ? installed->count : noah_combo_count;
}
combo_t *noah_effective_combo_get(uint16_t index) {
    if (index >= noah_effective_combo_count()) return NULL;
    return installed && installed->live ? &installed->rows[index] : &key_combos[index];
}
uint16_t noah_effective_combo_term(uint16_t index) {
    return installed && installed->valid && installed->live && index < installed->count ? installed->terms[index] : COMBO_TERM;
}
uint16_t noah_effective_combo_hold_term(void) {
    return installed && installed->valid && installed->live && installed->count ? installed->hold_term : TAPPING_TERM;
}
uint8_t noah_effective_combo_flags(uint16_t index) {
    if (installed && installed->valid && installed->live && index < installed->count) return installed->flags[index];
    uint8_t flags = 0;
#ifdef COMBO_MUST_PRESS_IN_ORDER
    flags |= 4u;
#endif
#ifdef COMBO_MUST_HOLD_MODS
    combo_t *combo = noah_effective_combo_get(index);
    uint16_t key = combo ? combo->keycode : 0u;
    if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) flags |= 1u;
#endif
    return flags;
}

#endif

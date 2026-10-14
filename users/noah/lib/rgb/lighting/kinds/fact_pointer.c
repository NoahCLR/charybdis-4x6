// Registered phase-3 fact: pointer.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_POINTER
#include "../../../compat/qmk_auto_mouse_contract.h"
#include "../../../pointing/defs/pd_modes.h"
#include "../../../compat/qmk_pointing_contract.h"
#include "../../../pointing/runtime/pd_sensor_observation.h"
#include "../../automouse/rgb_automouse.h"
void noah_lighting_slice_pointer_collect(uint8_t *slice) {
    bool on = noah_qmk_contract_auto_mouse_enabled() && layer_state_cmp(layer_state, noah_qmk_contract_auto_mouse_layer());
    slice[0] = (on ? 1u : 0u) | (on && automouse_rgb_current_progress() != 0u ? 2u : 0u) | ((pd_mode_auto_sniping_layer_active() || noah_qmk_contract_pointer_sniping_enabled()) ? 4u : 0u) | (noah_pointing_sensor_motion_observed() ? 8u : 0u);
}
void noah_lighting_slice_pointer_idle(uint8_t *slice) { slice[0] = 0u; }
bool noah_lighting_fact_pointer_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    if ((*cursor)++ || !(facts->pointer[0] & (1u << params[0]))) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BOARD, 255u, 0u);
    return true;
}
#endif

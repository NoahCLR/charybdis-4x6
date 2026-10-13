// ────────────────────────────────────────────────────────────────────────────
// Auto-Mouse Fade
// ────────────────────────────────────────────────────────────────────────────
//
// Outside the registries, as it always was: while auto-mouse fades, the
// engine walks the fade's scene (the bottom `base` rules) with and without
// the auto-mouse layer and blends the two by the timeout's progress. The
// progress and its split mirroring are rgb_automouse.c's, unchanged.

#include "lighting_engine.h"

#if defined(RGB_MATRIX_ENABLE) && NOAH_LIGHTING_EXTENSION_AUTOMOUSE_FADE && NOAH_LIGHTING_FACT_LAYER_ACTIVE

#    include "kinds/fact_layer_active.h"
#    include "../automouse/rgb_automouse.h"
#    include "../../compat/qmk_auto_mouse_contract.h"

bool noah_lighting_fade_active(const noah_lighting_scene_t *scene, noah_lighting_fade_t *fade) {
    const noah_profile_lighting_v1_program_t *program = scene->program;
    uint8_t                                   layer   = noah_qmk_contract_auto_mouse_layer();
    uint16_t                                  progress;

    if (!program->fade_present || (program->fade[0] & NOAH_PROFILE_LIGHTING_V1_FADE_ON) == 0u || !noah_lighting_fact_layer_active_has(scene->facts, layer)) return false;
    progress = automouse_rgb_current_progress();
    if (progress == 0u) return false;
    *fade = (noah_lighting_fade_t){.mode = program->fade[1], .blend = automouse_rgb_blend_amount(progress), .end = program->fade[2], .base = program->fade[3], .layer = layer};
    return true;
}

void noah_lighting_fade_end_facts(noah_lighting_facts_t *facts, uint8_t layer) {
    noah_lighting_fact_layer_active_without(facts, layer);
}

#elif defined(RGB_MATRIX_ENABLE)

bool noah_lighting_fade_active(const noah_lighting_scene_t *scene, noah_lighting_fade_t *fade) {
    (void)scene;
    (void)fade;
    return false;
}

void noah_lighting_fade_end_facts(noah_lighting_facts_t *facts, uint8_t layer) {
    (void)facts;
    (void)layer;
}

#endif

// The auto-mouse fade, which stays outside the registries, walks its scene
// once without the auto-mouse layer; these give it that view of the facts.
#pragma once

#include "../lighting.h"

#if NOAH_LIGHTING_FACT_LAYER_ACTIVE
bool noah_lighting_fact_layer_active_has(const noah_lighting_facts_t *facts, uint8_t layer);
void noah_lighting_fact_layer_active_without(noah_lighting_facts_t *facts, uint8_t layer);
#endif

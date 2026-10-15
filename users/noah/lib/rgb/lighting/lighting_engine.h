// ────────────────────────────────────────────────────────────────────────────
// Lighting Engine
// ────────────────────────────────────────────────────────────────────────────
//
// Resolves a rule list into colours for a set of LEDs. Rules apply bottom up,
// each painting over what is below, which is the spec's top-down walk
// evaluated for all LEDs at once.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "lighting.h"
#include "lighting_retained_render.h"

typedef struct {
    rgb_t                colour[NOAH_LIGHTING_LED_COUNT];
    noah_lighting_leds_t painted;
} noah_lighting_frame_t;

typedef struct {
    bool     effect_used; // some LED composed over the matrix effect
    bool     live;        // something shown changes every frame
    bool     wake_set;
    uint32_t wake; // when a timing next changes
} noah_lighting_resolve_info_t;

// Resolves `leds` of `frame` (others keep their colours). `update_gates`
// starts and expires timing state; a re-resolve within one frame only reads it.
void noah_lighting_resolve(const noah_lighting_scene_t *scene, noah_lighting_leds_t leds, uint32_t now, bool update_gates, noah_lighting_frame_t *frame, noah_lighting_resolve_info_t *info);

// The palette and timing state follow the published program.
void noah_lighting_engine_program_changed(void);

// Apply one authored rule's COMPLETE frozen histories over an existing frame.
// The caller supplies a same-generation store and the fact module's natural
// priority binding. No observation/filter qualification occurs during paint;
// occurrence filters and departed owners remain as captured. Main only, no
// mutation/reentry. Runtime bank selection and whole-scene integration are
// separate obligations; this function allocates no history storage.
typedef struct {
    const noah_lighting_history_store_t *store;
    noah_lighting_natural_priority_fn    natural;
    void                                *context;
    uint64_t                            *candidates;
} noah_lighting_retained_view_t;
bool noah_lighting_apply_retained_rule(const noah_lighting_scene_t *scene, uint8_t rule, const noah_lighting_retained_view_t *view, noah_lighting_leds_t leds, noah_lighting_frame_t *frame, noah_lighting_resolve_info_t *info);

// The auto-mouse fade, outside the registries (lighting_fade.c).
typedef struct {
    uint8_t mode;
    uint8_t blend;
    uint8_t end;
    uint8_t base;
    uint8_t layer;
} noah_lighting_fade_t;

bool noah_lighting_fade_active(const noah_lighting_scene_t *scene, noah_lighting_fade_t *fade);
void noah_lighting_fade_end_facts(noah_lighting_facts_t *facts, uint8_t layer);

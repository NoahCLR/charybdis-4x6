#pragma once
#include "lighting_working.h"
#include "lighting_events.h"
#if defined(RGB_MATRIX_ENABLE) && NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
bool                   noah_lighting_runtime_prepare(noah_lighting_scene_t *, uint32_t generation, uint32_t now);
void                   noah_lighting_runtime_task(void);
uint32_t               noah_lighting_runtime_revision(void);
bool                   noah_lighting_runtime_remote_scene(void);
noah_lighting_facts_t *noah_rgb_runtime_history_facts(void);
#else
static inline void noah_lighting_runtime_task(void) {}
#endif

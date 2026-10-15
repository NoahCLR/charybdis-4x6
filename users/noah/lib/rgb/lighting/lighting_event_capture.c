#include "lighting_event_capture.h"
#include "lighting_workspace.h"
#ifdef RGB_MATRIX_ENABLE
bool noah_lighting_event_capture(const noah_profile_lighting_v1_program_t *program, const noah_lighting_event_t *event, uint64_t order, uint32_t now, noah_lighting_facts_t *facts, noah_lighting_event_group_t *group, noah_lighting_event_collect_fn collect, noah_lighting_event_retain_fn retain, void *context) {
    if (!program || !program->valid || !event || !facts || !group || !collect || !retain) return false;
    collect(facts);
    if (!noah_lighting_event_qualify(program, facts, event, order, group)) return false;
    retain(context, group, now);
    return true;
}
bool noah_lighting_event_capture_shared(const noah_profile_lighting_v1_program_t *program, const noah_lighting_event_t *event, uint64_t order, uint32_t now, noah_lighting_event_collect_fn collect, noah_lighting_event_retain_fn retain, void *context) {
    return noah_lighting_event_capture(program, event, order, now, &noah_lighting_workspace.capture.facts, &noah_lighting_workspace.capture.group, collect, retain, context);
}
#endif

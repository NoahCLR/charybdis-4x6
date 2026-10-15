#include "lighting_event_capture.h"
#ifdef RGB_MATRIX_ENABLE
bool noah_lighting_event_capture(const noah_profile_lighting_v1_program_t *program, const noah_lighting_event_t *event, uint64_t order, uint32_t now, noah_lighting_facts_t *facts, noah_lighting_event_group_t *group, noah_lighting_event_collect_fn collect, noah_lighting_event_retain_fn retain, void *context) {
    if (!program || !program->valid || !event || !facts || !group || !collect || !retain) return false;
    collect(facts);
    if (!noah_lighting_event_qualify(program, facts, event, order, group)) return false;
    retain(context, group, now);
    return true;
}
#endif

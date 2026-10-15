#include "lighting_retained_group.h"
#include <string.h>

static bool qualified(const noah_lighting_event_group_t *group, uint16_t rule) {
    return (group->qualified[rule / 8u] & (1u << (rule % 8u))) != 0u;
}
bool noah_lighting_retained_group_apply(noah_lighting_batch_arena_t *arena, const noah_lighting_event_group_t *group, bool source_delivery, uint8_t encoded[NOAH_LIGHTING_GROUP_BATCH_BYTES], noah_lighting_group_prepare_fn prepare, noah_lighting_group_record_fn record, void *context) {
    if (!arena || !group || !encoded || !prepare || !record || (group->qualified[31] & 0x80u) || !prepare(context, group)) return false;
    uint16_t count = 0u;
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (qualified(group, rule)) {
            noah_lighting_lifetime_parameters_t parameters = {0};
            if (!record(context, (uint8_t)rule, group->thing, &parameters) || !parameters.flash || parameters.linger) return false;
            count++;
        }
    if (!count) return false;
    noah_lighting_subject_t subject;
    noah_lighting_subject_init(&subject, group->thing_kind, group->thing, group->slot);
    subject.outcome  = group->outcome;
    subject.sides    = group->sides;
    subject.has_keys = group->has_keys;
    memcpy(subject.keys, group->owners, sizeof subject.keys);
    noah_lighting_batch_writer_t writer;
    noah_lighting_batch_write_begin(&writer, encoded, NOAH_LIGHTING_GROUP_BATCH_BYTES);
    if (!noah_lighting_batch_write(&writer, &subject)) return false;
    noah_lighting_batch_ref_t reference;
    if (!noah_lighting_batch_acquire(arena, group->thing_kind, group->thing, encoded, (uint16_t)writer.used, &reference, NULL)) return false;
    if (!noah_lighting_batch_retain_many(arena, reference, count)) {
        (void)noah_lighting_batch_release(arena, reference);
        return false;
    }
    // From here all record slots, parameters and payload references are owned.
    // No renderer/input producer/publication can reenter this synchronous path.
    for (uint16_t rule = 0u; rule < 255u; rule++)
        if (qualified(group, rule)) {
            noah_lighting_lifetime_parameters_t parameters = {0};
            noah_lighting_retained_t           *cell       = record(context, (uint8_t)rule, group->thing, &parameters);
            noah_lighting_retained_install_event(cell, arena, &parameters, reference, group->order, source_delivery);
        }
    (void)noah_lighting_batch_release(arena, reference); // transaction reserve
    return true;
}

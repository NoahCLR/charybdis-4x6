// Registered phase-3 fact: mode status.
#include "../lighting.h"
#include <string.h>

#if NOAH_LIGHTING_FACT_MODE_STATUS
#include "../../../pointing/defs/pd_mode_flags.h"
void noah_lighting_slice_mode_status_collect(uint8_t *slice) {
    bool keys;
    pd_mode_snapshot_t snapshot = pd_mode_snapshot_with_owner_bitmap(slice + 3u, &keys);
    slice[0] = snapshot.local.active_index;
    slice[1] = snapshot.local.locked_index;
    slice[2] = keys ? snapshot.local.owner_sides : 0u;
}
void noah_lighting_slice_mode_status_idle(uint8_t *slice) { memset(slice, 0, 11u); slice[0] = slice[1] = 255u; }
bool noah_lighting_fact_mode_status_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    const uint8_t *slice = facts->mode_status;
    uint8_t mode = params[1] == 1u ? slice[1] : slice[0];
    if ((*cursor)++ || mode >= PD_MODE_COUNT || (params[1] == 0u && mode == slice[1]) || (params[0] != 255u && params[0] != mode)) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_POINTING_MODE, mode, 0u);
    key_origin_bitmap_copy(subject->keys, slice + 3u);
    subject->has_keys = key_origin_bitmap_has_any(subject->keys);
    subject->sides = slice[2];
    return true;
}
#endif

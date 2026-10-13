// Fact 3, pointing mode active: the mode shown, with the keys and halves of
// the key or combo that started it. Local: the runtime base packet already
// gives the other half the shown mode and its owner.
#include "../lighting.h"

#if NOAH_LIGHTING_FACT_POINTING_MODE_ACTIVE

#    include "../../../pointing/defs/pd_mode_flags.h"
#    include "../../../pointing/defs/pd_modes.h"

enum { SLICE_MODE = 0u, SLICE_SIDES, SLICE_HAS_KEYS, SLICE_KEYS };

void noah_lighting_slice_pointing_mode_active_collect(uint8_t *slice) {
    bool               has_keys = false;
    pd_mode_snapshot_t snapshot = pd_mode_snapshot_with_owner_bitmap(&slice[SLICE_KEYS], &has_keys);

    slice[SLICE_MODE]     = snapshot.display.active_index;
    slice[SLICE_SIDES]    = snapshot.display.owner_sides;
    slice[SLICE_HAS_KEYS] = has_keys ? 1u : 0u;
}

bool noah_lighting_fact_pointing_mode_active_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    const uint8_t *slice = facts->pointing_mode_active;
    uint8_t        mode  = slice[SLICE_MODE];

    if ((*cursor)++ != 0u || mode >= PD_MODE_COUNT || (params[0] != NOAH_PROFILE_LIGHTING_V1_ANY && params[0] != mode)) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_POINTING_MODE, mode, 0u);
    subject->sides    = slice[SLICE_SIDES];
    subject->has_keys = slice[SLICE_HAS_KEYS] != 0u;
    if (subject->has_keys) key_origin_bitmap_copy(subject->keys, &slice[SLICE_KEYS]);
    return true;
}

#endif

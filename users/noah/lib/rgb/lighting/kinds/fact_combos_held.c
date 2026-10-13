// Fact 4, combos held: one subject for every held combo of a role (owning the
// shown preview or mode, or the others), excluding a combo whose output holds
// a momentary layer. Synced from the USB half.
#include "../lighting.h"

#if NOAH_LIGHTING_FACT_COMBOS_HELD

void noah_lighting_slice_combos_held_collect(uint8_t *slice) {
    combo_feedback_bitmaps(slice, &slice[KEY_ORIGIN_BITMAP_SIZE]);
}

bool noah_lighting_fact_combos_held_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    const uint8_t *owning = facts->combos_held;
    const uint8_t *others = &facts->combos_held[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t        keys[KEY_ORIGIN_BITMAP_SIZE];

    if ((*cursor)++ != 0u) return false;
    key_origin_bitmap_clear(keys);
    if (params[0] != 1u) key_origin_bitmap_or_inplace(keys, owning);
    if (params[0] != 0u) key_origin_bitmap_or_inplace(keys, others);
    if (!key_origin_bitmap_has_any(keys)) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_COMBO, NOAH_PROFILE_LIGHTING_V1_ANY, 0u);
    key_origin_bitmap_copy(subject->keys, keys);
    subject->has_keys = true;
    subject->sides    = key_origin_bitmap_side_mask(keys);
    return true;
}

#endif

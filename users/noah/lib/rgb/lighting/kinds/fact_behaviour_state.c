// Fact 5, behaviour state: gestures whose shown state (branch selected, tap,
// hold, long hold) is in the mask. The slice carries the key runtime's maps
// exactly as it produces them: per-key state, branch and flash visibility, and
// the newest gesture owning each surface. Synced from the USB half.
#include "../lighting.h"

#if NOAH_LIGHTING_FACT_BEHAVIOUR_STATE

enum {
    SLICE_VISIBLE = 0u,
    SLICE_SEMANTIC = SLICE_VISIBLE + KEY_ORIGIN_BITMAP_SIZE,
    SLICE_OWNERS = SLICE_SEMANTIC + KEY_FEEDBACK_SEMANTIC_MAP_SIZE,
    SLICE_BRANCH = SLICE_OWNERS + KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE,
    KEY_COUNT = MATRIX_ROWS * MATRIX_COLS,
};

void noah_lighting_slice_behaviour_state_collect(uint8_t *slice) {
    key_feedback_semantic_map(&slice[SLICE_SEMANTIC]);
    key_feedback_tap_branch_map(&slice[SLICE_BRANCH]);
    key_feedback_flash_visibility_bitmap_for_semantic_map(&slice[SLICE_SEMANTIC], &slice[SLICE_VISIBLE]);
    key_feedback_broad_owner_map(&slice[SLICE_OWNERS]);
#    if defined(COMBO_ENABLE) && !NOAH_LIGHTING_FACT_COMBOS_HELD
    // Without the combos fact the frame carries no footprint to read below,
    // so the slice drops the branches a held combo covers as it is built.
    {
        uint8_t owning[KEY_ORIGIN_BITMAP_SIZE];
        uint8_t others[KEY_ORIGIN_BITMAP_SIZE];

        combo_feedback_bitmaps(owning, others);
        key_origin_bitmap_or_inplace(owning, others);
        for (uint8_t row = 0u; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0u; col < MATRIX_COLS; col++) {
                keypos_t key = {.row = row, .col = col};
                if (key_origin_bitmap_has_keypos(owning, key) && key_feedback_semantic_map_get(&slice[SLICE_SEMANTIC], key) == KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING) {
                    key_feedback_semantic_map_set(&slice[SLICE_SEMANTIC], key, KEY_FEEDBACK_SEMANTIC_NONE);
                }
            }
        }
    }
#    endif
}

void noah_lighting_slice_behaviour_state_idle(uint8_t *slice) {
    key_origin_bitmap_clear(&slice[SLICE_VISIBLE]);
    key_feedback_semantic_map_clear(&slice[SLICE_SEMANTIC]);
    key_feedback_broad_owner_map_clear(&slice[SLICE_OWNERS]);
    key_feedback_tap_branch_map_clear(&slice[SLICE_BRANCH]);
}

// A key in a held combo's footprint never shows a selected branch: the combo
// already lights that key. Read from the frame's combos slice, so both halves
// apply it to the same facts.
static bool combo_covers(const noah_lighting_facts_t *facts, keypos_t key) {
#    if NOAH_LIGHTING_FACT_COMBOS_HELD
    return key_origin_bitmap_has_keypos(facts->combos_held, key) || key_origin_bitmap_has_keypos(&facts->combos_held[KEY_ORIGIN_BITMAP_SIZE], key);
#    else
    (void)facts;
    (void)key;
    return false;
#    endif
}

// The state a key shows now: a flashing state counts only while lit.
static uint8_t shown_state(const noah_lighting_facts_t *facts, keypos_t key, uint8_t *branch) {
    const uint8_t          *slice    = facts->behaviour_state;
    key_feedback_semantic_t semantic = key_feedback_semantic_map_get(&slice[SLICE_SEMANTIC], key);

    if (key_feedback_semantic_is_flashing(semantic) && !key_origin_bitmap_has_keypos(&slice[SLICE_VISIBLE], key)) return 0u;
    switch (semantic) {
        case KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING:
            if (combo_covers(facts, key)) return 0u;
            *branch = key_feedback_tap_branch_map_get(&slice[SLICE_BRANCH], key);
            return NOAH_LIGHTING_STATE_BRANCH;
        case KEY_FEEDBACK_SEMANTIC_TAP_COMMITTED:
            return NOAH_LIGHTING_STATE_TAP;
        case KEY_FEEDBACK_SEMANTIC_HOLD_PENDING:
        case KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING:
            return NOAH_LIGHTING_STATE_HOLD;
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_STEADY:
        case KEY_FEEDBACK_SEMANTIC_LONG_HOLD_ACTIVE_FLASHING:
            return NOAH_LIGHTING_STATE_LONG_HOLD;
        default:
            return 0u;
    }
}

static uint8_t identity_slot(uint8_t state, uint8_t branch) {
    switch (state) {
        case NOAH_LIGHTING_STATE_HOLD:
            return NOAH_LIGHTING_BEHAVIOUR_SLOT_HOLD;
        case NOAH_LIGHTING_STATE_LONG_HOLD:
            return NOAH_LIGHTING_BEHAVIOUR_SLOT_LONG_HOLD;
        case NOAH_LIGHTING_STATE_TAP:
            return NOAH_LIGHTING_BEHAVIOUR_SLOT_TAP;
        default: {
            // Branch 2 takes the first branch colour; deeper ones clamp to the last.
            uint8_t last  = NOAH_PROFILE_TAP_DEPTH >= 2u ? (uint8_t)(NOAH_PROFILE_TAP_DEPTH - 2u) : 0u;
            uint8_t index = branch <= 2u ? 0u : (uint8_t)(branch - 2u);
            return (uint8_t)(NOAH_LIGHTING_BEHAVIOUR_SLOT_BRANCH + (index > last ? last : index));
        }
    }
}

// A subject for the gesture at `key` if its state is in `mask`.
static bool gesture(const noah_lighting_facts_t *facts, keypos_t key, uint8_t mask, uint8_t sides, noah_lighting_subject_t *subject) {
    uint8_t branch = 0u;
    uint8_t state;

    if (!key_origin_keypos_valid(key)) return false;
    state = shown_state(facts, key, &branch);
    if ((state & mask) == 0u) return false;
    noah_lighting_subject_init(subject, NOAH_LIGHTING_THING_BEHAVIOUR, NOAH_PROFILE_LIGHTING_V1_ANY, identity_slot(state, branch));
    key_origin_bitmap_fill_single(subject->keys, key);
    subject->has_keys = true;
    subject->sides    = sides ? sides : split_side_mask_add_keypos(SPLIT_SIDE_MASK_NONE, key);
    return true;
}

bool noah_lighting_fact_behaviour_state_next(const noah_lighting_facts_t *facts, const uint8_t *params, uint16_t *cursor, noah_lighting_subject_t *subject) {
    static const uint8_t per_state[4][2] = {
        {NOAH_LIGHTING_STATE_LONG_HOLD, KEY_FEEDBACK_BROAD_OWNER_GROUP_LONG_HOLD_ACTIVE},
        {NOAH_LIGHTING_STATE_HOLD, KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE},
        {NOAH_LIGHTING_STATE_TAP, KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_COMMITTED},
        {NOAH_LIGHTING_STATE_BRANCH, KEY_FEEDBACK_BROAD_OWNER_GROUP_TAP_BRANCH_PENDING},
    };
    const uint8_t *slice  = facts->behaviour_state;
    const uint8_t *owners = &slice[SLICE_OWNERS];
    uint8_t        mask   = params[0];

    switch (params[1]) {
        case NOAH_LIGHTING_SCOPE_EACH_KEY:
            while (*cursor < KEY_COUNT) {
                keypos_t key = {.row = (uint8_t)(*cursor / MATRIX_COLS), .col = (uint8_t)(*cursor % MATRIX_COLS)};
                (*cursor)++;
                if (gesture(facts, key, mask, SPLIT_SIDE_MASK_NONE, subject)) return true;
            }
            return false;
        case NOAH_LIGHTING_SCOPE_PER_HALF:
            while (*cursor < 2u) {
                bool left = *cursor == 0u;
                (*cursor)++;
                if (gesture(facts, key_feedback_broad_owner_map_get(owners, left ? KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF : KEY_FEEDBACK_BROAD_OWNER_RIGHT_HALF), mask, left ? SPLIT_SIDE_MASK_LEFT : SPLIT_SIDE_MASK_RIGHT, subject)) return true;
            }
            return false;
        case NOAH_LIGHTING_SCOPE_NEWEST:
            return (*cursor)++ == 0u && gesture(facts, key_feedback_broad_owner_map_get(owners, KEY_FEEDBACK_BROAD_OWNER_GLOBAL), mask, SPLIT_SIDE_MASK_NONE, subject);
        default:
            while (*cursor < 4u) {
                const uint8_t *row = per_state[*cursor];
                (*cursor)++;
                if ((mask & row[0]) != 0u && gesture(facts, key_feedback_broad_owner_map_get(owners, (key_feedback_broad_owner_slot_t)row[1]), row[0], SPLIT_SIDE_MASK_NONE, subject)) return true;
            }
            return false;
    }
}

#endif

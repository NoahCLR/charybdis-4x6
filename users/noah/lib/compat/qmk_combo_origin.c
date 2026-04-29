// ────────────────────────────────────────────────────────────────────────────
// QMK Combo Origin Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_combo_origin.h"

#if defined(COMBO_ENABLE)

#    include "noah_keymap_ids.h"

#    ifndef COMBO_ONLY_FROM_LAYER
uint8_t combo_ref_from_layer(uint8_t layer);
#    endif

typedef struct {
    bool     pressed;
    uint16_t combo_keycode;
    uint16_t pressed_at;
    uint32_t press_sequence;
} combo_origin_physical_key_state_t;

typedef struct {
    bool     active;
    uint16_t combo_index;
    uint16_t keycode;
    keypos_t owner_key_pos;
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
} combo_origin_active_cache_entry_t;

typedef struct {
    bool     active;
    uint16_t combo_index;
    uint16_t keycode;
    keypos_t owner_key_pos;
    uint16_t complete_at;
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
} combo_origin_pending_output_entry_t;

static combo_origin_physical_key_state_t physical_key_states[MATRIX_ROWS * MATRIX_COLS];

#    ifndef COMBO_BUFFER_LENGTH
#        define COMBO_BUFFER_LENGTH 4
#    endif

static combo_origin_active_cache_entry_t   combo_active_cache[COMBO_BUFFER_LENGTH];
static combo_origin_pending_output_entry_t combo_pending_output_cache[COMBO_BUFFER_LENGTH];
static uint32_t                            combo_origin_press_sequence       = 0;
static keypos_t                            combo_origin_last_pressed_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS};

static uint16_t combo_origin_combo_keycode_for_record(keyrecord_t *record) {
    uint16_t keycode = get_record_keycode(record, true);

#    ifdef COMBO_ONLY_FROM_LAYER
    keycode = keymap_key_to_keycode(COMBO_ONLY_FROM_LAYER, record->event.key);
#    else
    uint8_t highest_layer = get_highest_layer(layer_state | default_layer_state);
    uint8_t ref_layer     = combo_ref_from_layer(highest_layer);

    if (ref_layer != highest_layer) {
        keycode = keymap_key_to_keycode(ref_layer, record->event.key);
    }
#    endif

    return keycode;
}

static combo_t *combo_origin_combo_get(uint16_t combo_index) {
    if (combo_index >= noah_combo_count) {
        return NULL;
    }

    return &key_combos[combo_index];
}

static void combo_origin_bitmap_fill_all_keys(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            key_origin_bitmap_add_keypos(out_bitmap, (keypos_t){.row = row, .col = col});
        }
    }
}

static bool combo_origin_combo_is_active(const combo_t *combo) {
    if (!combo) {
        return false;
    }

#    ifndef EXTRA_SHORT_COMBOS
    return combo->active;
#    else
    return (combo->state & 0x80u) != 0u;
#    endif
}

static bool combo_origin_combo_is_disabled(const combo_t *combo) {
    if (!combo) {
        return false;
    }

#    ifndef EXTRA_SHORT_COMBOS
    return combo->disabled;
#    else
    return (combo->state & 0x40u) != 0u;
#    endif
}

static bool combo_origin_combo_build_from_pressed_keys(uint16_t combo_index, uint8_t *out_bitmap, keypos_t *out_owner_key_pos, uint16_t *out_complete_at) {
    combo_t *combo;
    keypos_t owner_key_pos         = {0};
    uint32_t latest_press_sequence = 0;
    uint16_t complete_at           = 0;
    bool     found_any             = false;

    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }

    if (out_owner_key_pos) {
        *out_owner_key_pos = (keypos_t){0};
    }

    if (out_complete_at) {
        *out_complete_at = 0;
    }

    combo = combo_origin_combo_get(combo_index);
    if (!(combo && out_bitmap && out_owner_key_pos)) {
        return false;
    }

    for (uint16_t member_index = 0;; member_index++) {
        uint16_t member_keycode  = pgm_read_word(&combo->keys[member_index]);
        keypos_t matched_key_pos = {0};
        bool     matched         = false;

        if (member_keycode == COMBO_END) {
            break;
        }

        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                keypos_t                           key_pos      = {.row = row, .col = col};
                combo_origin_physical_key_state_t *physical_key = &physical_key_states[key_origin_keypos_index(key_pos)];

                if (!(physical_key->pressed && physical_key->combo_keycode == member_keycode)) {
                    continue;
                }

                if (matched) {
                    key_origin_bitmap_clear(out_bitmap);
                    return false;
                }

                matched         = true;
                matched_key_pos = key_pos;
            }
        }

        if (!matched) {
            key_origin_bitmap_clear(out_bitmap);
            return false;
        }

        // Keep the runtime owner stable across repeated combo activations even
        // when the physical member press order changes.
        key_origin_bitmap_add_keypos(out_bitmap, matched_key_pos);
        found_any     = true;
        owner_key_pos = matched_key_pos;

        combo_origin_physical_key_state_t *physical_key = &physical_key_states[key_origin_keypos_index(matched_key_pos)];
        if (physical_key->press_sequence >= latest_press_sequence) {
            latest_press_sequence = physical_key->press_sequence;
            complete_at           = physical_key->pressed_at;
        }
    }

    if (!found_any) {
        return false;
    }

    *out_owner_key_pos = owner_key_pos;
    if (out_complete_at) {
        *out_complete_at = complete_at;
    }
    return true;
}

static combo_origin_active_cache_entry_t *combo_origin_cache_entry_for_store(uint16_t combo_index, uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (!combo_active_cache[index].active) {
            continue;
        }

        if (combo_index != UINT16_MAX && combo_active_cache[index].combo_index == combo_index) {
            return &combo_active_cache[index];
        }

        if (combo_index == UINT16_MAX && combo_active_cache[index].combo_index == UINT16_MAX && combo_active_cache[index].keycode == keycode) {
            return &combo_active_cache[index];
        }
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (!combo_active_cache[index].active) {
            return &combo_active_cache[index];
        }
    }

    return &combo_active_cache[0];
}

static void combo_origin_cache_store(uint16_t combo_index, uint16_t keycode, keypos_t owner_key_pos, const uint8_t *bitmap) {
    combo_origin_active_cache_entry_t *entry = combo_origin_cache_entry_for_store(combo_index, keycode);

    if (!entry) {
        return;
    }

    entry->active        = true;
    entry->combo_index   = combo_index;
    entry->keycode       = keycode;
    entry->owner_key_pos = owner_key_pos;
    key_origin_bitmap_copy(entry->bitmap, bitmap);
}

static void combo_origin_cache_clear(uint16_t combo_index, uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (!entry->active) {
            continue;
        }

        if (combo_index != UINT16_MAX) {
            if (entry->combo_index != combo_index) {
                continue;
            }
        } else if (!(entry->combo_index == UINT16_MAX && entry->keycode == keycode)) {
            continue;
        }

        entry->active        = false;
        entry->combo_index   = UINT16_MAX;
        entry->keycode       = KC_NO;
        entry->owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
        key_origin_bitmap_clear(entry->bitmap);
        return;
    }
}

static combo_origin_pending_output_entry_t *combo_origin_pending_output_entry_for_store(uint16_t combo_index, uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        if (!combo_pending_output_cache[index].active) {
            continue;
        }

        if (combo_pending_output_cache[index].combo_index == combo_index) {
            return &combo_pending_output_cache[index];
        }
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        if (!combo_pending_output_cache[index].active) {
            return &combo_pending_output_cache[index];
        }
    }

    (void)keycode;
    return &combo_pending_output_cache[0];
}

static void combo_origin_pending_output_store(uint16_t combo_index, uint16_t keycode, keypos_t owner_key_pos, uint16_t complete_at, const uint8_t *bitmap) {
    combo_origin_pending_output_entry_t *entry = combo_origin_pending_output_entry_for_store(combo_index, keycode);

    if (!entry) {
        return;
    }

    entry->active        = true;
    entry->combo_index   = combo_index;
    entry->keycode       = keycode;
    entry->owner_key_pos = owner_key_pos;
    entry->complete_at   = complete_at;
    key_origin_bitmap_copy(entry->bitmap, bitmap);
}

static void combo_origin_pending_output_clear(uint16_t combo_index, uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (!entry->active) {
            continue;
        }

        if (combo_index != UINT16_MAX) {
            if (entry->combo_index != combo_index) {
                continue;
            }
        } else if (entry->keycode != keycode) {
            continue;
        }

        entry->active        = false;
        entry->combo_index   = UINT16_MAX;
        entry->keycode       = KC_NO;
        entry->owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
        entry->complete_at   = 0;
        key_origin_bitmap_clear(entry->bitmap);
    }
}

static void combo_origin_note_completed_pressed_combos(void) {
    for (uint16_t combo_index = 0; combo_index < noah_combo_count; combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t owner_key_pos = {0};
        uint16_t complete_at   = 0;

        if (!combo || combo_origin_combo_is_disabled(combo)) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &owner_key_pos, &complete_at)) {
            combo_origin_pending_output_store(combo_index, combo->keycode, owner_key_pos, complete_at, combo_bitmap);
        }
    }
}

static bool combo_origin_latest_pressed_keypos(keypos_t *out_owner_key_pos) {
    uint32_t latest_press_sequence = 0;
    keypos_t latest_key_pos        = {0};
    bool     found_any             = false;

    if (out_owner_key_pos) {
        *out_owner_key_pos = (keypos_t){0};
    }

    if (!out_owner_key_pos) {
        return false;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                           key_pos      = {.row = row, .col = col};
            combo_origin_physical_key_state_t *physical_key = &physical_key_states[key_origin_keypos_index(key_pos)];

            if (!(physical_key->pressed && physical_key->press_sequence >= latest_press_sequence)) {
                continue;
            }

            latest_press_sequence = physical_key->press_sequence;
            latest_key_pos        = key_pos;
            found_any             = true;
        }
    }

    if (!found_any) {
        return false;
    }

    *out_owner_key_pos = latest_key_pos;
    return true;
}

static bool combo_origin_fallback_owner_keypos(keypos_t *out_owner_key_pos) {
    if (!out_owner_key_pos) {
        return false;
    }

    *out_owner_key_pos = (keypos_t){0};
    if (combo_origin_latest_pressed_keypos(out_owner_key_pos)) {
        return true;
    }

    if (key_origin_keypos_valid(combo_origin_last_pressed_key_pos)) {
        *out_owner_key_pos = combo_origin_last_pressed_key_pos;
        return true;
    }

    return false;
}

static bool combo_origin_collect_matching_active_combos(uint16_t keycode, uint16_t *primary_combo_index, keypos_t *primary_owner_key_pos, uint8_t *out_bitmap) {
    bool matched = false;

    if (primary_combo_index) {
        *primary_combo_index = UINT16_MAX;
    }
    if (primary_owner_key_pos) {
        *primary_owner_key_pos = (keypos_t){0};
    }
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }

    for (uint16_t combo_index = 0; combo_index < noah_combo_count; combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t combo_owner_key_pos = {0};

        if (!(combo && combo_origin_combo_is_active(combo) && combo->keycode == keycode)) {
            continue;
        }

        if (!combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &combo_owner_key_pos, NULL)) {
            continue;
        }

        if (!matched && primary_combo_index && primary_owner_key_pos) {
            *primary_combo_index   = combo_index;
            *primary_owner_key_pos = combo_owner_key_pos;
        }

        matched = true;
        if (out_bitmap) {
            for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
                out_bitmap[index] |= combo_bitmap[index];
            }
        }
    }

    return matched;
}

static bool combo_origin_collect_matching_pending_outputs(uint16_t keycode, uint16_t *primary_combo_index, keypos_t *primary_owner_key_pos, uint8_t *out_bitmap) {
    bool matched = false;

    if (primary_combo_index) {
        *primary_combo_index = UINT16_MAX;
    }
    if (primary_owner_key_pos) {
        *primary_owner_key_pos = (keypos_t){0};
    }
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (!(entry->active && entry->keycode == keycode)) {
            continue;
        }

        if (!matched && primary_combo_index && primary_owner_key_pos) {
            *primary_combo_index   = entry->combo_index;
            *primary_owner_key_pos = entry->owner_key_pos;
        }

        matched = true;
        if (out_bitmap) {
            key_origin_bitmap_or_inplace(out_bitmap, entry->bitmap);
        }
    }

    return matched;
}

static bool combo_origin_collect_matching_cached_combos(uint16_t keycode, uint16_t *primary_combo_index, keypos_t *primary_owner_key_pos, uint8_t *out_bitmap) {
    bool matched = false;

    if (primary_combo_index) {
        *primary_combo_index = UINT16_MAX;
    }
    if (primary_owner_key_pos) {
        *primary_owner_key_pos = (keypos_t){0};
    }
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (!(entry->active && entry->keycode == keycode)) {
            continue;
        }

        if (!matched && primary_combo_index && primary_owner_key_pos) {
            *primary_combo_index   = entry->combo_index;
            *primary_owner_key_pos = entry->owner_key_pos;
        }

        matched = true;
        if (out_bitmap) {
            for (uint8_t byte = 0; byte < KEY_ORIGIN_BITMAP_SIZE; byte++) {
                out_bitmap[byte] |= entry->bitmap[byte];
            }
        }
    }

    return matched;
}

void noah_qmk_combo_origin_reset(void) {
    combo_origin_press_sequence       = 0;
    combo_origin_last_pressed_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};

    for (uint16_t index = 0; index < ARRAY_SIZE(physical_key_states); index++) {
        physical_key_states[index] = (combo_origin_physical_key_state_t){0};
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_active_cache[index].active        = false;
        combo_active_cache[index].combo_index   = UINT16_MAX;
        combo_active_cache[index].keycode       = KC_NO;
        combo_active_cache[index].owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
        key_origin_bitmap_clear(combo_active_cache[index].bitmap);
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_pending_output_cache[index].active        = false;
        combo_pending_output_cache[index].combo_index   = UINT16_MAX;
        combo_pending_output_cache[index].keycode       = KC_NO;
        combo_pending_output_cache[index].owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
        combo_pending_output_cache[index].complete_at   = 0;
        key_origin_bitmap_clear(combo_pending_output_cache[index].bitmap);
    }
}

void noah_qmk_combo_origin_init(void) {
    noah_qmk_combo_origin_reset();
}

void noah_qmk_combo_origin_observe_physical_key_event(uint16_t keycode, keyrecord_t *record) {
    combo_origin_physical_key_state_t *entry;

    (void)keycode;

    if (!(record && key_origin_keypos_valid(record->event.key))) {
        return;
    }

    entry = &physical_key_states[key_origin_keypos_index(record->event.key)];
    if (record->event.pressed) {
        entry->pressed                    = true;
        entry->combo_keycode              = combo_origin_combo_keycode_for_record(record);
        entry->pressed_at                 = timer_read();
        entry->press_sequence             = ++combo_origin_press_sequence;
        combo_origin_last_pressed_key_pos = record->event.key;
        combo_origin_note_completed_pressed_combos();
        return;
    }

    entry->pressed       = false;
    entry->combo_keycode = KC_NO;
    entry->pressed_at    = 0;
}

void noah_qmk_combo_origin_normalize_record(uint16_t keycode, keyrecord_t *record) {
    uint16_t combo_index   = UINT16_MAX;
    keypos_t owner_key_pos = {0};
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    bool     matched = false;

    if (!(record && record->event.type == COMBO_EVENT)) {
        return;
    }

    if (record->event.pressed) {
        matched = combo_origin_collect_matching_active_combos(keycode, &combo_index, &owner_key_pos, bitmap);
        if (!matched) {
            matched = combo_origin_collect_matching_pending_outputs(keycode, &combo_index, &owner_key_pos, bitmap);
        }
        if (matched && key_origin_keypos_valid(owner_key_pos)) {
            record->event.key = owner_key_pos;
            key_origin_registry_set_bitmap(owner_key_pos, key_origin_bitmap_has_any(bitmap) ? bitmap : NULL);
            if (combo_index != UINT16_MAX) {
                combo_origin_cache_store(combo_index, keycode, owner_key_pos, bitmap);
            }
        } else if (combo_origin_fallback_owner_keypos(&owner_key_pos)) {
            combo_origin_bitmap_fill_all_keys(bitmap);
            record->event.key = owner_key_pos;
            key_origin_registry_set_bitmap(owner_key_pos, bitmap);
            combo_origin_cache_store(UINT16_MAX, keycode, owner_key_pos, bitmap);
        }
        return;
    }

    matched = combo_origin_collect_matching_cached_combos(keycode, &combo_index, &owner_key_pos, bitmap);
    if (matched && key_origin_keypos_valid(owner_key_pos)) {
        record->event.key = owner_key_pos;
        key_origin_registry_set_bitmap(owner_key_pos, key_origin_bitmap_has_any(bitmap) ? bitmap : NULL);
    }
    if (matched) {
        combo_origin_cache_clear(combo_index, keycode);
        combo_origin_pending_output_clear(combo_index, keycode);
    }
}

void noah_qmk_combo_origin_pressed_combo_bitmap(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);

    for (uint16_t combo_index = 0; combo_index < noah_combo_count; combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t owner_key_pos = {0};

        if (!combo || combo_origin_combo_is_disabled(combo)) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &owner_key_pos, NULL)) {
            key_origin_bitmap_or_inplace(out_bitmap, combo_bitmap);
        }
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        const combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (entry->active) {
            key_origin_bitmap_or_inplace(out_bitmap, entry->bitmap);
        }
    }
}

bool noah_qmk_combo_origin_pressed_combo_matches(uint16_t keycode, keypos_t owner_key_pos, uint16_t since, uint16_t term_ms) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        const combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (entry->active && entry->keycode == keycode && entry->owner_key_pos.row == owner_key_pos.row && entry->owner_key_pos.col == owner_key_pos.col && (uint16_t)(entry->complete_at - since) <= term_ms) {
            return true;
        }
    }

    for (uint16_t combo_index = 0; combo_index < noah_combo_count; combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t combo_owner_key_pos = {0};
        uint16_t combo_complete_at   = 0;

        if (!combo || combo_origin_combo_is_disabled(combo) || combo->keycode != keycode) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &combo_owner_key_pos, &combo_complete_at) && combo_owner_key_pos.row == owner_key_pos.row && combo_owner_key_pos.col == owner_key_pos.col && (uint16_t)(combo_complete_at - since) <= term_ms) {
            return true;
        }
    }

    return false;
}

void noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    if (out_underlay_bitmap) {
        key_origin_bitmap_clear(out_underlay_bitmap);
    }

    if (out_overlay_bitmap) {
        key_origin_bitmap_clear(out_overlay_bitmap);
    }

    if (!(out_underlay_bitmap && out_overlay_bitmap)) {
        return;
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];
        bool                               underlay_owner;

        if (!entry->active) {
            continue;
        }

        underlay_owner = (key_origin_keypos_valid(preview_owner_key_pos) && key_origin_keypos_valid(entry->owner_key_pos) && entry->owner_key_pos.row == preview_owner_key_pos.row && entry->owner_key_pos.col == preview_owner_key_pos.col) || (key_origin_keypos_valid(pd_owner_key_pos) && key_origin_keypos_valid(entry->owner_key_pos) && entry->owner_key_pos.row == pd_owner_key_pos.row && entry->owner_key_pos.col == pd_owner_key_pos.col);

        if (underlay_owner) {
            key_origin_bitmap_or_inplace(out_underlay_bitmap, entry->bitmap);
        } else {
            key_origin_bitmap_or_inplace(out_overlay_bitmap, entry->bitmap);
        }
    }
}

bool noah_qmk_combo_origin_event_owner_keypos(const keyrecord_t *record, keypos_t *out) {
    if (out) {
        *out = (keypos_t){0};
    }

    if (!(record && out && key_origin_keypos_valid(record->event.key))) {
        return false;
    }

    *out = record->event.key;
    return true;
}

bool noah_qmk_combo_origin_event_bitmap(const keyrecord_t *record, uint8_t *out_bitmap) {
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }

    if (!(record && out_bitmap)) {
        return false;
    }

    return key_origin_registry_get_bitmap(record->event.key, out_bitmap);
}

split_side_mask_t noah_qmk_combo_origin_event_side_mask(const keyrecord_t *record) {
    if (!record) {
        return SPLIT_SIDE_MASK_NONE;
    }

    return key_origin_registry_side_mask(record->event.key);
}

#else

void noah_qmk_combo_origin_init(void) {}
void noah_qmk_combo_origin_reset(void) {}
void noah_qmk_combo_origin_observe_physical_key_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}
void noah_qmk_combo_origin_normalize_record(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}
void noah_qmk_combo_origin_pressed_combo_bitmap(uint8_t *out_bitmap) {
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }
}
bool noah_qmk_combo_origin_pressed_combo_matches(uint16_t keycode, keypos_t owner_key_pos, uint16_t since, uint16_t term_ms) {
    (void)keycode;
    (void)owner_key_pos;
    (void)since;
    (void)term_ms;
    return false;
}
void noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    (void)preview_owner_key_pos;
    (void)pd_owner_key_pos;
    if (out_underlay_bitmap) {
        key_origin_bitmap_clear(out_underlay_bitmap);
    }
    if (out_overlay_bitmap) {
        key_origin_bitmap_clear(out_overlay_bitmap);
    }
}
bool noah_qmk_combo_origin_event_owner_keypos(const keyrecord_t *record, keypos_t *out) {
    (void)record;
    if (out) {
        *out = (keypos_t){0};
    }
    return false;
}
bool noah_qmk_combo_origin_event_bitmap(const keyrecord_t *record, uint8_t *out_bitmap) {
    (void)record;
    if (out_bitmap) {
        key_origin_bitmap_clear(out_bitmap);
    }
    return false;
}
split_side_mask_t noah_qmk_combo_origin_event_side_mask(const keyrecord_t *record) {
    (void)record;
    return SPLIT_SIDE_MASK_NONE;
}

#endif

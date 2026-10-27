// ────────────────────────────────────────────────────────────────────────────
// QMK Combo Origin Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_combo_origin.h"
#include "qmk_source_layer_contract.h"
#include "../key/behavior/participation.h"

#if defined(COMBO_ENABLE)

#    include "qmk_active_profile_combos.h"
#    include "../profile/storage/profile_storage_layout.h"
#    include "../split/runtime_sync.h"
#    include "../profile/runtime/active_profile_settings_runtime.h"

#    ifndef COMBO_ONLY_FROM_LAYER
uint8_t combo_ref_from_layer(uint8_t layer);
#    endif
#    ifdef COMBO_TERM_PER_COMBO
uint16_t get_combo_term(uint16_t combo_index, combo_t *combo);
#    endif

typedef struct {
    bool     pressed;
    bool     combo_allowed;
    uint8_t  source_layer;
    uint16_t combo_keycode;
    uint16_t pressed_at;
    uint32_t press_sequence;
} combo_origin_physical_key_state_t;

typedef struct {
    uint32_t generation;
    uint16_t combo_index;
    uint16_t keycode;
    uint16_t complete_at;
    keypos_t owner_key_pos;
    uint8_t  origin_layer;
    bool     output_behavior;
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    bool     active;
    // When QMK delivered this output's release (combo_origin_next_delivery()),
    // or 0 while it has not.
    uint16_t release_delivered;
} combo_origin_active_cache_entry_t;

typedef struct {
    uint32_t generation;
    uint16_t combo_index;
    uint16_t keycode;
    uint16_t complete_at;
    uint16_t applied_at;
    keypos_t owner_key_pos;
    uint8_t  origin_layer;
    bool     output_behavior;
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    bool     active : 1;
    // QMK fired this completion: its output press is delivered now or queued
    // behind an undecided tap-hold key, possibly after the chord is released.
    bool     applied : 1;
    bool     deadline_crossed : 1;
    // QMK delivered its output press, which can wait behind an undecided
    // tap-hold key before dispatch.
    bool     press_delivered : 1;
    // When QMK delivered its output release while the press still waited, as
    // in the active cache, or 0.
    uint16_t release_delivered;
} combo_origin_pending_output_entry_t;

static combo_origin_physical_key_state_t physical_key_states[MATRIX_ROWS * MATRIX_COLS];

#    ifndef COMBO_BUFFER_LENGTH
#        define COMBO_BUFFER_LENGTH 4
#    endif

// One completion candidate per combo index. A combo can only complete again
// after a member is released, and that release makes QMK fire or drop the
// earlier completion first, so a newer generation always supersedes an older
// one. Bounded by construction; no burst of chords can refuse a candidate.
#    define COMBO_ORIGIN_PENDING_CAPACITY NOAH_PROFILE_WIRE_V1_MAX_COMBOS

#    define COMBO_ORIGIN_PENDING_USED_WORDS ((COMBO_ORIGIN_PENDING_CAPACITY + 31u) / 32u)

static combo_origin_active_cache_entry_t      combo_active_cache[COMBO_BUFFER_LENGTH];
static combo_origin_pending_output_entry_t    combo_pending_output_cache[COMBO_ORIGIN_PENDING_CAPACITY];
// Which pending slots are active, so the per-scan reconcile costs nothing
// while no completion waits and visits only the ones that do. Kept by
// combo_origin_pending_output_store() and combo_origin_pending_output_entry_clear().
static uint32_t                               combo_pending_output_used[COMBO_ORIGIN_PENDING_USED_WORDS];
static noah_qmk_combo_origin_debug_snapshot_t combo_origin_diagnostics;
static uint32_t                               combo_origin_press_sequence        = 0;
static uint16_t                               combo_origin_delivery_sequence     = 0;
static uint16_t                               combo_origin_last_pressed_at       = 0;
static keypos_t                               combo_origin_last_pressed_key_pos  = {.row = MATRIX_ROWS, .col = MATRIX_COLS};
static keypos_t                               combo_origin_last_released_key_pos = {.row = MATRIX_ROWS, .col = MATRIX_COLS};

#    ifdef COMBO_ORIGIN_TEST_INSTRUMENTATION
static uint16_t combo_origin_pending_slot_visits;

void noah_qmk_combo_origin_test_pending_slot_visits_reset(void) {
    combo_origin_pending_slot_visits = 0u;
}
uint16_t noah_qmk_combo_origin_test_pending_slot_visits(void) {
    return combo_origin_pending_slot_visits;
}
#        define COMBO_ORIGIN_RECORD_PENDING_SLOT_VISIT() (combo_origin_pending_slot_visits++)
#    else
#        define COMBO_ORIGIN_RECORD_PENDING_SLOT_VISIT() ((void)0)
#    endif

static void combo_origin_increment_counter(uint16_t *counter) {
    if (counter && *counter != UINT16_MAX) {
        (*counter)++;
    }
}

static void combo_origin_pending_output_mark_used(uint8_t index, bool used) {
    uint32_t bit = (uint32_t)1u << (index % 32u);

    if (used) {
        combo_pending_output_used[index / 32u] |= bit;
    } else {
        combo_pending_output_used[index / 32u] &= ~bit;
    }
}

static bool combo_origin_pending_output_any_used(void) {
    for (uint8_t word = 0; word < COMBO_ORIGIN_PENDING_USED_WORDS; word++) {
        if (combo_pending_output_used[word] != 0u) {
            return true;
        }
    }
    return false;
}

static uint8_t combo_origin_pending_count(void) {
    uint8_t count = 0;

    for (uint8_t word = 0; word < COMBO_ORIGIN_PENDING_USED_WORDS; word++) {
        count = (uint8_t)(count + __builtin_popcount(combo_pending_output_used[word]));
    }
    return count;
}

static uint8_t combo_origin_active_count(void) {
    uint8_t count = 0;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (combo_active_cache[index].active) {
            count++;
        }
    }
    return count;
}

static bool combo_origin_generation_is_live(uint32_t generation) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        if (combo_pending_output_cache[index].active && combo_pending_output_cache[index].generation == generation) {
            return true;
        }
    }
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (combo_active_cache[index].active && combo_active_cache[index].generation == generation) {
            return true;
        }
    }
    return false;
}

static uint32_t combo_origin_allocate_press_sequence(void) {
    do {
        combo_origin_press_sequence++;
        if (combo_origin_press_sequence == 0u) {
            combo_origin_press_sequence++;
        }
    } while (combo_origin_generation_is_live(combo_origin_press_sequence));

    return combo_origin_press_sequence;
}

// Orders the output releases QMK delivers. They leave record admission (and
// QMK's tapping queue, behind a TT() or OSL() to layers 16-31) in this order,
// so dispatch pairs each with its completion.
// Only a few are ever waiting, so sixteen bits compare without ambiguity.
static uint16_t combo_origin_next_delivery(void) {
    combo_origin_delivery_sequence++;
    if (combo_origin_delivery_sequence == 0u) {
        combo_origin_delivery_sequence++;
    }
    return combo_origin_delivery_sequence;
}

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
    if (combo_index >= noah_qmk_combo_count()) {
        return NULL;
    }

    return noah_qmk_combo_get(combo_index);
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

static bool combo_origin_combo_build_from_pressed_keys(uint16_t combo_index, uint8_t *out_bitmap, keypos_t *out_owner_key_pos, uint16_t *out_complete_at, uint32_t *out_generation) {
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
    if (out_generation) {
        *out_generation = 0;
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

                // Mirror the member-record gate: excluded occurrences never
                // joined this combo and cannot make its eligible origin
                // ambiguous. Keep the physical decision frozen at the press.
                if (!(physical_key->pressed && physical_key->combo_allowed && physical_key->combo_keycode == member_keycode)
                    || !noah_participation_combo_row(combo_index, physical_key->source_layer)) {
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
    if (out_generation) {
        *out_generation = latest_press_sequence;
    }
    return true;
}

static void combo_origin_cache_entry_clear(combo_origin_active_cache_entry_t *entry) {
    if (!entry) {
        return;
    }
    *entry               = (combo_origin_active_cache_entry_t){0};
    entry->combo_index   = UINT16_MAX;
    entry->keycode       = KC_NO;
    entry->owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static combo_origin_active_cache_entry_t *combo_origin_cache_entry_for_store(uint16_t combo_index, uint32_t generation, uint16_t keycode) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (!combo_active_cache[index].active) {
            continue;
        }

        if (combo_index != UINT16_MAX && combo_active_cache[index].combo_index == combo_index && combo_active_cache[index].generation == generation) {
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

    return NULL;
}

static combo_origin_active_cache_entry_t *combo_origin_cache_store(uint16_t combo_index, uint32_t generation, uint16_t keycode, keypos_t owner_key_pos, uint16_t complete_at, uint8_t origin_layer, bool output_behavior, const uint8_t *bitmap) {
    combo_origin_active_cache_entry_t *entry = combo_origin_cache_entry_for_store(combo_index, generation, keycode);

    if (!entry) {
        return NULL;
    }

    entry->active            = true;
    entry->generation        = generation;
    entry->combo_index       = combo_index;
    entry->keycode           = keycode;
    entry->complete_at       = complete_at;
    entry->owner_key_pos     = owner_key_pos;
    entry->origin_layer      = origin_layer;
    entry->output_behavior   = output_behavior;
    entry->release_delivered = 0;
    key_origin_bitmap_copy(entry->bitmap, bitmap);
    return entry;
}

// QMK releases a combo while it processes a member's physical release.
static bool combo_origin_bitmap_has_released_key(const uint8_t *bitmap) {
    return key_origin_keypos_valid(combo_origin_last_released_key_pos) && key_origin_bitmap_has_keypos(bitmap, combo_origin_last_released_key_pos);
}

// The dispatched completion whose release QMK delivers now.
static combo_origin_active_cache_entry_t *combo_origin_cache_entry_for_release(uint16_t keycode) {
    combo_origin_active_cache_entry_t *fallback = NULL;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (!(entry->active && entry->keycode == keycode && !entry->release_delivered)) {
            continue;
        }
        if (!fallback) {
            fallback = entry;
        }
        if (combo_origin_bitmap_has_released_key(entry->bitmap)) {
            return entry;
        }
    }

    return fallback;
}

static void combo_origin_pending_output_entry_clear(combo_origin_pending_output_entry_t *entry) {
    if (!entry) {
        return;
    }
    combo_origin_pending_output_mark_used((uint8_t)(entry - combo_pending_output_cache), false);
    *entry               = (combo_origin_pending_output_entry_t){0};
    entry->combo_index   = UINT16_MAX;
    entry->keycode       = KC_NO;
    entry->owner_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

static combo_origin_pending_output_entry_t *combo_origin_pending_output_entry_for_store(uint16_t combo_index) {
    return combo_index < ARRAY_SIZE(combo_pending_output_cache) ? &combo_pending_output_cache[combo_index] : NULL;
}

static bool combo_origin_pending_output_store(uint16_t combo_index, uint32_t generation, uint16_t keycode, keypos_t owner_key_pos, uint16_t complete_at, const uint8_t *bitmap) {
    combo_origin_pending_output_entry_t *entry;
    uint8_t                              pending_count;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (combo_active_cache[index].active && combo_active_cache[index].combo_index == combo_index && combo_active_cache[index].generation == generation) {
            return true;
        }
    }

    entry = combo_origin_pending_output_entry_for_store(combo_index);

    if (!entry) {
        combo_origin_increment_counter(&combo_origin_diagnostics.cache_full_refusal_count);
        return false;
    }
    if (entry->active && entry->generation == generation) {
        return true;
    }

    combo_origin_pending_output_mark_used((uint8_t)combo_index, true);
    entry->active            = true;
    entry->applied           = false;
    entry->deadline_crossed  = false;
    entry->press_delivered   = false;
    entry->release_delivered = 0;
    entry->generation        = generation;
    entry->combo_index       = combo_index;
    entry->keycode           = keycode;
    entry->owner_key_pos     = owner_key_pos;
    entry->origin_layer      = physical_key_states[key_origin_keypos_index(owner_key_pos)].source_layer;
    // Snapshot at completion: QMK can queue a generated output without
    // asking its tapping classification until a previous tap-hold resolves.
    entry->output_behavior   = noah_participation_combo_output(combo_index, keycode, entry->origin_layer);
    entry->complete_at       = complete_at;
    key_origin_bitmap_copy(entry->bitmap, bitmap);
    pending_count = combo_origin_pending_count();
    if (pending_count > combo_origin_diagnostics.pending_high_water) {
        combo_origin_diagnostics.pending_high_water = pending_count;
    }
    return true;
}

#    ifndef COMBO_NO_TIMER
static uint16_t combo_origin_latest_legal_wait_ms(void) {
    uint16_t wait_ms = COMBO_TERM;

    // QMK's combo_task waits on one buffer-wide longest_term, not a separate
    // deadline per candidate. Use the largest configured term in this profile
    // so a shorter candidate cannot expire while another buffered combo still
    // legally delays the shared flush.
#        ifdef COMBO_TERM_PER_COMBO
    for (uint16_t combo_index = 0; combo_index < noah_qmk_combo_count(); combo_index++) {
        combo_t *combo      = combo_origin_combo_get(combo_index);
        uint16_t combo_term = combo ? get_combo_term(combo_index, combo) : 0;

        if (combo_term > wait_ms) {
            wait_ms = combo_term;
        }
    }
#        endif

#        if defined(COMBO_MUST_HOLD_PER_COMBO) || defined(COMBO_MUST_HOLD_MODS) || defined(COMBO_MUST_TAP_PER_COMBO)
    // Any prepared hold/tap-only combo can extend that same shared deadline.
    // A profile-wide upper bound is conservative and remains contract-derived.
    if (wait_ms < COMBO_HOLD_TERM) {
        wait_ms = COMBO_HOLD_TERM;
    }
#        endif
    return wait_ms;
}

// A fired combo whose output press QMK has not delivered yet can only be
// waiting in QMK's tapping queue, behind a tap key QMK decides itself (a TT()
// or OSL() to layers 16-31), which resolves within QMK's tapping term: the
// dual-role setting (get_tapping_term). A delivered press has no deadline
// (combo_origin_pending_output_reconcile).
static uint16_t combo_origin_applied_wait_ms(void) {
#        ifdef NO_ACTION_TAPPING
    return 0;
#        else
    return noah_setting(NOAH_SETTING_TAPPING_TERM, TAPPING_TERM);
#        endif
}
#    endif

static void combo_origin_pending_output_reconcile(bool observe_deadline) {
    bool changed = false;

    if (!combo_origin_pending_output_any_used()) {
        return;
    }

#    ifndef COMBO_NO_TIMER
    uint16_t now = timer_read();
#    else
    (void)observe_deadline;
#    endif

    // Ascending slot order, as a walk of the whole cache would visit them.
    // Clearing the current slot does not disturb the copied word.
    for (uint8_t word = 0; word < COMBO_ORIGIN_PENDING_USED_WORDS; word++) {
        for (uint32_t used = combo_pending_output_used[word]; used != 0u; used &= used - 1u) {
            combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[word * 32u + (uint8_t)__builtin_ctz(used)];
            combo_t                             *combo;
            bool                                 active;

            COMBO_ORIGIN_RECORD_PENDING_SLOT_VISIT();
            combo  = combo_origin_combo_get(entry->combo_index);
            active = combo_origin_combo_is_active(combo);
            // QMK marks a fired combo both active and disabled (apply_combo, then
            // drop_combo_from_buffer). Only a disabled combo that never fired was
            // suppressed; a fired one keeps its footprint until its output arrives.
            if (combo && active && !entry->applied) {
                entry->applied          = true;
                entry->applied_at       = timer_read();
                entry->deadline_crossed = false;
            }
            if (!combo || (!entry->applied && combo_origin_combo_is_disabled(combo))) {
                combo_origin_pending_output_entry_clear(entry);
                combo_origin_increment_counter(&combo_origin_diagnostics.suppressed_retirement_count);
                changed = true;
                continue;
            }
#    ifndef COMBO_NO_TIMER
            // QMK restarts its combo timer at every combo key press, so a
            // completion not fired yet can legally wait from the latest press,
            // which is never earlier than its own completing press. A delivered
            // press waits in record admission for as long as the key ahead of it
            // is undecided, which no one setting bounds (a row's own hold time, a
            // long hold: D-F27), and admission always dispatches it, so its
            // completion stays until then.
            if (observe_deadline && !active && !entry->press_delivered && (entry->applied ? (uint16_t)(now - entry->applied_at) > combo_origin_applied_wait_ms() : (uint16_t)(now - combo_origin_last_pressed_at) > combo_origin_latest_legal_wait_ms())) {
                if (!entry->deadline_crossed) {
                    entry->deadline_crossed = true;
                } else {
                    combo_origin_pending_output_entry_clear(entry);
                    combo_origin_increment_counter(&combo_origin_diagnostics.deadline_expiry_count);
                    changed = true;
                }
            }
#    endif
        }
    }

    if (changed) {
        split_runtime_sync_notify_combo_dirty();
    }
}

static void combo_origin_note_completed_pressed_combos(void) {
    for (uint16_t combo_index = 0; combo_index < noah_qmk_combo_count(); combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t owner_key_pos = {0};
        uint16_t complete_at   = 0;
        uint32_t generation    = 0;

        if (!combo || combo_origin_combo_is_disabled(combo)) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &owner_key_pos, &complete_at, &generation)) {
            (void)combo_origin_pending_output_store(combo_index, generation, combo->keycode, owner_key_pos, complete_at, combo_bitmap);
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

static bool combo_origin_generation_before(uint32_t lhs, uint32_t rhs) {
    return (int32_t)(lhs - rhs) < 0;
}

static bool combo_origin_delivery_before(uint16_t lhs, uint16_t rhs) {
    return (int16_t)(uint16_t)(lhs - rhs) < 0;
}

// The completion whose output press QMK delivers now. QMK does not put the
// combo index in COMBO_EVENT; it delivers fired outputs in firing order, which
// is completion order. So: the oldest completion QMK has fired (its combo is
// active, or was seen active before its chord was let go) whose press it has
// not delivered yet. Two chords with the same output can both be delivered
// before either is dispatched, behind an undecided tap-hold key.
static combo_origin_pending_output_entry_t *combo_origin_pending_entry_for_delivery(uint16_t keycode) {
    combo_origin_pending_output_entry_t *selected = NULL;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (!(entry->active && entry->keycode == keycode && !entry->press_delivered && (entry->applied || combo_origin_combo_is_active(combo_origin_combo_get(entry->combo_index))))) {
            continue;
        }
        if (!selected || combo_origin_generation_before(entry->generation, selected->generation)) {
            selected = entry;
        }
    }
    return selected;
}

// The completion whose output press is dispatched now: delivered presses are
// dispatched in delivery order, which is completion order, and a press not
// delivered yet takes the completion its delivery would.
static combo_origin_pending_output_entry_t *combo_origin_pending_entry_for_dispatch(uint16_t keycode) {
    combo_origin_pending_output_entry_t *selected = NULL;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (entry->active && entry->keycode == keycode && entry->press_delivered && (!selected || combo_origin_generation_before(entry->generation, selected->generation))) {
            selected = entry;
        }
    }
    return selected ? selected : combo_origin_pending_entry_for_delivery(keycode);
}

// The completion whose release QMK delivers now while its press still waits to
// be dispatched: the one holding the released member, else the oldest.
static combo_origin_pending_output_entry_t *combo_origin_pending_entry_for_release(uint16_t keycode) {
    combo_origin_pending_output_entry_t *selected = NULL;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (!(entry->active && entry->keycode == keycode && entry->press_delivered && !entry->release_delivered)) {
            continue;
        }
        if (combo_origin_bitmap_has_released_key(entry->bitmap)) {
            return entry;
        }
        if (!selected || combo_origin_generation_before(entry->generation, selected->generation)) {
            selected = entry;
        }
    }
    return selected;
}

// The dispatched completion whose release is dispatched now: delivered
// releases are dispatched in delivery order.
static combo_origin_active_cache_entry_t *combo_origin_cache_entry_for_release_dispatch(uint16_t keycode) {
    combo_origin_active_cache_entry_t *selected = NULL;

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (entry->active && entry->keycode == keycode && entry->release_delivered && (!selected || combo_origin_delivery_before(entry->release_delivered, selected->release_delivered))) {
            selected = entry;
        }
    }
    return selected ? selected : combo_origin_cache_entry_for_release(keycode);
}

static bool combo_origin_active_cache_has_combo(uint16_t combo_index) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (combo_active_cache[index].active && combo_active_cache[index].combo_index == combo_index) {
            return true;
        }
    }
    return false;
}

static bool combo_origin_collect_untracked_active_combo(uint16_t keycode, uint16_t *out_combo_index, uint32_t *out_generation, keypos_t *out_owner_key_pos, uint16_t *out_complete_at, uint8_t *out_bitmap) {
    if (!(out_combo_index && out_generation && out_owner_key_pos && out_complete_at && out_bitmap)) {
        return false;
    }
    *out_combo_index   = UINT16_MAX;
    *out_generation    = 0;
    *out_owner_key_pos = (keypos_t){0};
    *out_complete_at   = 0;
    key_origin_bitmap_clear(out_bitmap);

    for (uint16_t combo_index = 0; combo_index < noah_qmk_combo_count(); combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);

        if (!(combo && combo->keycode == keycode && combo_origin_combo_is_active(combo)) || combo_origin_active_cache_has_combo(combo_index)) {
            continue;
        }
        if (combo_origin_combo_build_from_pressed_keys(combo_index, out_bitmap, out_owner_key_pos, out_complete_at, out_generation)) {
            *out_combo_index = combo_index;
            return true;
        }
    }
    return false;
}

void noah_qmk_combo_origin_reset(void) {
    combo_origin_press_sequence        = 0;
    combo_origin_delivery_sequence     = 0;
    combo_origin_last_pressed_at       = 0;
    combo_origin_last_pressed_key_pos  = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
    combo_origin_last_released_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
    combo_origin_diagnostics           = (noah_qmk_combo_origin_debug_snapshot_t){0};

    for (uint16_t index = 0; index < ARRAY_SIZE(physical_key_states); index++) {
        physical_key_states[index] = (combo_origin_physical_key_state_t){0};
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        combo_origin_cache_entry_clear(&combo_active_cache[index]);
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        combo_origin_pending_output_entry_clear(&combo_pending_output_cache[index]);
    }
}

void noah_qmk_combo_origin_init(void) {
    noah_qmk_combo_origin_reset();
}

void noah_qmk_combo_origin_scan(void) {
    combo_origin_pending_output_reconcile(true);
}

void noah_qmk_combo_origin_observe_physical_key_event(uint16_t keycode, keyrecord_t *record) {
    combo_origin_physical_key_state_t *entry;

    (void)keycode;

    if (!(record && key_origin_keypos_valid(record->event.key))) {
        return;
    }

    // This observes QMK state left by the preceding physical record. Deadline
    // expiry stays scan-only so multiple records in one matrix pass cannot use
    // up the final combo_task grace opportunity.
    combo_origin_pending_output_reconcile(false);
    split_runtime_sync_notify_combo_dirty();

    entry = &physical_key_states[key_origin_keypos_index(record->event.key)];
    if (record->event.pressed) {
        entry->pressed                     = true;
        entry->combo_allowed               = noah_participation_record_combo(record);
        entry->source_layer                = noah_participation_record_source(record);
        entry->combo_keycode               = combo_origin_combo_keycode_for_record(record);
        entry->pressed_at                  = timer_read();
        entry->press_sequence              = combo_origin_allocate_press_sequence();
        combo_origin_last_pressed_at       = entry->pressed_at;
        combo_origin_last_pressed_key_pos  = record->event.key;
        combo_origin_last_released_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
        combo_origin_note_completed_pressed_combos();
        return;
    }

    entry->pressed                     = false;
    entry->combo_keycode               = KC_NO;
    entry->pressed_at                  = 0;
    combo_origin_last_released_key_pos = record->event.key;
}

uint8_t noah_qmk_combo_origin_record_source_layer(uint16_t keycode, const keyrecord_t *record) {
    if (record && record->event.pressed) {
        combo_origin_pending_output_entry_t *pending = combo_origin_pending_entry_for_delivery(keycode);
        if (pending) return pending->origin_layer;
    } else {
        combo_origin_active_cache_entry_t *active = combo_origin_cache_entry_for_release(keycode);
        if (active) return active->origin_layer;
    }
    return record && key_origin_keypos_valid(record->event.key) ? noah_qmk_contract_source_layer(record->event.key) : 0;
}

void noah_qmk_combo_origin_capture_record(uint16_t keycode, keyrecord_t *record) {
    if (!record || record->event.type != COMBO_EVENT || noah_participation_record_generated_captured(record)) return;
    if (!record->event.pressed) {
        combo_origin_active_cache_entry_t *active = combo_origin_cache_entry_for_release(keycode);
        // A release can reach tapping before the matching queued press reaches
        // dispatch. Its pending completion already holds that press's decision.
        combo_origin_pending_output_entry_t *pending = combo_origin_pending_entry_for_release(keycode);
        if (pending && (!active || (combo_origin_bitmap_has_released_key(pending->bitmap) && !combo_origin_bitmap_has_released_key(active->bitmap)))) {
            pending->release_delivered = combo_origin_next_delivery();
            noah_participation_record_generated_permission(record, pending->origin_layer, pending->output_behavior);
            return;
        }
        if (active) {
            active->release_delivered = combo_origin_next_delivery();
            noah_participation_record_generated_permission(record, active->origin_layer, active->output_behavior);
            return;
        }
    } else {
        combo_origin_pending_output_entry_t *pending = combo_origin_pending_entry_for_delivery(keycode);
        if (pending) {
            pending->press_delivered = true;
            noah_participation_record_generated_permission(record, pending->origin_layer, pending->output_behavior);
            return;
        }
    }
    uint8_t layer = noah_qmk_combo_origin_record_source_layer(keycode, record);
    noah_participation_record_generated_permission(record, layer, noah_participation_combo_output(UINT16_MAX, keycode, layer));
}

bool noah_qmk_combo_origin_release_owner(uint16_t keycode, keypos_t *out) {
    combo_origin_active_cache_entry_t *entry = combo_origin_cache_entry_for_release_dispatch(keycode);

    if (!(out && entry && key_origin_keypos_valid(entry->owner_key_pos))) {
        return false;
    }
    *out = entry->owner_key_pos;
    return true;
}

bool noah_qmk_combo_origin_release_delivered_at(keypos_t owner_key_pos) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        const combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (entry->active && entry->release_delivered && entry->owner_key_pos.row == owner_key_pos.row && entry->owner_key_pos.col == owner_key_pos.col) {
            return true;
        }
    }
    return false;
}

// Retire origin reconstruction's bitmap/workspace before the process stage
// can recursively project held actions. Keep this boundary in ordinary builds.
__attribute__((noinline)) void noah_qmk_combo_origin_normalize_record(uint16_t keycode, keyrecord_t *record) {
    uint16_t combo_index       = UINT16_MAX;
    uint16_t complete_at       = 0;
    uint32_t generation        = 0;
    uint16_t release_delivered = 0;
    keypos_t owner_key_pos     = {0};
    uint8_t  origin_layer      = 0;
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    bool     matched = false;

    if (!(record && record->event.type == COMBO_EVENT) || noah_participation_record_generated_normalized(record)) {
        return;
    }

    if (record->event.pressed) {
        combo_origin_pending_output_entry_t *pending = combo_origin_pending_entry_for_dispatch(keycode);

        if (pending) {
            noah_qmk_combo_origin_capture_record(keycode, record);
            combo_index       = pending->combo_index;
            generation        = pending->generation;
            complete_at       = pending->complete_at;
            release_delivered = pending->release_delivered;
            owner_key_pos     = pending->owner_key_pos;
            origin_layer      = pending->origin_layer;
            key_origin_bitmap_copy(bitmap, pending->bitmap);
            combo_origin_pending_output_entry_clear(pending);
            matched = true;
            split_runtime_sync_notify_combo_dirty();
        } else {
            combo_origin_increment_counter(&combo_origin_diagnostics.unmatched_delayed_output_count);
            matched = combo_origin_collect_untracked_active_combo(keycode, &combo_index, &generation, &owner_key_pos, &complete_at, bitmap);
            if (matched) {
                origin_layer = physical_key_states[key_origin_keypos_index(owner_key_pos)].source_layer;
                if (!noah_participation_record_generated_captured(record)) noah_participation_record_generated_permission(record, origin_layer, noah_participation_combo_output(combo_index, keycode, origin_layer));
            }
        }
        if (matched && key_origin_keypos_valid(owner_key_pos)) {
            record->event.key = owner_key_pos;
            key_origin_registry_set_bitmap(owner_key_pos, key_origin_bitmap_has_any(bitmap) ? bitmap : NULL);
            if (combo_index != UINT16_MAX) {
                combo_origin_active_cache_entry_t *active = combo_origin_cache_store(combo_index, generation, keycode, owner_key_pos, complete_at, origin_layer, noah_participation_record_behavior(record), bitmap);
                if (active) {
                    active->release_delivered = release_delivered;
                } else {
                    combo_origin_increment_counter(&combo_origin_diagnostics.unmatched_delayed_output_count);
                }
            }
        } else if (combo_origin_fallback_owner_keypos(&owner_key_pos)) {
            // No completion was seen firing (for example, it fired and was
            // released within one record). Preserve an owner conservatively.
            combo_origin_bitmap_fill_all_keys(bitmap);
            record->event.key = owner_key_pos;
            key_origin_registry_set_bitmap(owner_key_pos, bitmap);
            origin_layer = physical_key_states[key_origin_keypos_index(owner_key_pos)].source_layer;
            if (!noah_participation_record_generated_captured(record)) noah_participation_record_generated_permission(record, origin_layer, noah_participation_combo_output(UINT16_MAX, keycode, origin_layer));
            (void)combo_origin_cache_store(UINT16_MAX, 0, keycode, owner_key_pos, timer_read(), origin_layer, noah_participation_record_behavior(record), bitmap);
        }
        if (!noah_participation_record_generated_captured(record)) noah_qmk_combo_origin_capture_record(keycode, record);
        noah_participation_record_generated_normalize(record, keycode, origin_layer);
        return;
    }

    // Captured here unless QMK's tapping engine or record admission did, while
    // the released member is still known.
    noah_qmk_combo_origin_capture_record(keycode, record);
    combo_origin_active_cache_entry_t *active = combo_origin_cache_entry_for_release_dispatch(keycode);
    if (active) {
        record->event.key = active->owner_key_pos;
        origin_layer = active->origin_layer;
        key_origin_registry_set_bitmap(active->owner_key_pos, key_origin_bitmap_has_any(active->bitmap) ? active->bitmap : NULL);
        combo_origin_cache_entry_clear(active);
        split_runtime_sync_notify_combo_dirty();
    }
    noah_participation_record_generated_normalize(record, keycode, origin_layer);
    combo_origin_last_released_key_pos = (keypos_t){.row = MATRIX_ROWS, .col = MATRIX_COLS};
}

void noah_qmk_combo_origin_pressed_combo_bitmap(uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);

    for (uint16_t combo_index = 0; combo_index < noah_qmk_combo_count(); combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t owner_key_pos = {0};

        if (!combo || combo_origin_combo_is_disabled(combo)) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &owner_key_pos, NULL, NULL)) {
            key_origin_bitmap_or_inplace(out_bitmap, combo_bitmap);
        }
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_pending_output_cache); index++) {
        const combo_origin_pending_output_entry_t *entry = &combo_pending_output_cache[index];

        if (entry->active) {
            key_origin_bitmap_or_inplace(out_bitmap, entry->bitmap);
        }
    }

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        const combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

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

    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        const combo_origin_active_cache_entry_t *entry = &combo_active_cache[index];

        if (entry->active && entry->keycode == keycode && entry->owner_key_pos.row == owner_key_pos.row && entry->owner_key_pos.col == owner_key_pos.col && (uint16_t)(entry->complete_at - since) <= term_ms) {
            return true;
        }
    }

    for (uint16_t combo_index = 0; combo_index < noah_qmk_combo_count(); combo_index++) {
        combo_t *combo = combo_origin_combo_get(combo_index);
        uint8_t  combo_bitmap[KEY_ORIGIN_BITMAP_SIZE];
        keypos_t combo_owner_key_pos = {0};
        uint16_t combo_complete_at   = 0;

        if (!combo || combo_origin_combo_is_disabled(combo) || combo->keycode != keycode) {
            continue;
        }

        if (combo_origin_combo_build_from_pressed_keys(combo_index, combo_bitmap, &combo_owner_key_pos, &combo_complete_at, NULL) && combo_owner_key_pos.row == owner_key_pos.row && combo_owner_key_pos.col == owner_key_pos.col && (uint16_t)(combo_complete_at - since) <= term_ms) {
            return true;
        }
    }

    return false;
}

bool noah_qmk_combo_origin_any_active(void) {
    for (uint8_t index = 0; index < ARRAY_SIZE(combo_active_cache); index++) {
        if (combo_active_cache[index].active) {
            return true;
        }
    }
    return false;
}

void noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, const uint8_t *quiet_owner_bitmap, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
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

        // The entry stays: its release still has to find this owner.
        if (quiet_owner_bitmap && key_origin_keypos_valid(entry->owner_key_pos) && key_origin_bitmap_has_keypos(quiet_owner_bitmap, entry->owner_key_pos)) {
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

void noah_qmk_combo_origin_debug_snapshot(noah_qmk_combo_origin_debug_snapshot_t *out) {
    if (!out) {
        return;
    }
    *out               = combo_origin_diagnostics;
    out->pending_count = combo_origin_pending_count();
    out->active_count  = combo_origin_active_count();
}


#    ifdef NOAH_COMBO_PARTICIPATION_HOOK
// A member record can mutate combo state only if its press allowed combos (its
// source layer's switch and its placement, stored at the press and reused for
// the release) and the combo allows that source layer (participation-
// policy.md). Members may come from different layers; each passes on its own,
// so one that does not keeps its chord from forming. A press no combo accepts
// is not buffered.
bool combo_key_record_allowed(uint16_t combo_index, combo_t *combo, uint16_t keycode, keyrecord_t *record) {
    (void)combo;
    (void)keycode;
    // The stored master holds generated input too (D-F32); QMK's own flag,
    // the override, it checks itself.
    if (!noah_participation_combos_allowed()) {
        return false;
    }
    if (!record || record->event.type != KEY_EVENT) {
        return true;
    }
    return noah_participation_record_combo(record) && noah_participation_combo_row(combo_index, noah_participation_record_source(record));
}
#    endif
#else

uint8_t noah_qmk_combo_origin_record_source_layer(uint16_t keycode, const keyrecord_t *record) { (void)keycode; (void)record; return 0; }
void noah_qmk_combo_origin_capture_record(uint16_t keycode, keyrecord_t *record) { (void)keycode; (void)record; }
bool noah_qmk_combo_origin_release_owner(uint16_t keycode, keypos_t *out) { (void)keycode; (void)out; return false; }
bool noah_qmk_combo_origin_release_delivered_at(keypos_t owner_key_pos) { (void)owner_key_pos; return false; }

void noah_qmk_combo_origin_init(void) {}
void noah_qmk_combo_origin_reset(void) {}
void noah_qmk_combo_origin_scan(void) {}
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
bool noah_qmk_combo_origin_any_active(void) {
    return false;
}
void noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, const uint8_t *quiet_owner_bitmap, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    (void)preview_owner_key_pos;
    (void)pd_owner_key_pos;
    (void)quiet_owner_bitmap;
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
void noah_qmk_combo_origin_debug_snapshot(noah_qmk_combo_origin_debug_snapshot_t *out) {
    if (out) {
        *out = (noah_qmk_combo_origin_debug_snapshot_t){0};
    }
}

#endif

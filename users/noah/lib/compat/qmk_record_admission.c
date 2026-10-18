// ────────────────────────────────────────────────────────────────────────────
// QMK Record Admission
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_record_admission.h"

#include "../action/synthetic_record.h"
#include "qmk_combo_origin.h"
#include "../key/behavior/participation.h"
#include "../key/runtime/api.h"
#include "../key/runtime/reducer/state_query.h"

#ifndef NOAH_RECORD_ADMISSION_CAPACITY
#    define NOAH_RECORD_ADMISSION_CAPACITY 8u
#endif

static keyrecord_t record_admission_held[NOAH_RECORD_ADMISSION_CAPACITY];
static uint8_t     record_admission_held_count;
static bool        record_admission_replaying;

static bool record_admission_is_key_record(const keyrecord_t *record) {
    return record->event.type == KEY_EVENT || record->event.type == COMBO_EVENT;
}

static bool record_admission_same_key(const keyrecord_t *lhs, const keyrecord_t *rhs) {
    return lhs->event.type == rhs->event.type && lhs->event.key.row == rhs->event.key.row && lhs->event.key.col == rhs->event.key.col && lhs->keycode == rhs->keycode;
}

// Whether this release belongs to a press still held back. Every combo output
// with one keycode is the same key here, so two such presses can both be held
// before either release: each held release pairs with the earliest held press
// still open, and one whose press already went is skipped.
static bool record_admission_press_is_held(const keyrecord_t *release) {
    uint8_t open = 0;

    for (uint8_t index = 0; index < record_admission_held_count; index++) {
        if (record_admission_same_key(&record_admission_held[index], release)) {
            if (record_admission_held[index].event.pressed) {
                open++;
            } else if (open) {
                open--;
            }
        }
    }
    return open != 0u;
}

// Whether this release waits behind the undecided key at undecided. QMK
// keeps a modifier's or a layer key's release in its tapping buffer while
// another key is undecided, so that key's tap or hold still gets them; any
// other release goes at once (action_tapping.c, "Modifier/Layer should be
// retained till end of this tapping"). Only physical keys: a combo's runtime
// position is known only once its record is admitted, so a combo's release
// could not be told from the undecided key's.
static bool record_admission_release_waits(keyrecord_t *release, keypos_t undecided) {
    if (release->event.type != KEY_EVENT || (release->event.key.row == undecided.row && release->event.key.col == undecided.col)) {
        return false;
    }
    return key_runtime_core_release_is_mods_or_layer(release->keycode ? release->keycode : get_record_keycode(release, false), release);
}

// Replays a held record through process_record. A press ends other keys'
// pending layer taps first, as its physical press would have had it not been
// held back, so it resolves on the layers they leave on.
static void record_admission_replay(keyrecord_t *record) {
    (void)noah_key_runtime_settle_layer_taps_before_press(record);
    process_record(record);
}

static bool record_admission_capture(const keyrecord_t *record) {
    if (record_admission_held_count >= NOAH_RECORD_ADMISSION_CAPACITY) {
        keyrecord_t oldest = record_admission_held[0];

        // Overload trades waiting for progress, never causal order. Make room
        // by replaying only the oldest record, even if a key is undecided.
        // Remove it before delivery so re-entry cannot see it as held back.
        for (uint8_t index = 1u; index < record_admission_held_count; index++) {
            record_admission_held[index - 1u] = record_admission_held[index];
        }
        record_admission_held_count--;
        record_admission_replaying = true;
        record_admission_replay(&oldest);
        record_admission_replaying = false;
    }
    record_admission_held[record_admission_held_count] = *record;
    // QMK has delivered it; held back here, it settles when it is replayed.
    noah_participation_record_defer(&record_admission_held[record_admission_held_count++]);
    return false;
}

bool noah_record_admission_admit(keyrecord_t *record) {
    keypos_t undecided;

    if (!(record && record_admission_is_key_record(record)) || record_admission_replaying || noah_synthetic_record_active()) {
        return true;
    }

    if (record->event.type == COMBO_EVENT && !noah_participation_record_generated_captured(record)) {
        uint16_t keycode = get_record_keycode(record, false);
        noah_qmk_combo_origin_capture_record(keycode, record);
    }

    if (!record->event.pressed) {
        bool waits = (record_admission_held_count && record_admission_press_is_held(record)) || (key_runtime_core_undecided_dual_role_key_pos(&undecided) && record_admission_release_waits(record, undecided));
        return !waits || record_admission_capture(record);
    }

    if (record_admission_held_count) {
        return record_admission_capture(record);
    }

    if (key_runtime_core_undecided_dual_role_key_pos(&undecided) && !(record->event.type == KEY_EVENT && undecided.row == record->event.key.row && undecided.col == record->event.key.col)) {
        return record_admission_capture(record);
    }

    return true;
}

void noah_record_admission_task(void) {
    uint8_t  replayed = 0u;
    keypos_t undecided;

    if (!record_admission_held_count || key_runtime_core_undecided_dual_role_key_pos(NULL)) {
        return;
    }

    record_admission_replaying = true;
    while (replayed < record_admission_held_count) {
        keyrecord_t record = record_admission_held[replayed];

        // A replayed tap/hold key can itself be undecided; later presses and
        // modifier or layer releases wait for it in turn, while other
        // releases of already replayed keys go ahead.
        if (replayed > 0u && key_runtime_core_undecided_dual_role_key_pos(&undecided) && (record.event.pressed || record_admission_release_waits(&record, undecided))) {
            break;
        }
        replayed++;
        record_admission_replay(&record);
    }
    record_admission_replaying = false;

    for (uint8_t index = replayed; index < record_admission_held_count; index++) {
        record_admission_held[index - replayed] = record_admission_held[index];
    }
    record_admission_held_count = (uint8_t)(record_admission_held_count - replayed);
}

void noah_record_admission_reset(void) {
    noah_macro_input_guard_reset();
    record_admission_held_count = 0u;
    record_admission_replaying  = false;
}

bool noah_record_admission_release_pending(keypos_t pos, uint16_t since, uint16_t term) {
    for (uint8_t index = 0; index < record_admission_held_count; index++) {
        const keyevent_t *event = &record_admission_held[index].event;

        if (event->type == KEY_EVENT && !event->pressed && event->key.row == pos.row && event->key.col == pos.col && (uint16_t)(event->time - since) <= term) {
            return true;
        }
    }
    return false;
}

uint8_t noah_record_admission_held_count(void) {
    return record_admission_held_count;
}

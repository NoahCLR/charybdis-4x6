// ────────────────────────────────────────────────────────────────────────────
// QMK Record Admission
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_record_admission.h"

#include "../action/synthetic_record.h"
#include "qmk_combo_origin.h"
#include "../key/behavior/participation.h"
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

// Whether this release belongs to a press still held back.
static bool record_admission_press_is_held(const keyrecord_t *release) {
    bool held = false;

    for (uint8_t index = 0; index < record_admission_held_count; index++) {
        if (record_admission_same_key(&record_admission_held[index], release)) {
            held = record_admission_held[index].event.pressed;
        }
    }
    return held;
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
        process_record(&oldest);
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
        noah_participation_record_generated(record, keycode, noah_qmk_combo_origin_record_source_layer(keycode, record));
    }

    if (!record->event.pressed) {
        return !(record_admission_held_count && record_admission_press_is_held(record)) || record_admission_capture(record);
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
    uint8_t replayed = 0u;

    if (!record_admission_held_count || key_runtime_core_undecided_dual_role_key_pos(NULL)) {
        return;
    }

    record_admission_replaying = true;
    while (replayed < record_admission_held_count) {
        keyrecord_t record = record_admission_held[replayed];

        // A replayed tap/hold key can itself be undecided; later presses wait
        // for it in turn, while releases of already replayed keys go ahead.
        if (replayed > 0u && record.event.pressed && key_runtime_core_undecided_dual_role_key_pos(NULL)) {
            break;
        }
        replayed++;
        process_record(&record);
    }
    record_admission_replaying = false;

    for (uint8_t index = replayed; index < record_admission_held_count; index++) {
        record_admission_held[index - replayed] = record_admission_held[index];
    }
    record_admission_held_count = (uint8_t)(record_admission_held_count - replayed);
}

void noah_record_admission_reset(void) {
    record_admission_held_count = 0u;
    record_admission_replaying  = false;
}

uint8_t noah_record_admission_held_count(void) {
    return record_admission_held_count;
}

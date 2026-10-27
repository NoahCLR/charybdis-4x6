// ───────────────────────────────────────────────────────────────────────────
// Edit Mode (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// How Ark's edits reach this keyboard. Stored outside the profile, in QMK's
// EEPROM user datablock on each half (qmk_profile_edit_mode.c), never in a
// profile, a review or a backup. A keyboard never set is in Live preview +
// save. Live edit saves after the quiet period; Live preview + save waits for
// an explicit Save; Review and save saves only adjustment-key edits.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NOAH_PROFILE_EDIT_MODE_APPLY       = 0u,
    NOAH_PROFILE_EDIT_MODE_LIVE        = 1u,
    NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW = 2u,
    NOAH_PROFILE_EDIT_MODE_COUNT,
    NOAH_PROFILE_EDIT_MODE_DEFAULT = NOAH_PROFILE_EDIT_MODE_LIVE_REVIEW,
} noah_profile_edit_mode_t;

// Each change takes a counter above both halves' records, so after a power
// cut between the two writes the higher counter wins on reconnect. Counter
// zero is a half that never stored one (the default mode).
typedef struct {
    uint32_t counter;
    uint8_t  mode;
} noah_profile_edit_mode_record_t;

typedef struct {
    bool (*load)(void *context, noah_profile_edit_mode_record_t *record);
    bool (*store)(void *context, const noah_profile_edit_mode_record_t *record);
    // Whether a store may run now: QMK's EEPROM is wear-levelled flash, so a
    // write waits for the quiet keyboard a slot erase waits for (D-F29).
    // None: always.
    bool (*may_store)(void *context);
    void *context;
} noah_profile_edit_mode_storage_t;

static inline bool noah_profile_edit_mode_valid(uint8_t mode) {
    return mode < NOAH_PROFILE_EDIT_MODE_COUNT;
}

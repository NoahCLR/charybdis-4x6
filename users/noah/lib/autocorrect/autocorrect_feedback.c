// ────────────────────────────────────────────────────────────────────────────
// Autocorrect Feedback
// ────────────────────────────────────────────────────────────────────────────

#include "autocorrect_feedback.h"

#include "../profile/runtime/active_profile_settings_runtime.h"

static struct {
    noah_autocorrect_feedback_t shown;
    uint32_t                    began;
    uint8_t                     keys[KEY_ORIGIN_BITMAP_SIZE]; // Of the key last given.
} autocorrect_feedback;

void noah_autocorrect_feedback_reset(void) {
    autocorrect_feedback.shown = (noah_autocorrect_feedback_t){0};
}

void noah_autocorrect_feedback_key(keypos_t key_pos) {
    (void)key_origin_registry_get_bitmap(key_pos, autocorrect_feedback.keys);
}

void noah_autocorrect_feedback_key_of_leading(void) {
    if (autocorrect_feedback.shown.kind == NOAH_AUTOCORRECT_FEEDBACK_LEADING) key_origin_bitmap_copy(autocorrect_feedback.keys, autocorrect_feedback.shown.keys);
    else key_origin_bitmap_clear(autocorrect_feedback.keys);
}

void noah_autocorrect_feedback_note(noah_autocorrect_feedback_kind_t kind) {
    autocorrect_feedback.shown.kind = (uint8_t)kind;
    key_origin_bitmap_copy(autocorrect_feedback.shown.keys, autocorrect_feedback.keys);
    autocorrect_feedback.began = timer_read32();
}

void noah_autocorrect_feedback_lead_begin(keypos_t key_pos) {
    autocorrect_feedback.shown = (noah_autocorrect_feedback_t){.kind = NOAH_AUTOCORRECT_FEEDBACK_LEADING};
    (void)key_origin_registry_get_bitmap(key_pos, autocorrect_feedback.shown.keys);
}

void noah_autocorrect_feedback_lead_end(void) {
    if (autocorrect_feedback.shown.kind == NOAH_AUTOCORRECT_FEEDBACK_LEADING) noah_autocorrect_feedback_reset();
}

noah_autocorrect_feedback_t noah_autocorrect_feedback(void) {
    // Leading is held, not timed: the typing stage ends it (D-F72).
    if (autocorrect_feedback.shown.kind != NOAH_AUTOCORRECT_FEEDBACK_NONE && autocorrect_feedback.shown.kind != NOAH_AUTOCORRECT_FEEDBACK_LEADING && timer_elapsed32(autocorrect_feedback.began) >= noah_setting(NOAH_SETTING_AUTOCORRECT_FEEDBACK_PERIOD, AUTOCORRECT_FEEDBACK_FLASH_MS)) {
        noah_autocorrect_feedback_reset();
    }
    return autocorrect_feedback.shown;
}

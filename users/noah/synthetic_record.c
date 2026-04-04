// ────────────────────────────────────────────────────────────────────────────
// Synthetic Record Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "synthetic_record.h"

static uint8_t synthetic_record_depth = 0;

static keyrecord_t noah_make_synthetic_record(bool pressed, uint8_t tap_count) {
    keyrecord_t record = {0};

    record.event = MAKE_KEYEVENT(UINT8_MAX, UINT8_MAX, pressed);
#ifndef NO_ACTION_TAPPING
    record.tap = (tap_t){
        .count = tap_count,
    };
#endif

    return record;
}

bool noah_synthetic_record_active(void) {
    return synthetic_record_depth > 0;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    keyrecord_t record = noah_make_synthetic_record(pressed, 0);

    synthetic_record_depth++;
    bool keep_processing = process_record_user(keycode, &record);
    synthetic_record_depth--;

    return keep_processing;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    noah_dispatch_synthetic_record(keycode, true);
    noah_dispatch_synthetic_record(keycode, false);
}

void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    keyrecord_t record = noah_make_synthetic_record(pressed, tap_count);

#if defined(COMBO_ENABLE) || defined(REPEAT_KEY_ENABLE)
    record.keycode = keycode;
    synthetic_record_depth++;
    process_record(&record);
    synthetic_record_depth--;
#else
    synthetic_record_depth++;
    process_action(&record, action_for_keycode(keycode));
    synthetic_record_depth--;
#endif
}

void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    noah_dispatch_synthetic_qmk_record(keycode, true, 1);
    noah_dispatch_synthetic_qmk_record(keycode, false, 1);
}

// ────────────────────────────────────────────────────────────────────────────
// Synthetic Record Dispatch
// ────────────────────────────────────────────────────────────────────────────

#include "synthetic_record.h"

static uint8_t synthetic_record_depth = 0;

bool noah_synthetic_record_active(void) {
    return synthetic_record_depth > 0;
}

bool noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    keyrecord_t record = {0};

    record.event.pressed = pressed;
    record.event.key.row = UINT8_MAX;
    record.event.key.col = UINT8_MAX;

    synthetic_record_depth++;
    bool keep_processing = process_record_user(keycode, &record);
    synthetic_record_depth--;

    return keep_processing;
}

void noah_dispatch_synthetic_tap(uint16_t keycode) {
    noah_dispatch_synthetic_record(keycode, true);
    noah_dispatch_synthetic_record(keycode, false);
}

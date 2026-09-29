#pragma once

#include QMK_KEYBOARD_H

// The selected fork exposes read-only physical queue facts. Narrow host tests
// deliver records synchronously; the QMK pipeline runner uses real timestamps
// and queue implementations instead. No queue is mirrored in userspace.
#if !defined(NOAH_HOST_TEST_ENV) || defined(NOAH_TEST_QMK_GESTURES)
bool combo_key_event_pending(uint8_t row, uint8_t col, bool pressed, uint16_t since, uint16_t term);
bool tapping_key_event_pending(uint8_t row, uint8_t col, bool pressed, uint16_t since, uint16_t term);
#endif

static inline uint16_t noah_qmk_gesture_event_time(const keyrecord_t *record) {
#if !defined(NOAH_HOST_TEST_ENV) || defined(NOAH_TEST_QMK_GESTURES)
    if (record->event.type == KEY_EVENT) return record->event.time;
#endif
    return timer_read();
}

static inline bool noah_qmk_gesture_event_pending(keypos_t pos, bool pressed, uint16_t since, uint16_t term) {
#if !defined(NOAH_HOST_TEST_ENV) || defined(NOAH_TEST_QMK_GESTURES)
#    if defined(COMBO_ENABLE) || defined(NOAH_TEST_QMK_GESTURES)
    if (combo_key_event_pending(pos.row, pos.col, pressed, since, term)) return true;
#    endif
#    ifndef NO_ACTION_TAPPING
    if (tapping_key_event_pending(pos.row, pos.col, pressed, since, term)) return true;
#    endif
#endif
    return false;
}

static inline bool noah_qmk_gesture_press_pending(keypos_t pos, uint16_t since, uint16_t term) {
    return noah_qmk_gesture_event_pending(pos, true, since, term);
}
static inline bool noah_qmk_gesture_release_pending(keypos_t pos, uint16_t since, uint16_t now) {
    return noah_qmk_gesture_event_pending(pos, false, since, (uint16_t)(now - since));
}

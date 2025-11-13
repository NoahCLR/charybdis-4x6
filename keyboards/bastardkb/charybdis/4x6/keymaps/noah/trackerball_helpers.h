// --- Tunables ---
#ifndef CARET_THRESHOLD_X
#    define CARET_THRESHOLD_X 40
#endif
#ifndef CARET_THRESHOLD_Y
#    define CARET_THRESHOLD_Y 50
#endif
#ifndef VOLUME_THRESHOLD
#    define VOLUME_THRESHOLD 60
#endif

// --- Internal state (ensure these exist once in your file) ---
static bool    caret_active  = false;
static int32_t acc_x         = 0;
static int32_t acc_y         = 0;
static int8_t  last_x_dir    = 0;
static int8_t  last_y_dir    = 0;
static char    dominant_axis = 'x';

static bool    volmode_active = false; // set in process_record_user for VOLMODE key
static int32_t vol_acc        = 0;
static int8_t  vol_last_dir   = 0;

// --- Small helpers ---
static inline report_mouse_t freeze_mouse(report_mouse_t r) {
    return (report_mouse_t){0};
}

static inline void caret_send_arrow(uint16_t keycode) {
    if (get_mods() & MOD_MASK_SHIFT)
        tap_code16(S(keycode));
    else
        tap_code(keycode);
}

static inline int32_t caret_emit_many(int32_t delta, uint16_t positive, uint16_t negative, uint16_t threshold) {
    while (delta >= threshold) {
        caret_send_arrow(positive);
        delta -= threshold;
    }
    while (delta <= -(int32_t)threshold) {
        caret_send_arrow(negative);
        delta += threshold;
    }
    return delta;
}

static inline void caret_update_dominant_axis(int16_t dx, int16_t dy) {
    int16_t ax = dx >= 0 ? dx : -dx;
    int16_t ay = dy >= 0 ? dy : -dy;
    if (ax > ay && ax > 0)
        dominant_axis = 'x';
    else if (ay > ax && ay > 0)
        dominant_axis = 'y';
}

// --- Mode handlers ---
static inline report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        if (vol_last_dir != 0 && dir != vol_last_dir) vol_acc = 0; // reset on direction flip
        vol_last_dir = dir;

        vol_acc += dy;
        while (vol_acc >= VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_DOWN);
            vol_acc -= VOLUME_THRESHOLD;
        }
        while (vol_acc <= -VOLUME_THRESHOLD) {
            tap_code(KC_AUDIO_VOL_UP);
            vol_acc += VOLUME_THRESHOLD;
        }
    }

    return freeze_mouse(mouse_report);
}

static inline report_mouse_t handle_caret_mode(report_mouse_t mouse_report) {
    int16_t dx = mouse_report.x;
    int16_t dy = mouse_report.y;

    caret_update_dominant_axis(dx, dy);

    if (dominant_axis == 'x') {
        if (dx != 0) {
            int8_t dir = (dx > 0) ? 1 : -1;
            if (last_x_dir != 0 && dir != last_x_dir) acc_x = 0;
            last_x_dir = dir;
        }
        acc_x += dx;
        acc_x = caret_emit_many(acc_x, KC_RIGHT, KC_LEFT, CARET_THRESHOLD_X);

        if (dy != 0) {
            acc_y      = 0;
            last_y_dir = 0;
        }
    } else {
        if (dy != 0) {
            int8_t dir = (dy > 0) ? 1 : -1;
            if (last_y_dir != 0 && dir != last_y_dir) acc_y = 0;
            last_y_dir = dir;
        }
        acc_y += dy;
        acc_y = caret_emit_many(acc_y, KC_DOWN, KC_UP, CARET_THRESHOLD_Y);

        if (dx != 0) {
            acc_x      = 0;
            last_x_dir = 0;
        }
    }

    return freeze_mouse(mouse_report);
}
#include "pd_mode_configured.h"

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
#include "pd_mode_handler_common.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/modifiers/keyboard_mod_policy.h"
#include "../../profile/schema/profile_pd_v1.h"

static const uint8_t *active;
static pd_mode_axis_state_t axes[2];
static bool axis_x;
// Eight directions accumulate one motion vector rather than one axis at a
// time: the wedge a tap belongs to depends on both components together.
static int32_t eight[2];
// Per-origin release ownership survives mode replacement. A consumed bit is
// retained after releasing modifiers so an old release cannot hit a new mode.
static struct { uint8_t modifiers; bool consumed; } buttons[MATRIX_ROWS][MATRIX_COLS];
static uint8_t scroll_modifiers;

static uint16_t u16(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)p[1] << 8u;
}

static void owned_modifiers(uint8_t mask, bool pressed) {
    for (uint8_t bit = 0; bit < 8; bit++) {
        if (!(mask & (1u << bit))) continue;
        if (pressed) keyboard_mod_ownership_register(KC_LEFT_CTRL + bit);
        else keyboard_mod_ownership_unregister(KC_LEFT_CTRL + bit);
    }
}

static void emit_tap(const uint8_t *tap) {
    uint16_t key = u16(tap);
    if (!key) return;
    if (tap[2] == 1u) noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(key, tap[3], true);
    else if (tap[2] == 2u) noah_emit_literal_tap(key, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);
    else noah_emit_synthetic_qmk_tap(key, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
}

static void direction_tap(uint16_t direction) {
    emit_tap(active + 36u + direction * 4u);
}

enum { DIR_LEFT = 0, DIR_RIGHT, DIR_UP, DIR_DOWN };

// Adds one report's component, dropping the backlog on a reversal and capping
// it at NOAH_PD_MODE_MAX_BACKLOG_TAPS taps, as the single-axis path does.
static void eight_accumulate(int32_t *acc, int16_t delta, uint16_t threshold) {
    int32_t cap = (int32_t)threshold * (NOAH_PD_MODE_MAX_BACKLOG_TAPS + 1);
    if ((delta > 0 && *acc < 0) || (delta < 0 && *acc > 0)) *acc = 0;
    *acc += delta;
    if (*acc > cap) *acc = cap;
    if (*acc < -cap) *acc = -cap;
}

// 45-degree wedges: motion within 22.5 degrees of an axis is straight.
// tan(22.5) ~= 70/169, which keeps the comparison in integers.
static bool eight_straight(uint32_t along, uint32_t across) {
    return across * 169u <= along * 70u;
}

static void eight_way(report_mouse_t report) {
    uint16_t tx = u16(active + 32), ty = u16(active + 34);
    uint8_t  budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;

    eight_accumulate(&eight[0], report.x, tx);
    eight_accumulate(&eight[1], report.y, ty);
    while (budget) {
        int32_t  x = eight[0], y = eight[1];
        uint32_t ax = (uint32_t)(x < 0 ? -x : x), ay = (uint32_t)(y < 0 ? -y : y);
        uint16_t horizontal = x < 0 ? DIR_LEFT : DIR_RIGHT, vertical = y < 0 ? DIR_UP : DIR_DOWN;

        if (eight_straight(ax, ay)) {
            if (ax < tx) break;
            direction_tap(horizontal);
            eight[0] += x < 0 ? tx : -(int32_t)tx;
            eight[1] = 0; // drift across a straight tap is not saved up
            budget--;
            continue;
        }
        if (eight_straight(ay, ax)) {
            if (ay < ty) break;
            direction_tap(vertical);
            eight[1] += y < 0 ? ty : -(int32_t)ty;
            eight[0] = 0;
            budget--;
            continue;
        }
        if (ax < tx || ay < ty) break;
        // Diagonals are stored up-left, up-right, down-left, down-right.
        const uint8_t *diagonal = active + 70u + ((y < 0 ? 0u : 2u) + (x < 0 ? 0u : 1u)) * 4u;
        if (u16(diagonal)) {
            emit_tap(diagonal);
            budget--;
        } else if (active[86] == NOAH_PD_EMPTY_DIAGONAL_BOTH) {
            if (budget < 2u) break;
            direction_tap(horizontal);
            direction_tap(vertical);
            budget -= 2u;
        } else if (active[86] == NOAH_PD_EMPTY_DIAGONAL_NEAREST) {
            // The straight direction this motion leans toward, measured
            // against each axis's own threshold.
            direction_tap((uint64_t)ax * ty >= (uint64_t)ay * tx ? horizontal : vertical);
            budget--;
        } else {
            budget--; // a dead zone still consumes the motion
        }
        eight[0] += x < 0 ? tx : -(int32_t)tx;
        eight[1] += y < 0 ? ty : -(int32_t)ty;
    }
}

void noah_pd_engine_exit(void) {
    owned_modifiers(scroll_modifiers, false);
    scroll_modifiers = 0;
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            owned_modifiers(buttons[row][col].modifiers, false);
            buttons[row][col].modifiers = 0;
        }
    }
    active = NULL;
    pd_mode_axis_reset(&axes[0]);
    pd_mode_axis_reset(&axes[1]);
    eight[0] = eight[1] = 0;
    axis_x = true;
    reset_dragscroll_mode();
}

void noah_pd_engine_enter(const uint8_t *record) {
    noah_pd_engine_exit();
    if (!record || !record[1]) return;
    active = record;
    scroll_modifiers = record[1] == 2u ? record[6] : 0u;
    owned_modifiers(scroll_modifiers, true);
}

uint8_t noah_pd_engine_masked_mods(const uint8_t *record) {
    return record && record[1] == 2u ? keyboard_mod_policy_managed_only_mask(record[6]) : 0u;
}

report_mouse_t noah_pd_engine_motion(report_mouse_t report) {
    if (!active) return report;
    if (active[1] == 2u) return noah_pd_configured_scroll(active, report);
    uint8_t policy = active[3], budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;
    bool was_x = axis_x;
    if (policy == NOAH_PD_AXIS_EIGHT) {
        eight_way(report);
        return pd_mode_freeze_mouse();
    }
    if (policy == 2u) {
        int32_t x = report.x, y = report.y;
        if (x < 0) x = -x;
        if (y < 0) y = -y;
        if (x > y) axis_x = true;
        else if (y > x) axis_x = false;
        if (axis_x != was_x) pd_mode_axis_reset(&axes[axis_x ? 1 : 0]);
    } else axis_x = policy == 1u;
    if (axis_x) pd_mode_axis_emit(&axes[0], report.x, 1u, 0u, u16(active + 32), &budget, direction_tap);
    else pd_mode_axis_emit(&axes[1], report.y, 3u, 2u, u16(active + 34), &budget, direction_tap);
    return pd_mode_freeze_mouse();
}

bool noah_pd_engine_key(uint16_t keycode, keyrecord_t *record) {
    if (!record || record->event.key.row >= MATRIX_ROWS || record->event.key.col >= MATRIX_COLS) return false;
    uint8_t row = record->event.key.row, col = record->event.key.col;
    if (buttons[row][col].consumed) {
        if (!record->event.pressed) {
            owned_modifiers(buttons[row][col].modifiers, false);
            buttons[row][col].modifiers = 0;
            buttons[row][col].consumed = false;
        }
        return true;
    }
    if (!active || !record->event.pressed || keycode < QK_MOUSE_BUTTON_1 || keycode > QK_MOUSE_BUTTON_1 + 2u) return false;
    const uint8_t *button = active + 52u + (keycode - QK_MOUSE_BUTTON_1) * 6u;
    if (!button[0]) return false;
    buttons[row][col].consumed = true;
    if (button[0] == 2u) emit_tap(button + 2);
    if (button[0] == 3u) {
        buttons[row][col].modifiers = button[1];
        owned_modifiers(button[1], true);
    }
    return true;
}

bool noah_pd_engine_pending_release(void) {
    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) if (buttons[row][col].consumed) return true;
    }
    return false;
}
#endif

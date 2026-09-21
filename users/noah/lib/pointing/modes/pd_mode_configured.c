#include "pd_mode_configured.h"

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
#include "pd_mode_handler_common.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/modifiers/keyboard_mod_policy.h"

static const uint8_t *active;
static pd_mode_axis_state_t axes[2];
static bool axis_x;
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

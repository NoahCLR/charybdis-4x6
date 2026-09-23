#include "pd_mode_configured.h"

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
#include "pd_mode_handler_common.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/modifiers/keyboard_mod_policy.h"
#include "../../profile/schema/profile_pd_v1.h"

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

enum { DIR_LEFT = 0, DIR_RIGHT, DIR_UP, DIR_DOWN };

// ── Directional engine: dominant axis (four directions) and eight directions ──
//
// One engine for both. Motion is measured in steps of each axis's own
// threshold (4096 is one step, rounded per report so small moves add up
// exactly), so a direction is judged the same way the taps are counted. A smoothed heading picks a direction and holds it until the
// heading is clearly in another one, DIRECTION_HYSTERESIS past the boundary.
// Only progress along the held direction counts; sideways drift is dropped. A
// diagonal step is the same distance as a straight one. After a pause of
// DIRECTION_IDLE_MS the next move chooses afresh, keeping its progress only if
// it continues the same way; a move against the held direction releases it. Entering a diagonal takes half a step more
// before its first tap, so turning from one axis to the other passes through
// the diagonal without firing it.
enum {
    STEP = 4096,
    DIRECTION_IDLE_MS = 150,
    DIRECTION_NONE = 0xff,
    // The heading must be at least this far along before it decides anything.
    HEADING_MINIMUM = STEP / 16,
};
// Directions: 0 right, 1 down-right, 2 down, 3 down-left, 4 left, 5 up-left,
// 6 up, 7 up-right. Even ones are straight. Report y grows downward.
static const int8_t direction_x[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t direction_y[8] = {0, 1, 1, 1, 0, -1, -1, -1};
typedef struct {
    int32_t  heading_x, heading_y; // smoothed motion, in STEP units
    int32_t  progress;             // along the held direction, in STEP units
    uint32_t last_motion;
    uint8_t  held;                 // DIRECTION_NONE when free
    bool     motion_known;
    bool     fresh; // after a pause: choose without hysteresis, keep progress if unchanged
} direction_state_t;
static direction_state_t direction;

// Diagonal slots in the record are stored up-left, up-right, down-left,
// down-right; this maps a diagonal direction onto that order.
static const uint8_t *diagonal_slot(uint8_t dir) {
    uint8_t index = (direction_y[dir] < 0 ? 0u : 2u) + (direction_x[dir] < 0 ? 0u : 1u);
    return active + 70u + index * 4u;
}

// Whether the diagonal of this quadrant takes part in the choice: never in
// dominant-axis mode, and not when it is empty and empty diagonals go to the
// nearest straight direction.
static bool diagonal_available(uint8_t dir) {
    return active[3] == NOAH_PD_AXIS_EIGHT && (u16(diagonal_slot(dir)) || active[86] != NOAH_PD_EMPTY_DIAGONAL_NEAREST);
}

// The diagonal direction of the quadrant a heading points into.
static uint8_t quadrant_diagonal(int32_t x, int32_t y) {
    return y < 0 ? (x < 0 ? 5u : 7u) : (x < 0 ? 3u : 1u);
}

// Boundary tangents, as ratios: 22.5 degrees splits straight from diagonal and
// 45 degrees splits the axes when no diagonal takes part. A held direction
// keeps its range widened by 7.5 degrees on each side.
//   tan 15 ~= 15/56, tan 22.5 ~= 70/169, tan 30 ~= 15/26, tan 52.5 ~= 30/23
static uint8_t choose_direction(int32_t x, int32_t y) {
    uint64_t ax = (uint64_t)(x < 0 ? -(int64_t)x : x), ay = (uint64_t)(y < 0 ? -(int64_t)y : y);
    uint8_t  held = direction.held, diagonal = quadrant_diagonal(x, y);
    bool     diagonals = diagonal_available(diagonal);

    if (held != DIRECTION_NONE && !direction.fresh) {
        int8_t hx = direction_x[held], hy = direction_y[held];
        if (held & 1u) {
            // A held diagonal stays while its quadrant is right and neither
            // axis clearly dominates.
            if (held == diagonal && ay * 56u >= ax * 15u && ax * 56u >= ay * 15u) return held;
        } else if (hx && (x < 0) == (hx < 0) && x != 0) {
            if (diagonals ? ay * 26u <= ax * 15u : ay * 23u <= ax * 30u) return held;
        } else if (hy && (y < 0) == (hy < 0) && y != 0) {
            if (diagonals ? ax * 26u <= ay * 15u : ax * 23u <= ay * 30u) return held;
        }
    }
    if (diagonals && ay * 169u > ax * 70u && ax * 169u > ay * 70u) return diagonal;
    if (ax >= ay) return x < 0 ? 4u : 0u;
    return y < 0 ? 6u : 2u;
}

static void directional_step(uint8_t dir, uint8_t *budget) {
    static const uint8_t straight[8] = {DIR_RIGHT, 0, DIR_DOWN, 0, DIR_LEFT, 0, DIR_UP, 0};
    if (!(dir & 1u)) {
        direction_tap(straight[dir]);
        (*budget)--;
        return;
    }
    if (u16(diagonal_slot(dir))) {
        emit_tap(diagonal_slot(dir));
        (*budget)--;
    } else if (active[86] == NOAH_PD_EMPTY_DIAGONAL_BOTH) {
        direction_tap(direction_x[dir] < 0 ? DIR_LEFT : DIR_RIGHT);
        direction_tap(direction_y[dir] < 0 ? DIR_UP : DIR_DOWN);
        *budget = *budget >= 2u ? (uint8_t)(*budget - 2u) : 0u;
    } else {
        (*budget)--; // a dead zone still consumes the motion
    }
}

// A report's motion in STEP units of one axis's threshold, rounded.
static int32_t steps(int16_t counts, uint16_t threshold) {
    int32_t scaled = (int32_t)counts * STEP;
    if (!threshold) return 0;
    return (scaled < 0 ? scaled - threshold / 2 : scaled + threshold / 2) / threshold;
}

static void directional(report_mouse_t report) {
    uint16_t tx = u16(active + 32), ty = u16(active + 34);
    uint8_t  budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK;
    int32_t  cap    = (int32_t)STEP * (NOAH_PD_MODE_MAX_BACKLOG_TAPS + 1);
    uint32_t now;

    if (!report.x && !report.y) return;
    now = timer_read32();
    if (direction.motion_known && timer_elapsed32(direction.last_motion) > DIRECTION_IDLE_MS) {
        direction.fresh = true;
    }
    direction.last_motion  = now;
    direction.motion_known = true;

    int32_t nx = steps(report.x, tx), ny = steps(report.y, ty);
    if (direction.held != DIRECTION_NONE && (int64_t)direction_x[direction.held] * nx + (int64_t)direction_y[direction.held] * ny < 0) {
        direction.held = DIRECTION_NONE; // a reversal starts a new move
    }
    // Half of each report into the heading: the widened ranges ride out a
    // wobbling hand, and a real turn is followed within a report or two.
    direction.heading_x += (nx - direction.heading_x) / 2;
    direction.heading_y += (ny - direction.heading_y) / 2;
    if (direction.held == DIRECTION_NONE || direction.fresh) {
        direction.heading_x = nx;
        direction.heading_y = ny;
    }
    int32_t hx = direction.heading_x, hy = direction.heading_y;
    if ((hx < 0 ? -hx : hx) < HEADING_MINIMUM && (hy < 0 ? -hy : hy) < HEADING_MINIMUM) return;

    uint8_t chosen = choose_direction(hx, hy);
    direction.fresh = false;
    if (chosen != direction.held) {
        // A new direction starts from nothing, as dominant axis always did.
        direction.held     = chosen;
        direction.progress = chosen & 1u ? -STEP / 2 : 0;
    }
    int8_t  dx = direction_x[chosen], dy = direction_y[chosen];
    int64_t along = (int64_t)dx * nx + (int64_t)dy * ny;
    if (dx && dy) along = along * 181 / 256; // one diagonal step is one step long
    if (along > cap) along = cap;
    direction.progress += (int32_t)along;
    if (direction.progress < -STEP / 2) direction.progress = -STEP / 2;
    if (direction.progress > cap) direction.progress = cap;
    while (budget && direction.progress >= STEP) {
        directional_step(chosen, &budget);
        direction.progress -= STEP;
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
    direction = (direction_state_t){.held = DIRECTION_NONE};
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
    if (policy == NOAH_PD_AXIS_DOMINANT || policy == NOAH_PD_AXIS_EIGHT) {
        directional(report);
        return pd_mode_freeze_mouse();
    }
    axis_x = policy == NOAH_PD_AXIS_HORIZONTAL;
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

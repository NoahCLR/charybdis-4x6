#include "pd_mode_configured.h"

#if defined(NOAH_PD_PROFILE_ENABLE) && defined(POINTING_DEVICE_ENABLE)
#include "pd_mode_handler_common.h"
#include "pd_mode_handlers.h"
#include "../../state/ownership/keyboard_mod_ownership.h"
#include "../../state/modifiers/keyboard_mod_policy.h"
#include "../../profile/schema/profile_pd_v1.h"

static const uint8_t *active;
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

static bool emit_tap(const uint8_t *tap) {
    uint16_t key = u16(tap);
    if (!key) return false;
    if (tap[2] == 1u) noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(key, tap[3], true);
    else if (tap[2] == 2u) noah_emit_literal_tap(key, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS_AND_PRESERVE_MODS);
    else noah_emit_synthetic_qmk_tap(key, NOAH_EMIT_POLICY_SETTLE_FALLBACK_HOLDS);
    return true;
}

// ── Directional engine ──────────────────────────────────────────────────────
//
// Every directional mode runs this engine; the axis policy only says which
// directions exist: vertical and horizontal two, dominant axis the four
// straight ones, eight directions all eight. Motion toward a direction that
// does not exist goes to the nearest one that does, which for a single axis is
// exactly counting that axis. A direction that exists but has no shortcut
// follows the empty-direction policy (byte 86).
//
// Motion is counted in units of threshold_x * threshold_y, so each axis moves
// in steps of its own threshold and a straight direction accumulates the
// sensor counts exactly. A smoothed heading (half of each report) picks the
// nearest available direction and holds it until another is nearer by
// DIRECTION_MARGIN, which moves each boundary 7.5 degrees past the held side.
// Only progress along the held direction counts; sideways drift is dropped. A
// diagonal step is one threshold step along the diagonal, and entering a
// diagonal takes half a step more before its first tap, so a turn between
// axes passes through it without firing. A move against the held direction
// releases it at once; after DIRECTION_IDLE_MS still, the next move chooses
// afresh and keeps its progress only if it continues the same way.
//
// Byte 87 says how often a direction sends: once per threshold step, or once
// per movement. A movement starts on the first motion after the mode starts or
// after DIRECTION_IDLE_MS still, and the first step that sends anything spends
// it in the direction that sent. From then on only a direction pointing back
// against that one, more than 90 degrees from it, may send, and its sending
// spends the movement in turn; any other motion is dropped rather than banked,
// so the next movement starts from nothing. A stray report against the held
// direction releases the hold, but sends only if it alone carries a whole step
// back, so a long move with one wobble does not send twice. A step that sends
// nothing, in a dead zone or with both neighbours empty, leaves the movement
// unspent.
enum {
    DIRECTION_IDLE_MS = 150,
    DIRECTION_NONE    = 0xff,
    ANGLE_TURN        = 2880, // angles in eighths of a degree
    DIRECTION_ANGLE   = ANGLE_TURN / 8,
    DIRECTION_MARGIN  = 15 * 8,
};
// Directions: 0 right, 1 down-right, 2 down, 3 down-left, 4 left, 5 up-left,
// 6 up, 7 up-right; even ones are straight. Report y grows downward.
static const int8_t  direction_x[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t  direction_y[8] = {0, 1, 1, 1, 0, -1, -1, -1};
// The record's straight slots are left, right, up, down.
enum { SLOT_LEFT = 0, SLOT_RIGHT, SLOT_UP, SLOT_DOWN };
static const uint8_t straight_slot[8] = {SLOT_RIGHT, 0, SLOT_DOWN, 0, SLOT_LEFT, 0, SLOT_UP, 0};
static const uint8_t axis_directions[4] = {
    [NOAH_PD_AXIS_VERTICAL] = 1u << 2 | 1u << 6,
    [NOAH_PD_AXIS_HORIZONTAL] = 1u << 0 | 1u << 4,
    [NOAH_PD_AXIS_DOMINANT] = 0x55u,
    [NOAH_PD_AXIS_EIGHT] = 0xffu,
};

typedef struct {
    int64_t  heading_x, heading_y; // smoothed motion, in the engine's units
    int64_t  progress;             // along the held direction
    uint32_t last_motion;
    uint8_t  held;                 // DIRECTION_NONE when free
    bool     motion_known;
    bool     fresh; // after a pause: choose without the margin
    bool     spent;     // a once-per-movement mode has sent this movement's output
    uint8_t  spent_dir; // the direction that sent it
} direction_state_t;
static direction_state_t direction;

// Diagonal slots are stored up-left, up-right, down-left, down-right.
static const uint8_t *direction_slot(uint8_t dir) {
    if (!(dir & 1u)) return active + 36u + straight_slot[dir] * 4u;
    return active + 70u + ((direction_y[dir] < 0 ? 0u : 2u) + (direction_x[dir] < 0 ? 0u : 1u)) * 4u;
}

// The directions a heading may be given: those the axis policy has, less the
// empty ones whose share goes to their neighbours. "Both" keeps an empty
// direction when both its compass neighbours, 45 degrees either side, exist;
// only eight directions has them, so elsewhere it acts as nearest.
static uint8_t available_directions(void) {
    uint8_t mask = axis_directions[active[3] & 3u], empty = active[86];
    for (uint8_t dir = 0; dir < 8; dir++) {
        if (!(mask & (1u << dir)) || u16(direction_slot(dir))) continue;
        if (empty == NOAH_PD_EMPTY_DIRECTION_NEAREST || (empty == NOAH_PD_EMPTY_DIRECTION_BOTH && active[3] != NOAH_PD_AXIS_EIGHT)) mask &= (uint8_t)~(1u << dir);
    }
    return mask;
}

// The angle of (x, y), clockwise from right on a y-down report, in eighths of
// a degree. atan t ~= 45t + 15.64 t (1 - t) degrees, within a quarter degree.
static uint16_t heading_angle(int64_t x, int64_t y) {
    uint64_t ax = (uint64_t)(x < 0 ? -x : x), ay = (uint64_t)(y < 0 ? -y : y);
    uint64_t lo = ax < ay ? ax : ay, hi = ax < ay ? ay : ax;
    if (!hi) return 0;
    uint64_t t = (lo << 15) / hi;
    uint32_t a = (uint32_t)((360u * t + ((125u * t * (32768u - t)) >> 15)) >> 15);
    if (ay > ax) a = 720u - a;
    if (x >= 0 && y >= 0) return (uint16_t)a;
    if (x < 0 && y >= 0) return (uint16_t)(1440u - a);
    if (x < 0) return (uint16_t)(1440u + a);
    return (uint16_t)((ANGLE_TURN - a) % ANGLE_TURN);
}

static uint16_t angle_between(uint16_t a, uint8_t dir) {
    int32_t d = (int32_t)a - (int32_t)dir * DIRECTION_ANGLE;
    if (d < 0) d = -d;
    return (uint16_t)(d > ANGLE_TURN / 2 ? ANGLE_TURN - d : d);
}

static uint8_t choose_direction(int64_t x, int64_t y, uint8_t available) {
    uint16_t angle = heading_angle(x, y);
    uint8_t  best  = DIRECTION_NONE, held = direction.held;
    uint16_t best_distance = UINT16_MAX;

    for (uint8_t dir = 0; dir < 8; dir++) {
        if (!(available & (1u << dir))) continue;
        uint16_t distance = angle_between(angle, dir);
        if (distance < best_distance || (distance == best_distance && dir == held)) {
            best          = dir;
            best_distance = distance;
        }
    }
    if (held != DIRECTION_NONE && !direction.fresh && (available & (1u << held)) && angle_between(angle, held) <= best_distance + DIRECTION_MARGIN) return held;
    return best;
}

// One threshold step toward `dir`; returns whether it sent anything.
static bool directional_step(uint8_t dir, uint8_t *budget) {
    const uint8_t *slot = direction_slot(dir);
    bool           sent = false;
    if (u16(slot)) {
        sent = emit_tap(slot);
        (*budget)--;
    } else if (active[86] == NOAH_PD_EMPTY_DIRECTION_BOTH) {
        // Both compass neighbours: a diagonal's two straight directions, a
        // straight direction's two diagonals. An empty neighbour sends nothing.
        // The one the movement leans toward goes first.
        uint8_t  first = (uint8_t)((dir + 7u) & 7u), second = (uint8_t)((dir + 1u) & 7u);
        uint16_t angle = heading_angle(direction.heading_x, direction.heading_y);
        if (angle_between(angle, second) < angle_between(angle, first)) {
            uint8_t swap = first;
            first        = second;
            second       = swap;
        }
        sent = emit_tap(direction_slot(first));
        sent = emit_tap(direction_slot(second)) || sent;
        *budget = *budget >= 2u ? (uint8_t)(*budget - 2u) : 0u;
    } else {
        (*budget)--; // a dead zone still consumes the motion
    }
    return sent;
}

// Whether a spent movement keeps the held direction from sending: it does
// unless the held direction points back against the one that sent.
static bool directional_blocked(void) {
    uint8_t held = direction.held, sent = direction.spent_dir;
    return direction.spent && direction_x[held] * direction_x[sent] + direction_y[held] * direction_y[sent] >= 0;
}

// Sends the held direction's whole steps, up to the per-report budget. A
// blocked direction drops its progress instead.
static void directional_drain(int64_t step, uint8_t *budget) {
    while (*budget && !directional_blocked() && direction.progress >= step) {
        if (directional_step(direction.held, budget) && active[87] == NOAH_PD_DIRECTION_OUTPUT_ONCE) {
            direction.spent     = true;
            direction.spent_dir = direction.held;
        }
        direction.progress -= step;
    }
    if (directional_blocked()) direction.progress = 0;
}

static void directional(report_mouse_t report) {
    int64_t  sx = u16(active + 32) ? u16(active + 32) : 1, sy = u16(active + 34) ? u16(active + 34) : 1;
    int64_t  step   = sx * sy;
    uint8_t  budget = NOAH_PD_MODE_MAX_TAPS_PER_TICK, available = available_directions();
    uint32_t now;

    if (!available) return;
    if (!report.x && !report.y) {
        // A still report drains what the held direction has banked, up to the
        // per-report budget, as the single-axis accumulator always did.
        if (direction.held == DIRECTION_NONE) return;
        directional_drain(step, &budget);
        return;
    }
    now = timer_read32();
    if (direction.motion_known && timer_elapsed32(direction.last_motion) > DIRECTION_IDLE_MS) {
        direction.fresh = true;
        direction.spent = false; // a pause ends the movement
    }
    direction.last_motion  = now;
    direction.motion_known = true;

    int64_t nx = (int64_t)report.x * sy, ny = (int64_t)report.y * sx;
    if (direction.held != DIRECTION_NONE && direction_x[direction.held] * nx + direction_y[direction.held] * ny < 0) {
        direction.held = DIRECTION_NONE; // a reversal starts a new move
    }
    if (direction.held == DIRECTION_NONE || direction.fresh) {
        direction.heading_x = nx;
        direction.heading_y = ny;
    } else {
        direction.heading_x += (nx - direction.heading_x) / 2;
        direction.heading_y += (ny - direction.heading_y) / 2;
    }
    uint8_t chosen = choose_direction(direction.heading_x, direction.heading_y, available);
    direction.fresh = false;
    if (chosen == DIRECTION_NONE) return;
    if (chosen != direction.held) {
        direction.held     = chosen;
        direction.progress = chosen & 1u ? -step / 2 : 0;
    }
    int8_t  dx = direction_x[chosen], dy = direction_y[chosen];
    int64_t along = dx * nx + dy * ny;
    if (dx && dy) along = along * 181 / 256; // one diagonal step is one step long
    direction.progress += along;
    if (direction.progress < -step / 2) direction.progress = -step / 2;
    // The backlog keeps at most NOAH_PD_MODE_MAX_BACKLOG_TAPS whole steps and
    // the part of one, as the single-axis accumulator always did.
    if (direction.progress > step * NOAH_PD_MODE_MAX_BACKLOG_TAPS) direction.progress = step * NOAH_PD_MODE_MAX_BACKLOG_TAPS + direction.progress % step;
    directional_drain(step, &budget);
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
    direction = (direction_state_t){.held = DIRECTION_NONE};
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
    directional(report);
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

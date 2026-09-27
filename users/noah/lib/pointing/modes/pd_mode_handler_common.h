// Shared bounds and report helper for the configured pointing engine.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../../action/action_dispatch.h"

#ifndef NOAH_PD_MODE_MAX_TAPS_PER_TICK
#    define NOAH_PD_MODE_MAX_TAPS_PER_TICK 4
#endif
#ifndef NOAH_PD_MODE_MAX_BACKLOG_TAPS
#    define NOAH_PD_MODE_MAX_BACKLOG_TAPS 32
#endif

_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK > 0, "pointing tap budget must be nonzero");
_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK <= UINT8_MAX, "pointing tap budget must fit uint8_t");
_Static_assert(NOAH_PD_MODE_MAX_BACKLOG_TAPS > 0, "pointing backlog must be nonzero");
_Static_assert(NOAH_PD_MODE_MAX_BACKLOG_TAPS <= UINT16_MAX, "pointing backlog must fit uint16_t diagnostics");
_Static_assert(NOAH_PD_MODE_MAX_TAPS_PER_TICK <= NOAH_PD_MODE_MAX_BACKLOG_TAPS, "pointing tap budget must not exceed its backlog cap");

static inline report_mouse_t pd_mode_freeze_mouse(void) {
    return (report_mouse_t){0};
}

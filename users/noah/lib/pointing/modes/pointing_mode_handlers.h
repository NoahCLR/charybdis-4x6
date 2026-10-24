// Shared dragscroll backend used by configured pointing slots.
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

// The scroll engine behind every scrolling slot, driven through
// noah_pd_configured_scroll (pointing_mode_configured.h). Reset clears its
// residual motion and axis lock.
void reset_dragscroll_mode(void);

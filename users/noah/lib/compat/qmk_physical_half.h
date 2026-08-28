// ──────────────────────────────────────────────────────────────────────────
// QMK Physical-Half Identity Boundary
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_PHYSICAL_HALF_ORIGIN_LEFT  = 0u,
    NOAH_PHYSICAL_HALF_ORIGIN_RIGHT = 1u,
};

// Returns a stable, flash-provisioned physical origin when this firmware was
// built with NOAH_PHYSICAL_HALF=left or NOAH_PHYSICAL_HALF=right. Current USB
// role and EEPROM contents never participate in this identity.
bool noah_qmk_physical_half_origin(uint8_t *origin);

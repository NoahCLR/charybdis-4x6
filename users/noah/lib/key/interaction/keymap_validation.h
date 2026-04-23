// ────────────────────────────────────────────────────────────────────────────
// Keymap Validation
// ────────────────────────────────────────────────────────────────────────────
//
// Shared validation for authored keymap data that should be checked once
// after keyboard init.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

uint8_t noah_keymap_validate(void);

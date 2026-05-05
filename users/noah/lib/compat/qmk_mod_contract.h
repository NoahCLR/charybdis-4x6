// ────────────────────────────────────────────────────────────────────────────
// QMK Modifier Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Declares this userspace's intentional override of QMK's register_mods() and
// unregister_mods() symbols so modifier ownership remains centralized.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

void register_mods(uint8_t mods);
void unregister_mods(uint8_t mods);

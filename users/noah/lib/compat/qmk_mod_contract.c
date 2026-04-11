// ────────────────────────────────────────────────────────────────────────────
// QMK Modifier Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include "../state/keyboard_mod_ownership.h"

// This userspace intentionally overrides QMK's register_mods()/unregister_mods()
// symbols on this fork so all modifier registration flows through the shared
// ownership refcount model. If the upstream fork changes how those symbols are
// declared or routed, re-validate this compatibility boundary.
void register_mods(uint8_t mods) {
    keyboard_mod_ownership_register_mods(mods);
}

void unregister_mods(uint8_t mods) {
    keyboard_mod_ownership_unregister_mods(mods);
}

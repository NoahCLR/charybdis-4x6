// ────────────────────────────────────────────────────────────────────────────
// Autocorrect Switch
// ────────────────────────────────────────────────────────────────────────────

#include "autocorrect_switch.h"

static bool autocorrect_override = true;

void autocorrect_switch_set_override(bool allowed) {
    autocorrect_override = allowed;
}

bool autocorrect_switch_override(void) {
    return autocorrect_override;
}

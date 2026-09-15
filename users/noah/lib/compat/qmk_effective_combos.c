#include "qmk_effective_combos.h"
#if defined(COMBO_ENABLE) && defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
// These override QMK's weak introspection hooks. Engine, origin tracking and
// readback all use the same stable native table.
uint16_t combo_count(void) {
    return noah_effective_combo_count();
}
combo_t *combo_get(uint16_t index) {
    return noah_effective_combo_get(index);
}
uint16_t get_combo_term(uint16_t index, combo_t *combo) {
    (void)combo;
    return noah_effective_combo_term(index);
}
bool get_combo_must_hold(uint16_t index, combo_t *combo) {
    (void)combo;
    return (noah_effective_combo_flags(index) & 1u) != 0u;
}
bool get_combo_must_tap(uint16_t index, combo_t *combo) {
    (void)combo;
    return (noah_effective_combo_flags(index) & 2u) != 0u;
}
bool get_combo_must_press_in_order(uint16_t index, combo_t *combo) {
    (void)combo;
    return (noah_effective_combo_flags(index) & 4u) != 0u;
}
uint16_t noah_qmk_combo_hold_term(void) {
    return noah_effective_combo_hold_term();
}
#endif

#include "users/noah/lib/runtime_v2/runtime_v2.h"

void runtime_v2_layer_lock_set(uint8_t layer, bool active) {
    (void)layer;
    (void)active;
}

void runtime_v2_pd_mode_lock_set(pd_mode_mask_t mode, bool active) {
    (void)mode;
    (void)active;
}

void runtime_v2_observe_release_dispatch_deferred(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    (void)key_pos;
    (void)action;
    (void)mods;
}

void runtime_v2_observe_release_dispatch_drained(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    (void)key_pos;
    (void)action;
    (void)mods;
}

void runtime_v2_observe_deferred_release_blocker_profile(keypos_t key_pos, bool active, bool blocks_before_tap_term, bool blocks_after_tap_term) {
    (void)key_pos;
    (void)active;
    (void)blocks_before_tap_term;
    (void)blocks_after_tap_term;
}

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

bool runtime_v2_blocker_queries_authoritative(void) {
    return false;
}

bool runtime_v2_has_any_deferred_release_blocker(void) {
    return false;
}

bool runtime_v2_has_foreign_deferred_release_blocker_except(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

uint8_t runtime_v2_pending_release_count(void) {
    return 0u;
}

bool runtime_v2_queue_pending_release_dispatch(keypos_t key_pos, uint16_t action, keyboard_mod_state_t mods) {
    (void)key_pos;
    (void)action;
    (void)mods;
    return false;
}

bool runtime_v2_pending_release_at_order(uint8_t order, pending_release_t *out) {
    (void)order;
    if (out) {
        *out = (pending_release_t){0};
    }
    return false;
}

uint8_t runtime_v2_take_pending_release_dispatches(pending_release_t *out, uint8_t capacity) {
    (void)out;
    (void)capacity;
    return 0u;
}

bool runtime_v2_take_pending_multi_tap_flush(keypos_t key_pos, uint16_t *action, uint8_t *repeat_count) {
    (void)key_pos;
    if (action) {
        *action = KC_NO;
    }
    if (repeat_count) {
        *repeat_count = 0u;
    }
    return false;
}

bool runtime_v2_reset_pending_multi_tap(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

bool runtime_v2_retire_press_token(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

void runtime_v2_observe_held_action_register(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void runtime_v2_observe_held_action_unregister(keypos_t key_pos, uint16_t action) {
    (void)key_pos;
    (void)action;
}

void runtime_v2_observe_repeat_start(keypos_t key_pos, uint16_t action, uint16_t repeat_hz) {
    (void)key_pos;
    (void)action;
    (void)repeat_hz;
}

bool runtime_v2_release_owned_state_by_key(keypos_t key_pos) {
    (void)key_pos;
    return false;
}

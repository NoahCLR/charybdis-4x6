#include <stdbool.h>
#include <stdint.h>

#include "users/noah/lib/action/action_dispatch.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

#if defined(__GNUC__) || defined(__clang__)
#    define HOST_WEAK __attribute__((weak))
#else
#    define HOST_WEAK
#endif

HOST_WEAK void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    (void)action;
    (void)pressed;
}

HOST_WEAK bool owned_keycode_register(uint16_t keycode) {
    (void)keycode;
    return false;
}

HOST_WEAK bool owned_keycode_unregister(uint16_t keycode) {
    (void)keycode;
    return false;
}

HOST_WEAK void noah_dispatch_synthetic_tap(uint16_t keycode) {
    (void)keycode;
}

HOST_WEAK void noah_dispatch_synthetic_record(uint16_t keycode, bool pressed) {
    (void)keycode;
    (void)pressed;
}

HOST_WEAK void noah_dispatch_synthetic_qmk_tap(uint16_t keycode) {
    (void)keycode;
}

HOST_WEAK void noah_dispatch_synthetic_qmk_record(uint16_t keycode, bool pressed, uint8_t tap_count) {
    (void)keycode;
    (void)pressed;
    (void)tap_count;
}

HOST_WEAK const pd_mode_def_t *pd_mode_lock_action_lookup(uint16_t action) {
    (void)action;
    return NULL;
}

HOST_WEAK bool pd_mode_toggle_lock_state(pd_mode_mask_t mode) {
    (void)mode;
    return false;
}

HOST_WEAK void layer_ownership_toggle_lock_state(uint8_t layer) {
    (void)layer;
}

HOST_WEAK void layer_ownership_momentary_press(keypos_t key_pos, uint8_t layer) {
    (void)key_pos;
    (void)layer;
}

HOST_WEAK void layer_ownership_momentary_release(keypos_t key_pos) {
    (void)key_pos;
}

HOST_WEAK void tap_code16(uint16_t keycode) {
    (void)keycode;
}

HOST_WEAK void register_code16(uint16_t keycode) {
    (void)keycode;
}

HOST_WEAK void unregister_code16(uint16_t keycode) {
    (void)keycode;
}

#undef HOST_WEAK

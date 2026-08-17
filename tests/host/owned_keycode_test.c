#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/action/owned_keycode.h"

static uint8_t  register_code_calls[16];
static uint8_t  unregister_code_calls[16];
static uint8_t  register_code_count;
static uint8_t  unregister_code_count;
static uint16_t mod_register_calls[16];
static uint16_t mod_unregister_calls[16];
static uint8_t  mod_register_count;
static uint8_t  mod_unregister_count;
static uint8_t  register_mods_calls[16];
static uint8_t  unregister_mods_calls[16];
static uint8_t  register_mods_count;
static uint8_t  unregister_mods_count;
static uint16_t wait_calls[16];
static uint8_t  wait_call_count;
static uint16_t pointer_action_calls[16];
static bool     pointer_action_pressed[16];
static uint8_t  pointer_action_call_count;
static bool     can_register_mods;
static bool     can_unregister_mods;
// Ordering between the usage stubs and the modifier stubs is what distinguishes
// a correct shifted hold from one the host reads as unshifted, so every report
// mutation takes a ticket from one shared counter.
static uint8_t call_sequence;
static uint8_t register_code_seq[16];
static uint8_t unregister_code_seq[16];
static uint8_t register_mods_seq[16];
static uint8_t unregister_mods_seq[16];
static uint8_t fake_report_mods;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

static void test_reset_stubs(void) {
    owned_keycode_reset_for_test();
    register_code_count       = 0;
    unregister_code_count     = 0;
    mod_register_count        = 0;
    mod_unregister_count      = 0;
    register_mods_count       = 0;
    unregister_mods_count     = 0;
    wait_call_count           = 0;
    pointer_action_call_count = 0;
    can_register_mods         = true;
    can_unregister_mods       = true;
    call_sequence             = 0;
    fake_report_mods          = 0;
}

static keyrecord_t test_record(bool pressed) {
    return (keyrecord_t){
        .event =
            {
                .type    = KEY_EVENT,
                .key     = {.row = 0, .col = 0},
                .pressed = pressed,
            },
    };
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    CHECK(mod_register_count < ARRAY_SIZE(mod_register_calls));
    mod_register_calls[mod_register_count++] = keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    CHECK(mod_unregister_count < ARRAY_SIZE(mod_unregister_calls));
    mod_unregister_calls[mod_unregister_count++] = keycode;
}

bool keyboard_mod_ownership_can_register_mods(uint8_t mods) {
    (void)mods;
    return can_register_mods;
}

bool keyboard_mod_ownership_can_unregister_mods(uint8_t mods) {
    (void)mods;
    return can_unregister_mods;
}

bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    uint8_t added = (uint8_t)(mods & ~fake_report_mods);

    CHECK(register_mods_count < ARRAY_SIZE(register_mods_calls));
    register_mods_seq[register_mods_count]     = call_sequence++;
    register_mods_calls[register_mods_count++] = mods;
    fake_report_mods |= mods;
    return added != 0u;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    CHECK(unregister_mods_count < ARRAY_SIZE(unregister_mods_calls));
    unregister_mods_seq[unregister_mods_count]     = call_sequence++;
    unregister_mods_calls[unregister_mods_count++] = mods;
}

void register_code(uint8_t keycode) {
    CHECK(register_code_count < ARRAY_SIZE(register_code_calls));
    register_code_seq[register_code_count]     = call_sequence++;
    register_code_calls[register_code_count++] = keycode;
}

void unregister_code(uint8_t keycode) {
    CHECK(unregister_code_count < ARRAY_SIZE(unregister_code_calls));
    unregister_code_seq[unregister_code_count]     = call_sequence++;
    unregister_code_calls[unregister_code_count++] = keycode;
}

void wait_ms(uint16_t ms) {
    CHECK(wait_call_count < ARRAY_SIZE(wait_calls));
    wait_calls[wait_call_count++] = ms;
}

void pointer_layer_policy_note_action(uint16_t action, bool pressed) {
    CHECK(pointer_action_call_count < ARRAY_SIZE(pointer_action_calls));
    pointer_action_calls[pointer_action_call_count]   = action;
    pointer_action_pressed[pointer_action_call_count] = pressed;
    pointer_action_call_count++;
}

static void test_plain_key_registers_and_unregisters_directly(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(KC_C));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_C);
    CHECK(mod_register_count == 0);
    CHECK(register_mods_count == 0);

    CHECK(owned_keycode_unregister(KC_C));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_C);
    CHECK(mod_unregister_count == 0);
    CHECK(unregister_mods_count == 0);
}

static void test_plain_modifier_uses_mod_ownership(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(KC_LEFT_SHIFT));
    CHECK(mod_register_count == 0);
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(register_code_count == 0);

    CHECK(owned_keycode_unregister(KC_LEFT_SHIFT));
    CHECK(mod_unregister_count == 0);
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(unregister_code_count == 0);
}

static void test_modded_key_registers_mods_and_basic_key(void) {
    uint16_t keycode = G(KC_V);

    test_reset_stubs();

    CHECK(owned_keycode_register(keycode));
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_LEFT_GUI));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_V);

    CHECK(owned_keycode_unregister(keycode));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_V);
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_LEFT_GUI));
}

static void test_right_modded_key_uses_right_side_modifier_mask(void) {
    uint16_t keycode = (uint16_t)(QK_RMODS_MIN | QK_LSFT | KC_C);

    test_reset_stubs();

    CHECK(owned_keycode_register(keycode));
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_C);

    CHECK(owned_keycode_unregister(keycode));
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_RIGHT_SHIFT));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_C);
}

static void test_non_8bit_non_modded_keycodes_are_rejected(void) {
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    CHECK(!owned_keycode_register(SAFE_RANGE));
    CHECK(!owned_keycode_unregister(SAFE_RANGE));
    CHECK(register_code_count == 0);
    CHECK(unregister_code_count == 0);
    CHECK(mod_register_count == 0);
    CHECK(mod_unregister_count == 0);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.unsupported_count == 2);
}

static void test_tap_uses_standard_and_caps_delays(void) {
    test_reset_stubs();

    CHECK(owned_keycode_tap(KC_C));
    CHECK(register_code_count == 1);
    CHECK(unregister_code_count == 1);
    CHECK(wait_call_count == 1);
    CHECK(wait_calls[0] == TAP_CODE_DELAY);

    test_reset_stubs();

    CHECK(owned_keycode_tap(KC_CAPS_LOCK));
    CHECK(register_code_count == 1);
    CHECK(unregister_code_count == 1);
    CHECK(wait_call_count == 1);
    CHECK(wait_calls[0] == TAP_HOLD_CAPS_DELAY);
}

static void test_mouse_buttons_notify_pointer_policy(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(MS_BTN1));
    CHECK(pointer_action_call_count == 1);
    CHECK(pointer_action_calls[0] == MS_BTN1);
    CHECK(pointer_action_pressed[0]);

    CHECK(owned_keycode_unregister(MS_BTN1));
    CHECK(pointer_action_call_count == 2);
    CHECK(pointer_action_calls[1] == MS_BTN1);
    CHECK(!pointer_action_pressed[1]);
}

static void test_two_leases_share_one_basic_report_transition(void) {
    owned_keycode_lease_t          lease_a = {0};
    owned_keycode_lease_t          lease_b = {0};
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_C, &lease_a));
    CHECK(owned_keycode_acquire(KC_C, &lease_b));
    CHECK(register_code_count == 1);

    CHECK(owned_keycode_release(&lease_a));
    CHECK(unregister_code_count == 0);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == 1);

    CHECK(owned_keycode_release(&lease_b));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_C);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == 0);
}

static void test_modded_leases_share_the_basic_component(void) {
    owned_keycode_lease_t gui_c   = {0};
    owned_keycode_lease_t shift_c = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(G(KC_C), &gui_c));
    CHECK(owned_keycode_acquire(S(KC_C), &shift_c));
    CHECK(register_code_count == 1);
    CHECK(register_mods_count == 2);

    CHECK(owned_keycode_release(&gui_c));
    CHECK(unregister_code_count == 0);
    CHECK(unregister_mods_count == 1);

    CHECK(owned_keycode_release(&shift_c));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_mods_count == 2);
}

static void test_physical_owner_keeps_synthetic_tap_from_toggling_report(void) {
    keyrecord_t press   = test_record(true);
    keyrecord_t release = test_record(false);

    test_reset_stubs();

    owned_keycode_track_physical_event(KC_C, &press);
    CHECK(owned_keycode_tap(KC_C));
    CHECK(register_code_count == 0);
    CHECK(unregister_code_count == 0);
    CHECK(pointer_action_call_count == 0);
    CHECK(wait_call_count == 1);

    owned_keycode_track_physical_event(KC_C, &release);
    CHECK(!owned_keycode_should_suppress_default(KC_C, &release));
}

static void test_synthetic_owner_suppresses_overlapping_physical_default(void) {
    owned_keycode_lease_t lease   = {0};
    keyrecord_t           press   = test_record(true);
    keyrecord_t           release = test_record(false);

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_C, &lease));
    owned_keycode_track_physical_event(KC_C, &press);
    CHECK(owned_keycode_should_suppress_default(KC_C, &press));
    owned_keycode_track_physical_event(KC_C, &release);
    CHECK(owned_keycode_should_suppress_default(KC_C, &release));

    CHECK(owned_keycode_release(&lease));
    CHECK(register_code_count == 1);
    CHECK(unregister_code_count == 1);
}

static void test_synthetic_release_defers_to_remaining_physical_owner(void) {
    owned_keycode_lease_t lease   = {0};
    keyrecord_t           press   = test_record(true);
    keyrecord_t           release = test_record(false);

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_C, &lease));
    owned_keycode_track_physical_event(KC_C, &press);
    CHECK(owned_keycode_release(&lease));
    CHECK(unregister_code_count == 0);
    CHECK(!owned_keycode_should_suppress_default(KC_C, &release));

    owned_keycode_track_physical_event(KC_C, &release);
    CHECK(unregister_code_count == 0);
}

static void test_release_is_idempotent_and_records_the_duplicate(void) {
    owned_keycode_lease_t          lease = {0};
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_C, &lease));
    CHECK(owned_keycode_release(&lease));
    CHECK(owned_keycode_release(&lease));
    CHECK(unregister_code_count == 1);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.idempotent_release_count == 1);
}

static void test_legacy_underflow_is_rejected_without_report_mutation(void) {
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    CHECK(!owned_keycode_unregister(KC_C));
    CHECK(unregister_code_count == 0);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.underflow_count == 1);
}

static void test_basic_refcount_saturation_is_rejected(void) {
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    for (uint16_t i = 0; i < UINT8_MAX; i++) {
        CHECK(owned_keycode_register(KC_C));
    }
    CHECK(!owned_keycode_register(KC_C));
    CHECK(register_code_count == 1);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == UINT8_MAX);
    CHECK(snapshot.saturation_count == 1);

    for (uint16_t i = 0; i < UINT8_MAX; i++) {
        CHECK(owned_keycode_unregister(KC_C));
    }
    CHECK(unregister_code_count == 1);
}

static void test_mod_capacity_failure_does_not_acquire_basic_component(void) {
    owned_keycode_lease_t          lease = {.has_basic = true, .basic = KC_X, .mods = MOD_BIT(KC_LEFT_SHIFT)};
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();
    can_register_mods = false;

    CHECK(!owned_keycode_acquire(G(KC_C), &lease));
    CHECK(!lease.active);
    CHECK(!lease.has_basic);
    CHECK(lease.basic == KC_NO);
    CHECK(lease.mods == 0u);
    CHECK(register_code_count == 0);
    CHECK(register_mods_count == 0);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == 0);
    CHECK(snapshot.saturation_count == 1);
}

static void test_modifier_release_underflow_is_atomic_with_basic_component(void) {
    owned_keycode_lease_t          lease = {0};
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();
    CHECK(owned_keycode_acquire(G(KC_C), &lease));
    can_unregister_mods = false;

    CHECK(!owned_keycode_release(&lease));
    CHECK(!lease.active);
    CHECK(unregister_code_count == 0);
    CHECK(unregister_mods_count == 0);
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == 1);
    CHECK(snapshot.underflow_count == 1);
}

static void test_overlapping_mouse_leases_notify_only_aggregate_edges(void) {
    owned_keycode_lease_t lease_a = {0};
    owned_keycode_lease_t lease_b = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(MS_BTN1, &lease_a));
    CHECK(owned_keycode_acquire(MS_BTN1, &lease_b));
    CHECK(pointer_action_call_count == 1);

    CHECK(owned_keycode_release(&lease_b));
    CHECK(pointer_action_call_count == 1);
    CHECK(owned_keycode_release(&lease_a));
    CHECK(pointer_action_call_count == 2);
}

static void test_consumer_usage_shares_physical_and_managed_ownership(void) {
    owned_keycode_lease_t lease   = {0};
    keyrecord_t           press   = test_record(true);
    keyrecord_t           release = test_record(false);

    test_reset_stubs();

    owned_keycode_track_physical_event(KC_AUDIO_MUTE, &press);
    CHECK(owned_keycode_acquire(KC_AUDIO_MUTE, &lease));
    CHECK(register_code_count == 0);
    CHECK(owned_keycode_release(&lease));
    CHECK(unregister_code_count == 0);
    owned_keycode_track_physical_event(KC_AUDIO_MUTE, &release);
}

// PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR) on KC_8. Shipping KC_8 to the report
// before LSFT gives the host one report holding a bare 8, which it commits as
// an unshifted character before the modifier lands; every typematic repeat
// after that arrives shifted, so the key types "8**" instead of "***".
static void test_modded_hold_registers_mods_before_the_usage(void) {
    owned_keycode_lease_t lease = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(S(KC_8), &lease));
    CHECK(register_mods_count == 1);
    CHECK(register_mods_calls[0] == MOD_BIT(KC_LEFT_SHIFT));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_8);
    CHECK(register_mods_seq[0] < register_code_seq[0]);

    CHECK(owned_keycode_release(&lease));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_code_seq[0] < unregister_mods_seq[0]);
}

// The mouse button leaves on a different USB endpoint than the modifier, so
// report order alone does not survive the trip to the host. A fresh modifier
// gets a settle window; one already in the report does not, because no
// keyboard report was sent for it and there is nothing to outrun.
static void test_fresh_modifier_settles_before_a_mouse_button(void) {
    owned_keycode_lease_t lease = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(A(MS_BTN2), &lease));
    CHECK(register_mods_seq[0] < register_code_seq[0]);
    CHECK(wait_call_count == 1);
    CHECK(wait_calls[0] == OWNED_KEYCODE_MOD_TO_MOUSE_SETTLE_MS);
}

static void test_modifier_already_in_the_report_skips_the_settle(void) {
    owned_keycode_lease_t held  = {0};
    owned_keycode_lease_t lease = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_LEFT_ALT, &held));
    CHECK(owned_keycode_acquire(A(MS_BTN2), &lease));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == MS_BTN2);
    CHECK(wait_call_count == 0);
}

static void test_modded_non_mouse_usage_never_settles(void) {
    owned_keycode_lease_t lease = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(A(KC_8), &lease));
    CHECK(register_code_count == 1);
    CHECK(wait_call_count == 0);
}

static void test_bare_mouse_button_never_settles(void) {
    owned_keycode_lease_t lease = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(MS_BTN2, &lease));
    CHECK(register_code_count == 1);
    CHECK(wait_call_count == 0);
}

static void test_legacy_modded_register_matches_lease_report_order(void) {
    test_reset_stubs();

    CHECK(owned_keycode_register(S(KC_8)));
    CHECK(register_mods_seq[0] < register_code_seq[0]);

    CHECK(owned_keycode_unregister(S(KC_8)));
    CHECK(unregister_code_seq[0] < unregister_mods_seq[0]);
}

static void test_basic_saturation_unwinds_the_mods_it_already_registered(void) {
    owned_keycode_lease_t          lease = {0};
    owned_keycode_debug_snapshot_t snapshot;

    test_reset_stubs();

    for (uint16_t i = 0; i < UINT8_MAX; i++) {
        CHECK(owned_keycode_register(KC_C));
    }

    CHECK(!owned_keycode_acquire(S(KC_C), &lease));
    CHECK(!lease.active);
    CHECK(register_mods_count == 1);
    CHECK(unregister_mods_count == 1);
    CHECK(unregister_mods_calls[0] == MOD_BIT(KC_LEFT_SHIFT));
    owned_keycode_debug_snapshot(KC_C, &snapshot);
    CHECK(snapshot.managed_count == UINT8_MAX);
}

static void test_system_usage_shares_two_managed_owners(void) {
    owned_keycode_lease_t lease_a = {0};
    owned_keycode_lease_t lease_b = {0};

    test_reset_stubs();

    CHECK(owned_keycode_acquire(KC_SYSTEM_POWER, &lease_a));
    CHECK(owned_keycode_acquire(KC_SYSTEM_POWER, &lease_b));
    CHECK(register_code_count == 1);
    CHECK(register_code_calls[0] == KC_SYSTEM_POWER);

    CHECK(owned_keycode_release(&lease_a));
    CHECK(unregister_code_count == 0);
    CHECK(owned_keycode_release(&lease_b));
    CHECK(unregister_code_count == 1);
    CHECK(unregister_code_calls[0] == KC_SYSTEM_POWER);
}

int main(void) {
    test_plain_key_registers_and_unregisters_directly();
    test_plain_modifier_uses_mod_ownership();
    test_modded_key_registers_mods_and_basic_key();
    test_right_modded_key_uses_right_side_modifier_mask();
    test_non_8bit_non_modded_keycodes_are_rejected();
    test_tap_uses_standard_and_caps_delays();
    test_mouse_buttons_notify_pointer_policy();
    test_two_leases_share_one_basic_report_transition();
    test_modded_leases_share_the_basic_component();
    test_physical_owner_keeps_synthetic_tap_from_toggling_report();
    test_synthetic_owner_suppresses_overlapping_physical_default();
    test_synthetic_release_defers_to_remaining_physical_owner();
    test_release_is_idempotent_and_records_the_duplicate();
    test_legacy_underflow_is_rejected_without_report_mutation();
    test_basic_refcount_saturation_is_rejected();
    test_mod_capacity_failure_does_not_acquire_basic_component();
    test_modifier_release_underflow_is_atomic_with_basic_component();
    test_overlapping_mouse_leases_notify_only_aggregate_edges();
    test_consumer_usage_shares_physical_and_managed_ownership();
    test_modded_hold_registers_mods_before_the_usage();
    test_fresh_modifier_settles_before_a_mouse_button();
    test_modifier_already_in_the_report_skips_the_settle();
    test_modded_non_mouse_usage_never_settles();
    test_bare_mouse_button_never_settles();
    test_legacy_modded_register_matches_lease_report_order();
    test_basic_saturation_unwinds_the_mods_it_already_registered();
    test_system_usage_shares_two_managed_owners();

    puts("owned_keycode host tests passed");
    return 0;
}

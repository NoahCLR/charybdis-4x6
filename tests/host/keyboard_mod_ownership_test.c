#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/modifiers/keyboard_mod_policy.h"
#include "users/noah/lib/state/modifiers/keyboard_mod_state.h"

static uint8_t fake_mods;
static uint8_t fake_weak_mods;
static uint8_t fake_oneshot_mods;
static uint8_t fake_oneshot_locked_mods;
layer_state_t  layer_state;

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

uint8_t get_mods(void) {
    return fake_mods;
}

uint8_t get_weak_mods(void) {
    return fake_weak_mods;
}

uint8_t get_oneshot_mods(void) {
    return fake_oneshot_mods;
}

uint8_t get_oneshot_locked_mods(void) {
    return fake_oneshot_locked_mods;
}

void set_mods(uint8_t mods) {
    fake_mods = mods;
}

void set_weak_mods(uint8_t mods) {
    fake_weak_mods = mods;
}

void set_oneshot_mods(uint8_t mods) {
    fake_oneshot_mods = mods;
}

void set_oneshot_locked_mods(uint8_t mods) {
    fake_oneshot_locked_mods = mods;
}

void clear_mods(void) {
    fake_mods = 0;
}

void clear_weak_mods(void) {
    fake_weak_mods = 0;
}

void clear_oneshot_mods(void) {
    fake_oneshot_mods = 0;
}

void clear_oneshot_locked_mods(void) {
    fake_oneshot_locked_mods = 0;
}

void add_mods(uint8_t mods) {
    fake_mods |= mods;
}

void del_mods(uint8_t mods) {
    fake_mods &= (uint8_t)~mods;
}

void send_keyboard_report(void) {}

void register_mods(uint8_t mods);
void unregister_mods(uint8_t mods);

static void test_reset_state(void) {
    fake_mods                = 0;
    fake_weak_mods           = 0;
    fake_oneshot_mods        = 0;
    fake_oneshot_locked_mods = 0;
    keyboard_mod_ownership_reset_for_test();
}

static void test_suspend_allows_nested_same_mod_registration(void) {
    uint8_t              gui_mask = MOD_BIT(KC_LEFT_GUI);
    keyboard_mod_state_t saved;

    test_reset_state();

    keyboard_mod_ownership_register(KC_LEFT_GUI);
    CHECK(get_mods() == gui_mask);

    saved = keyboard_mod_state_suspend();
    CHECK(saved.real == gui_mask);
    CHECK(get_mods() == 0);

    register_mods(gui_mask);
    CHECK(get_mods() == gui_mask);

    unregister_mods(gui_mask);
    CHECK(get_mods() == gui_mask);

    keyboard_mod_state_apply(saved);
    CHECK(get_mods() == gui_mask);

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    CHECK(get_mods() == 0);
}

static void test_managed_only_mask_reports_managed_gui_without_physical_owner(void) {
    uint8_t gui_mask = MOD_BIT(KC_LEFT_GUI);

    test_reset_state();

    keyboard_mod_ownership_register(KC_LEFT_GUI);

    CHECK(keyboard_mod_ownership_managed_only_mask(gui_mask) == gui_mask);
    CHECK(keyboard_mod_ownership_managed_only_mask(MOD_BIT(KC_LEFT_SHIFT)) == 0);
}

static void test_managed_only_mask_keeps_physically_held_gui_visible(void) {
    uint8_t     gui_mask = MOD_BIT(KC_LEFT_GUI);
    keyrecord_t press    = {
        .event =
            {
                .type    = KEY_EVENT,
                .key     = {.row = 0, .col = 0},
                .pressed = true,
            },
    };
    keyrecord_t release = {
        .event =
            {
                .type    = KEY_EVENT,
                .key     = {.row = 0, .col = 0},
                .pressed = false,
            },
    };

    test_reset_state();

    keyboard_mod_ownership_track_physical_keycode_event(KC_LEFT_GUI, &press);
    keyboard_mod_ownership_register(KC_LEFT_GUI);

    CHECK(keyboard_mod_ownership_managed_only_mask(gui_mask) == 0);

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    keyboard_mod_ownership_track_physical_keycode_event(KC_LEFT_GUI, &release);
    CHECK(keyboard_mod_ownership_managed_only_mask(gui_mask) == 0);
}

static void test_keyboard_mod_policy_reads_current_state(void) {
    keyboard_mod_state_t current;

    test_reset_state();

    fake_mods                = 0x01;
    fake_weak_mods           = 0x02;
    fake_oneshot_mods        = 0x04;
    fake_oneshot_locked_mods = 0x08;

    current = keyboard_mod_policy_current_state();

    CHECK(current.real == 0x01);
    CHECK(current.weak == 0x02);
    CHECK(current.oneshot == 0x04);
    CHECK(current.oneshot_locked == 0x08);
}

static void test_keyboard_mod_policy_filters_replay_state(void) {
    keyboard_mod_state_t state = {
        .real           = 0x0F,
        .weak           = 0xF0,
        .oneshot        = 0x33,
        .oneshot_locked = 0xCC,
    };
    keyboard_mod_state_t without_all  = keyboard_mod_policy_without_mods(state, 0x03);
    keyboard_mod_state_t without_real = keyboard_mod_policy_without_real_mods(state, 0x05);
    keyboard_mod_state_t with_real    = keyboard_mod_policy_with_real_mods((keyboard_mod_state_t){.real = 0x01}, 0x04);

    CHECK(without_all.real == 0x0C);
    CHECK(without_all.weak == 0xF0);
    CHECK(without_all.oneshot == 0x30);
    CHECK(without_all.oneshot_locked == 0xCC);

    CHECK(without_real.real == 0x0A);
    CHECK(without_real.weak == state.weak);
    CHECK(without_real.oneshot == state.oneshot);
    CHECK(without_real.oneshot_locked == state.oneshot_locked);

    CHECK(with_real.real == 0x05);
}

static void test_keyboard_mod_policy_preserves_oneshot_emitted_by_replay(void) {
    keyboard_mod_state_t saved = {
        .real           = 0x01,
        .weak           = 0x02,
        .oneshot        = 0x04,
        .oneshot_locked = 0x08,
    };
    keyboard_mod_state_t restored;

    test_reset_state();

    fake_mods                = 0x10;
    fake_weak_mods           = 0x20;
    fake_oneshot_mods        = 0x40;
    fake_oneshot_locked_mods = 0x80;

    restored = keyboard_mod_policy_restore_after_action_replay(OSM(MOD_LCTL), saved);

    CHECK(restored.real == saved.real);
    CHECK(restored.weak == saved.weak);
    CHECK(restored.oneshot == (uint8_t)(saved.oneshot | fake_oneshot_mods));
    CHECK(restored.oneshot_locked == (uint8_t)(saved.oneshot_locked | fake_oneshot_locked_mods));

    restored = keyboard_mod_policy_restore_after_action_replay(KC_C, saved);
    CHECK(restored.real == saved.real);
    CHECK(restored.weak == saved.weak);
    CHECK(restored.oneshot == saved.oneshot);
    CHECK(restored.oneshot_locked == saved.oneshot_locked);
}

static void test_keyboard_mod_policy_preserve_all_window_restores_saved_state(void) {
    keyboard_mod_state_t saved;

    test_reset_state();

    fake_mods                = 0x01;
    fake_weak_mods           = 0x02;
    fake_oneshot_mods        = 0x04;
    fake_oneshot_locked_mods = 0x08;

    saved = keyboard_mod_policy_begin_preserve_all();

    CHECK(saved.real == 0x01);
    CHECK(saved.weak == 0x02);
    CHECK(saved.oneshot == 0x04);
    CHECK(saved.oneshot_locked == 0x08);
    CHECK(fake_mods == 0);
    CHECK(fake_weak_mods == 0);
    CHECK(fake_oneshot_mods == 0);
    CHECK(fake_oneshot_locked_mods == 0);

    fake_mods                = 0x10;
    fake_weak_mods           = 0x20;
    fake_oneshot_mods        = 0x40;
    fake_oneshot_locked_mods = 0x80;

    keyboard_mod_policy_end_preserve_all(saved);

    CHECK(fake_mods == 0x01);
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x04);
    CHECK(fake_oneshot_locked_mods == 0x08);
}

static void test_keyboard_mod_policy_masked_emit_window_restores_saved_state(void) {
    keyboard_mod_state_t saved;

    test_reset_state();

    fake_mods                = 0x0F;
    fake_weak_mods           = 0xF0;
    fake_oneshot_mods        = 0x33;
    fake_oneshot_locked_mods = 0xCC;

    saved = keyboard_mod_policy_begin_masked_emit(0x03);

    CHECK(saved.real == 0x0F);
    CHECK(saved.weak == 0xF0);
    CHECK(saved.oneshot == 0x33);
    CHECK(saved.oneshot_locked == 0xCC);
    CHECK(fake_mods == 0x0C);
    CHECK(fake_weak_mods == 0xF0);
    CHECK(fake_oneshot_mods == 0x30);
    CHECK(fake_oneshot_locked_mods == 0xCC);

    fake_mods                = 0x10;
    fake_weak_mods           = 0x20;
    fake_oneshot_mods        = 0x40;
    fake_oneshot_locked_mods = 0x80;

    keyboard_mod_policy_end_masked_emit(saved);

    CHECK(fake_mods == 0x0F);
    CHECK(fake_weak_mods == 0xF0);
    CHECK(fake_oneshot_mods == 0x33);
    CHECK(fake_oneshot_locked_mods == 0xCC);
}

static void test_keyboard_mod_policy_action_replay_window_preserves_replayed_oneshot_output(void) {
    keyboard_mod_state_t replay = {
        .real           = 0x10,
        .weak           = 0x20,
        .oneshot        = 0x40,
        .oneshot_locked = 0x80,
    };
    keyboard_mod_state_t saved;

    test_reset_state();

    fake_mods                = 0x01;
    fake_weak_mods           = 0x02;
    fake_oneshot_mods        = 0x04;
    fake_oneshot_locked_mods = 0x08;

    saved = keyboard_mod_policy_begin_action_replay(replay);

    CHECK(saved.real == 0x01);
    CHECK(saved.weak == 0x02);
    CHECK(saved.oneshot == 0x04);
    CHECK(saved.oneshot_locked == 0x08);
    CHECK(fake_mods == replay.real);
    CHECK(fake_weak_mods == replay.weak);
    CHECK(fake_oneshot_mods == replay.oneshot);
    CHECK(fake_oneshot_locked_mods == replay.oneshot_locked);

    fake_oneshot_mods        = 0x40;
    fake_oneshot_locked_mods = 0x80;

    keyboard_mod_policy_end_action_replay(OSM(MOD_LCTL), saved);

    CHECK(fake_mods == 0x01);
    CHECK(fake_weak_mods == 0x02);
    CHECK(fake_oneshot_mods == 0x44);
    CHECK(fake_oneshot_locked_mods == 0x88);
}

static void test_keyboard_mod_policy_real_mod_mask_window_restores_managed_only_mods(void) {
    uint8_t gui_mask = MOD_BIT(KC_LEFT_GUI);

    test_reset_state();

    keyboard_mod_ownership_register(KC_LEFT_GUI);
    CHECK(fake_mods == gui_mask);

    CHECK(keyboard_mod_policy_begin_real_mod_mask(gui_mask));
    CHECK(fake_mods == 0);

    keyboard_mod_policy_end_real_mod_mask(gui_mask);
    CHECK(fake_mods == gui_mask);
}

static void test_modifier_capacity_preflight_is_all_or_nothing(void) {
    keyboard_mod_ownership_debug_snapshot_t snapshot;
    uint8_t                                  mods = MOD_BIT(KC_LEFT_GUI) | MOD_BIT(KC_LEFT_SHIFT);

    test_reset_state();

    for (uint16_t i = 0; i < UINT8_MAX; i++) {
        keyboard_mod_ownership_register(KC_LEFT_GUI);
    }
    CHECK(!keyboard_mod_ownership_can_register_mods(mods));

    keyboard_mod_ownership_debug_snapshot(&snapshot);
    CHECK(snapshot.managed_refcounts[3] == UINT8_MAX);
    CHECK(snapshot.managed_refcounts[1] == 0);

    keyboard_mod_ownership_unregister(KC_LEFT_GUI);
    CHECK(keyboard_mod_ownership_can_register_mods(mods));
    CHECK(!keyboard_mod_ownership_can_unregister_mods(mods));

    keyboard_mod_ownership_register(KC_LEFT_SHIFT);
    CHECK(keyboard_mod_ownership_can_unregister_mods(mods));
}

int main(void) {
    test_suspend_allows_nested_same_mod_registration();
    test_managed_only_mask_reports_managed_gui_without_physical_owner();
    test_managed_only_mask_keeps_physically_held_gui_visible();
    test_keyboard_mod_policy_reads_current_state();
    test_keyboard_mod_policy_filters_replay_state();
    test_keyboard_mod_policy_preserves_oneshot_emitted_by_replay();
    test_keyboard_mod_policy_preserve_all_window_restores_saved_state();
    test_keyboard_mod_policy_masked_emit_window_restores_saved_state();
    test_keyboard_mod_policy_action_replay_window_preserves_replayed_oneshot_output();
    test_keyboard_mod_policy_real_mod_mask_window_restores_managed_only_mods();
    test_modifier_capacity_preflight_is_all_or_nothing();
    puts("keyboard_mod_ownership host tests passed");
    return 0;
}

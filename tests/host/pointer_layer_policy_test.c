#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/policy/pointer_layer_policy.h"

static bool           fake_auto_mouse_toggle;
static int8_t         fake_auto_mouse_key_tracker;
static uint8_t        fake_auto_mouse_layer;
static pd_mode_mask_t fake_active_modes;
static uint8_t        auto_mouse_keyevent_calls;
static bool           auto_mouse_keyevent_pressed[8];

layer_state_t layer_state = 0;

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
    fake_auto_mouse_toggle      = false;
    fake_auto_mouse_key_tracker = 0;
    fake_auto_mouse_layer       = 4;
    fake_active_modes           = 0;
    auto_mouse_keyevent_calls   = 0;
    layer_state                 = 0;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return (state & ((layer_state_t)1u << layer)) != 0;
}

bool get_auto_mouse_toggle(void) {
    return fake_auto_mouse_toggle;
}

int8_t get_auto_mouse_key_tracker(void) {
    return fake_auto_mouse_key_tracker;
}

uint8_t get_auto_mouse_layer(void) {
    return fake_auto_mouse_layer;
}

void auto_mouse_keyevent(bool pressed) {
    CHECK(auto_mouse_keyevent_calls < ARRAY_SIZE(auto_mouse_keyevent_pressed));
    auto_mouse_keyevent_pressed[auto_mouse_keyevent_calls++] = pressed;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    switch (keycode) {
        case VOLUME_MODE:
            return PD_MODE_VOLUME;
        case ARROW_MODE:
            return PD_MODE_ARROW;
        default:
            return 0;
    }
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool pd_mode_has_trait(pd_mode_mask_t mode, pd_mode_traits_t trait) {
    pd_mode_traits_t mode_traits = PD_MODE_TRAIT_NONE;

    switch (mode) {
        case PD_MODE_VOLUME:
            mode_traits = PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED;
            break;
        case PD_MODE_ARROW:
            mode_traits = PD_MODE_TRAIT_PREFER_TYPING_LAYER;
            break;
        default:
            break;
    }

    return (mode_traits & trait) == trait;
}

pd_mode_snapshot_t pd_mode_snapshot(void) {
    pd_mode_snapshot_t snapshot = {
        .local.active_mode    = fake_active_modes,
        .local.active_index   = PD_MODE_COUNT,
        .local.locked_index   = PD_MODE_COUNT,
        .display.active_mode  = fake_active_modes,
        .display.active_index = PD_MODE_COUNT,
        .display.locked_index = PD_MODE_COUNT,
    };

    for (uint8_t index = 0; index < PD_MODE_COUNT; index++) {
        pd_mode_mask_t mode = (pd_mode_mask_t)1u << index;

        if ((fake_active_modes & mode) != 0) {
            if (snapshot.local.active_index == PD_MODE_COUNT) {
                snapshot.local.active_index = index;
                snapshot.local.active_mode  = mode;
            }

            if (pd_mode_has_trait(mode, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED)) {
                snapshot.local.active_traits |= PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED;
            }

            if (pd_mode_has_trait(mode, PD_MODE_TRAIT_PREFER_TYPING_LAYER)) {
                snapshot.local.active_traits |= PD_MODE_TRAIT_PREFER_TYPING_LAYER;
            }
        }
    }

    snapshot.display = snapshot.local;
    return snapshot;
}

static void test_non_arrow_pd_mode_marks_layer_holds_as_mouse_records(void) {
    test_reset_stubs();
    fake_active_modes = PD_MODE_VOLUME;

    CHECK(pointer_layer_policy_is_mouse_record(MO(1)));
    CHECK(pointer_layer_policy_is_mouse_record(LT(2, KC_C)));
}

static void test_arrow_mode_does_not_anchor_layer_hold_keys(void) {
    test_reset_stubs();
    fake_active_modes = PD_MODE_ARROW;

    CHECK(!pointer_layer_policy_is_mouse_record(MO(1)));
}

static void test_non_arrow_pd_mode_keys_and_dpi_keys_count_as_mouse_records(void) {
    test_reset_stubs();

    CHECK(pointer_layer_policy_is_mouse_record(VOLUME_MODE));
    CHECK(!pointer_layer_policy_is_mouse_record(ARROW_MODE));
    CHECK(pointer_layer_policy_is_mouse_record(DPI_MOD));
    CHECK(pointer_layer_policy_is_mouse_record(DPI_RMOD));
    CHECK(pointer_layer_policy_is_mouse_record(S_D_MOD));
    CHECK(pointer_layer_policy_is_mouse_record(S_D_RMOD));
}

static void test_mouse_button_actions_notify_auto_mouse(void) {
    test_reset_stubs();

    CHECK(pointer_layer_policy_is_mouse_action(MS_BTN1));
    CHECK(!pointer_layer_policy_is_mouse_action(KC_C));

    pointer_layer_policy_note_action(MS_BTN1, true);
    pointer_layer_policy_note_action(MS_BTN1, false);
    pointer_layer_policy_note_action(KC_C, true);

    CHECK(auto_mouse_keyevent_calls == 2);
    CHECK(auto_mouse_keyevent_pressed[0]);
    CHECK(!auto_mouse_keyevent_pressed[1]);
}

static void test_arrow_mode_prefers_typing_layer_over_auto_mouse_layer(void) {
    test_reset_stubs();
    fake_active_modes     = PD_MODE_ARROW;
    fake_auto_mouse_layer = 4;

    layer_state_t state = ((layer_state_t)1u << 0) | ((layer_state_t)1u << 4);
    layer_state_t next  = pointer_layer_policy_apply(state);

    CHECK((next & ((layer_state_t)1u << 4)) == 0);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
}

static void test_anchored_pd_mode_restores_auto_mouse_layer_when_dropped(void) {
    test_reset_stubs();
    fake_active_modes     = PD_MODE_VOLUME;
    fake_auto_mouse_layer = 4;

    layer_state_t next = pointer_layer_policy_apply((layer_state_t)1u << 0);

    CHECK((next & ((layer_state_t)1u << 4)) != 0);
}

static void test_auto_mouse_toggle_restores_auto_mouse_layer_when_dropped(void) {
    test_reset_stubs();
    fake_auto_mouse_toggle = true;
    fake_auto_mouse_layer  = 4;

    layer_state_t next = pointer_layer_policy_apply((layer_state_t)1u << 0);

    CHECK((next & ((layer_state_t)1u << 4)) != 0);
}

static void test_sniping_layer_strips_unanchored_auto_mouse_layer(void) {
    test_reset_stubs();
    fake_auto_mouse_layer = 4;

    layer_state_t state = ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER) | ((layer_state_t)1u << 4);
    layer_state_t next  = pointer_layer_policy_apply(state);

    CHECK((next & ((layer_state_t)1u << 4)) == 0);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
}

static void test_auto_mouse_layer_coexists_with_non_nav_layers(void) {
    test_reset_stubs();
    fake_auto_mouse_layer = 4;

    layer_state_t state = ((layer_state_t)1u << 1) | ((layer_state_t)1u << 4);
    layer_state_t next  = pointer_layer_policy_apply(state);

    CHECK((next & ((layer_state_t)1u << 1)) != 0);
    CHECK((next & ((layer_state_t)1u << 4)) != 0);
}

static void test_sniping_layer_strips_pd_mode_anchored_auto_mouse_layer(void) {
    test_reset_stubs();
    fake_active_modes     = PD_MODE_VOLUME;
    fake_auto_mouse_layer = 4;

    layer_state_t state = ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER) | ((layer_state_t)1u << 4);
    layer_state_t next  = pointer_layer_policy_apply(state);

    CHECK((next & ((layer_state_t)1u << 4)) == 0);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
}

static void test_sniping_layer_strips_key_tracker_anchored_auto_mouse_layer(void) {
    test_reset_stubs();
    fake_auto_mouse_key_tracker = 1;
    fake_auto_mouse_layer       = 4;

    layer_state_t state = ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER) | ((layer_state_t)1u << 4);
    layer_state_t next  = pointer_layer_policy_apply(state);

    CHECK((next & ((layer_state_t)1u << 4)) == 0);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
}

static void test_sniping_layer_blocks_anchored_auto_mouse_restore_when_pointer_missing(void) {
    test_reset_stubs();
    fake_auto_mouse_toggle = true;
    fake_auto_mouse_layer  = 4;

    layer_state_t next = pointer_layer_policy_apply((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER);

    CHECK((next & ((layer_state_t)1u << 4)) == 0);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
}

int main(void) {
    test_non_arrow_pd_mode_marks_layer_holds_as_mouse_records();
    test_arrow_mode_does_not_anchor_layer_hold_keys();
    test_non_arrow_pd_mode_keys_and_dpi_keys_count_as_mouse_records();
    test_mouse_button_actions_notify_auto_mouse();
    test_arrow_mode_prefers_typing_layer_over_auto_mouse_layer();
    test_anchored_pd_mode_restores_auto_mouse_layer_when_dropped();
    test_auto_mouse_toggle_restores_auto_mouse_layer_when_dropped();
    test_sniping_layer_strips_unanchored_auto_mouse_layer();
    test_auto_mouse_layer_coexists_with_non_nav_layers();
    test_sniping_layer_strips_pd_mode_anchored_auto_mouse_layer();
    test_sniping_layer_strips_key_tracker_anchored_auto_mouse_layer();
    test_sniping_layer_blocks_anchored_auto_mouse_restore_when_pointer_missing();

    puts("pointer_layer_policy host tests passed");
    return 0;
}

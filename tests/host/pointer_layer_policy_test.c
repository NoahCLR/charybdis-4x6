#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/policy/pointer_layer_policy.h"
#include "users/noah/lib/compat/qmk_tapping_contract.h"

static bool           fake_auto_mouse_toggle;
static int8_t         fake_auto_mouse_key_tracker;
static uint8_t        fake_auto_mouse_layer;
static pd_mode_mask_t fake_active_modes;
static uint8_t        auto_mouse_keyevent_calls;
static bool           auto_mouse_keyevent_pressed[8];
static uint16_t       fake_anchored_behavior_keycode;
static bool           fake_auto_mouse_enabled;
static layer_state_t  fake_locked_layers;
static uint8_t        auto_mouse_toggle_calls;
static uint8_t        fake_source_layer;
static uint8_t        auto_mouse_reset_presses;

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
    pointer_layer_policy_settle_record();
    fake_auto_mouse_toggle         = false;
    fake_auto_mouse_key_tracker    = 0;
    fake_auto_mouse_layer          = 4;
    fake_active_modes              = 0;
    auto_mouse_keyevent_calls      = 0;
    layer_state                    = 0;
    fake_anchored_behavior_keycode = KC_NO;
    fake_auto_mouse_enabled        = true;
    fake_locked_layers             = 0;
    auto_mouse_toggle_calls        = 0;
    fake_source_layer              = 0;
    auto_mouse_reset_presses       = 0;
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
    fake_auto_mouse_key_tracker += pressed ? 1 : -1;
}

bool is_auto_mouse_active(void) {
    return fake_auto_mouse_toggle || fake_auto_mouse_key_tracker != 0;
}

void auto_mouse_reset_trigger(bool pressed) {
    if (pressed) {
        auto_mouse_reset_presses++;
        fake_auto_mouse_toggle      = false;
        fake_auto_mouse_key_tracker = 0;
    }
}

uint8_t read_source_layers_cache(keypos_t key) {
    (void)key;
    return fake_source_layer;
}

bool get_auto_mouse_enable(void) {
    return fake_auto_mouse_enabled;
}

void auto_mouse_toggle(void) {
    fake_auto_mouse_toggle = !fake_auto_mouse_toggle;
    auto_mouse_toggle_calls++;
}

bool layer_ownership_is_locked(uint8_t layer) {
    return (fake_locked_layers & ((layer_state_t)1u << layer)) != 0;
}

pd_mode_mask_t pd_mode_for_keycode(uint16_t keycode) {
    switch (keycode) {
        case PD_SLOT_1:
            return PD_MODE_VOLUME;
        case PD_SLOT_4:
            return PD_MODE_ARROW;
        default:
            return 0;
    }
}

bool is_pd_mode_lock_action(uint16_t action) {
    (void)action;
    return false;
}

bool key_behavior_keeps_auto_mouse_anchored(uint16_t keycode) {
    return fake_anchored_behavior_keycode != KC_NO && keycode == fake_anchored_behavior_keycode;
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

    CHECK(pointer_layer_policy_is_mouse_record(PD_SLOT_1));
    CHECK(!pointer_layer_policy_is_mouse_record(PD_SLOT_4));
    CHECK(pointer_layer_policy_is_mouse_record(DPI_MOD));
    CHECK(pointer_layer_policy_is_mouse_record(DPI_RMOD));
    CHECK(pointer_layer_policy_is_mouse_record(S_D_MOD));
    CHECK(pointer_layer_policy_is_mouse_record(S_D_RMOD));
}

// A keycode that is neither a mouse keycode nor a pd-mode key can still claim
// the anchor through its authored behavior row, so QMK keeps the pointer layer
// up across the press instead of resetting auto mouse on it.
static void test_authored_anchor_row_counts_as_a_mouse_record(void) {
    test_reset_stubs();

    CHECK(!pointer_layer_policy_is_mouse_record(KC_C));

    fake_anchored_behavior_keycode = KC_C;

    CHECK(pointer_layer_policy_is_mouse_record(KC_C));
    CHECK(!pointer_layer_policy_is_mouse_record(KC_D));
}

// process_auto_mouse()'s default branch, which asks is_mouse_record().
static void test_auto_mouse_key_record(uint16_t keycode, uint8_t row, bool pressed) {
    keyrecord_t record = {
        .event =
            {
                .type    = KEY_EVENT,
                .key     = {.row = row, .col = 1},
                .pressed = pressed,
            },
    };

    if (pointer_layer_policy_is_mouse_key_record(keycode, &record)) {
        auto_mouse_keyevent(pressed);
    } else if (!is_auto_mouse_active()) {
        auto_mouse_reset_trigger(pressed);
    }
}

// A key resolved from the auto-mouse layer keeps that layer through its own
// record, so QMK's action lookup agrees with the keycode every hook saw, then
// gets the reset QMK would have given it on the press.
static void test_auto_mouse_layer_key_resets_after_its_record(void) {
    test_reset_stubs();
    fake_source_layer = fake_auto_mouse_layer;

    test_auto_mouse_key_record(KC_J, 1, true);
    CHECK(fake_auto_mouse_key_tracker == 1);
    CHECK(auto_mouse_reset_presses == 0);

    pointer_layer_policy_settle_record();
    CHECK(fake_auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_reset_presses == 1);

    pointer_layer_policy_settle_record();
    CHECK(auto_mouse_reset_presses == 1);

    // Its release is an ordinary non-mouse release.
    test_auto_mouse_key_record(KC_J, 1, false);
    CHECK(fake_auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_reset_presses == 1);
}

static void test_other_layer_key_resets_in_its_record(void) {
    test_reset_stubs();
    fake_source_layer = 0;

    test_auto_mouse_key_record(KC_J, 1, true);
    CHECK(auto_mouse_reset_presses == 1);
    pointer_layer_policy_settle_record();
    CHECK(auto_mouse_reset_presses == 1);
}

// An anchored key keeps the layer as before: nothing to settle.
static void test_anchored_auto_mouse_layer_key_keeps_the_layer(void) {
    test_reset_stubs();
    fake_source_layer              = fake_auto_mouse_layer;
    fake_anchored_behavior_keycode = KC_J;

    test_auto_mouse_key_record(KC_J, 1, true);
    pointer_layer_policy_settle_record();
    CHECK(fake_auto_mouse_key_tracker == 1);
    CHECK(auto_mouse_reset_presses == 0);
    test_auto_mouse_key_record(KC_J, 1, false);
    CHECK(fake_auto_mouse_key_tracker == 0);
}

// QMK resets only a layer nothing else holds; the settle asks after the key
// ran, so a key that locked the layer, or a held mouse key, keeps it.
static void test_settle_keeps_a_layer_something_else_holds(void) {
    test_reset_stubs();
    fake_source_layer = fake_auto_mouse_layer;

    test_auto_mouse_key_record(MS_BTN1, 2, true);
    test_auto_mouse_key_record(KC_J, 1, true);
    CHECK(fake_auto_mouse_key_tracker == 2);
    pointer_layer_policy_settle_record();
    CHECK(fake_auto_mouse_key_tracker == 1);
    CHECK(auto_mouse_reset_presses == 0);
}

// A record that stopped before process_record_user() is settled by the next
// physical record, and a synthetic record in the middle of one is not.
static void test_unsettled_press_settles_on_next_physical_record(void) {
    test_reset_stubs();
    fake_source_layer = fake_auto_mouse_layer;

    test_auto_mouse_key_record(KC_J, 1, true);
    test_auto_mouse_key_record(KC_K, UINT8_MAX, true);
    CHECK(auto_mouse_reset_presses == 0);
    CHECK(fake_auto_mouse_key_tracker == 1);

    fake_source_layer = 0;
    test_auto_mouse_key_record(KC_K, 2, true);
    CHECK(fake_auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_reset_presses >= 1);
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

static void test_debug_snapshot_reports_effective_anchor_inputs(void) {
    pointer_layer_policy_debug_snapshot_t snapshot;

    test_reset_stubs();
    fake_active_modes           = PD_MODE_ARROW;
    fake_auto_mouse_toggle      = true;
    fake_auto_mouse_key_tracker = 2;
    fake_auto_mouse_layer       = 4;

    pointer_layer_policy_debug_snapshot((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER, &snapshot);

    CHECK(snapshot.auto_mouse_anchored);
    CHECK(!snapshot.pd_mode_anchor_active);
    CHECK(snapshot.prefers_typing_layer);
    CHECK(snapshot.auto_mouse_toggle_enabled);
    CHECK(snapshot.sniping_layer_active);
    CHECK(snapshot.auto_mouse_key_tracker == 2);
    CHECK(snapshot.auto_mouse_layer == 4);
}

// Leaves the policy's own anchor released, whatever a test left locked.
static void test_release_layer_lock_anchor(void) {
    fake_locked_layers          = 0;
    fake_auto_mouse_enabled     = true;
    fake_auto_mouse_key_tracker = 1;
    pointer_layer_policy_sync_layer_lock_anchor();
    test_reset_stubs();
}

static void test_pointer_layer_lock_anchors_auto_mouse_once(void) {
    test_reset_stubs();

    fake_locked_layers = (layer_state_t)1u << 4;
    pointer_layer_policy_sync_layer_lock_anchor();
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(auto_mouse_keyevent_calls == 1);
    CHECK(auto_mouse_keyevent_pressed[0]);
    CHECK(fake_auto_mouse_key_tracker == 1);

    fake_locked_layers = 0;
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(auto_mouse_keyevent_calls == 2);
    CHECK(!auto_mouse_keyevent_pressed[1]);
    CHECK(fake_auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_toggle_calls == 0);
}

static void test_layer_lock_anchor_needs_the_auto_mouse_layer_and_auto_mouse_on(void) {
    test_reset_stubs();

    fake_locked_layers = (layer_state_t)1u << 2;
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(auto_mouse_keyevent_calls == 0);

    fake_locked_layers      = (layer_state_t)1u << 4;
    fake_auto_mouse_enabled = false;
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(auto_mouse_keyevent_calls == 0);
    test_release_layer_lock_anchor();
}

// QMK zeroes its tracker when auto-mouse is switched off or retargeted; the
// anchor does not take back a count that is no longer there.
static void test_layer_lock_anchor_is_not_taken_back_after_qmk_reset(void) {
    test_reset_stubs();

    fake_locked_layers = (layer_state_t)1u << 4;
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(fake_auto_mouse_key_tracker == 1);

    fake_auto_mouse_key_tracker = 0;
    fake_locked_layers          = 0;
    pointer_layer_policy_sync_layer_lock_anchor();
    CHECK(auto_mouse_keyevent_calls == 1);
    CHECK(fake_auto_mouse_key_tracker == 0);
}

static void test_layer_change_syncs_the_layer_lock_anchor(void) {
    test_reset_stubs();

    fake_locked_layers = (layer_state_t)1u << 4;
    CHECK(pointer_layer_policy_apply((layer_state_t)1u << 4) == ((layer_state_t)1u << 4));
    CHECK(auto_mouse_keyevent_calls == 1);
    CHECK(auto_mouse_keyevent_pressed[0]);
    test_release_layer_lock_anchor();
}

static keyrecord_t test_record(bool pressed, uint8_t tap_count) {
    keyrecord_t record = {.event = {.key = {.row = 0, .col = 0}, .pressed = pressed}};
    record.tap.count   = tap_count;
    return record;
}

static void test_take_back_undoes_qmks_toggle_on_its_layer_keys(void) {
    keyrecord_t release = test_record(false, 0u);
    keyrecord_t press   = test_record(true, 0u);
    keyrecord_t locking = test_record(false, NOAH_QMK_TAPPING_TOGGLE);

    test_reset_stubs();

    pointer_layer_policy_take_back_qmk_toggle(TO(4), &release);
    pointer_layer_policy_take_back_qmk_toggle(TG(4), &release);
    pointer_layer_policy_take_back_qmk_toggle(TT(4), &locking);
    CHECK(auto_mouse_toggle_calls == 3);

    pointer_layer_policy_take_back_qmk_toggle(TO(4), &press);
    pointer_layer_policy_take_back_qmk_toggle(TO(2), &release);
    pointer_layer_policy_take_back_qmk_toggle(TG(0), &release);
    pointer_layer_policy_take_back_qmk_toggle(TT(4), &release);
    pointer_layer_policy_take_back_qmk_toggle(MO(4), &release);
    pointer_layer_policy_take_back_qmk_toggle(KC_A, &release);
    CHECK(auto_mouse_toggle_calls == 3);

    fake_auto_mouse_enabled = false;
    pointer_layer_policy_take_back_qmk_toggle(TO(4), &release);
    CHECK(auto_mouse_toggle_calls == 3);
}

int main(void) {
    test_auto_mouse_layer_key_resets_after_its_record();
    test_other_layer_key_resets_in_its_record();
    test_anchored_auto_mouse_layer_key_keeps_the_layer();
    test_settle_keeps_a_layer_something_else_holds();
    test_unsettled_press_settles_on_next_physical_record();
    test_non_arrow_pd_mode_marks_layer_holds_as_mouse_records();
    test_arrow_mode_does_not_anchor_layer_hold_keys();
    test_non_arrow_pd_mode_keys_and_dpi_keys_count_as_mouse_records();
    test_authored_anchor_row_counts_as_a_mouse_record();
    test_mouse_button_actions_notify_auto_mouse();
    test_arrow_mode_prefers_typing_layer_over_auto_mouse_layer();
    test_anchored_pd_mode_restores_auto_mouse_layer_when_dropped();
    test_auto_mouse_toggle_restores_auto_mouse_layer_when_dropped();
    test_sniping_layer_strips_unanchored_auto_mouse_layer();
    test_auto_mouse_layer_coexists_with_non_nav_layers();
    test_sniping_layer_strips_pd_mode_anchored_auto_mouse_layer();
    test_sniping_layer_strips_key_tracker_anchored_auto_mouse_layer();
    test_sniping_layer_blocks_anchored_auto_mouse_restore_when_pointer_missing();
    test_debug_snapshot_reports_effective_anchor_inputs();
    test_pointer_layer_lock_anchors_auto_mouse_once();
    test_layer_lock_anchor_needs_the_auto_mouse_layer_and_auto_mouse_on();
    test_layer_lock_anchor_is_not_taken_back_after_qmk_reset();
    test_layer_change_syncs_the_layer_lock_anchor();
    test_take_back_undoes_qmks_toggle_on_its_layer_keys();

    puts("pointer_layer_policy host tests passed");
    return 0;
}

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/key/runtime/slot/origin_registry.h"
#include "users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h"
#include "users/noah/lib/pointing/runtime/pd_mode_keyboard_event_internal.h"
#include "users/noah/lib/pointing/runtime/pd_mode_internal.h"
#include "users/noah/lib/split/runtime_sync.h"
#include "host_runtime_reset_fixture.h"

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;
HOST_RUNTIME_FIXTURE_DEFINE_RESET_QMK_STUBS(runtime_fixture)

#ifndef CHARYBDIS_DRAGSCROLL_DPI
#    define CHARYBDIS_DRAGSCROLL_DPI 100
#endif

enum {
    TEST_KEYCODE = SAFE_RANGE + 0x30,
};

static uint16_t current_cpi;
static uint16_t cpi_set_count;
static uint16_t default_dpi;
static uint16_t sniping_dpi;

static bool    sniping_enabled;
static bool    auto_mouse_toggle_enabled;
static bool    auto_mouse_enabled;
static bool    auto_mouse_active;
static bool    fake_is_master;
static int8_t  auto_mouse_key_tracker;
static uint8_t auto_mouse_layer;
static uint8_t auto_mouse_toggle_count;
static uint8_t auto_mouse_layer_off_count;
static uint8_t auto_mouse_keyevent_calls;

static uint8_t split_sync_count;

static uint8_t  keyboard_mod_register_count;
static uint8_t  keyboard_mod_unregister_count;
static uint16_t last_registered_keycode;
static uint16_t last_unregistered_keycode;
static uint8_t  keyboard_mod_managed_only_mask_count;
static uint8_t  last_managed_only_mask_request;
static uint8_t  managed_only_mask_result;

static uint8_t arrow_key_handler_count;
static bool    arrow_key_handler_result;

static uint8_t reset_volume_count;
static uint8_t reset_brightness_count;
static uint8_t reset_zoom_count;
static uint8_t reset_arrow_count;

split_runtime_sync_remote_t split_runtime_sync_remote = SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;

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

static bool test_bitmap_has_keypos(const uint8_t *bitmap, uint8_t row, uint8_t col) {
    return key_origin_bitmap_has_keypos(bitmap, (keypos_t){.row = row, .col = col});
}

static void test_reset_runtime(void) {
    host_runtime_fixture_reset_userspace_runtime();
}

static void test_reset_stubs(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    test_reset_runtime();
    key_origin_registry_reset();

    current_cpi                          = 0;
    cpi_set_count                        = 0;
    default_dpi                          = 900;
    sniping_dpi                          = 350;
    sniping_enabled                      = false;
    auto_mouse_toggle_enabled            = false;
    auto_mouse_enabled                   = false;
    auto_mouse_active                    = false;
    fake_is_master                       = true;
    auto_mouse_key_tracker               = 0;
    auto_mouse_layer                     = 4;
    auto_mouse_toggle_count              = 0;
    auto_mouse_layer_off_count           = 0;
    auto_mouse_keyevent_calls            = 0;
    split_sync_count                     = 0;
    keyboard_mod_register_count          = 0;
    keyboard_mod_unregister_count        = 0;
    last_registered_keycode              = KC_NO;
    last_unregistered_keycode            = KC_NO;
    keyboard_mod_managed_only_mask_count = 0;
    last_managed_only_mask_request       = 0;
    managed_only_mask_result             = 0;
    arrow_key_handler_count              = 0;
    arrow_key_handler_result             = false;
    reset_volume_count                   = 0;
    reset_brightness_count               = 0;
    reset_zoom_count                     = 0;
    reset_arrow_count                    = 0;
    split_runtime_sync_remote            = (split_runtime_sync_remote_t)SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
}

void split_runtime_sync_init(void) {}
void split_runtime_sync_tick(void) {}
void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
void split_runtime_sync(void) {
    split_sync_count++;
}

void split_runtime_sync_request(void) {
    split_runtime_sync();
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return false;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return sniping_enabled;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return default_dpi;
}

uint16_t charybdis_get_pointer_sniping_dpi(void) {
    return sniping_dpi;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    (void)enabled;
}

void charybdis_set_pointer_sniping_enabled(bool enabled) {
    sniping_enabled = enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    current_cpi = cpi;
    cpi_set_count++;
}

bool get_auto_mouse_toggle(void) {
    return auto_mouse_toggle_enabled;
}

int8_t get_auto_mouse_key_tracker(void) {
    return auto_mouse_key_tracker;
}

uint8_t get_auto_mouse_layer(void) {
    return auto_mouse_layer;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return 0;
}

bool is_auto_mouse_active(void) {
    return auto_mouse_active;
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

void set_auto_mouse_enable(bool enable) {
    auto_mouse_enabled = enable;
}

void set_auto_mouse_layer(uint8_t layer) {
    auto_mouse_layer = layer;
}

void auto_mouse_layer_off(void) {
    auto_mouse_layer_off_count++;
}

void auto_mouse_toggle(void) {
    auto_mouse_toggle_enabled = !auto_mouse_toggle_enabled;
    auto_mouse_toggle_count++;
}

void auto_mouse_keyevent(bool pressed) {
    auto_mouse_keyevent_calls++;
    auto_mouse_key_tracker += pressed ? 1 : -1;
    if (auto_mouse_key_tracker < 0) {
        auto_mouse_key_tracker = 0;
    }
}

void keyboard_mod_ownership_track_physical_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
}

bool keyboard_mod_ownership_should_suppress_default(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    return false;
}

void keyboard_mod_ownership_register_mods(uint8_t mods) {
    (void)mods;
}

void keyboard_mod_ownership_unregister_mods(uint8_t mods) {
    (void)mods;
}

void keyboard_mod_ownership_register(uint16_t keycode) {
    keyboard_mod_register_count++;
    last_registered_keycode = keycode;
}

void keyboard_mod_ownership_unregister(uint16_t keycode) {
    keyboard_mod_unregister_count++;
    last_unregistered_keycode = keycode;
}

uint8_t keyboard_mod_ownership_managed_only_mask(uint8_t mods) {
    keyboard_mod_managed_only_mask_count++;
    last_managed_only_mask_request = mods;
    return (uint8_t)(mods & managed_only_mask_result);
}

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    return mouse_report;
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    arrow_key_handler_count++;
    return arrow_key_handler_result;
}

void reset_volume_mode(void) {
    reset_volume_count++;
}

void reset_dragscroll_mode(void) {}

void reset_brightness_mode(void) {
    reset_brightness_count++;
}

void reset_zoom_mode(void) {
    reset_zoom_count++;
}

void reset_arrow_mode(void) {
    reset_arrow_count++;
}

static void test_registry_metadata_matches_manifest(void) {
    const pd_mode_def_t *volume_mode;
    const pd_mode_def_t *arrow_mode;
    const pd_mode_def_t *dragscroll_mode;
    const pd_mode_def_t *pinch_mode;

    test_reset_stubs();

    volume_mode     = pd_mode_lookup(PD_MODE_VOLUME);
    arrow_mode      = pd_mode_lookup(PD_MODE_ARROW);
    dragscroll_mode = pd_mode_lookup(PD_MODE_DRAGSCROLL);
    pinch_mode      = pd_mode_lookup(PD_MODE_PINCH);

    CHECK(volume_mode != NULL);
    CHECK(volume_mode->mode_flag == PD_MODE_VOLUME);
    CHECK(volume_mode->keycode == VOLUME_MODE);
    CHECK(volume_mode->lock_action == VOLUME_MODE_LOCK);
    CHECK(volume_mode->dpi == PD_MODE_VOLUME_DPI);
    CHECK(volume_mode->lifecycle == NULL);
    CHECK(pd_mode_lock_action_lookup(VOLUME_MODE_LOCK) == volume_mode);
    CHECK(is_pd_mode_lock_action(VOLUME_MODE_LOCK));
    CHECK(pd_mode_for_keycode(VOLUME_MODE) == PD_MODE_VOLUME);

    CHECK(arrow_mode != NULL);
    CHECK(arrow_mode->keycode == ARROW_MODE);
    CHECK(arrow_mode->lock_action == ARROW_MODE_LOCK);
    CHECK(arrow_mode->key_handler == handle_arrow_mode_key);
    CHECK(arrow_mode->lifecycle == NULL);

    CHECK(dragscroll_mode != NULL);
    CHECK(dragscroll_mode->handler == handle_dragscroll_mode);
    CHECK(dragscroll_mode->reset == reset_dragscroll_mode);
    CHECK(dragscroll_mode->lifecycle != NULL);
    CHECK(pinch_mode != NULL);
    CHECK(pinch_mode->handler == handle_dragscroll_mode);
    CHECK(pinch_mode->reset == reset_dragscroll_mode);
    CHECK(pinch_mode->lifecycle != NULL);
}

static void test_trait_queries_match_manifest_policy(void) {
    pd_mode_snapshot_t snapshot;

    test_reset_stubs();

    CHECK(pd_mode_has_trait(PD_MODE_VOLUME, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));
    CHECK(!pd_mode_has_trait(PD_MODE_VOLUME, PD_MODE_TRAIT_PREFER_TYPING_LAYER));
    CHECK(pd_mode_has_trait(PD_MODE_ARROW, PD_MODE_TRAIT_PREFER_TYPING_LAYER));
    CHECK(!pd_mode_has_trait(PD_MODE_ARROW, PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));
    CHECK(pd_mode_has_trait(PD_MODE_PINCH, PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND | PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE));
    CHECK(!pd_any_active_mode_has_trait(PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));

    pd_mode_activate(PD_MODE_ARROW);
    snapshot = pd_mode_snapshot();
    CHECK(pd_any_active_mode_has_trait(PD_MODE_TRAIT_PREFER_TYPING_LAYER));
    CHECK(!pd_any_active_mode_has_trait(PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));
    CHECK(snapshot.local.active_mode == PD_MODE_ARROW);
    CHECK(snapshot.local.active_index == PD_MODE_INDEX_ARROW);
    CHECK((snapshot.local.active_traits & PD_MODE_TRAIT_PREFER_TYPING_LAYER) != 0);
    CHECK(pd_mode_display_active_snapshot() == PD_MODE_ARROW);

    pd_mode_activate(PD_MODE_PINCH);
    snapshot = pd_mode_snapshot();
    CHECK(pd_any_active_mode_has_trait(PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED));
    CHECK(pd_any_active_mode_has_trait(PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND));
    CHECK(snapshot.local.active_mode == PD_MODE_PINCH);
    CHECK(snapshot.local.active_index == PD_MODE_INDEX_PINCH);
    CHECK((snapshot.local.active_traits & PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED) != 0);
    CHECK((snapshot.local.active_traits & PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND) != 0);

    pd_mode_deactivate(PD_MODE_PINCH);
}

static void test_apply_remote_snapshot_keeps_only_one_effective_mode(void) {
    pd_mode_snapshot_t snapshot;

    test_reset_stubs();
    fake_is_master = false;

    pd_mode_apply_remote_snapshot(PD_MODE_VOLUME, PD_MODE_ARROW);
    snapshot = pd_mode_snapshot();

    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(pd_mode_display_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_display_locked_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_display_active(PD_MODE_ARROW));
    CHECK(pd_mode_display_locked(PD_MODE_ARROW));
    CHECK(!pd_mode_display_locked(PD_MODE_VOLUME));
    CHECK(!pd_mode_display_active(PD_MODE_VOLUME));
    CHECK(!pd_mode_local_active(PD_MODE_ARROW));
    CHECK(!pd_mode_local_locked(PD_MODE_ARROW));
    CHECK(snapshot.local.active_mode == 0);
    CHECK(snapshot.local.active_index == PD_MODE_COUNT);
    CHECK(snapshot.display.active_mode == PD_MODE_ARROW);
    CHECK(snapshot.display.locked_mode == PD_MODE_ARROW);
    CHECK(snapshot.display.active_index == PD_MODE_INDEX_ARROW);
    CHECK(snapshot.display.locked_index == PD_MODE_INDEX_ARROW);
    CHECK((snapshot.display.active_traits & PD_MODE_TRAIT_PREFER_TYPING_LAYER) != 0);
}

static void test_keycode_press_at_tracks_trigger_half(void) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press_at(VOLUME_MODE, (keypos_t){.row = 0, .col = 0}));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_local_owner_bitmap_snapshot(bitmap));
    CHECK(test_bitmap_has_keypos(bitmap, 0, 0));
    CHECK(!test_bitmap_has_keypos(bitmap, 4, 0));
    CHECK(split_sync_count == 0);

    CHECK(pd_mode_handle_keycode_press_at(ARROW_MODE, (keypos_t){.row = 4, .col = 0}));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_RIGHT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_RIGHT);
    CHECK(pd_mode_local_owner_bitmap_snapshot(bitmap));
    CHECK(!test_bitmap_has_keypos(bitmap, 0, 0));
    CHECK(test_bitmap_has_keypos(bitmap, 4, 0));
}

static void test_same_mode_key_owners_release_independently(void) {
    keypos_t left_owner  = {.row = 0, .col = 0};
    keypos_t right_owner = {.row = 4, .col = 0};

    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press_at(VOLUME_MODE, left_owner));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);

    CHECK(pd_mode_handle_keycode_press_at(VOLUME_MODE, right_owner));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_BOTH);

    CHECK(pd_mode_handle_keycode_release_at(VOLUME_MODE, left_owner));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_RIGHT);

    CHECK(pd_mode_handle_keycode_release_at(VOLUME_MODE, right_owner));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_NONE);
}

static void test_lock_state_at_tracks_trigger_half_and_clears_on_unlock(void) {
    test_reset_stubs();

    CHECK(pd_mode_set_lock_state_at(PD_MODE_VOLUME, true, (keypos_t){.row = 4, .col = 1}));
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_RIGHT);

    CHECK(pd_mode_set_lock_state_at(PD_MODE_VOLUME, false, (keypos_t){.row = 4, .col = 1}));
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_NONE);
}

static void test_ownerless_mode_change_clears_previous_trigger_half(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press_at(VOLUME_MODE, (keypos_t){.row = 0, .col = 0}));
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);

    CHECK(pd_mode_set_lock_state(PD_MODE_ARROW, true));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_NONE);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_NONE);
}

static void test_combo_origin_bitmap_promotes_trigger_half_to_both_sides(void) {
    uint8_t  bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t  owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    keypos_t owner_key_pos = {.row = 0, .col = 0};

    test_reset_stubs();

    key_origin_bitmap_fill_single(bitmap, owner_key_pos);
    key_origin_bitmap_add_keypos(bitmap, (keypos_t){.row = 4, .col = 0});
    CHECK(key_origin_registry_set_bitmap(owner_key_pos, bitmap));

    CHECK(pd_mode_handle_keycode_press_at(ZOOM_MODE, owner_key_pos));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_ZOOM);
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_BOTH);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_BOTH);
    CHECK(pd_mode_local_owner_bitmap_snapshot(owner_bitmap));
    CHECK(test_bitmap_has_keypos(owner_bitmap, 0, 0));
    CHECK(test_bitmap_has_keypos(owner_bitmap, 4, 0));
}

static void test_apply_remote_mode_ids_tracks_display_owner_half(void) {
    pd_mode_snapshot_t snapshot;

    test_reset_stubs();
    fake_is_master = false;

    pd_mode_apply_remote_mode_ids(pd_mode_id_from_mask(PD_MODE_ZOOM), pd_mode_id_from_mask(PD_MODE_ZOOM), SPLIT_SIDE_MASK_LEFT);
    snapshot = pd_mode_snapshot();

    CHECK(snapshot.display.active_mode == PD_MODE_ZOOM);
    CHECK(snapshot.display.locked_mode == PD_MODE_ZOOM);
    CHECK(snapshot.display.owner_sides == SPLIT_SIDE_MASK_LEFT);
    CHECK(pd_mode_display_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
}

static void test_apply_remote_mode_ids_tracks_display_owner_bitmap(void) {
    uint8_t bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t snapshot_bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();
    fake_is_master = false;

    key_origin_bitmap_clear(bitmap);
    key_origin_bitmap_fill_single(bitmap, (keypos_t){.row = 4, .col = 1});

    pd_mode_apply_remote_mode_ids_with_owner_bitmap(pd_mode_id_from_mask(PD_MODE_ZOOM), pd_mode_id_from_mask(PD_MODE_ZOOM), SPLIT_SIDE_MASK_RIGHT, bitmap);

    CHECK(pd_mode_display_owner_bitmap_snapshot(snapshot_bitmap));
    CHECK(test_bitmap_has_keypos(snapshot_bitmap, 4, 1));
    CHECK(!test_bitmap_has_keypos(snapshot_bitmap, 0, 1));
}

static void test_same_side_owner_change_requires_split_sync_for_exact_rgb(void) {
    pd_mode_apply_result_t result;
    uint8_t                bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press_at(VOLUME_MODE, (keypos_t){.row = 0, .col = 0}));
    split_sync_count = 0;

    result = pd_mode_apply_command((pd_mode_command_t){
        .kind          = PD_MODE_COMMAND_KEY_PRESS,
        .keycode       = VOLUME_MODE,
        .owner_sides   = key_origin_registry_side_mask((keypos_t){.row = 0, .col = 1}),
        .owner_key_pos = {.row = 0, .col = 1},
    });

    CHECK(result.handled);
    CHECK(!result.local_state_changed);
    CHECK(result.split_sync_required);
    CHECK(pd_mode_local_owner_bitmap_snapshot(bitmap));
    CHECK(test_bitmap_has_keypos(bitmap, 0, 0));
    CHECK(test_bitmap_has_keypos(bitmap, 0, 1));
    CHECK(pd_mode_local_owner_sides_snapshot() == SPLIT_SIDE_MASK_LEFT);
}

static void test_set_lock_state_switches_to_single_locked_mode(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_ARROW);
    pd_mode_lock(PD_MODE_BRIGHTNESS);

    reset_arrow_count      = 0;
    reset_brightness_count = 0;
    cpi_set_count          = 0;

    CHECK(pd_mode_set_lock_state(PD_MODE_VOLUME, true));

    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(pd_mode_local_locked(PD_MODE_VOLUME));
    CHECK(!pd_mode_local_active(PD_MODE_ARROW));
    CHECK(!pd_mode_local_active(PD_MODE_BRIGHTNESS));
    CHECK(reset_arrow_count == 0);
    CHECK(reset_brightness_count == 1);
    CHECK(cpi_set_count == 0);

    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);
}

static void test_activate_switches_to_single_unlocked_mode(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_ARROW);
    reset_arrow_count = 0;

    pd_mode_activate(PD_MODE_VOLUME);

    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));
    CHECK(!pd_mode_local_active(PD_MODE_ARROW));
    CHECK(!pd_mode_local_locked(PD_MODE_ARROW));
    CHECK(reset_arrow_count == 1);
}

static void test_apply_command_reports_before_after_and_sync_intent_for_key_press(void) {
    pd_mode_apply_result_t result;

    test_reset_stubs();

    pd_mode_lock(PD_MODE_ARROW);
    reset_arrow_count = 0;
    split_sync_count  = 0;

    result = pd_mode_apply_command((pd_mode_command_t){
        .kind    = PD_MODE_COMMAND_KEY_PRESS,
        .keycode = BRIGHTNESS_MODE,
    });

    CHECK(result.handled);
    CHECK(result.local_state_changed);
    CHECK(result.display_state_changed);
    CHECK(result.split_sync_required);
    CHECK(result.before.local.active_mode == PD_MODE_ARROW);
    CHECK(result.before.local.locked_mode == PD_MODE_ARROW);
    CHECK(result.after.local.active_mode == PD_MODE_BRIGHTNESS);
    CHECK(result.after.local.locked_mode == 0);
    CHECK(split_sync_count == 0);
    CHECK(reset_arrow_count == 1);
}

static void test_apply_command_reports_display_only_remote_snapshot_change(void) {
    pd_mode_apply_result_t result;

    test_reset_stubs();
    fake_is_master = false;

    result = pd_mode_apply_command((pd_mode_command_t){
        .kind           = PD_MODE_COMMAND_REMOTE_SNAPSHOT,
        .active_mode_id = pd_mode_id_from_mask(PD_MODE_ARROW),
        .locked_mode_id = pd_mode_id_from_mask(PD_MODE_ARROW),
    });

    CHECK(result.handled);
    CHECK(!result.local_state_changed);
    CHECK(result.display_state_changed);
    CHECK(!result.split_sync_required);
    CHECK(result.before.local.active_mode == 0);
    CHECK(result.after.local.active_mode == 0);
    CHECK(result.after.display.active_mode == PD_MODE_ARROW);
    CHECK(result.after.display.locked_mode == PD_MODE_ARROW);
}

static void test_apply_command_release_reports_handled_without_sync_when_locked(void) {
    pd_mode_apply_result_t result;

    test_reset_stubs();

    pd_mode_lock(PD_MODE_VOLUME);

    result = pd_mode_apply_command((pd_mode_command_t){
        .kind    = PD_MODE_COMMAND_KEY_RELEASE,
        .keycode = VOLUME_MODE,
    });

    CHECK(result.handled);
    CHECK(!result.local_state_changed);
    CHECK(!result.split_sync_required);
    CHECK(result.after.local.active_mode == PD_MODE_VOLUME);
    CHECK(result.after.local.locked_mode == PD_MODE_VOLUME);
}

static void test_handle_keycode_press_and_release_updates_state_and_syncs(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_ARROW);
    reset_arrow_count = 0;
    split_sync_count  = 0;

    CHECK(pd_mode_handle_keycode_press(BRIGHTNESS_MODE));
    CHECK(split_sync_count == 0);
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_BRIGHTNESS);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(reset_arrow_count == 1);

    CHECK(pd_mode_handle_keycode_release(BRIGHTNESS_MODE));
    CHECK(split_sync_count == 0);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(pd_mode_local_locked_snapshot() == 0);
    CHECK(reset_brightness_count == 1);
}

static void test_handle_keycode_press_and_release_defer_dpi_until_service(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(current_cpi == 0);
    CHECK(cpi_set_count == 0);

    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);

    CHECK(pd_mode_handle_keycode_release(VOLUME_MODE));
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);

    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi);
    CHECK(cpi_set_count == 2);
}

static void test_locked_mode_release_keeps_mode_active(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_VOLUME);
    split_sync_count   = 0;
    reset_volume_count = 0;

    CHECK(pd_mode_handle_keycode_release(VOLUME_MODE));
    CHECK(pd_mode_local_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(split_sync_count == 0);
    CHECK(reset_volume_count == 0);
}

static void test_apply_active_dpi_respects_pointer_state(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_VOLUME);
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);

    cpi_set_count = 0;
    current_cpi   = 7777;
    pd_mode_activate(PD_MODE_DRAGSCROLL);
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == CHARYBDIS_DRAGSCROLL_DPI);
    CHECK(cpi_set_count == 1);

    pd_mode_deactivate(PD_MODE_DRAGSCROLL);
    cpi_set_count   = 0;
    current_cpi     = 7777;
    sniping_enabled = true;
    pd_mode_apply_active_dpi();
    CHECK(current_cpi == sniping_dpi);
    CHECK(cpi_set_count == 1);

    sniping_enabled = false;
    pd_mode_deactivate(PD_MODE_VOLUME);
    cpi_set_count = 0;
    current_cpi   = 0;
    pd_mode_apply_active_dpi();
    CHECK(current_cpi == default_dpi);
    CHECK(cpi_set_count == 1);
}

static void test_pinch_mode_registers_gui_and_dragscroll_side_effects(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_PINCH);
    CHECK(pd_mode_local_active(PD_MODE_PINCH));
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == CHARYBDIS_DRAGSCROLL_DPI);
    CHECK(keyboard_mod_register_count == 1);
    CHECK(last_registered_keycode == KC_LEFT_GUI);

    pd_mode_deactivate(PD_MODE_PINCH);
    CHECK(!pd_mode_local_active(PD_MODE_PINCH));
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi);
    CHECK(keyboard_mod_unregister_count == 1);
    CHECK(last_unregistered_keycode == KC_LEFT_GUI);
}

static void test_pinch_buffered_tap_mask_uses_managed_only_gui_policy(void) {
    test_reset_stubs();
    managed_only_mask_result = MOD_BIT(KC_LEFT_GUI);

    CHECK(pd_mode_buffered_tap_masked_real_mods(PINCH_MODE) == MOD_BIT(KC_LEFT_GUI));
    CHECK(keyboard_mod_managed_only_mask_count == 1);
    CHECK(last_managed_only_mask_request == MOD_BIT(KC_LEFT_GUI));

    CHECK(pd_mode_buffered_tap_masked_real_mods(VOLUME_MODE) == 0);
    CHECK(keyboard_mod_managed_only_mask_count == 1);
}

static void test_pinch_keyboard_event_mask_uses_managed_only_gui_policy(void) {
    test_reset_stubs();
    managed_only_mask_result = MOD_BIT(KC_LEFT_GUI);

    CHECK(pd_mode_active_keyboard_event_masked_real_mods() == 0);
    CHECK(keyboard_mod_managed_only_mask_count == 0);

    pd_mode_activate(PD_MODE_PINCH);
    CHECK(pd_mode_active_keyboard_event_masked_real_mods() == MOD_BIT(KC_LEFT_GUI));
    CHECK(keyboard_mod_managed_only_mask_count == 1);
    CHECK(last_managed_only_mask_request == MOD_BIT(KC_LEFT_GUI));

    pd_mode_activate(PD_MODE_VOLUME);
    CHECK(pd_mode_active_keyboard_event_masked_real_mods() == 0);
    CHECK(keyboard_mod_managed_only_mask_count == 1);
}

static void test_lock_owned_auto_mouse_toggle_tracks_mode_ownership(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 1);

    pd_mode_unlock(PD_MODE_DRAGSCROLL);
    CHECK(!auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 2);

    test_reset_stubs();
    auto_mouse_toggle_enabled = true;

    pd_mode_lock(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 0);

    pd_mode_unlock(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 0);
}

static void test_active_dragscroll_mode_does_not_add_synthetic_auto_mouse_anchor(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_keyevent_calls == 0);
    CHECK(auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_toggle_count == 0);

    pd_mode_deactivate(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_keyevent_calls == 0);
    CHECK(auto_mouse_key_tracker == 0);
    CHECK(auto_mouse_toggle_count == 0);
}

static void test_locked_non_toggle_mode_uses_synthetic_auto_mouse_anchor(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_VOLUME);
    CHECK(auto_mouse_toggle_count == 0);
    CHECK(auto_mouse_keyevent_calls == 1);
    CHECK(auto_mouse_key_tracker == 1);

    pd_mode_unlock(PD_MODE_VOLUME);
    CHECK(auto_mouse_toggle_count == 0);
    CHECK(auto_mouse_keyevent_calls == 2);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_locked_toggle_owned_dragscroll_mode_does_not_double_anchor_auto_mouse(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_DRAGSCROLL);
    CHECK(auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 1);
    CHECK(auto_mouse_keyevent_calls == 0);
    CHECK(auto_mouse_key_tracker == 0);

    pd_mode_unlock(PD_MODE_DRAGSCROLL);
    CHECK(!auto_mouse_toggle_enabled);
    CHECK(auto_mouse_toggle_count == 2);
    CHECK(auto_mouse_keyevent_calls == 0);
    CHECK(auto_mouse_key_tracker == 0);
}

static void test_active_key_handler_only_runs_for_active_modes(void) {
    keyrecord_t record = {
        .event =
            {
                .type    = KEY_EVENT,
                .key     = {.row = 1, .col = 2},
                .pressed = true,
            },
    };

    test_reset_stubs();
    arrow_key_handler_result = true;

    CHECK(!pd_mode_handle_key_event(TEST_KEYCODE, &record));
    CHECK(arrow_key_handler_count == 0);

    pd_mode_activate(PD_MODE_ARROW);
    CHECK(pd_mode_handle_key_event(TEST_KEYCODE, &record));
    CHECK(arrow_key_handler_count == 1);
}

int main(void) {
    test_registry_metadata_matches_manifest();
    test_trait_queries_match_manifest_policy();
    test_apply_remote_snapshot_keeps_only_one_effective_mode();
    test_keycode_press_at_tracks_trigger_half();
    test_same_mode_key_owners_release_independently();
    test_lock_state_at_tracks_trigger_half_and_clears_on_unlock();
    test_ownerless_mode_change_clears_previous_trigger_half();
    test_combo_origin_bitmap_promotes_trigger_half_to_both_sides();
    test_apply_remote_mode_ids_tracks_display_owner_half();
    test_apply_remote_mode_ids_tracks_display_owner_bitmap();
    test_same_side_owner_change_requires_split_sync_for_exact_rgb();
    test_set_lock_state_switches_to_single_locked_mode();
    test_activate_switches_to_single_unlocked_mode();
    test_apply_command_reports_before_after_and_sync_intent_for_key_press();
    test_apply_command_reports_display_only_remote_snapshot_change();
    test_apply_command_release_reports_handled_without_sync_when_locked();
    test_handle_keycode_press_and_release_updates_state_and_syncs();
    test_handle_keycode_press_and_release_defer_dpi_until_service();
    test_locked_mode_release_keeps_mode_active();
    test_apply_active_dpi_respects_pointer_state();
    test_pinch_mode_registers_gui_and_dragscroll_side_effects();
    test_pinch_buffered_tap_mask_uses_managed_only_gui_policy();
    test_pinch_keyboard_event_mask_uses_managed_only_gui_policy();
    test_lock_owned_auto_mouse_toggle_tracks_mode_ownership();
    test_active_dragscroll_mode_does_not_add_synthetic_auto_mouse_anchor();
    test_locked_non_toggle_mode_uses_synthetic_auto_mouse_anchor();
    test_locked_toggle_owned_dragscroll_mode_does_not_double_anchor_auto_mouse();
    test_active_key_handler_only_runs_for_active_modes();

    puts("pd_mode host tests passed");
    return 0;
}

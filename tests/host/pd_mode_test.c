#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/pointing/pd_mode_internal.h"
#include "users/noah/lib/state/runtime_shared_state.h"
#include "users/noah/lib/state/split_runtime_sync.h"

enum {
    TEST_KEYCODE = SAFE_RANGE + 0x30,
};

static uint16_t current_cpi;
static uint16_t cpi_set_count;
static uint16_t default_dpi;

static bool dragscroll_enabled;
static bool sniping_enabled;

static uint8_t split_sync_count;

static uint8_t  keyboard_mod_register_count;
static uint8_t  keyboard_mod_unregister_count;
static uint16_t last_registered_keycode;
static uint16_t last_unregistered_keycode;

static uint8_t arrow_key_handler_count;
static bool    arrow_key_handler_result;

static uint8_t reset_volume_count;
static uint8_t reset_brightness_count;
static uint8_t reset_zoom_count;
static uint8_t reset_arrow_count;

split_runtime_sync_packet_t split_runtime_sync_remote = {0};

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

static void test_reset_runtime(void) {
    noah_runtime_shared_state = (runtime_shared_state_t){0};
}

static void test_reset_stubs(void) {
    test_reset_runtime();

    current_cpi                   = 0;
    cpi_set_count                 = 0;
    default_dpi                   = 900;
    dragscroll_enabled            = false;
    sniping_enabled               = false;
    split_sync_count              = 0;
    keyboard_mod_register_count   = 0;
    keyboard_mod_unregister_count = 0;
    last_registered_keycode       = KC_NO;
    last_unregistered_keycode     = KC_NO;
    arrow_key_handler_count       = 0;
    arrow_key_handler_result      = false;
    reset_volume_count            = 0;
    reset_brightness_count        = 0;
    reset_zoom_count              = 0;
    reset_arrow_count             = 0;
    split_runtime_sync_remote     = (split_runtime_sync_packet_t){0};
}

void split_runtime_sync_init(void) {}
void split_runtime_sync_tick(void) {}
void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
void split_runtime_sync(void) {
    split_sync_count++;
}

bool charybdis_get_pointer_dragscroll_enabled(void) {
    return dragscroll_enabled;
}

bool charybdis_get_pointer_sniping_enabled(void) {
    return sniping_enabled;
}

uint16_t charybdis_get_pointer_default_dpi(void) {
    return default_dpi;
}

void charybdis_set_pointer_dragscroll_enabled(bool enabled) {
    dragscroll_enabled = enabled;
}

void pointing_device_set_cpi(uint16_t cpi) {
    current_cpi = cpi;
    cpi_set_count++;
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

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
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

    test_reset_stubs();

    volume_mode = pd_mode_lookup(PD_MODE_VOLUME);
    arrow_mode  = pd_mode_lookup(PD_MODE_ARROW);

    CHECK(volume_mode != NULL);
    CHECK(volume_mode->mode_flag == PD_MODE_VOLUME);
    CHECK(volume_mode->keycode == VOLUME_MODE);
    CHECK(volume_mode->lock_action == VOLUME_MODE_LOCK);
    CHECK(volume_mode->dpi == PD_MODE_VOLUME_DPI);
    CHECK(pd_mode_lock_action_lookup(VOLUME_MODE_LOCK) == volume_mode);
    CHECK(is_pd_mode_lock_action(VOLUME_MODE_LOCK));
    CHECK(pd_mode_for_keycode(VOLUME_MODE) == PD_MODE_VOLUME);

    CHECK(arrow_mode != NULL);
    CHECK(arrow_mode->keycode == ARROW_MODE);
    CHECK(arrow_mode->lock_action == ARROW_MODE_LOCK);
    CHECK(arrow_mode->key_handler == handle_arrow_mode_key);

    pd_mode_apply_remote_snapshot(PD_MODE_ZOOM | PD_MODE_VOLUME, 0);
    CHECK(pd_mode_first_active_index() == PD_MODE_INDEX_VOLUME);
}

static void test_apply_remote_snapshot_merges_locked_modes_into_active(void) {
    test_reset_stubs();

    pd_mode_apply_remote_snapshot(PD_MODE_VOLUME, PD_MODE_ARROW);

    CHECK(pd_mode_active_snapshot() == (PD_MODE_VOLUME | PD_MODE_ARROW));
    CHECK(pd_mode_locked_snapshot() == PD_MODE_ARROW);
    CHECK(pd_mode_active(PD_MODE_VOLUME));
    CHECK(pd_mode_active(PD_MODE_ARROW));
    CHECK(pd_mode_locked(PD_MODE_ARROW));
    CHECK(!pd_mode_locked(PD_MODE_VOLUME));
}

static void test_set_lock_state_switches_to_single_locked_mode(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_ARROW);
    pd_mode_lock(PD_MODE_BRIGHTNESS);

    reset_arrow_count      = 0;
    reset_brightness_count = 0;
    cpi_set_count          = 0;

    CHECK(pd_mode_set_lock_state(PD_MODE_VOLUME, true));

    CHECK(pd_mode_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_active(PD_MODE_VOLUME));
    CHECK(pd_mode_locked(PD_MODE_VOLUME));
    CHECK(!pd_mode_active(PD_MODE_ARROW));
    CHECK(!pd_mode_active(PD_MODE_BRIGHTNESS));
    CHECK(reset_arrow_count == 1);
    CHECK(reset_brightness_count == 1);
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count >= 1);
}

static void test_handle_keycode_press_and_release_updates_state_and_syncs(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_ARROW);
    pd_mode_activate(PD_MODE_VOLUME);
    reset_arrow_count  = 0;
    reset_volume_count = 0;
    split_sync_count   = 0;

    CHECK(pd_mode_handle_keycode_press(BRIGHTNESS_MODE));
    CHECK(split_sync_count == 1);
    CHECK(pd_mode_active_snapshot() == PD_MODE_BRIGHTNESS);
    CHECK(pd_mode_locked_snapshot() == 0);
    CHECK(reset_arrow_count == 1);
    CHECK(reset_volume_count == 1);

    CHECK(pd_mode_handle_keycode_release(BRIGHTNESS_MODE));
    CHECK(split_sync_count == 2);
    CHECK(pd_mode_active_snapshot() == 0);
    CHECK(pd_mode_locked_snapshot() == 0);
    CHECK(reset_brightness_count == 1);
}

static void test_locked_mode_release_keeps_mode_active(void) {
    test_reset_stubs();

    pd_mode_lock(PD_MODE_VOLUME);
    split_sync_count   = 0;
    reset_volume_count = 0;

    CHECK(pd_mode_handle_keycode_release(VOLUME_MODE));
    CHECK(pd_mode_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_locked_snapshot() == PD_MODE_VOLUME);
    CHECK(split_sync_count == 0);
    CHECK(reset_volume_count == 0);
}

static void test_apply_active_dpi_respects_pointer_state(void) {
    test_reset_stubs();

    pd_mode_activate(PD_MODE_VOLUME);
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);

    cpi_set_count      = 0;
    current_cpi        = 7777;
    dragscroll_enabled = true;
    pd_mode_apply_active_dpi();
    CHECK(current_cpi == 7777);
    CHECK(cpi_set_count == 0);

    dragscroll_enabled = false;
    sniping_enabled    = true;
    pd_mode_apply_active_dpi();
    CHECK(current_cpi == 7777);
    CHECK(cpi_set_count == 0);

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
    CHECK(pd_mode_active(PD_MODE_PINCH));
    CHECK(dragscroll_enabled);
    CHECK(keyboard_mod_register_count == 1);
    CHECK(last_registered_keycode == KC_LEFT_GUI);

    pd_mode_deactivate(PD_MODE_PINCH);
    CHECK(!pd_mode_active(PD_MODE_PINCH));
    CHECK(!dragscroll_enabled);
    CHECK(keyboard_mod_unregister_count == 1);
    CHECK(last_unregistered_keycode == KC_LEFT_GUI);
}

static void test_active_key_handler_only_runs_for_active_modes(void) {
    keyrecord_t record = {
        .event =
            {
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
    test_apply_remote_snapshot_merges_locked_modes_into_active();
    test_set_lock_state_switches_to_single_locked_mode();
    test_handle_keycode_press_and_release_updates_state_and_syncs();
    test_locked_mode_release_keeps_mode_active();
    test_apply_active_dpi_respects_pointer_state();
    test_pinch_mode_registers_gui_and_dragscroll_side_effects();
    test_active_key_handler_only_runs_for_active_modes();

    puts("pd_mode host tests passed");
    return 0;
}

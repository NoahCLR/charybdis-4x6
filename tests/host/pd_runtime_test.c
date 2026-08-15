#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/noah_runtime.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "host_runtime_reset_fixture.h"

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;
HOST_RUNTIME_FIXTURE_DEFINE_RESET_QMK_STUBS(runtime_fixture)

static uint16_t current_cpi;
static uint16_t cpi_set_count;
static uint16_t default_dpi;
static uint16_t sniping_dpi;
static uint32_t fake_timer_ms;
static uint32_t fake_last_input_idle_ms;
static uint32_t fake_last_matrix_idle_ms;
static uint8_t  last_input_activity_elapsed_count;
static uint8_t  last_matrix_activity_elapsed_count;

static bool    fake_is_master;
static bool    sniping_enabled;
static bool    auto_mouse_enabled;
static bool    auto_mouse_toggle_enabled;
static bool    auto_mouse_active;
static int8_t  auto_mouse_key_tracker;
static uint8_t auto_mouse_layer;
static uint8_t auto_mouse_layer_off_count;
static uint8_t auto_mouse_toggle_count;
static uint8_t auto_mouse_keyevent_calls;

static uint8_t volume_handler_calls;
static uint8_t brightness_handler_calls;
static uint8_t dragscroll_handler_calls;
static uint8_t zoom_handler_calls;
static uint8_t arrow_handler_calls;
static uint8_t arrow_key_handler_calls;

static uint8_t reset_volume_count;
static uint8_t reset_brightness_count;
static uint8_t reset_dragscroll_count;
static uint8_t reset_zoom_count;
static uint8_t reset_arrow_count;

static uint8_t  keyboard_mod_register_count;
static uint8_t  keyboard_mod_unregister_count;
static uint16_t last_registered_keycode;
static uint16_t last_unregistered_keycode;

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

static bool report_mouse_equal(report_mouse_t lhs, report_mouse_t rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.h == rhs.h && lhs.v == rhs.v && lhs.buttons == rhs.buttons;
}

static void test_reset_runtime(void) {
    host_runtime_fixture_reset_userspace_runtime();
}

static void test_reset_stubs(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    test_reset_runtime();

    current_cpi                   = 0;
    cpi_set_count                 = 0;
    default_dpi                   = 900;
    sniping_dpi                   = 350;
    fake_timer_ms                 = 0;
    fake_last_input_idle_ms       = 0;
    fake_last_matrix_idle_ms      = UINT32_MAX;
    last_input_activity_elapsed_count  = 0;
    last_matrix_activity_elapsed_count = 0;
    fake_is_master                = true;
    sniping_enabled               = false;
    auto_mouse_enabled            = false;
    auto_mouse_toggle_enabled     = false;
    auto_mouse_active             = false;
    auto_mouse_key_tracker        = 0;
    auto_mouse_layer              = AUTO_MOUSE_DEFAULT_LAYER;
    auto_mouse_layer_off_count    = 0;
    auto_mouse_toggle_count       = 0;
    auto_mouse_keyevent_calls     = 0;
    volume_handler_calls          = 0;
    brightness_handler_calls      = 0;
    dragscroll_handler_calls      = 0;
    zoom_handler_calls            = 0;
    arrow_handler_calls           = 0;
    arrow_key_handler_calls       = 0;
    reset_volume_count            = 0;
    reset_brightness_count        = 0;
    reset_dragscroll_count        = 0;
    reset_zoom_count              = 0;
    reset_arrow_count             = 0;
    keyboard_mod_register_count   = 0;
    keyboard_mod_unregister_count = 0;
    last_registered_keycode       = KC_NO;
    last_unregistered_keycode     = KC_NO;
}

uint16_t timer_read(void) {
    return (uint16_t)fake_timer_ms;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(timer_read() - last);
}

uint32_t timer_read32(void) {
    return fake_timer_ms;
}

uint32_t timer_elapsed32(uint32_t last) {
    return timer_read32() - last;
}

uint32_t last_input_activity_elapsed(void) {
    last_input_activity_elapsed_count++;
    return fake_last_input_idle_ms;
}

uint32_t last_matrix_activity_elapsed(void) {
    last_matrix_activity_elapsed_count++;
    return fake_last_matrix_idle_ms;
}

bool is_keyboard_master(void) {
    return fake_is_master;
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
    (void)pressed;
    auto_mouse_keyevent_calls++;
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
    return mods;
}

report_mouse_t handle_volume_mode(report_mouse_t mouse_report) {
    volume_handler_calls++;
    mouse_report.x += 3;
    mouse_report.buttons |= 0x01u;
    return mouse_report;
}

report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report) {
    dragscroll_handler_calls++;
    mouse_report.h += 4;
    mouse_report.v -= 4;
    return mouse_report;
}

report_mouse_t handle_brightness_mode(report_mouse_t mouse_report) {
    brightness_handler_calls++;
    mouse_report.y -= 5;
    mouse_report.buttons |= 0x02u;
    return mouse_report;
}

report_mouse_t handle_zoom_mode(report_mouse_t mouse_report) {
    zoom_handler_calls++;
    mouse_report.v += 6;
    return mouse_report;
}

report_mouse_t handle_arrow_mode(report_mouse_t mouse_report) {
    arrow_handler_calls++;
    mouse_report.h += 7;
    return mouse_report;
}

bool handle_arrow_mode_key(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
    arrow_key_handler_calls++;
    return false;
}

void reset_volume_mode(void) {
    reset_volume_count++;
}

void reset_dragscroll_mode(void) {
    reset_dragscroll_count++;
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

static void test_pointing_device_init_enables_auto_mouse_and_default_layer(void) {
    test_reset_stubs();
    auto_mouse_layer = 0;

    noah_pointing_device_init_user();

    CHECK(auto_mouse_enabled);
    CHECK(auto_mouse_layer == AUTO_MOUSE_DEFAULT_LAYER);
}

static void test_pointing_device_task_returns_original_report_without_local_mode(void) {
    test_reset_stubs();

    report_mouse_t input = {.x = 1, .y = -2, .h = 3, .v = -4, .buttons = 5};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
    CHECK(volume_handler_calls == 0);
    CHECK(brightness_handler_calls == 0);
    CHECK(arrow_handler_calls == 0);
}

static void test_pointing_device_task_ignores_remote_display_only_mode_on_slave(void) {
    test_reset_stubs();
    fake_is_master = false;

    report_mouse_t input = {.x = 1, .y = -2, .h = 3, .v = -4, .buttons = 5};

    pd_mode_apply_remote_snapshot(PD_MODE_VOLUME, PD_MODE_VOLUME);

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
    CHECK(pd_mode_display_active_snapshot() == PD_MODE_VOLUME);
    CHECK(pd_mode_local_active_snapshot() == 0);
    CHECK(volume_handler_calls == 0);
}

static void test_pointing_device_task_dispatches_active_local_mode_and_tracks_latest_selection(void) {
    test_reset_stubs();

    report_mouse_t input = {.x = 10, .y = 20, .h = 30, .v = 40, .buttons = 0};
    report_mouse_t output;

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 13, .y = 20, .h = 30, .v = 40, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 0);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));

    CHECK(pd_mode_handle_keycode_press(BRIGHTNESS_MODE));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 10, .y = 15, .h = 30, .v = 40, .buttons = 2}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 1);
    CHECK(pd_mode_local_active(PD_MODE_BRIGHTNESS));
    CHECK(reset_volume_count == 1);
}

static void test_pointing_device_task_dispatches_zero_report_to_active_mode(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    report_mouse_t output = noah_pointing_device_task_user((report_mouse_t){0});

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 3, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
}

static void test_pointing_device_task_suppresses_idle_noise_after_quiet_window(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 1000u;
    fake_last_matrix_idle_ms = 300000u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), (report_mouse_t){0}));
}

static void test_pointing_device_task_keeps_small_motion_while_recently_active(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_last_input_idle_ms = 999u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_pointing_device_task_keeps_small_motion_with_buttons_after_quiet_window(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 5000u;
    fake_last_matrix_idle_ms = 300000u;

    report_mouse_t input = {.x = 1, .y = 0, .h = 0, .v = 0, .buttons = 1};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_pointing_device_task_keeps_larger_motion_after_quiet_window(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 5000u;
    fake_last_matrix_idle_ms = 300000u;

    report_mouse_t input = {.x = 2, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
    CHECK(last_input_activity_elapsed_count == 0);
    CHECK(last_matrix_activity_elapsed_count == 0);
}

static void test_pointing_device_task_keeps_small_motion_in_active_local_mode(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 5000u;
    fake_last_matrix_idle_ms = 300000u;
    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));

    report_mouse_t input = {.x = 1, .y = 0, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), (report_mouse_t){.x = 4, .y = 0, .h = 0, .v = 0, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
}

static void test_pointing_device_task_keeps_small_motion_before_arm_window_elapses(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 299999u;
    fake_last_input_idle_ms  = 1000u;
    fake_last_matrix_idle_ms = 300000u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_pointing_device_task_trusted_pointer_activity_delays_arm_window(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 5000u;
    fake_last_matrix_idle_ms = 300000u;

    report_mouse_t large_input = {.x = 3, .y = 0, .h = 0, .v = 0, .buttons = 0};
    report_mouse_t small_input = {.x = 1, .y = 0, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(large_input), large_input));

    fake_timer_ms = 300100u;
    CHECK(report_mouse_equal(noah_pointing_device_task_user(small_input), small_input));

    fake_timer_ms = 600100u;
    CHECK(report_mouse_equal(noah_pointing_device_task_user(small_input), (report_mouse_t){0}));
}

static void test_pointing_device_task_recent_matrix_activity_delays_arm_window(void) {
    test_reset_stubs();
    noah_pointing_device_init_user();
    fake_timer_ms            = 300000u;
    fake_last_input_idle_ms  = 1000u;
    fake_last_matrix_idle_ms = 10u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_layer_state_set_restores_active_mode_dpi_and_pointer_layer_after_sniping_drops(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    pd_mode_service_active_dpi_sync();
    current_cpi   = 0;
    cpi_set_count = 0;
    pd_mode_set_auto_sniping_layer_active(true);

    layer_state_t next = noah_layer_state_set_user((layer_state_t)1u << 0);

    CHECK(!sniping_enabled);
    CHECK(current_cpi == 0);
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) != 0);
}

static void test_layer_state_set_strips_pointer_layer_for_arrow_mode(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(ARROW_MODE));
    pd_mode_service_active_dpi_sync();
    current_cpi   = 0;
    cpi_set_count = 0;

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER));

    CHECK(!sniping_enabled);
    CHECK(current_cpi == 0);
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == PD_MODE_ARROW_DPI);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) == 0);
}

static void test_layer_state_set_enables_sniping_and_blocks_dpi_restore_while_sniping_layer_active(void) {
    test_reset_stubs();

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER) | ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER));

    CHECK(!sniping_enabled);
    CHECK(pd_mode_auto_sniping_layer_active());
    CHECK(cpi_set_count == 0);
    pd_mode_service_active_dpi_sync();
    CHECK(current_cpi == sniping_dpi);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) == 0);
}

static void test_is_mouse_record_user_delegates_pointer_layer_policy(void) {
    test_reset_stubs();

    keyrecord_t record = {0};

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    CHECK(noah_is_mouse_record_user(MO(1), &record));
    CHECK(noah_is_mouse_record_user(VOLUME_MODE, &record));
    CHECK(noah_is_mouse_record_user(DPI_MOD, &record));
    CHECK(!noah_is_mouse_record_user(ARROW_MODE, &record));
    CHECK(!noah_is_mouse_record_user(KC_C, &record));

    test_reset_stubs();
    CHECK(pd_mode_handle_keycode_press(ARROW_MODE));
    CHECK(!noah_is_mouse_record_user(MO(1), &record));
    CHECK(!noah_is_mouse_record_user(ARROW_MODE, &record));
}

int main(void) {
    test_pointing_device_init_enables_auto_mouse_and_default_layer();
    test_pointing_device_task_returns_original_report_without_local_mode();
    test_pointing_device_task_ignores_remote_display_only_mode_on_slave();
    test_pointing_device_task_dispatches_active_local_mode_and_tracks_latest_selection();
    test_pointing_device_task_dispatches_zero_report_to_active_mode();
    test_pointing_device_task_suppresses_idle_noise_after_quiet_window();
    test_pointing_device_task_keeps_small_motion_while_recently_active();
    test_pointing_device_task_keeps_small_motion_with_buttons_after_quiet_window();
    test_pointing_device_task_keeps_larger_motion_after_quiet_window();
    test_pointing_device_task_keeps_small_motion_in_active_local_mode();
    test_pointing_device_task_keeps_small_motion_before_arm_window_elapses();
    test_pointing_device_task_trusted_pointer_activity_delays_arm_window();
    test_pointing_device_task_recent_matrix_activity_delays_arm_window();
    test_layer_state_set_restores_active_mode_dpi_and_pointer_layer_after_sniping_drops();
    test_layer_state_set_strips_pointer_layer_for_arrow_mode();
    test_layer_state_set_enables_sniping_and_blocks_dpi_restore_while_sniping_layer_active();
    test_is_mouse_record_user_delegates_pointer_layer_policy();
    return 0;
}

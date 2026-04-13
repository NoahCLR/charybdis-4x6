#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/noah_runtime.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "users/noah/lib/state/runtime/runtime_shared_state.h"

static uint16_t current_cpi;
static uint16_t cpi_set_count;
static uint16_t default_dpi;
static uint32_t fake_timer_ms;
static uint32_t fake_last_input_idle_ms;

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
static char     console_log[4096];
static size_t   console_log_len;

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
    runtime_shared_state_reset(&noah_runtime_shared_state);
}

static void test_reset_console_log(void) {
    console_log[0] = '\0';
    console_log_len = 0;
}

static size_t test_console_line_count(void) {
    size_t count = 0;

    for (size_t i = 0; i < console_log_len; ++i) {
        if (console_log[i] == '\n') {
            count++;
        }
    }

    return count;
}

static void test_reset_stubs(void) {
    test_reset_runtime();

    current_cpi                 = 0;
    cpi_set_count               = 0;
    default_dpi                 = 900;
    fake_timer_ms               = 0;
    fake_last_input_idle_ms     = 0;
    fake_is_master              = true;
    sniping_enabled             = false;
    auto_mouse_enabled          = false;
    auto_mouse_toggle_enabled   = false;
    auto_mouse_active           = false;
    auto_mouse_key_tracker      = 0;
    auto_mouse_layer            = AUTO_MOUSE_DEFAULT_LAYER;
    auto_mouse_layer_off_count  = 0;
    auto_mouse_toggle_count     = 0;
    auto_mouse_keyevent_calls   = 0;
    volume_handler_calls        = 0;
    brightness_handler_calls    = 0;
    dragscroll_handler_calls    = 0;
    zoom_handler_calls          = 0;
    arrow_handler_calls         = 0;
    arrow_key_handler_calls     = 0;
    reset_volume_count          = 0;
    reset_brightness_count      = 0;
    reset_dragscroll_count      = 0;
    reset_zoom_count            = 0;
    reset_arrow_count           = 0;
    keyboard_mod_register_count = 0;
    keyboard_mod_unregister_count = 0;
    last_registered_keycode       = KC_NO;
    last_unregistered_keycode     = KC_NO;
    test_reset_console_log();
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
    return fake_last_input_idle_ms;
}

int uprintf(const char *fmt, ...) {
    va_list args;
    int     written;
    size_t  remaining = sizeof(console_log) - console_log_len;

    if (remaining == 0) {
        return 0;
    }

    va_start(args, fmt);
    written = vsnprintf(console_log + console_log_len, remaining, fmt, args);
    va_end(args);

    if (written < 0) {
        return written;
    }

    if ((size_t)written >= remaining) {
        console_log_len = sizeof(console_log) - 1u;
    } else {
        console_log_len += (size_t)written;
    }

    return written;
}

bool layer_state_cmp(layer_state_t state, uint8_t layer) {
    return (state & ((layer_state_t)1u << layer)) != 0;
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

void keyboard_mod_ownership_debug_snapshot(keyboard_mod_ownership_debug_snapshot_t *out) {
    if (!out) {
        return;
    }

    *out = (keyboard_mod_ownership_debug_snapshot_t){0};
}

void keyboard_mod_ownership_reset_for_test(void) {}

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

static void test_pointing_device_task_suppresses_idle_noise_after_quiet_window(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 1000u;
    fake_timer_ms           = 1000u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), (report_mouse_t){0}));
    CHECK(strstr(console_log, "raw=(-1,1,0,0 btn=0x00 abs=2)") != NULL);
    CHECK(strstr(console_log, "out=(0,0,0,0 btn=0x00 abs=0)") != NULL);
    CHECK(strstr(console_log, "caught=1") != NULL);
}

static void test_pointing_device_task_keeps_small_motion_while_recently_active(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 999u;

    report_mouse_t input = {.x = -1, .y = 1, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_pointing_device_task_keeps_small_motion_with_buttons_after_quiet_window(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 5000u;

    report_mouse_t input = {.x = 1, .y = 0, .h = 0, .v = 0, .buttons = 1};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
}

static void test_pointing_device_task_keeps_small_motion_in_active_local_mode(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 5000u;
    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));

    report_mouse_t input = {.x = 1, .y = 0, .h = 0, .v = 0, .buttons = 0};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), (report_mouse_t){.x = 4, .y = 0, .h = 0, .v = 0, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
}

static void test_pointing_motion_trace_stays_silent_for_zero_reports(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 4321u;
    fake_timer_ms           = 1000u;

    CHECK(report_mouse_equal(noah_pointing_device_task_user((report_mouse_t){0}), (report_mouse_t){0}));
    CHECK(console_log_len == 0u);
}

static void test_pointing_motion_trace_logs_motion_and_rate_limits_bursts(void) {
    test_reset_stubs();
    fake_last_input_idle_ms = 4321u;
    fake_timer_ms           = 1000u;

    report_mouse_t input = {.x = 1, .y = -2, .h = 3, .v = -4, .buttons = 5};

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
    CHECK(strstr(console_log, "Pointing motion trace idle=4321") != NULL);
    CHECK(strstr(console_log, "burst=0 reports=1") != NULL);
    CHECK(strstr(console_log, "raw=(1,-2,3,-4 btn=0x05 abs=10)") != NULL);
    CHECK(strstr(console_log, "raw_sum=10 raw_peak=10 raw_axes=(1,2,3,4)") != NULL);
    CHECK(strstr(console_log, "out=(1,-2,3,-4 btn=0x05 abs=10)") != NULL);
    CHECK(strstr(console_log, "caught=0") != NULL);
    CHECK(strstr(console_log, "mode=0x0000") != NULL);
    CHECK(test_console_line_count() == 1u);

    fake_last_input_idle_ms = 12u;
    fake_timer_ms           = 1100u;
    (void)noah_pointing_device_task_user(input);
    CHECK(test_console_line_count() == 1u);

    fake_last_input_idle_ms = 34u;
    fake_timer_ms           = 1300u;
    (void)noah_pointing_device_task_user(input);
    CHECK(test_console_line_count() == 2u);
    CHECK(strstr(console_log, "burst=300 reports=3") != NULL);
    CHECK(strstr(console_log, "raw_sum=30 raw_peak=10 raw_axes=(1,2,3,4)") != NULL);
}

static void test_pointing_motion_trace_logs_active_mode_transform(void) {
    char expected_mode[32];

    test_reset_stubs();
    fake_last_input_idle_ms = 60000u;
    fake_timer_ms           = 2000u;
    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));

    report_mouse_t input = {.x = 10, .y = 20, .h = 30, .v = 40, .buttons = 0};
    report_mouse_t output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 13, .y = 20, .h = 30, .v = 40, .buttons = 1}));
    CHECK(strstr(console_log, "burst=0 reports=1") != NULL);
    CHECK(strstr(console_log, "raw=(10,20,30,40 btn=0x00 abs=100)") != NULL);
    CHECK(strstr(console_log, "raw_sum=100 raw_peak=100 raw_axes=(10,20,30,40)") != NULL);
    CHECK(strstr(console_log, "out=(13,20,30,40 btn=0x01 abs=103)") != NULL);
    CHECK(strstr(console_log, "caught=0") != NULL);
    snprintf(expected_mode, sizeof(expected_mode), "mode=0x%04X", (unsigned int)PD_MODE_VOLUME);
    CHECK(strstr(console_log, expected_mode) != NULL);
}

static void test_layer_state_set_restores_active_mode_dpi_and_pointer_layer_after_sniping_drops(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(VOLUME_MODE));
    current_cpi   = 0;
    cpi_set_count = 0;
    sniping_enabled = true;

    layer_state_t next = noah_layer_state_set_user((layer_state_t)1u << 0);

    CHECK(!sniping_enabled);
    CHECK(current_cpi == PD_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) != 0);
}

static void test_layer_state_set_strips_pointer_layer_for_arrow_mode(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(ARROW_MODE));
    current_cpi   = 0;
    cpi_set_count = 0;

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER));

    CHECK(!sniping_enabled);
    CHECK(current_cpi == PD_MODE_ARROW_DPI);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) == 0);
}

static void test_layer_state_set_enables_sniping_and_blocks_dpi_restore_while_sniping_layer_active(void) {
    test_reset_stubs();

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER) | ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER));

    CHECK(sniping_enabled);
    CHECK(cpi_set_count == 0);
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
    test_pointing_device_task_suppresses_idle_noise_after_quiet_window();
    test_pointing_device_task_keeps_small_motion_while_recently_active();
    test_pointing_device_task_keeps_small_motion_with_buttons_after_quiet_window();
    test_pointing_device_task_keeps_small_motion_in_active_local_mode();
    test_pointing_motion_trace_stays_silent_for_zero_reports();
    test_pointing_motion_trace_logs_motion_and_rate_limits_bursts();
    test_pointing_motion_trace_logs_active_mode_transform();
    test_layer_state_set_restores_active_mode_dpi_and_pointer_layer_after_sniping_drops();
    test_layer_state_set_strips_pointer_layer_for_arrow_mode();
    test_layer_state_set_enables_sniping_and_blocks_dpi_restore_while_sniping_layer_active();
    test_is_mouse_record_user_delegates_pointer_layer_policy();
    return 0;
}

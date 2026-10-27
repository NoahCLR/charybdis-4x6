#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/noah_runtime.h"
#include "users/noah/lib/key/runtime/slot/origin_registry.h"
#include "users/noah/lib/pointing/defs/pointing_modes.h"
#include "users/noah/lib/pointing/modes/pointing_mode_configured.h"
#include "users/noah/lib/pointing/modes/pointing_mode_handlers.h"
#include "users/noah/lib/profile/runtime/active_profile_pd_runtime.h"
#include "users/noah/lib/profile/runtime/active_profile_settings_runtime.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "host_runtime_reset_fixture.h"

// Local dispatch fakes let this test observe the configured slot selection.
report_mouse_t handle_dragscroll_mode(report_mouse_t mouse_report);
report_mouse_t handle_volume_mode(report_mouse_t mouse_report);
report_mouse_t handle_brightness_mode(report_mouse_t mouse_report);
report_mouse_t handle_zoom_mode(report_mouse_t mouse_report);
report_mouse_t handle_arrow_mode(report_mouse_t mouse_report);
void reset_volume_mode(void);

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;
HOST_RUNTIME_FIXTURE_DEFINE_RESET_QMK_STUBS(runtime_fixture)

static uint16_t current_cpi;
static uint16_t cpi_set_count;
static uint16_t default_dpi;
static uint16_t sniping_dpi;
static uint32_t fake_timer_ms;
static uint32_t timer_read_count;
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

static layer_state_t turned_on_layers;
static layer_state_t held_layers;
static uint32_t      sniping_layers_setting;
static uint32_t      sniping_on_setting;

// The active profile's settings, as far as the pointing runtime reads them.
uint32_t noah_setting(uint8_t id, uint32_t fallback) {
    switch (id) {
        case NOAH_SETTING_AUTO_SNIPING_ENABLED:
            return sniping_on_setting;
        case NOAH_SETTING_AUTO_SNIPING_LAYERS:
            return sniping_layers_setting;
        default:
            return fallback;
    }
}

static void test_reset_runtime(void) {
    host_runtime_fixture_reset_userspace_runtime();
}

static void test_reset_stubs(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    test_reset_runtime();
    turned_on_layers         = 0u;
    held_layers            = 0;
    sniping_on_setting     = 1u;
    sniping_layers_setting = (uint32_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER;

    current_cpi                        = 0;
    cpi_set_count                      = 0;
    default_dpi                        = 900;
    sniping_dpi                        = 350;
    fake_timer_ms                      = 0;
    timer_read_count                   = 0;
    fake_last_input_idle_ms            = 0;
    fake_last_matrix_idle_ms           = UINT32_MAX;
    last_input_activity_elapsed_count  = 0;
    last_matrix_activity_elapsed_count = 0;
    fake_is_master                     = true;
    sniping_enabled                    = false;
    auto_mouse_enabled                 = false;
    auto_mouse_toggle_enabled          = false;
    auto_mouse_active                  = false;
    auto_mouse_key_tracker             = 0;
    auto_mouse_layer                   = AUTO_MOUSE_DEFAULT_LAYER;
    auto_mouse_layer_off_count         = 0;
    auto_mouse_toggle_count            = 0;
    auto_mouse_keyevent_calls          = 0;
    volume_handler_calls               = 0;
    brightness_handler_calls           = 0;
    dragscroll_handler_calls           = 0;
    zoom_handler_calls                 = 0;
    arrow_handler_calls                = 0;
    arrow_key_handler_calls            = 0;
    reset_volume_count                 = 0;
    reset_brightness_count             = 0;
    reset_dragscroll_count             = 0;
    reset_zoom_count                   = 0;
    reset_arrow_count                  = 0;
    keyboard_mod_register_count        = 0;
    keyboard_mod_unregister_count      = 0;
    last_registered_keycode            = KC_NO;
    last_unregistered_keycode          = KC_NO;
}

uint16_t timer_read(void) {
    timer_read_count++;
    return (uint16_t)fake_timer_ms;
}

uint16_t timer_elapsed(uint16_t last) {
    return (uint16_t)(timer_read() - last);
}

uint32_t timer_read32(void) {
    timer_read_count++;
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

// No custom key here turns a layer on (D-F26); the pointer layer policy asks.
bool noah_custom_key_has_layers(uint16_t keycode) {
    (void)keycode;
    return false;
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

bool get_auto_mouse_enable(void) {
    return auto_mouse_enabled;
}

bool layer_ownership_is_locked(uint8_t layer) {
    (void)layer;
    return false;
}

bool layer_ownership_is_held(uint8_t layer) {
    (void)layer;
    return false;
}

// The layers a running mode's key turns on, as layer ownership would apply them: on
// beside the layers the test holds, through the layer-state hook.
static void test_apply_layers(void) {
    layer_state_t state = held_layers | turned_on_layers;

    layer_state = noah_layer_state_set_user(state);
}

bool layer_ownership_set_pointing_mode_layers(layer_state_t layers) {
    if (layers == turned_on_layers) return false;
    turned_on_layers = layers;
    test_apply_layers();
    return true;
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
    auto_mouse_key_tracker += pressed ? 1 : -1;
    auto_mouse_keyevent_calls++;
}

void auto_mouse_reset_trigger(bool pressed) {
    (void)pressed;
}

uint8_t read_source_layers_cache(keypos_t key) {
    (void)key;
    return 0;
}

bool key_behavior_keeps_auto_mouse_anchored(uint16_t keycode) {
    (void)keycode;
    return false;
}

void keyboard_mod_ownership_track_report_keycode_event(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;
    (void)record;
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

bool keyboard_mod_ownership_register_mods(uint8_t mods) {
    return mods != 0u;
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

// Dispatch-test records model the published eight-slot cache. The motion
// stubs below let this test observe which slot the shared engine selected.
static const noah_pd_config_t test_pd_records[8] = {
    [0] = {.id = 0, .kind = NOAH_PD_KIND_SCROLLING, .dpi = 100},
    [1] = {.id = 1, .kind = NOAH_PD_KIND_DIRECTIONAL, .dpi = 1400},
    [2] = {.id = 2, .kind = NOAH_PD_KIND_DIRECTIONAL, .dpi = 1500},
    [3] = {.id = 3, .kind = NOAH_PD_KIND_DIRECTIONAL, .dpi = 1600},
    [4] = {.id = 4, .kind = NOAH_PD_KIND_DIRECTIONAL, .pointer_layer = NOAH_PD_POINTER_LAYER_PREFER_TYPING, .dpi = 1700},
    [5] = {.id = 5, .kind = NOAH_PD_KIND_SCROLLING, .dpi = 100},
};
static const noah_pd_config_t *active_pd_record;

const noah_pd_config_t *noah_active_profile_pd_for_mask(uint32_t mode) {
    for (uint8_t slot = 0; slot < 6; slot++) {
        if (mode == (UINT32_C(1) << slot)) return &test_pd_records[slot];
    }
    return NULL;
}

void noah_pd_engine_enter(const noah_pd_config_t *slot) { active_pd_record = slot; }
void noah_pd_engine_exit(void) {
    if (active_pd_record && active_pd_record->id == 1) reset_volume_mode();
    active_pd_record = NULL;
}
uint8_t noah_pd_engine_masked_mods(const noah_pd_config_t *slot) { (void)slot; return 0; }
report_mouse_t noah_pd_engine_motion(report_mouse_t report) {
    if (!active_pd_record) return report;
    switch (active_pd_record->id) {
        case 0: case 5: return handle_dragscroll_mode(report);
        case 1: return handle_volume_mode(report);
        case 2: return handle_brightness_mode(report);
        case 3: return handle_zoom_mode(report);
        case 4: return handle_arrow_mode(report);
        default: return report;
    }
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

    pointing_mode_apply_remote_snapshot(POINTING_MODE_VOLUME, POINTING_MODE_VOLUME);

    CHECK(report_mouse_equal(noah_pointing_device_task_user(input), input));
    CHECK(pointing_mode_display_active_snapshot() == POINTING_MODE_VOLUME);
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(volume_handler_calls == 0);
}

static void test_pointing_device_task_dispatches_active_local_mode_and_tracks_latest_selection(void) {
    test_reset_stubs();

    report_mouse_t input = {.x = 10, .y = 20, .h = 30, .v = 40, .buttons = 0};
    report_mouse_t output;

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 13, .y = 20, .h = 30, .v = 40, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 0);
    CHECK(pointing_mode_local_active(POINTING_MODE_VOLUME));

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_2));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 10, .y = 15, .h = 30, .v = 40, .buttons = 2}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 1);
    CHECK(pointing_mode_local_active(POINTING_MODE_BRIGHTNESS));
    CHECK(reset_volume_count == 1);
}

static void test_pointing_device_task_dispatches_zero_report_to_active_mode(void) {
    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
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
    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));

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

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
    pointing_mode_set_auto_sniping_layer_active(true);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == sniping_dpi);
    current_cpi   = 0;
    cpi_set_count = 0;

    layer_state_t next = noah_layer_state_set_user((layer_state_t)1u << 0);

    CHECK(!sniping_enabled);
    CHECK(current_cpi == 0);
    CHECK(cpi_set_count == 0);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == POINTING_MODE_VOLUME_DPI);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) != 0);
}

static void test_layer_state_set_strips_pointer_layer_for_arrow_mode(void) {
    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_4));
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == POINTING_MODE_ARROW_DPI);
    cpi_set_count = 0;

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER));

    CHECK(!sniping_enabled);
    CHECK(cpi_set_count == 0);
    // The mode's DPI stays: the sensor already holds it, so nothing is written.
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == POINTING_MODE_ARROW_DPI);
    CHECK(cpi_set_count == 0);
    CHECK((next & ((layer_state_t)1u << 0)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) == 0);
}

static void test_layer_state_set_enables_sniping_and_blocks_dpi_restore_while_sniping_layer_active(void) {
    test_reset_stubs();

    layer_state_t next = noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER) | ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER));

    CHECK(!sniping_enabled);
    CHECK(pointing_mode_auto_sniping_layer_active());
    CHECK(cpi_set_count == 0);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == sniping_dpi);
    CHECK(cpi_set_count == 1);
    CHECK((next & ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER)) != 0);
    CHECK((next & ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER)) == 0);
}

// Auto-sniping runs while any of its layers is on and takes the sniping DPI;
// its master switch turns all of them off without forgetting them.
static void test_layer_state_set_snipes_on_every_sniping_layer(void) {
    const uint8_t layers[] = {2, 6};

    for (uint8_t index = 0; index < ARRAY_SIZE(layers); index++) {
        test_reset_stubs();
        sniping_layers_setting = ((uint32_t)1u << 2) | ((uint32_t)1u << 6);
        (void)noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << layers[index]));
        CHECK(pointing_mode_auto_sniping_layer_active());
        pointing_mode_service_active_dpi_sync();
        CHECK(current_cpi == sniping_dpi);
    }

    test_reset_stubs();
    sniping_layers_setting = ((uint32_t)1u << 2) | ((uint32_t)1u << 6);
    (void)noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << 5));
    CHECK(!pointing_mode_auto_sniping_layer_active());
    sniping_on_setting = 0u;
    (void)noah_layer_state_set_user(((layer_state_t)1u << 0) | ((layer_state_t)1u << 6));
    CHECK(!pointing_mode_auto_sniping_layer_active());
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi);
}

// QMK's auto-mouse turns its layer on again every debounce window while the
// ball moves, and every layer change asks for the DPI policy. A CPI the sensor
// already holds is not written again: each write is two register writes with
// busy-waits and a drop out of burst mode.
static void test_layer_changes_write_the_cpi_only_when_it_changes(void) {
    const layer_state_t sniping = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER) | ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER);
    const layer_state_t pointer = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER);

    test_reset_stubs();
    (void)noah_layer_state_set_user(sniping);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == sniping_dpi && cpi_set_count == 1);
    for (int pass = 0; pass < 40; pass++) {
        (void)noah_layer_state_set_user(sniping);
        pointing_mode_service_active_dpi_sync();
    }
    CHECK(current_cpi == sniping_dpi && cpi_set_count == 1);

    // A different CPI is written at once, and only once.
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi && cpi_set_count == 2);
    // A mode with its own DPI, the same, and back.
    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == POINTING_MODE_VOLUME_DPI && cpi_set_count == 3);
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(cpi_set_count == 3);
    // Sniping over the mode, at a sniping DPI that changed meanwhile.
    sniping_dpi = 400;
    (void)noah_layer_state_set_user(sniping);
    pointing_mode_service_active_dpi_sync();
    (void)noah_layer_state_set_user(sniping);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == 400 && cpi_set_count == 4);
}

// Whatever else writes the sensor's CPI (its own init, QMK's Charybdis code,
// a DPI adjustment) says so, and the next request writes the policy's CPI
// again even when it is the one written last.
static void test_a_cpi_written_behind_the_policy_is_written_again(void) {
    const layer_state_t pointer = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER);

    test_reset_stubs();
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi && cpi_set_count == 1);
    pointing_device_set_cpi(1234);
    pointing_mode_forget_applied_dpi();
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi && cpi_set_count == 3);
    // The pointing device's init (a sensor reset at boot) is one such writer.
    pointing_device_set_cpi(1234);
    noah_pointing_device_init_user();
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(current_cpi == default_dpi && cpi_set_count == 5);
    // Nothing written behind it: nothing written again.
    (void)noah_layer_state_set_user(pointer);
    pointing_mode_service_active_dpi_sync();
    CHECK(cpi_set_count == 5);
}

// One main-loop pass on the master half as far as the DPI policy sees it: the
// scan services a requested sync, then the pass's one clock read is handed to
// the refresh.
static void test_dpi_pass(uint32_t now) {
    pointing_mode_service_active_dpi_sync();
    pointing_mode_refresh_applied_dpi(now);
}

// A brownout or glitch can reset the sensor to its own CPI without resetting
// the MCU, and the policy writes only a CPI that changed. So it writes the CPI
// it intends again once a period, even unchanged: never sooner, never twice in
// one period, and with no clock read of its own on any pass.
static void test_a_cpi_the_sensor_lost_is_restored_within_a_period(void) {
    const layer_state_t pointer = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER);
    const uint32_t      start   = 1000u;

    test_reset_stubs();
    (void)noah_layer_state_set_user(pointer);
    test_dpi_pass(start);
    CHECK(current_cpi == default_dpi && cpi_set_count == 1);

    current_cpi = 500; // the sensor's own default, behind the firmware's back
    timer_read_count = 0;
    for (uint32_t now = start + 1u; now < start + POINTING_MODE_CPI_REFRESH_MS; now++) {
        test_dpi_pass(now);
    }
    CHECK(current_cpi == 500 && cpi_set_count == 1);
    test_dpi_pass(start + POINTING_MODE_CPI_REFRESH_MS);
    CHECK(current_cpi == default_dpi && cpi_set_count == 2);

    // Once a period: nothing more until the next one is over.
    for (uint32_t now = start + POINTING_MODE_CPI_REFRESH_MS + 1u; now < start + 2u * POINTING_MODE_CPI_REFRESH_MS; now++) {
        test_dpi_pass(now);
    }
    CHECK(cpi_set_count == 2);
    test_dpi_pass(start + 2u * POINTING_MODE_CPI_REFRESH_MS);
    CHECK(current_cpi == default_dpi && cpi_set_count == 3);
    // Every pass ran on the time the loop already read.
    CHECK(timer_read_count == 0u);
}

// The CPI written is the one the policy works out now, here the active mode's.
static void test_the_refresh_writes_the_cpi_the_policy_intends(void) {
    test_reset_stubs();
    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
    test_dpi_pass(10u);
    CHECK(current_cpi == POINTING_MODE_VOLUME_DPI && cpi_set_count == 1);
    current_cpi = 500;
    test_dpi_pass(10u + POINTING_MODE_CPI_REFRESH_MS);
    CHECK(current_cpi == POINTING_MODE_VOLUME_DPI && cpi_set_count == 2);
}

// The clock wraps after 49 days; the period runs across it.
static void test_the_refresh_period_runs_across_the_clock_wrap(void) {
    const layer_state_t pointer = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER);
    const uint32_t      start   = UINT32_MAX - 1000u;

    test_reset_stubs();
    (void)noah_layer_state_set_user(pointer);
    test_dpi_pass(start);
    CHECK(cpi_set_count == 1);
    test_dpi_pass(UINT32_MAX);
    test_dpi_pass(0u);
    test_dpi_pass(start + POINTING_MODE_CPI_REFRESH_MS - 1u);
    CHECK(cpi_set_count == 1);
    test_dpi_pass(start + POINTING_MODE_CPI_REFRESH_MS);
    CHECK(cpi_set_count == 2);
}

// A write that happens anyway, the policy's own for a new CPI or another
// writer's, restarts the period.
static void test_a_write_that_happens_anyway_restarts_the_refresh_period(void) {
    const layer_state_t sniping = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER) | ((layer_state_t)1u << CHARYBDIS_AUTO_SNIPING_LAYER);
    const layer_state_t pointer = ((layer_state_t)1u << 0) | ((layer_state_t)1u << AUTO_MOUSE_DEFAULT_LAYER);
    const uint32_t      changed = 4000u;
    const uint32_t      forgot  = changed + 3000u;

    test_reset_stubs();
    (void)noah_layer_state_set_user(pointer);
    test_dpi_pass(1000u);
    CHECK(cpi_set_count == 1);
    (void)noah_layer_state_set_user(sniping);
    test_dpi_pass(changed);
    CHECK(current_cpi == sniping_dpi && cpi_set_count == 2);
    // The same CPI asked for again writes nothing and keeps the period.
    (void)noah_layer_state_set_user(sniping);
    test_dpi_pass(changed + 1000u);
    test_dpi_pass(1000u + POINTING_MODE_CPI_REFRESH_MS);
    CHECK(cpi_set_count == 2);

    // QMK writes its own CPI and says so.
    pointing_device_set_cpi(1234);
    pointing_mode_forget_applied_dpi();
    test_dpi_pass(forgot);
    CHECK(current_cpi == 1234 && cpi_set_count == 3);
    test_dpi_pass(changed + POINTING_MODE_CPI_REFRESH_MS);
    test_dpi_pass(forgot + POINTING_MODE_CPI_REFRESH_MS - 1u);
    CHECK(current_cpi == 1234 && cpi_set_count == 3);
    test_dpi_pass(forgot + POINTING_MODE_CPI_REFRESH_MS);
    CHECK(current_cpi == sniping_dpi && cpi_set_count == 4);
}

static void test_is_mouse_record_user_delegates_pointer_layer_policy(void) {
    test_reset_stubs();

    keyrecord_t record = {0};

    CHECK(pointing_mode_handle_keycode_press(PD_MODE_1));
    CHECK(noah_is_mouse_record_user(MO(1), &record));
    CHECK(noah_is_mouse_record_user(PD_MODE_1, &record));
    CHECK(noah_is_mouse_record_user(DPI_MOD, &record));
    CHECK(!noah_is_mouse_record_user(PD_MODE_4, &record));
    CHECK(!noah_is_mouse_record_user(KC_C, &record));

    test_reset_stubs();
    CHECK(pointing_mode_handle_keycode_press(PD_MODE_4));
    CHECK(!noah_is_mouse_record_user(MO(1), &record));
    CHECK(!noah_is_mouse_record_user(PD_MODE_4, &record));
}

// A slot with no record (here slot 7) is inert: its hold key neither
// activates a mode nor becomes the active mode's owner, and leaves nothing
// behind when released.
static void test_an_empty_slot_hold_key_owns_nothing(void) {
    const keypos_t volume_key = {.row = 1, .col = 2};
    const keypos_t empty_key  = {.row = 6, .col = 3};
    keypos_t       owner;

    test_reset_stubs();
    CHECK(pointing_mode_for_keycode(PD_MODE_0 + 7u) != 0 && !noah_active_profile_pd_for_mask(pointing_mode_for_keycode(PD_MODE_0 + 7u)));

    // With nothing active.
    (void)pointing_mode_handle_keycode_press_at(PD_MODE_0 + 7u, empty_key);
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(!pointing_mode_local_owner_key_pos_snapshot(&owner));
    CHECK(pointing_mode_snapshot().local.owner_sides == SPLIT_SIDE_MASK_NONE);
    (void)pointing_mode_handle_keycode_release_at(PD_MODE_0 + 7u, empty_key);
    CHECK(!pointing_mode_local_owner_key_pos_snapshot(&owner));

    // While another mode is held: its owner stays its own.
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, volume_key));
    CHECK(pointing_mode_local_owner_key_pos_snapshot(&owner) && owner.row == volume_key.row && owner.col == volume_key.col);
    (void)pointing_mode_handle_keycode_press_at(PD_MODE_0 + 7u, empty_key);
    CHECK(pointing_mode_local_active_snapshot() == POINTING_MODE_VOLUME);
    CHECK(pointing_mode_local_owner_key_pos_snapshot(&owner) && owner.row == volume_key.row && owner.col == volume_key.col);
    (void)pointing_mode_handle_keycode_release_at(PD_MODE_0 + 7u, empty_key);
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, volume_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(!pointing_mode_local_owner_key_pos_snapshot(&owner));
    CHECK(pointing_mode_snapshot().local.owner_sides == SPLIT_SIDE_MASK_NONE);

    // While a mode is locked by its lock action, which leaves it no owner
    // key: the empty slot's key does not become one.
    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, true));
    CHECK(!pointing_mode_local_owner_key_pos_snapshot(&owner));
    (void)pointing_mode_handle_keycode_press_at(PD_MODE_0 + 7u, empty_key);
    CHECK(pointing_mode_local_locked(POINTING_MODE_VOLUME) && !pointing_mode_local_owner_key_pos_snapshot(&owner));
    CHECK(pointing_mode_snapshot().local.owner_sides == SPLIT_SIDE_MASK_NONE);
    (void)pointing_mode_handle_keycode_release_at(PD_MODE_0 + 7u, empty_key);
    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, false));
}

// A locked mode with no toggle of its own holds one count on QMK's tracker.
// QMK zeroes it whenever it resets auto-mouse, as a live settings edit that
// changes the auto-mouse timeout does; the unlock then takes back nothing.
static void test_a_locked_modes_anchor_is_not_taken_back_after_a_qmk_reset(void) {
    test_reset_stubs();
    auto_mouse_enabled = true;

    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, true));
    CHECK(auto_mouse_key_tracker == 1 && auto_mouse_keyevent_calls == 1);
    auto_mouse_key_tracker = 0;
    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, false));
    CHECK(auto_mouse_key_tracker == 0 && auto_mouse_keyevent_calls == 1);

    // Without a reset the unlock takes its count back.
    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, true));
    CHECK(pointing_mode_set_lock_state(POINTING_MODE_VOLUME, false));
    CHECK(auto_mouse_key_tracker == 0 && auto_mouse_keyevent_calls == 3);
}

static bool test_owner_key_is(keypos_t expected) {
    keypos_t owner;

    return pointing_mode_local_owner_key_pos_snapshot(&owner) && owner.row == expected.row && owner.col == expected.col;
}

static bool test_owner_bitmap_is(const keypos_t *keys, uint8_t key_count) {
    uint8_t expected[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t local[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t display[KEY_ORIGIN_BITMAP_SIZE];
    bool    has_local;
    bool    has_display;

    key_origin_bitmap_clear(expected);
    for (uint8_t index = 0; index < key_count; index++) {
        key_origin_bitmap_add_keypos(expected, keys[index]);
    }

    has_local   = pointing_mode_local_owner_bitmap_snapshot(local);
    has_display = pointing_mode_display_owner_bitmap_snapshot(display);
    for (uint8_t index = 0; index < KEY_ORIGIN_BITMAP_SIZE; index++) {
        if (local[index] != expected[index] || display[index] != expected[index]) {
            return false;
        }
    }

    return has_local == (key_count != 0u) && has_display == has_local;
}

static bool test_owner_sides_are(split_side_mask_t sides) {
    return pointing_mode_local_owner_sides_snapshot() == sides && pointing_mode_display_owner_sides_snapshot() == sides && pointing_mode_snapshot().local.owner_sides == sides;
}

// Every key holding the active mode owns it: the owner keys are all of them,
// the owner sides are the halves they are on, and the mode lasts until the
// last of them is released.
static void test_two_keys_holding_one_mode_both_own_it(void) {
    const keypos_t left_key     = {.row = 1, .col = 2};
    const keypos_t right_key    = {.row = 5, .col = 1};
    const keypos_t both_keys[2] = {left_key, right_key};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, left_key));
    CHECK(pointing_mode_local_active(POINTING_MODE_VOLUME));
    CHECK(test_owner_key_is(left_key));
    CHECK(test_owner_bitmap_is(&left_key, 1));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_LEFT));

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, right_key));
    CHECK(pointing_mode_local_active(POINTING_MODE_VOLUME));
    CHECK(test_owner_bitmap_is(both_keys, 2));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_BOTH));

    // Releasing one keeps the mode, owned by the other.
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, left_key));
    CHECK(pointing_mode_local_active(POINTING_MODE_VOLUME));
    CHECK(test_owner_key_is(right_key));
    CHECK(test_owner_bitmap_is(&right_key, 1));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_RIGHT));

    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, right_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(!test_owner_key_is(right_key));
    CHECK(test_owner_bitmap_is(NULL, 0));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_NONE));
}

// The single owner key (what combo feedback compares against) is the key that
// took the mode first, not the lowest matrix position. (After an interleaved
// release a later key takes a released key's place ahead of an earlier one:
// test_a_later_key_takes_a_released_owners_place.)
static void test_the_owner_key_is_the_first_key_to_hold_the_mode(void) {
    const keypos_t first_key  = {.row = 5, .col = 1};
    const keypos_t second_key = {.row = 1, .col = 2};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, first_key));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, second_key));
    CHECK(test_owner_key_is(first_key));

    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, first_key));
    CHECK(test_owner_key_is(second_key));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, second_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
}

// The owners form a list in which a press takes the lowest free entry, and the
// first entry in use is the owner key. So a key pressed after an earlier
// owner's release takes that owner's place ahead of a key still held.
static void test_a_later_key_takes_a_released_owners_place(void) {
    const keypos_t first_key  = {.row = 1, .col = 2};
    const keypos_t second_key = {.row = 0, .col = 0};
    const keypos_t third_key  = {.row = 6, .col = 5};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, first_key));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, second_key));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, first_key));
    CHECK(test_owner_key_is(second_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_LEFT));

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, third_key));
    CHECK(test_owner_key_is(third_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_BOTH));

    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, third_key));
    CHECK(test_owner_key_is(second_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_LEFT));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, second_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
}

// A key pressed again while it already holds the mode is still one owner: its
// one release ends the mode.
static void test_a_repeated_press_is_one_owner(void) {
    const keypos_t key = {.row = 2, .col = 3};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, key));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, key));
    CHECK(test_owner_bitmap_is(&key, 1));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_NONE));
}

// A key held for another mode gives the mode up: the new mode's key is its
// only owner, and releasing the old key changes nothing.
static void test_a_new_mode_drops_the_old_modes_owners(void) {
    const keypos_t volume_key = {.row = 1, .col = 2};
    const keypos_t zoom_key   = {.row = 6, .col = 0};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, volume_key));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_3, zoom_key));
    CHECK(pointing_mode_local_active(POINTING_MODE_ZOOM));
    CHECK(test_owner_key_is(zoom_key));
    CHECK(test_owner_bitmap_is(&zoom_key, 1));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_RIGHT));

    (void)pointing_mode_handle_keycode_release_at(PD_MODE_1, volume_key);
    CHECK(pointing_mode_local_active(POINTING_MODE_ZOOM));
    CHECK(test_owner_key_is(zoom_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_RIGHT));

    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_3, zoom_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(test_owner_bitmap_is(NULL, 0));
}

// Locking a held mode with a lock key makes the lock key its only owner; the
// keys that held it no longer own it, and their release keeps the lock.
static void test_a_lock_key_replaces_the_holding_owners(void) {
    const keypos_t left_key  = {.row = 1, .col = 2};
    const keypos_t right_key = {.row = 5, .col = 1};
    const keypos_t lock_key  = {.row = 7, .col = 7};

    test_reset_stubs();

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, left_key));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, right_key));
    CHECK(pointing_mode_set_lock_state_at(POINTING_MODE_VOLUME, true, lock_key));
    CHECK(pointing_mode_local_locked(POINTING_MODE_VOLUME));
    CHECK(test_owner_key_is(lock_key));
    CHECK(test_owner_bitmap_is(&lock_key, 1));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_RIGHT));

    (void)pointing_mode_handle_keycode_release_at(PD_MODE_1, left_key);
    (void)pointing_mode_handle_keycode_release_at(PD_MODE_1, right_key);
    CHECK(pointing_mode_local_locked(POINTING_MODE_VOLUME));
    CHECK(test_owner_key_is(lock_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_RIGHT));

    CHECK(pointing_mode_set_lock_state_at(POINTING_MODE_VOLUME, false, lock_key));
    CHECK(pointing_mode_local_active_snapshot() == 0);
    CHECK(!test_owner_key_is(lock_key));
    CHECK(test_owner_bitmap_is(NULL, 0));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_NONE));
}

// A combo output's owner key stands for its whole footprint: the owner keys are
// the footprint and the owner sides are every half it covers.
static void test_a_combo_owner_owns_its_footprint(void) {
    const keypos_t owner_key    = {.row = 2, .col = 0};
    const keypos_t member_key   = {.row = 6, .col = 4};
    const keypos_t footprint[2] = {owner_key, member_key};
    uint8_t        bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();
    key_origin_registry_reset();
    key_origin_bitmap_clear(bitmap);
    key_origin_bitmap_add_keypos(bitmap, owner_key);
    key_origin_bitmap_add_keypos(bitmap, member_key);
    CHECK(key_origin_registry_set_bitmap(owner_key, bitmap));

    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, owner_key));
    CHECK(test_owner_key_is(owner_key));
    CHECK(test_owner_bitmap_is(footprint, 2));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_BOTH));

    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, owner_key));
    CHECK(test_owner_bitmap_is(NULL, 0));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_NONE));
    key_origin_registry_reset();
}

// An owner's sides are its footprint's when it pressed: a combo footprint
// rewritten while the key is held does not move them.
static void test_owner_sides_are_taken_at_the_press(void) {
    const keypos_t owner_key  = {.row = 2, .col = 0};
    const keypos_t member_key = {.row = 6, .col = 4};
    const keypos_t later_key  = {.row = 1, .col = 2};
    uint8_t        bitmap[KEY_ORIGIN_BITMAP_SIZE];

    test_reset_stubs();
    key_origin_registry_reset();
    key_origin_bitmap_clear(bitmap);
    key_origin_bitmap_add_keypos(bitmap, owner_key);
    key_origin_bitmap_add_keypos(bitmap, member_key);
    CHECK(key_origin_registry_set_bitmap(owner_key, bitmap));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, owner_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_BOTH));

    CHECK(key_origin_registry_set_bitmap(owner_key, NULL));
    CHECK(pointing_mode_handle_keycode_press_at(PD_MODE_1, later_key));
    CHECK(test_owner_sides_are(SPLIT_SIDE_MASK_BOTH));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, later_key));
    CHECK(pointing_mode_handle_keycode_release_at(PD_MODE_1, owner_key));
    key_origin_registry_reset();
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
    test_layer_state_set_snipes_on_every_sniping_layer();
    test_layer_changes_write_the_cpi_only_when_it_changes();
    test_a_cpi_written_behind_the_policy_is_written_again();
    test_a_cpi_the_sensor_lost_is_restored_within_a_period();
    test_the_refresh_writes_the_cpi_the_policy_intends();
    test_the_refresh_period_runs_across_the_clock_wrap();
    test_a_write_that_happens_anyway_restarts_the_refresh_period();
    test_is_mouse_record_user_delegates_pointer_layer_policy();
    test_an_empty_slot_hold_key_owns_nothing();
    test_a_locked_modes_anchor_is_not_taken_back_after_a_qmk_reset();
    test_two_keys_holding_one_mode_both_own_it();
    test_the_owner_key_is_the_first_key_to_hold_the_mode();
    test_a_later_key_takes_a_released_owners_place();
    test_a_repeated_press_is_one_owner();
    test_a_new_mode_drops_the_old_modes_owners();
    test_a_lock_key_replaces_the_holding_owners();
    test_a_combo_owner_owns_its_footprint();
    test_owner_sides_are_taken_at_the_press();
    return 0;
}

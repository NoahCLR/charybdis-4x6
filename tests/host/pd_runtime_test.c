#include "users/noah/lib/pointing/runtime/pd_sensor_observation.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/noah_runtime.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"
#include "users/noah/lib/pointing/modes/pd_mode_configured.h"
#include "users/noah/lib/pointing/modes/pd_mode_handlers.h"
#include "users/noah/lib/profile/runtime/effective_pd_runtime.h"
#include "users/noah/lib/state/ownership/keyboard_mod_ownership.h"
#include "host_runtime_reset_fixture.h"
#include "users/noah/lib/rgb/lighting/lighting_events.h"
#include "users/noah/lib/key/runtime/slot/origin_registry.h"
#include "users/noah/lib/pointing/runtime/pd_mode_snapshot_internal.h"
#include "users/noah/lib/pointing/runtime/pd_mode_internal.h"

// Local dispatch fakes let this test observe the configured slot selection.
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

    current_cpi                        = 0;
    cpi_set_count                      = 0;
    default_dpi                        = 900;
    sniping_dpi                        = 350;
    fake_timer_ms                      = 0;
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
static const uint8_t test_pd_records[8][96] = {
    [0] = {[0] = 0, [1] = 2, [4] = 100},
    [1] = {[0] = 1, [1] = 1, [4] = 120, [5] = 5},
    [2] = {[0] = 2, [1] = 1, [4] = 220, [5] = 5},
    [3] = {[0] = 3, [1] = 1, [4] = 64, [5] = 6},
    [4] = {[0] = 4, [1] = 1, [2] = 1, [4] = 164, [5] = 6},
    [5] = {[0] = 5, [1] = 2, [4] = 100},
};
static const uint8_t *active_pd_record;

const uint8_t *noah_effective_pd_for_mask(uint32_t mode) {
    for (uint8_t slot = 0; slot < 6; slot++) {
        if (mode == (UINT32_C(1) << slot)) return test_pd_records[slot];
    }
    return NULL;
}

void noah_pd_engine_enter(const uint8_t *record) { active_pd_record = record; }
void noah_pd_engine_exit(void) {
    if (active_pd_record && active_pd_record[0] == 1) reset_volume_mode();
    active_pd_record = NULL;
}
uint8_t noah_pd_engine_masked_mods(const uint8_t *record) { (void)record; return 0; }
bool noah_pd_engine_key(uint16_t keycode, keyrecord_t *record) { (void)keycode; (void)record; return false; }
report_mouse_t noah_pd_engine_motion(report_mouse_t report) {
    if (!active_pd_record) return report;
    switch (active_pd_record[0]) {
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

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_1));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 13, .y = 20, .h = 30, .v = 40, .buttons = 1}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 0);
    CHECK(pd_mode_local_active(PD_MODE_VOLUME));

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_2));
    output = noah_pointing_device_task_user(input);

    CHECK(report_mouse_equal(output, (report_mouse_t){.x = 10, .y = 15, .h = 30, .v = 40, .buttons = 2}));
    CHECK(volume_handler_calls == 1);
    CHECK(brightness_handler_calls == 1);
    CHECK(pd_mode_local_active(PD_MODE_BRIGHTNESS));
    CHECK(reset_volume_count == 1);
}

static void test_pointing_device_task_dispatches_zero_report_to_active_mode(void) {
    test_reset_stubs();

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_1));
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
    CHECK(noah_pointing_sensor_motion_observed()); // literal sensor sample, even when suppressed
    (void)noah_pointing_device_task_user((report_mouse_t){0});
    CHECK(!noah_pointing_sensor_motion_observed()); // no continuation
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
    CHECK(pd_mode_handle_keycode_press(PD_SLOT_1));

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

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_1));
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

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_4));
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

    CHECK(pd_mode_handle_keycode_press(PD_SLOT_1));
    CHECK(noah_is_mouse_record_user(MO(1), &record));
    CHECK(noah_is_mouse_record_user(PD_SLOT_1, &record));
    CHECK(noah_is_mouse_record_user(DPI_MOD, &record));
    CHECK(!noah_is_mouse_record_user(PD_SLOT_4, &record));
    CHECK(!noah_is_mouse_record_user(KC_C, &record));

    test_reset_stubs();
    CHECK(pd_mode_handle_keycode_press(PD_SLOT_4));
    CHECK(!noah_is_mouse_record_user(MO(1), &record));
    CHECK(!noah_is_mouse_record_user(PD_SLOT_4, &record));
}

#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
static noah_lighting_event_t mode_events[8];
static uint8_t mode_event_owners[8][KEY_ORIGIN_BITMAP_SIZE];
static unsigned mode_event_count;
static bool recording_mode_events;
void noah_lighting_event_observe(const noah_lighting_event_t *event) {
    if (!recording_mode_events) return;
    CHECK(mode_event_count<8 && event->owners && event->thing<PD_MODE_COUNT);
    CHECK(event->kind==NOAH_LIGHTING_EVENT_MODE_STARTED || event->kind==NOAH_LIGHTING_EVENT_MODE_ENDED);
    mode_events[mode_event_count]=*event;
    memcpy(mode_event_owners[mode_event_count],event->owners,KEY_ORIGIN_BITMAP_SIZE);
    mode_events[mode_event_count].owners=mode_event_owners[mode_event_count];mode_event_count++;
}
static void test_mode_events_local_transition_owners_and_no_remote_replay(void) {
    test_reset_stubs();mode_event_count=0;recording_mode_events=true;
    keypos_t a={.row=0,.col=0},b={.row=1,.col=1};
    pd_mode_command_t start={.kind=PD_MODE_COMMAND_KEY_PRESS,.keycode=PD_SLOT_0,.owner_key_pos=a,.owner_sides=SPLIT_SIDE_MASK_LEFT};
    pd_mode_apply_result_t result=pd_mode_apply_command(start);
    CHECK(result.handled && result.local_state_changed && result.display_state_changed && result.split_sync_required);
    CHECK(result.before.local.active_mode==0 && result.after.local.active_mode==pd_mode_mask_from_id(0));
    CHECK(result.after.display.active_mode==result.after.local.active_mode);
    CHECK(mode_event_count==1 && mode_events[0].kind==NOAH_LIGHTING_EVENT_MODE_STARTED && mode_events[0].thing==0);
    CHECK(key_origin_bitmap_has_keypos(mode_event_owners[0],a));
    result=pd_mode_apply_command(start);CHECK(mode_event_count==1);
    CHECK(result.handled && !result.local_state_changed && !result.display_state_changed && !result.split_sync_required);
    start.keycode=PD_SLOT_1;start.owner_key_pos=b;
    (void)pd_mode_apply_command(start);
    CHECK(mode_event_count==3 && mode_events[1].kind==NOAH_LIGHTING_EVENT_MODE_ENDED && mode_events[1].thing==0);
    CHECK(mode_events[2].kind==NOAH_LIGHTING_EVENT_MODE_STARTED && mode_events[2].thing==1);
    CHECK(key_origin_bitmap_has_keypos(mode_event_owners[1],a) && key_origin_bitmap_has_keypos(mode_event_owners[2],b));
    pd_mode_command_t remote={.kind=PD_MODE_COMMAND_REMOTE_SNAPSHOT,.active_mode_id=2,.locked_mode_id=2};
    (void)pd_mode_apply_command(remote);CHECK(mode_event_count==3);
    pd_mode_command_t end={.kind=PD_MODE_COMMAND_KEY_RELEASE,.keycode=PD_SLOT_1,.owner_key_pos=b};
    (void)pd_mode_apply_command(end);
    CHECK(mode_event_count==4 && mode_events[3].kind==NOAH_LIGHTING_EVENT_MODE_ENDED && mode_events[3].thing==1);
    CHECK(key_origin_bitmap_has_keypos(mode_event_owners[3],b));
    (void)pd_mode_apply_command(end);CHECK(mode_event_count==4);
    // Copies outlive the producer's owner cleanup.
    CHECK(key_origin_bitmap_has_keypos(mode_event_owners[2],b));
    // Internal flag-only wrappers preserve transition/owner/event behavior.
    mode_event_count=0;
    CHECK(pd_mode_handle_keycode_press_at(PD_SLOT_0,a));
    CHECK(mode_event_count==1 && mode_events[0].thing==0 && key_origin_bitmap_has_keypos(mode_event_owners[0],a));
    CHECK(pd_mode_handle_keycode_release_at(PD_SLOT_0,a));
    CHECK(mode_event_count==2 && mode_events[1].kind==NOAH_LIGHTING_EVENT_MODE_ENDED && key_origin_bitmap_has_keypos(mode_event_owners[1],a));
    recording_mode_events=false;mode_event_count=0;test_reset_stubs();
}
#endif

static void check_compact_mode_views_against_public_snapshot(void) {
    uint8_t owners[KEY_ORIGIN_BITMAP_SIZE], expected[KEY_ORIGIN_BITMAP_SIZE];
    bool has_keys;
    pd_mode_owner_view_t view;
    pd_mode_snapshot_t full = pd_mode_snapshot_with_owner_bitmap(expected, &has_keys);
    memset(owners, 0xff, sizeof owners);
    pd_mode_display_owner_view(&view, owners);
    CHECK(view.active_index == full.display.active_index && view.locked_index == full.display.locked_index);
    CHECK(view.owner_sides == full.display.owner_sides && view.has_owner_keys == has_keys);
    CHECK(memcmp(owners, expected, sizeof owners) == 0);
    has_keys = pd_mode_local_owner_bitmap_snapshot(expected);
    memset(owners, 0xff, sizeof owners);
    pd_mode_local_owner_view(&view, owners);
    CHECK(view.active_index == full.local.active_index && view.locked_index == full.local.locked_index);
    CHECK(view.owner_sides == full.local.owner_sides && view.has_owner_keys == has_keys);
    CHECK(memcmp(owners, expected, sizeof owners) == 0);
}

static unsigned compact_publication_reads;
static void read_compact_view_during_pending_publication(void) {
    pd_mode_owner_view_t view;
    uint8_t owners[KEY_ORIGIN_BITMAP_SIZE];
    check_compact_mode_views_against_public_snapshot();
    pd_mode_display_owner_view(&view, owners);
    CHECK(view.active_index == 0u && view.locked_index == 0u);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(view.owner_sides == SPLIT_SIDE_MASK_LEFT && view.has_owner_keys);
    CHECK(key_origin_bitmap_has_keypos(owners, (keypos_t){.row = 0u, .col = 0u}));
    CHECK(!key_origin_bitmap_has_keypos(owners, (keypos_t){.row = MATRIX_ROWS - 1u, .col = MATRIX_COLS - 1u}));
#else
    CHECK(view.owner_sides == SPLIT_SIDE_MASK_NONE && !view.has_owner_keys);
    CHECK(!key_origin_bitmap_has_any(owners));
#endif
    compact_publication_reads++;
}

static void test_compact_mode_queries_preserve_views_and_publication(void) {
    test_reset_stubs();
    check_compact_mode_views_against_public_snapshot(); // inactive sentinel
    keypos_t a = {.row = 0u, .col = 0u}, b = {.row = MATRIX_ROWS - 1u, .col = MATRIX_COLS - 1u};
    CHECK(pd_mode_handle_keycode_press_at(PD_SLOT_0, a));
    CHECK(pd_mode_handle_keycode_press_at(PD_SLOT_0, b));
    check_compact_mode_views_against_public_snapshot(); // multiple actual owners
    uint8_t owners[KEY_ORIGIN_BITMAP_SIZE];
    pd_mode_owner_view_t view;
    pd_mode_local_owner_view(&view, owners);
    CHECK(key_origin_bitmap_has_keypos(owners, a) && key_origin_bitmap_has_keypos(owners, b));
    CHECK(pd_mode_handle_keycode_release_at(PD_SLOT_0, a));
    check_compact_mode_views_against_public_snapshot();
    CHECK(pd_mode_handle_keycode_release_at(PD_SLOT_0, b));
    check_compact_mode_views_against_public_snapshot();
    CHECK(pd_mode_apply_command((pd_mode_command_t){.kind = PD_MODE_COMMAND_LOCK, .mode = PD_MODE_VOLUME}).handled);
    check_compact_mode_views_against_public_snapshot(); // locked and ownerless
    fake_is_master = false;
    uint8_t old_owners[KEY_ORIGIN_BITMAP_SIZE] = {0}, new_owners[KEY_ORIGIN_BITMAP_SIZE] = {0};
    key_origin_bitmap_add_keypos(old_owners, a);
    key_origin_bitmap_add_keypos(new_owners, b);
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(0u, 0u, SPLIT_SIDE_MASK_LEFT, old_owners);
    check_compact_mode_views_against_public_snapshot(); // local differs from display
    compact_publication_reads = 0u;
    pd_mode_test_set_publish_seam(read_compact_view_during_pending_publication);
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(1u, PD_MODE_ID_NONE, SPLIT_SIDE_MASK_RIGHT, new_owners);
    pd_mode_test_set_publish_seam(NULL);
    CHECK(compact_publication_reads == 1u);
    check_compact_mode_views_against_public_snapshot();
    pd_mode_display_owner_view(&view, owners);
    CHECK(view.active_index == 1u && view.locked_index == PD_MODE_COUNT);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(view.owner_sides == SPLIT_SIDE_MASK_RIGHT && view.has_owner_keys);
    CHECK(!key_origin_bitmap_has_keypos(owners, a) && key_origin_bitmap_has_keypos(owners, b));
#else
    CHECK(view.owner_sides == SPLIT_SIDE_MASK_NONE && !view.has_owner_keys);
    CHECK(!key_origin_bitmap_has_any(owners));
#endif
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(PD_MODE_ID_NONE, PD_MODE_ID_NONE, SPLIT_SIDE_MASK_NONE, NULL);
    check_compact_mode_views_against_public_snapshot(); // clears prior bytes
}

int main(void) {
    test_compact_mode_queries_preserve_views_and_publication();
#if NOAH_LIGHTING_EVENT_CAPTURE_ENABLE
    test_mode_events_local_transition_owners_and_no_remote_replay();
#endif
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

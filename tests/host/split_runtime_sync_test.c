#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "transactions.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/state/runtime/split_runtime_sync.h"

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;

#define fake_time32 runtime_fixture.time32
#define fake_is_master runtime_fixture.is_master

static uint16_t       fake_auto_mouse_elapsed;
static bool           fake_auto_mouse_active;
static bool           fake_any_mode_locked;
static pd_mode_mask_t fake_pd_active_flags;
static pd_mode_mask_t fake_pd_locked_flags;
static split_half_t   fake_pd_owner_half;
static uint8_t        fake_key_feedback_flags;
static uint8_t        fake_key_feedback_key;
static uint8_t        fake_key_preview_layer;

static uint8_t                     rpc_register_count;
static int8_t                      rpc_registered_id;
static slave_callback_t            rpc_registered_callback;
static uint8_t                     rpc_send_count;
static int8_t                      rpc_last_send_id;
static uint8_t                     rpc_last_send_size;
static split_runtime_sync_packet_t rpc_last_packet;
static bool                        fake_rpc_send_result = true;

static uint8_t        remote_snapshot_apply_count;
static pd_mode_mask_t remote_snapshot_active;
static pd_mode_mask_t remote_snapshot_locked;
static split_half_t   remote_snapshot_owner_half;

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
    host_runtime_fixture_reset(&runtime_fixture);
    fake_auto_mouse_elapsed     = 83u;
    fake_auto_mouse_active      = true;
    fake_any_mode_locked        = false;
    fake_pd_active_flags        = PD_MODE_ZOOM;
    fake_pd_locked_flags        = 0;
    fake_pd_owner_half          = SPLIT_HALF_RIGHT;
    fake_key_feedback_flags     = KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
    fake_key_feedback_key       = 37u;
    fake_key_preview_layer      = 3u;
    rpc_register_count          = 0;
    rpc_registered_id           = -1;
    rpc_registered_callback     = NULL;
    rpc_send_count              = 0;
    rpc_last_send_id            = -1;
    rpc_last_send_size          = 0;
    rpc_last_packet             = (split_runtime_sync_packet_t){0};
    fake_rpc_send_result        = true;
    remote_snapshot_apply_count = 0;
    remote_snapshot_active      = 0;
    remote_snapshot_locked      = 0;
    remote_snapshot_owner_half  = SPLIT_HALF_NONE;
}

HOST_RUNTIME_FIXTURE_DEFINE_BASIC_QMK_STUBS(runtime_fixture)

uint16_t auto_mouse_get_time_elapsed(void) {
    return fake_auto_mouse_elapsed;
}

bool is_auto_mouse_active(void) {
    return fake_auto_mouse_active;
}

bool pd_any_local_mode_locked(void) {
    return fake_any_mode_locked;
}

pd_mode_mask_t pd_mode_local_active_snapshot(void) {
    return fake_pd_active_flags;
}

pd_mode_mask_t pd_mode_local_locked_snapshot(void) {
    return fake_pd_locked_flags;
}

split_half_t pd_mode_local_owner_half_snapshot(void) {
    return fake_pd_owner_half;
}

uint8_t key_feedback_pack(void) {
    return fake_key_feedback_flags;
}

uint8_t key_feedback_key(void) {
    return fake_key_feedback_key;
}

uint8_t key_feedback_preview_layer(void) {
    return fake_key_preview_layer;
}

void pd_mode_apply_remote_mode_ids(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_half_t owner_half) {
    remote_snapshot_apply_count++;
    remote_snapshot_active = pd_mode_mask_from_id(active_mode_id);
    remote_snapshot_locked = pd_mode_mask_from_id(locked_mode_id);
    remote_snapshot_owner_half = owner_half;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    rpc_register_count++;
    rpc_registered_id       = transaction_id;
    rpc_registered_callback = callback;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    rpc_send_count++;
    rpc_last_send_id   = transaction_id;
    rpc_last_send_size = initiator2target_buffer_size;
    CHECK(initiator2target_buffer_size == sizeof(split_runtime_sync_packet_t));
    memcpy(&rpc_last_packet, initiator2target_buffer, sizeof(rpc_last_packet));
    return fake_rpc_send_result;
}

static void test_init_registers_rpc_and_sends_initial_packet_on_master(void) {
    test_reset_stubs();

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(sizeof(split_runtime_sync_packet_t) == 8u);
#else
    CHECK(sizeof(split_runtime_sync_packet_t) == 7u);
#endif

    split_runtime_sync_init();

    CHECK(rpc_register_count == 1);
    CHECK(rpc_registered_id == PUT_SPLIT_RUNTIME_SYNC);
    CHECK(rpc_registered_callback != NULL);
    CHECK(rpc_send_count == 1);
    CHECK(rpc_last_packet.automouse_progress == 60u);
    CHECK(rpc_last_packet.active_mode_id == pd_mode_id_from_mask(fake_pd_active_flags));
    CHECK(rpc_last_packet.locked_mode_id == pd_mode_id_from_mask(fake_pd_locked_flags));
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(rpc_last_packet.pd_mode_owner_half == fake_pd_owner_half);
#endif
    CHECK(rpc_last_packet.key_feedback_flags == fake_key_feedback_flags);
    CHECK(rpc_last_packet.key_feedback_key == fake_key_feedback_key);
    CHECK(rpc_last_packet.key_preview_layer == fake_key_preview_layer);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_init_registers_rpc_without_sending_on_slave(void) {
    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();

    CHECK(rpc_register_count == 1);
    CHECK(rpc_registered_callback != NULL);
    CHECK(rpc_send_count == 0);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_elapsed_skips_unchanged_packet_until_heartbeat(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;

    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0);

    fake_time32 += 249u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0);

    fake_time32 += 1u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 1);
}

static void test_force_sync_sends_even_when_packet_is_unchanged(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;

    split_runtime_sync();

    CHECK(rpc_send_count == 1);
}

static void test_request_force_sync_defers_send_until_tick(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;

    split_runtime_sync_request();
    CHECK(rpc_send_count == 0);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 1);

    rpc_send_count = 0;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0);
}

static void test_locked_pd_mode_zeroes_automouse_progress(void) {
    test_reset_stubs();
    fake_any_mode_locked = true;

    split_runtime_sync_init();

    CHECK(rpc_last_packet.automouse_progress == 0);
}

static void test_inactive_automouse_keeps_timeout_window_progress(void) {
    test_reset_stubs();
    fake_auto_mouse_active = false;

    split_runtime_sync_init();

    CHECK(rpc_last_packet.automouse_progress == 60u);
}

static void test_timeout_end_inactive_automouse_keeps_max_progress(void) {
    test_reset_stubs();
    fake_auto_mouse_active  = false;
    fake_auto_mouse_elapsed = AUTO_MOUSE_TIME;

    split_runtime_sync_init();

    CHECK(rpc_last_packet.automouse_progress == 100u);
}

static void test_tick_uses_auto_mouse_elapsed_when_packet_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count          = 0;
    fake_auto_mouse_elapsed = 91u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1);
    CHECK(rpc_last_packet.automouse_progress == 70u);
}

static void test_automouse_progress_quantizes_concrete_boundaries(void) {
    test_reset_stubs();
    fake_auto_mouse_elapsed = 0u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 0u);

    test_reset_stubs();
    fake_auto_mouse_elapsed = 20u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 0u);

    test_reset_stubs();
    fake_auto_mouse_elapsed = 21u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 0u);

    test_reset_stubs();
    fake_auto_mouse_elapsed = 30u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 10u);

    test_reset_stubs();
    fake_auto_mouse_elapsed = 120u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 100u);

    test_reset_stubs();
    fake_auto_mouse_elapsed = 250u;
    split_runtime_sync_init();
    CHECK(rpc_last_packet.automouse_progress == 100u);
}

static void test_slave_rpc_applies_exact_packet_and_snapshot(void) {
    split_runtime_sync_packet_t packet = {
        .automouse_progress = 42u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ARROW),
        .locked_mode_id     = pd_mode_id_from_mask(PD_MODE_VOLUME),
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_half = SPLIT_HALF_LEFT,
#endif
        .key_feedback_flags = KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE,
        .key_feedback_key   = 12u,
        .key_preview_layer  = 6u,
    };

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();
    CHECK(rpc_registered_callback != NULL);

    rpc_registered_callback(sizeof(packet), &packet, 0, NULL);

    CHECK(split_runtime_sync_remote.automouse_progress == packet.automouse_progress);
    CHECK(split_runtime_sync_remote.active_mode_id == packet.active_mode_id);
    CHECK(split_runtime_sync_remote.locked_mode_id == packet.locked_mode_id);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(split_runtime_sync_remote.pd_mode_owner_half == packet.pd_mode_owner_half);
    CHECK(remote_snapshot_owner_half == packet.pd_mode_owner_half);
#else
    CHECK(remote_snapshot_owner_half == SPLIT_HALF_NONE);
#endif
    CHECK(split_runtime_sync_remote.key_feedback_flags == packet.key_feedback_flags);
    CHECK(split_runtime_sync_remote.key_feedback_key == packet.key_feedback_key);
    CHECK(split_runtime_sync_remote.key_preview_layer == packet.key_preview_layer);
    CHECK(remote_snapshot_apply_count == 1);
    CHECK(remote_snapshot_active == pd_mode_mask_from_id(packet.active_mode_id));
    CHECK(remote_snapshot_locked == pd_mode_mask_from_id(packet.locked_mode_id));
}

static void test_slave_rpc_ignores_short_packets(void) {
    split_runtime_sync_packet_t packet = {
        .automouse_progress = 12u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ZOOM),
        .key_preview_layer  = 5u,
    };

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();
    CHECK(rpc_registered_callback != NULL);

    rpc_registered_callback((uint8_t)(sizeof(packet) - 1u), &packet, 0, NULL);

    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
    CHECK(remote_snapshot_apply_count == 0);
}

int main(void) {
    test_init_registers_rpc_and_sends_initial_packet_on_master();
    test_init_registers_rpc_without_sending_on_slave();
    test_elapsed_skips_unchanged_packet_until_heartbeat();
    test_force_sync_sends_even_when_packet_is_unchanged();
    test_request_force_sync_defers_send_until_tick();
    test_locked_pd_mode_zeroes_automouse_progress();
    test_inactive_automouse_keeps_timeout_window_progress();
    test_timeout_end_inactive_automouse_keeps_max_progress();
    test_tick_uses_auto_mouse_elapsed_when_packet_changes();
    test_automouse_progress_quantizes_concrete_boundaries();
    test_slave_rpc_applies_exact_packet_and_snapshot();
    test_slave_rpc_ignores_short_packets();

    puts("split_runtime_sync host tests passed");
    return 0;
}

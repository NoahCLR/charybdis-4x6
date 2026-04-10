#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "transactions.h"
#include "users/noah/lib/key/key_runtime_feedback.h"
#include "users/noah/lib/state/split_runtime_sync.h"

static uint32_t       fake_time32;
static bool           fake_is_master;
static uint16_t       fake_auto_mouse_elapsed;
static bool           fake_auto_mouse_active;
static bool           fake_any_mode_locked;
static pd_mode_mask_t fake_pd_active_flags;
static pd_mode_mask_t fake_pd_locked_flags;
static uint8_t        fake_key_feedback_flags;
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
    fake_time32                 = 1000u;
    fake_is_master              = true;
    fake_auto_mouse_elapsed     = 83u;
    fake_auto_mouse_active      = true;
    fake_any_mode_locked        = false;
    fake_pd_active_flags        = PD_MODE_VOLUME | PD_MODE_ZOOM;
    fake_pd_locked_flags        = PD_MODE_VOLUME;
    fake_key_feedback_flags     = KEY_FEEDBACK_FLAG_HOLD_ACTIVE;
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
}

uint32_t timer_read32(void) {
    return fake_time32;
}

uint32_t timer_elapsed32(uint32_t last) {
    return fake_time32 - last;
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    return fake_auto_mouse_elapsed;
}

bool is_auto_mouse_active(void) {
    return fake_auto_mouse_active;
}

bool pd_any_mode_locked(void) {
    return fake_any_mode_locked;
}

pd_mode_mask_t pd_mode_active_snapshot(void) {
    return fake_pd_active_flags;
}

pd_mode_mask_t pd_mode_locked_snapshot(void) {
    return fake_pd_locked_flags;
}

uint8_t key_feedback_pack(void) {
    return fake_key_feedback_flags;
}

uint8_t key_feedback_preview_layer(void) {
    return fake_key_preview_layer;
}

void pd_mode_apply_remote_snapshot(pd_mode_mask_t active_flags, pd_mode_mask_t locked_flags) {
    remote_snapshot_apply_count++;
    remote_snapshot_active = active_flags;
    remote_snapshot_locked = locked_flags;
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

    split_runtime_sync_init();

    CHECK(rpc_register_count == 1);
    CHECK(rpc_registered_id == PUT_SPLIT_RUNTIME_SYNC);
    CHECK(rpc_registered_callback != NULL);
    CHECK(rpc_send_count == 1);
    CHECK(rpc_last_packet.automouse_progress == 60u);
    CHECK(rpc_last_packet.pd_mode_flags == fake_pd_active_flags);
    CHECK(rpc_last_packet.pd_mode_locked_flags == fake_pd_locked_flags);
    CHECK(rpc_last_packet.key_feedback_flags == fake_key_feedback_flags);
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
        .automouse_progress   = 42u,
        .pd_mode_flags        = PD_MODE_ARROW,
        .pd_mode_locked_flags = PD_MODE_VOLUME,
        .key_feedback_flags   = KEY_FEEDBACK_FLAG_LONG_HOLD_ACTIVE,
        .key_preview_layer    = 6u,
    };

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();
    CHECK(rpc_registered_callback != NULL);

    rpc_registered_callback(sizeof(packet), &packet, 0, NULL);

    CHECK(split_runtime_sync_remote.automouse_progress == packet.automouse_progress);
    CHECK(split_runtime_sync_remote.pd_mode_flags == packet.pd_mode_flags);
    CHECK(split_runtime_sync_remote.pd_mode_locked_flags == packet.pd_mode_locked_flags);
    CHECK(split_runtime_sync_remote.key_feedback_flags == packet.key_feedback_flags);
    CHECK(split_runtime_sync_remote.key_preview_layer == packet.key_preview_layer);
    CHECK(remote_snapshot_apply_count == 1);
    CHECK(remote_snapshot_active == packet.pd_mode_flags);
    CHECK(remote_snapshot_locked == packet.pd_mode_locked_flags);
}

static void test_slave_rpc_ignores_short_packets(void) {
    split_runtime_sync_packet_t packet = {
        .automouse_progress = 12u,
        .pd_mode_flags      = PD_MODE_ZOOM,
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

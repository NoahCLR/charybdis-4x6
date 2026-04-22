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

static uint16_t          fake_auto_mouse_elapsed;
static bool              fake_auto_mouse_active;
static bool              fake_any_mode_locked;
static pd_mode_mask_t    fake_pd_active_flags;
static pd_mode_mask_t    fake_pd_locked_flags;
static split_side_mask_t fake_pd_owner_sides;
static uint8_t           fake_combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
static uint8_t           fake_key_feedback_flash_meta;
static uint8_t           fake_key_preview_layer;

static uint8_t          rpc_register_count;
static int8_t           rpc_registered_ids[3];
static slave_callback_t rpc_registered_callbacks[3];
static uint8_t          rpc_send_count;
static uint8_t          rpc_send_count_base;
static uint8_t          rpc_send_count_combo;
static uint8_t          rpc_send_count_key_feedback;
static int8_t           rpc_last_send_id;
static uint8_t          rpc_last_send_size;
static split_runtime_base_sync_packet_t      rpc_last_base_packet;
static split_runtime_combo_feedback_packet_t rpc_last_combo_packet;
static split_runtime_key_feedback_packet_t   rpc_last_key_feedback_packet;
static bool             fake_rpc_send_result = true;

static uint8_t          remote_snapshot_apply_count;
static pd_mode_mask_t   remote_snapshot_active;
static pd_mode_mask_t   remote_snapshot_locked;
static split_side_mask_t remote_snapshot_owner_sides;

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
    fake_auto_mouse_elapsed        = 83u;
    fake_auto_mouse_active         = true;
    fake_any_mode_locked           = false;
    fake_pd_active_flags           = PD_MODE_ZOOM;
    fake_pd_locked_flags           = 0;
    fake_pd_owner_sides            = SPLIT_SIDE_MASK_RIGHT;
    key_origin_bitmap_clear(fake_combo_underlay_bitmap);
    key_origin_bitmap_clear(fake_combo_overlay_bitmap);
    fake_combo_overlay_bitmap[0]   = 0x24u;
    key_feedback_semantic_map_clear(fake_key_feedback_semantic_map);
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 0, .col = 0}, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    fake_key_feedback_flash_meta   = KEY_FEEDBACK_FLASH_META_PHASE;
    fake_key_preview_layer         = 3u;
    rpc_register_count             = 0;
    memset(rpc_registered_ids, -1, sizeof(rpc_registered_ids));
    memset(rpc_registered_callbacks, 0, sizeof(rpc_registered_callbacks));
    rpc_send_count                 = 0;
    rpc_send_count_base            = 0;
    rpc_send_count_combo           = 0;
    rpc_send_count_key_feedback    = 0;
    rpc_last_send_id               = -1;
    rpc_last_send_size             = 0;
    rpc_last_base_packet           = (split_runtime_base_sync_packet_t){0};
    rpc_last_combo_packet          = (split_runtime_combo_feedback_packet_t){0};
    rpc_last_key_feedback_packet   = (split_runtime_key_feedback_packet_t){0};
    fake_rpc_send_result           = true;
    remote_snapshot_apply_count    = 0;
    remote_snapshot_active         = 0;
    remote_snapshot_locked         = 0;
    remote_snapshot_owner_sides    = SPLIT_SIDE_MASK_NONE;
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

split_side_mask_t pd_mode_local_owner_sides_snapshot(void) {
    return fake_pd_owner_sides;
}

uint8_t key_feedback_flash_meta(void) {
    return fake_key_feedback_flash_meta;
}

void key_feedback_semantic_map(uint8_t *out_map) {
    memcpy(out_map, fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

void combo_feedback_underlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_combo_underlay_bitmap);
}

void combo_feedback_overlay_bitmap(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_combo_overlay_bitmap);
}

uint8_t key_feedback_preview_layer(void) {
    return fake_key_preview_layer;
}

void pd_mode_apply_remote_mode_ids(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_side_mask_t owner_sides) {
    remote_snapshot_apply_count++;
    remote_snapshot_active      = pd_mode_mask_from_id(active_mode_id);
    remote_snapshot_locked      = pd_mode_mask_from_id(locked_mode_id);
    remote_snapshot_owner_sides = owner_sides;
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(rpc_register_count < 3u);
    rpc_registered_ids[rpc_register_count]       = transaction_id;
    rpc_registered_callbacks[rpc_register_count] = callback;
    rpc_register_count++;
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    rpc_send_count++;
    rpc_last_send_id   = transaction_id;
    rpc_last_send_size = initiator2target_buffer_size;

    if (transaction_id == PUT_SPLIT_RUNTIME_BASE_SYNC) {
        rpc_send_count_base++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_base_sync_packet_t));
        memcpy(&rpc_last_base_packet, initiator2target_buffer, sizeof(rpc_last_base_packet));
    } else if (transaction_id == PUT_SPLIT_COMBO_FEEDBACK_SYNC) {
        rpc_send_count_combo++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_combo_feedback_packet_t));
        memcpy(&rpc_last_combo_packet, initiator2target_buffer, sizeof(rpc_last_combo_packet));
    } else if (transaction_id == PUT_SPLIT_KEY_FEEDBACK_SYNC) {
        rpc_send_count_key_feedback++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_key_feedback_packet_t));
        memcpy(&rpc_last_key_feedback_packet, initiator2target_buffer, sizeof(rpc_last_key_feedback_packet));
    } else {
        CHECK(false);
    }

    return fake_rpc_send_result;
}

static slave_callback_t test_registered_callback(int8_t id) {
    for (uint8_t index = 0; index < rpc_register_count; index++) {
        if (rpc_registered_ids[index] == id) {
            return rpc_registered_callbacks[index];
        }
    }

    return NULL;
}

static void test_init_registers_rpcs_and_sends_initial_packets_on_master(void) {
    test_reset_stubs();

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(sizeof(split_runtime_base_sync_packet_t) == 6u);
#else
    CHECK(sizeof(split_runtime_base_sync_packet_t) == 5u);
#endif
    CHECK(sizeof(split_runtime_combo_feedback_packet_t) == (size_t)(2u * KEY_ORIGIN_BITMAP_SIZE));
    CHECK(sizeof(split_runtime_key_feedback_packet_t) == (size_t)(1u + KEY_FEEDBACK_SEMANTIC_MAP_SIZE));

    split_runtime_sync_init();

    CHECK(rpc_register_count == 3u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SYNC) != NULL);
    CHECK(rpc_send_count == 3u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback == 1u);
    CHECK(rpc_last_base_packet.automouse_progress == 60u);
    CHECK(rpc_last_base_packet.active_mode_id == pd_mode_id_from_mask(fake_pd_active_flags));
    CHECK(rpc_last_base_packet.locked_mode_id == pd_mode_id_from_mask(fake_pd_locked_flags));
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(rpc_last_base_packet.pd_mode_owner_sides == fake_pd_owner_sides);
#endif
    CHECK(rpc_last_base_packet.key_preview_layer == fake_key_preview_layer);
    CHECK(memcmp(rpc_last_combo_packet.combo_underlay_bitmap, fake_combo_underlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_combo_packet.combo_overlay_bitmap, fake_combo_overlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(rpc_last_key_feedback_packet.key_feedback_flash_meta == fake_key_feedback_flash_meta);
    CHECK(memcmp(rpc_last_key_feedback_packet.key_feedback_semantic_map, fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE) == 0);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_init_registers_rpcs_without_sending_on_slave(void) {
    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();

    CHECK(rpc_register_count == 3u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SYNC) != NULL);
    CHECK(rpc_send_count == 0u);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_elapsed_skips_unchanged_packets_until_heartbeat(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;
    rpc_send_count_base = 0;
    rpc_send_count_combo = 0;
    rpc_send_count_key_feedback = 0;

    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0u);

    fake_time32 += 249u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0u);

    fake_time32 += 1u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 3u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback == 1u);
}

static void test_force_sync_sends_all_packets_even_when_unchanged(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;
    rpc_send_count_base = 0;
    rpc_send_count_combo = 0;
    rpc_send_count_key_feedback = 0;

    split_runtime_sync();

    CHECK(rpc_send_count == 3u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback == 1u);
}

static void test_request_force_sync_defers_send_until_tick(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;

    split_runtime_sync_request();
    CHECK(rpc_send_count == 0u);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 3u);

    rpc_send_count = 0;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);
}

static void test_locked_pd_mode_zeroes_automouse_progress(void) {
    test_reset_stubs();
    fake_any_mode_locked = true;

    split_runtime_sync_init();

    CHECK(rpc_last_base_packet.automouse_progress == 0u);
}

static void test_tick_sends_only_base_packet_when_only_automouse_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;
    rpc_send_count_base = 0;
    rpc_send_count_combo = 0;
    rpc_send_count_key_feedback = 0;
    fake_auto_mouse_elapsed = 91u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback == 0u);
    CHECK(rpc_last_base_packet.automouse_progress == 70u);
}

static void test_tick_sends_only_combo_packet_when_only_combo_feedback_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;
    rpc_send_count_base = 0;
    rpc_send_count_combo = 0;
    rpc_send_count_key_feedback = 0;

    fake_combo_underlay_bitmap[0] = 0x01u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback == 0u);
    CHECK(rpc_last_send_id == PUT_SPLIT_COMBO_FEEDBACK_SYNC);
    CHECK(rpc_last_combo_packet.combo_underlay_bitmap[0] == 0x01u);
}

static void test_tick_sends_only_key_feedback_packet_when_only_key_feedback_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    rpc_send_count = 0;
    rpc_send_count_base = 0;
    rpc_send_count_combo = 0;
    rpc_send_count_key_feedback = 0;

    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING);

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback == 1u);
    CHECK(rpc_last_send_id == PUT_SPLIT_KEY_FEEDBACK_SYNC);
    CHECK(key_feedback_semantic_map_get(rpc_last_key_feedback_packet.key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}) == KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING);
}

static void test_slave_rpcs_apply_exact_remote_state(void) {
    split_runtime_base_sync_packet_t base_packet = {
        .automouse_progress = 42u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ARROW),
        .locked_mode_id     = pd_mode_id_from_mask(PD_MODE_VOLUME),
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides = SPLIT_SIDE_MASK_LEFT,
#endif
        .key_preview_layer  = 6u,
    };
    split_runtime_combo_feedback_packet_t combo_packet = {0};
    split_runtime_key_feedback_packet_t   key_packet   = {0};

    test_reset_stubs();
    fake_is_master = false;
    combo_packet.combo_underlay_bitmap[0] = 0x11u;
    combo_packet.combo_overlay_bitmap[1]  = 0x22u;
    key_packet.key_feedback_flash_meta    = KEY_FEEDBACK_FLASH_META_PHASE;
    key_feedback_semantic_map_set(key_packet.key_feedback_semantic_map, (keypos_t){.row = 0, .col = 1}, KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING);

    split_runtime_sync_init();
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SYNC) != NULL);

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(base_packet), &base_packet, 0u, NULL);
    test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC)(sizeof(combo_packet), &combo_packet, 0u, NULL);
    test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SYNC)(sizeof(key_packet), &key_packet, 0u, NULL);

    CHECK(split_runtime_sync_remote.automouse_progress == base_packet.automouse_progress);
    CHECK(split_runtime_sync_remote.active_mode_id == base_packet.active_mode_id);
    CHECK(split_runtime_sync_remote.locked_mode_id == base_packet.locked_mode_id);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(split_runtime_sync_remote.pd_mode_owner_sides == base_packet.pd_mode_owner_sides);
    CHECK(remote_snapshot_owner_sides == base_packet.pd_mode_owner_sides);
#else
    CHECK(remote_snapshot_owner_sides == SPLIT_SIDE_MASK_NONE);
#endif
    CHECK(split_runtime_sync_remote.key_preview_layer == base_packet.key_preview_layer);
    CHECK(memcmp(split_runtime_sync_remote.combo_underlay_bitmap, combo_packet.combo_underlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.combo_overlay_bitmap, combo_packet.combo_overlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(split_runtime_sync_remote.key_feedback_flash_meta == key_packet.key_feedback_flash_meta);
    CHECK(memcmp(split_runtime_sync_remote.key_feedback_semantic_map, key_packet.key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE) == 0);
    CHECK(remote_snapshot_apply_count == 1u);
    CHECK(remote_snapshot_active == pd_mode_mask_from_id(base_packet.active_mode_id));
    CHECK(remote_snapshot_locked == pd_mode_mask_from_id(base_packet.locked_mode_id));
}

static void test_slave_base_rpc_ignores_short_packets(void) {
    split_runtime_base_sync_packet_t packet = {
        .automouse_progress = 12u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ZOOM),
        .key_preview_layer  = 5u,
    };

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)((uint8_t)(sizeof(packet) - 1u), &packet, 0u, NULL);

    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
    CHECK(remote_snapshot_apply_count == 0u);
}

int main(void) {
    test_init_registers_rpcs_and_sends_initial_packets_on_master();
    test_init_registers_rpcs_without_sending_on_slave();
    test_elapsed_skips_unchanged_packets_until_heartbeat();
    test_force_sync_sends_all_packets_even_when_unchanged();
    test_request_force_sync_defers_send_until_tick();
    test_locked_pd_mode_zeroes_automouse_progress();
    test_tick_sends_only_base_packet_when_only_automouse_changes();
    test_tick_sends_only_combo_packet_when_only_combo_feedback_changes();
    test_tick_sends_only_key_feedback_packet_when_only_key_feedback_changes();
    test_slave_rpcs_apply_exact_remote_state();
    test_slave_base_rpc_ignores_short_packets();

    puts("split_runtime_sync host tests passed");
    return 0;
}

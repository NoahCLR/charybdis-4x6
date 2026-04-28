#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "transactions.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/split/runtime_sync.h"

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;

#define fake_time32 runtime_fixture.time32
#define fake_is_master runtime_fixture.is_master

static uint16_t          fake_auto_mouse_elapsed;
static bool              fake_auto_mouse_active;
static bool              fake_any_mode_locked;
static pd_mode_mask_t    fake_pd_active_flags;
static pd_mode_mask_t    fake_pd_locked_flags;
static split_side_mask_t fake_pd_owner_sides;
static uint8_t           fake_pd_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
static uint8_t           fake_key_feedback_broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
static uint8_t           fake_key_feedback_tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
static uint8_t           fake_key_feedback_flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
static uint8_t           fake_key_preview_layer;

static uint8_t                                      rpc_register_count;
static int8_t                                       rpc_registered_ids[4];
static slave_callback_t                             rpc_registered_callbacks[4];
static uint8_t                                      rpc_send_count;
static uint8_t                                      rpc_send_count_base;
static uint8_t                                      rpc_send_count_combo;
static uint8_t                                      rpc_send_count_key_feedback_semantic;
static uint8_t                                      rpc_send_count_key_feedback_branch;
static int8_t                                       rpc_last_send_id;
static uint8_t                                      rpc_last_send_size;
static split_runtime_base_sync_packet_t             rpc_last_base_packet;
static split_runtime_combo_feedback_packet_t        rpc_last_combo_packet;
static split_runtime_key_feedback_semantic_packet_t rpc_last_key_feedback_semantic_packet;
static split_runtime_key_feedback_branch_packet_t   rpc_last_key_feedback_branch_packet;
static bool                                         fake_rpc_send_result = true;

static uint8_t           remote_snapshot_apply_count;
static pd_mode_mask_t    remote_snapshot_active;
static pd_mode_mask_t    remote_snapshot_locked;
static split_side_mask_t remote_snapshot_owner_sides;
static uint8_t           remote_snapshot_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];

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
    fake_auto_mouse_elapsed = 83u;
    fake_auto_mouse_active  = true;
    fake_any_mode_locked    = false;
    fake_pd_active_flags    = PD_MODE_ZOOM;
    fake_pd_locked_flags    = 0;
    fake_pd_owner_sides     = SPLIT_SIDE_MASK_RIGHT;
    key_origin_bitmap_clear(fake_pd_owner_bitmap);
    key_origin_bitmap_fill_single(fake_pd_owner_bitmap, (keypos_t){.row = 4, .col = 0});
    key_origin_bitmap_clear(fake_combo_underlay_bitmap);
    key_origin_bitmap_clear(fake_combo_overlay_bitmap);
    fake_combo_overlay_bitmap[0] = 0x24u;
    key_feedback_semantic_map_clear(fake_key_feedback_semantic_map);
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 0, .col = 0}, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    key_feedback_broad_owner_map_clear(fake_key_feedback_broad_owner_map);
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, (keypos_t){.row = 0, .col = 0});
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, (keypos_t){.row = 0, .col = 0});
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GROUP_HOLD_ACTIVE, (keypos_t){.row = 0, .col = 0});
    key_feedback_tap_branch_map_clear(fake_key_feedback_tap_branch_map);
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);
    key_origin_bitmap_add_keypos(fake_key_feedback_flash_visibility_bitmap, (keypos_t){.row = 0, .col = 0});
    fake_key_preview_layer       = 3u;
    rpc_register_count           = 0;
    memset(rpc_registered_ids, -1, sizeof(rpc_registered_ids));
    memset(rpc_registered_callbacks, 0, sizeof(rpc_registered_callbacks));
    rpc_send_count               = 0;
    rpc_send_count_base          = 0;
    rpc_send_count_combo         = 0;
    rpc_send_count_key_feedback_semantic = 0;
    rpc_send_count_key_feedback_branch   = 0;
    rpc_last_send_id             = -1;
    rpc_last_send_size           = 0;
    rpc_last_base_packet         = (split_runtime_base_sync_packet_t){0};
    rpc_last_combo_packet        = (split_runtime_combo_feedback_packet_t){0};
    rpc_last_key_feedback_semantic_packet = (split_runtime_key_feedback_semantic_packet_t){0};
    rpc_last_key_feedback_branch_packet   = (split_runtime_key_feedback_branch_packet_t){0};
    fake_rpc_send_result         = true;
    remote_snapshot_apply_count  = 0;
    remote_snapshot_active       = 0;
    remote_snapshot_locked       = 0;
    remote_snapshot_owner_sides  = SPLIT_SIDE_MASK_NONE;
    key_origin_bitmap_clear(remote_snapshot_owner_bitmap);
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

bool pd_mode_local_owner_bitmap_snapshot(uint8_t *out_bitmap) {
    key_origin_bitmap_copy(out_bitmap, fake_pd_owner_bitmap);
    return key_origin_bitmap_has_any(fake_pd_owner_bitmap);
}

void key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) {
    if (!out_bitmap) {
        return;
    }

    key_origin_bitmap_clear(out_bitmap);
    if (!semantic_map) {
        return;
    }

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            keypos_t                key_pos  = {.row = row, .col = col};
            key_feedback_semantic_t semantic = key_feedback_semantic_map_get(semantic_map, key_pos);

            if (key_feedback_semantic_is_flashing(semantic) && key_origin_bitmap_has_keypos(fake_key_feedback_flash_visibility_bitmap, key_pos)) {
                key_origin_bitmap_add_keypos(out_bitmap, key_pos);
            }
        }
    }
}

void key_feedback_semantic_map(uint8_t *out_map) {
    memcpy(out_map, fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

void key_feedback_tap_branch_map(uint8_t *out_map) {
    memcpy(out_map, fake_key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

void key_feedback_broad_owner_map(uint8_t *out_map) {
    memcpy(out_map, fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
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

void pd_mode_apply_remote_mode_ids_with_owner_bitmap(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_side_mask_t owner_sides, const uint8_t *owner_bitmap) {
    remote_snapshot_apply_count++;
    remote_snapshot_active      = pd_mode_mask_from_id(active_mode_id);
    remote_snapshot_locked      = pd_mode_mask_from_id(locked_mode_id);
    remote_snapshot_owner_sides = owner_sides;
    if (owner_bitmap) {
        key_origin_bitmap_copy(remote_snapshot_owner_bitmap, owner_bitmap);
    } else {
        key_origin_bitmap_clear(remote_snapshot_owner_bitmap);
    }
}

void pd_mode_apply_remote_mode_ids(pd_mode_id_t active_mode_id, pd_mode_id_t locked_mode_id, split_side_mask_t owner_sides) {
    pd_mode_apply_remote_mode_ids_with_owner_bitmap(active_mode_id, locked_mode_id, owner_sides, NULL);
}

void transaction_register_rpc(int8_t transaction_id, slave_callback_t callback) {
    CHECK(rpc_register_count < 4u);
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
    } else if (transaction_id == PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC) {
        rpc_send_count_key_feedback_semantic++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_key_feedback_semantic_packet_t));
        memcpy(&rpc_last_key_feedback_semantic_packet, initiator2target_buffer, sizeof(rpc_last_key_feedback_semantic_packet));
    } else if (transaction_id == PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC) {
        rpc_send_count_key_feedback_branch++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_key_feedback_branch_packet_t));
        memcpy(&rpc_last_key_feedback_branch_packet, initiator2target_buffer, sizeof(rpc_last_key_feedback_branch_packet));
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

static void test_reset_rpc_send_counts(void) {
    rpc_send_count                       = 0;
    rpc_send_count_base                  = 0;
    rpc_send_count_combo                 = 0;
    rpc_send_count_key_feedback_semantic = 0;
    rpc_send_count_key_feedback_branch   = 0;
}

static void test_init_registers_rpcs_and_sends_initial_packets_on_master(void) {
    test_reset_stubs();

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(sizeof(split_runtime_base_sync_packet_t) == (size_t)(6u + KEY_ORIGIN_BITMAP_SIZE));
#else
    CHECK(sizeof(split_runtime_base_sync_packet_t) == 5u);
#endif
    CHECK(sizeof(split_runtime_combo_feedback_packet_t) == (size_t)(2u * KEY_ORIGIN_BITMAP_SIZE));
    CHECK(sizeof(split_runtime_key_feedback_semantic_packet_t) == (size_t)(KEY_ORIGIN_BITMAP_SIZE + KEY_FEEDBACK_SEMANTIC_MAP_SIZE));
    CHECK(sizeof(split_runtime_key_feedback_branch_packet_t) == (size_t)(KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE + KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE));

    split_runtime_sync_init();

    CHECK(rpc_register_count == 4u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC) != NULL);
    CHECK(rpc_send_count == 4u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
    CHECK(rpc_last_base_packet.automouse_progress == 60u);
    CHECK(rpc_last_base_packet.active_mode_id == pd_mode_id_from_mask(fake_pd_active_flags));
    CHECK(rpc_last_base_packet.locked_mode_id == pd_mode_id_from_mask(fake_pd_locked_flags));
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(rpc_last_base_packet.pd_mode_owner_sides == fake_pd_owner_sides);
    CHECK(memcmp(rpc_last_base_packet.pd_mode_owner_bitmap, fake_pd_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
#endif
    CHECK(rpc_last_base_packet.key_preview_layer == fake_key_preview_layer);
    CHECK(memcmp(rpc_last_combo_packet.combo_underlay_bitmap, fake_combo_underlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_combo_packet.combo_overlay_bitmap, fake_combo_overlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_key_feedback_semantic_packet.key_feedback_flash_visibility_bitmap, fake_key_feedback_flash_visibility_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_key_feedback_semantic_packet.key_feedback_semantic_map, fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_key_feedback_branch_packet.key_feedback_broad_owner_map, fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE) == 0);
    CHECK(memcmp(rpc_last_key_feedback_branch_packet.key_feedback_tap_branch_map, fake_key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE) == 0);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_init_registers_rpcs_without_sending_on_slave(void) {
    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();

    CHECK(rpc_register_count == 4u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC) != NULL);
    CHECK(rpc_send_count == 0u);
    CHECK(split_runtime_sync_remote.key_preview_layer == UINT8_MAX);
}

static void test_elapsed_skips_unchanged_packets_until_heartbeat(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0u);

    fake_time32 += 249u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 0u);

    fake_time32 += 1u;
    split_runtime_sync_elapsed(fake_auto_mouse_elapsed);
    CHECK(rpc_send_count == 4u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
}

static void test_force_sync_sends_all_packets_even_when_unchanged(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    split_runtime_sync();

    CHECK(rpc_send_count == 4u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
}

static void test_key_feedback_visibility_is_ignored_without_flashing_semantics(void) {
    test_reset_stubs();
    key_feedback_semantic_map_clear(fake_key_feedback_semantic_map);
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);
    key_origin_bitmap_add_keypos(fake_key_feedback_flash_visibility_bitmap, (keypos_t){.row = 1, .col = 1});

    split_runtime_sync_init();
    CHECK(!key_origin_bitmap_has_any(rpc_last_key_feedback_semantic_packet.key_feedback_flash_visibility_bitmap));

    test_reset_rpc_send_counts();
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
}

static void test_key_feedback_visibility_changes_when_flashing_semantics_are_present(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    CHECK(key_origin_bitmap_has_keypos(rpc_last_key_feedback_semantic_packet.key_feedback_flash_visibility_bitmap, (keypos_t){.row = 0, .col = 0}));

    test_reset_rpc_send_counts();
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
    CHECK(!key_origin_bitmap_has_any(rpc_last_key_feedback_semantic_packet.key_feedback_flash_visibility_bitmap));
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
    test_reset_rpc_send_counts();
    fake_auto_mouse_elapsed     = 91u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
    CHECK(rpc_last_base_packet.automouse_progress == 70u);
}

static void test_tick_sends_only_combo_packet_when_only_combo_feedback_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    fake_combo_underlay_bitmap[0] = 0x01u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
    CHECK(rpc_last_send_id == PUT_SPLIT_COMBO_FEEDBACK_SYNC);
    CHECK(rpc_last_combo_packet.combo_underlay_bitmap[0] == 0x01u);
}

static void test_tick_sends_only_key_feedback_packet_when_only_key_feedback_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
    CHECK(rpc_last_send_id == PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC);
    CHECK(key_feedback_semantic_map_get(rpc_last_key_feedback_semantic_packet.key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}) == KEY_FEEDBACK_SEMANTIC_UNRESOLVED_TAP_BRANCH);
}

static void test_tick_sends_only_key_feedback_branch_packet_when_only_tap_branch_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    key_feedback_tap_branch_map_set(fake_key_feedback_tap_branch_map, (keypos_t){.row = 1, .col = 1}, 3u);

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
    CHECK(rpc_last_send_id == PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC);
    CHECK(key_feedback_tap_branch_map_get(rpc_last_key_feedback_branch_packet.key_feedback_tap_branch_map, (keypos_t){.row = 1, .col = 1}) == 3u);
}

static void test_tick_sends_only_key_feedback_branch_packet_when_only_broad_owner_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, (keypos_t){.row = 1, .col = 1});
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, (keypos_t){.row = 1, .col = 1});

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
    CHECK(rpc_last_send_id == PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC);
    CHECK(memcmp(rpc_last_key_feedback_branch_packet.key_feedback_broad_owner_map, fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE) == 0);
}

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
static void test_tick_sends_base_packet_when_only_pd_owner_bitmap_changes(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    key_origin_bitmap_clear(fake_pd_owner_bitmap);
    key_origin_bitmap_fill_single(fake_pd_owner_bitmap, (keypos_t){.row = 4, .col = 1});

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 0u);
    CHECK(rpc_send_count_key_feedback_semantic == 0u);
    CHECK(rpc_send_count_key_feedback_branch == 0u);
    CHECK(memcmp(rpc_last_base_packet.pd_mode_owner_bitmap, fake_pd_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
}
#endif

static void test_idle_packets_use_idle_heartbeat(void) {
    test_reset_stubs();
    fake_auto_mouse_elapsed = 0u;
    fake_pd_active_flags    = 0;
    fake_pd_locked_flags    = 0;
    fake_pd_owner_sides     = SPLIT_SIDE_MASK_NONE;
    key_origin_bitmap_clear(fake_pd_owner_bitmap);
    fake_key_preview_layer = UINT8_MAX;
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);
    key_origin_bitmap_clear(fake_combo_underlay_bitmap);
    key_origin_bitmap_clear(fake_combo_overlay_bitmap);
    key_feedback_semantic_map_clear(fake_key_feedback_semantic_map);
    key_feedback_broad_owner_map_clear(fake_key_feedback_broad_owner_map);
    key_feedback_tap_branch_map_clear(fake_key_feedback_tap_branch_map);

    split_runtime_sync_init();
    CHECK(!key_origin_bitmap_has_any(rpc_last_key_feedback_semantic_packet.key_feedback_flash_visibility_bitmap));

    test_reset_rpc_send_counts();

    fake_time32 += 999u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);

    fake_time32 += 1u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 4u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_combo == 1u);
    CHECK(rpc_send_count_key_feedback_semantic == 1u);
    CHECK(rpc_send_count_key_feedback_branch == 1u);
}

static void test_slave_rpcs_apply_exact_remote_state(void) {
    split_runtime_base_sync_packet_t base_packet = {
        .automouse_progress = 42u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ARROW),
        .locked_mode_id     = pd_mode_id_from_mask(PD_MODE_VOLUME),
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides  = SPLIT_SIDE_MASK_LEFT,
        .pd_mode_owner_bitmap = {0x08u},
#endif
        .key_preview_layer = 6u,
    };
    split_runtime_combo_feedback_packet_t        combo_packet        = {0};
    split_runtime_key_feedback_semantic_packet_t key_semantic_packet = {0};
    split_runtime_key_feedback_branch_packet_t   key_branch_packet   = {0};

    test_reset_stubs();
    fake_is_master                        = false;
    combo_packet.combo_underlay_bitmap[0] = 0x11u;
    combo_packet.combo_overlay_bitmap[1]  = 0x22u;
    key_origin_bitmap_add_keypos(key_semantic_packet.key_feedback_flash_visibility_bitmap, (keypos_t){.row = 0, .col = 1});
    key_feedback_semantic_map_set(key_semantic_packet.key_feedback_semantic_map, (keypos_t){.row = 0, .col = 1}, KEY_FEEDBACK_SEMANTIC_HOLD_ACTIVE_FLASHING);
    key_feedback_broad_owner_map_clear(key_branch_packet.key_feedback_broad_owner_map);
    key_feedback_broad_owner_map_set(key_branch_packet.key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, (keypos_t){.row = 0, .col = 1});
    key_feedback_broad_owner_map_set(key_branch_packet.key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, (keypos_t){.row = 0, .col = 1});
    key_feedback_tap_branch_map_set(key_branch_packet.key_feedback_tap_branch_map, (keypos_t){.row = 0, .col = 1}, 2u);

    split_runtime_sync_init();
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC) != NULL);

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(base_packet), &base_packet, 0u, NULL);
    test_registered_callback(PUT_SPLIT_COMBO_FEEDBACK_SYNC)(sizeof(combo_packet), &combo_packet, 0u, NULL);
    test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC)(sizeof(key_semantic_packet), &key_semantic_packet, 0u, NULL);
    test_registered_callback(PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC)(sizeof(key_branch_packet), &key_branch_packet, 0u, NULL);

    CHECK(split_runtime_sync_remote.automouse_progress == base_packet.automouse_progress);
    CHECK(split_runtime_sync_remote.active_mode_id == base_packet.active_mode_id);
    CHECK(split_runtime_sync_remote.locked_mode_id == base_packet.locked_mode_id);
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(split_runtime_sync_remote.pd_mode_owner_sides == base_packet.pd_mode_owner_sides);
    CHECK(memcmp(split_runtime_sync_remote.pd_mode_owner_bitmap, base_packet.pd_mode_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(remote_snapshot_owner_sides == base_packet.pd_mode_owner_sides);
    CHECK(memcmp(remote_snapshot_owner_bitmap, base_packet.pd_mode_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
#else
    CHECK(remote_snapshot_owner_sides == SPLIT_SIDE_MASK_NONE);
#endif
    CHECK(split_runtime_sync_remote.key_preview_layer == base_packet.key_preview_layer);
    CHECK(memcmp(split_runtime_sync_remote.combo_underlay_bitmap, combo_packet.combo_underlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.combo_overlay_bitmap, combo_packet.combo_overlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.key_feedback_flash_visibility_bitmap, key_semantic_packet.key_feedback_flash_visibility_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.key_feedback_semantic_map, key_semantic_packet.key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.key_feedback_broad_owner_map, key_branch_packet.key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE) == 0);
    CHECK(memcmp(split_runtime_sync_remote.key_feedback_tap_branch_map, key_branch_packet.key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE) == 0);
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
    test_key_feedback_visibility_is_ignored_without_flashing_semantics();
    test_key_feedback_visibility_changes_when_flashing_semantics_are_present();
    test_locked_pd_mode_zeroes_automouse_progress();
    test_tick_sends_only_base_packet_when_only_automouse_changes();
    test_tick_sends_only_combo_packet_when_only_combo_feedback_changes();
    test_tick_sends_only_key_feedback_packet_when_only_key_feedback_changes();
    test_tick_sends_only_key_feedback_branch_packet_when_only_tap_branch_changes();
    test_tick_sends_only_key_feedback_branch_packet_when_only_broad_owner_changes();
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    test_tick_sends_base_packet_when_only_pd_owner_bitmap_changes();
#endif
    test_idle_packets_use_idle_heartbeat();
    test_slave_rpcs_apply_exact_remote_state();
    test_slave_base_rpc_ignores_short_packets();

    puts("split_runtime_sync host tests passed");
    return 0;
}

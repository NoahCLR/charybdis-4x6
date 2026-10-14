#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host_runtime_reset_fixture.h"
#include "transactions.h"
#include "users/noah/lib/key/runtime/feedback.h"
#include "users/noah/lib/rgb/automouse/rgb_automouse.h"
#include "users/noah/lib/split/runtime_sync.h"
#include "users/noah/lib/rgb/lighting/lighting.h"

void test_split_peer_reset(void);
void test_split_peer_deliver(const split_runtime_lighting_facts_packet_t *packet);
bool test_split_peer_read(uint8_t *frame);

static host_runtime_fixture_t runtime_fixture = HOST_RUNTIME_FIXTURE_INIT;

#define fake_time32 runtime_fixture.time32
#define fake_is_master runtime_fixture.is_master

static uint16_t          fake_auto_mouse_elapsed;
static bool              fake_auto_mouse_active;
static bool              fake_auto_mouse_enabled;
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
static uint8_t           combo_underlay_read_count;
static uint8_t           combo_overlay_read_count;
static uint8_t           pd_owner_bitmap_read_count;
static uint8_t           key_feedback_semantic_read_count;
static uint8_t           key_feedback_broad_owner_read_count;
static uint8_t           key_feedback_tap_branch_read_count;
static uint8_t           key_feedback_flash_visibility_read_count;
static uint16_t          timer_read_count;
static uint16_t          timer_elapsed_count;
static uint16_t          timer_read32_count;
static uint16_t          timer_elapsed32_count;
static uint16_t          auto_mouse_elapsed_read_count;
static uint16_t          auto_mouse_elapsed_at_read_count;
static uint16_t          auto_mouse_elapsed_at_last_now;

static bool rpc_inject_during_registration;
static uint8_t                                      rpc_register_count;
static int8_t                                       rpc_registered_ids[4];
static slave_callback_t                             rpc_registered_callbacks[4];
static uint8_t                          rpc_send_count;
static uint8_t                          rpc_send_count_base;
static uint8_t                          rpc_send_count_lighting;
static int8_t                           rpc_last_send_id;
static uint8_t                          rpc_last_send_size;
static split_runtime_base_sync_packet_t rpc_last_base_packet;
// Every lighting chunk sent, in order, and the frame they add up to.
static split_runtime_lighting_facts_packet_t rpc_lighting_chunks[32];
static uint8_t                               rpc_lighting_frame[NOAH_LIGHTING_SYNCED_BYTES];
static bool                                  fake_rpc_send_result = true;

static uint8_t fake_lighting_identity = 1u;

enum { FRAME_BYTES = NOAH_LIGHTING_SYNCED_BYTES, CHUNKS = SPLIT_RUNTIME_SYNC_FACTS_CHUNKS };
// Synced slices in registry order: preview, combos, behaviour.
enum {
    FRAME_PREVIEW              = 0u,
    FRAME_COMBOS               = 1u,
    FRAME_VISIBLE              = FRAME_COMBOS + 2u * KEY_ORIGIN_BITMAP_SIZE,
    FRAME_SEMANTIC             = FRAME_VISIBLE + KEY_ORIGIN_BITMAP_SIZE,
    FRAME_OWNERS               = FRAME_SEMANTIC + KEY_FEEDBACK_SEMANTIC_MAP_SIZE,
    FRAME_BRANCH               = FRAME_OWNERS + KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE,
    FRAME_COMBO_IDENTITIES     = FRAME_BRANCH + KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE,
    FRAME_BEHAVIOUR_IDENTITIES = FRAME_COMBO_IDENTITIES + NOAH_LIGHTING_COMBO_SLICE_BYTES,
};
_Static_assert(FRAME_BEHAVIOUR_IDENTITIES + KEY_FEEDBACK_LIGHTING_BYTES == NOAH_LIGHTING_SYNCED_BYTES, "frame layout");

#define RPC_SEND_SCRIPT_CAPACITY 32u
static bool     rpc_send_script[RPC_SEND_SCRIPT_CAPACITY];
static uint8_t  rpc_send_script_count;
static uint8_t  rpc_send_script_index;
static uint32_t rpc_send_times[RPC_SEND_SCRIPT_CAPACITY];
static int8_t   rpc_send_ids[RPC_SEND_SCRIPT_CAPACITY];

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

static bool test_read_lighting(uint8_t *out) {
    return fake_is_master ? test_split_peer_read(out) : split_runtime_sync_remote_read_lighting_facts(out);
}

static void test_reset_stubs(void) {
    host_runtime_fixture_reset(&runtime_fixture);
    test_split_peer_reset();
    rpc_inject_during_registration = false;
    fake_auto_mouse_elapsed = 83u;
    fake_auto_mouse_active  = true;
    fake_auto_mouse_enabled = true;
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
    fake_key_preview_layer                   = 3u;
    combo_underlay_read_count                = 0;
    combo_overlay_read_count                 = 0;
    pd_owner_bitmap_read_count               = 0;
    key_feedback_semantic_read_count         = 0;
    key_feedback_broad_owner_read_count      = 0;
    key_feedback_tap_branch_read_count       = 0;
    key_feedback_flash_visibility_read_count = 0;
    timer_read_count                         = 0;
    timer_elapsed_count                      = 0;
    timer_read32_count                       = 0;
    timer_elapsed32_count                    = 0;
    auto_mouse_elapsed_read_count            = 0;
    auto_mouse_elapsed_at_read_count         = 0;
    auto_mouse_elapsed_at_last_now           = 0;
    rpc_register_count                       = 0;
    memset(rpc_registered_ids, -1, sizeof(rpc_registered_ids));
    memset(rpc_registered_callbacks, 0, sizeof(rpc_registered_callbacks));
    rpc_send_count          = 0;
    rpc_send_count_base     = 0;
    rpc_send_count_lighting = 0;
    rpc_last_send_id        = -1;
    rpc_last_send_size      = 0;
    rpc_last_base_packet    = (split_runtime_base_sync_packet_t){0};
    memset(rpc_lighting_chunks, 0, sizeof(rpc_lighting_chunks));
    memset(rpc_lighting_frame, 0, sizeof(rpc_lighting_frame));
    fake_rpc_send_result = true;
    rpc_send_script_count                 = 0u;
    rpc_send_script_index                 = 0u;
    memset(rpc_send_script, 0, sizeof(rpc_send_script));
    memset(rpc_send_times, 0, sizeof(rpc_send_times));
    memset(rpc_send_ids, -1, sizeof(rpc_send_ids));
    remote_snapshot_apply_count = 0;
    remote_snapshot_active      = 0;
    remote_snapshot_locked      = 0;
    remote_snapshot_owner_sides = SPLIT_SIDE_MASK_NONE;
    key_origin_bitmap_clear(remote_snapshot_owner_bitmap);
}

static void test_reset_builder_counts(void) {
    combo_underlay_read_count                = 0;
    combo_overlay_read_count                 = 0;
    pd_owner_bitmap_read_count               = 0;
    key_feedback_semantic_read_count         = 0;
    key_feedback_broad_owner_read_count      = 0;
    key_feedback_tap_branch_read_count       = 0;
    key_feedback_flash_visibility_read_count = 0;
}

HOST_RUNTIME_FIXTURE_DEFINE_MOD_REPORT_STUBS(runtime_fixture)

uint16_t timer_read(void) {
    timer_read_count++;
    return (uint16_t)fake_time32;
}

uint16_t timer_elapsed(uint16_t last) {
    timer_elapsed_count++;
    return (uint16_t)(timer_read() - last);
}

uint32_t timer_read32(void) {
    timer_read32_count++;
    return fake_time32;
}

uint32_t timer_elapsed32(uint32_t last) {
    timer_elapsed32_count++;
    return fake_time32 - last;
}

bool is_keyboard_master(void) {
    return fake_is_master;
}

uint16_t auto_mouse_get_time_elapsed(void) {
    auto_mouse_elapsed_read_count++;
    return fake_auto_mouse_elapsed;
}

uint16_t auto_mouse_get_time_elapsed_at(uint16_t now) {
    auto_mouse_elapsed_at_read_count++;
    auto_mouse_elapsed_at_last_now = now;
    return fake_auto_mouse_elapsed;
}

bool is_auto_mouse_active(void) {
    return fake_auto_mouse_active;
}

bool get_auto_mouse_enable(void) {
    return fake_auto_mouse_enabled;
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
    pd_owner_bitmap_read_count++;
    key_origin_bitmap_copy(out_bitmap, fake_pd_owner_bitmap);
    return key_origin_bitmap_has_any(fake_pd_owner_bitmap);
}

void key_feedback_flash_visibility_bitmap_for_semantic_map(const uint8_t *semantic_map, uint8_t *out_bitmap) {
    key_feedback_flash_visibility_read_count++;

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

bool key_runtime_core_preview_owner_key_pos(keypos_t *key) {
    (void)key;
    return false;
}
bool pd_mode_local_owner_key_pos_snapshot(keypos_t *key) {
    (void)key;
    return false;
}
void layer_ownership_momentary_owner_bitmap(uint8_t *out) {
    key_origin_bitmap_clear(out);
}
bool noah_qmk_combo_origin_lighting_next(keypos_t preview, keypos_t mode, const uint8_t *quiet, uint16_t *cursor, uint8_t *id, uint8_t *role, uint8_t *keys) {
    (void)preview;
    (void)mode;
    (void)quiet;
    (void)cursor;
    (void)id;
    (void)role;
    (void)keys;
    return false;
}
void key_feedback_lighting_map(uint8_t *out) {
    memset(out, 0, KEY_FEEDBACK_LIGHTING_BYTES);
    bool active = false;
    for (uint8_t i = 0; i < KEY_FEEDBACK_SEMANTIC_MAP_SIZE; i++)
        active |= fake_key_feedback_semantic_map[i] != 0u;
    if (!active) return;
    out[0]                                = fake_lighting_identity;
    out[1]                                = 3u;
    out[KEY_FEEDBACK_LIGHTING_BYTES - 3u] = 128u;
    out[KEY_FEEDBACK_LIGHTING_BYTES - 2u] = 1u;
    out[KEY_FEEDBACK_LIGHTING_BYTES - 1u] = 34u;
}

void key_feedback_semantic_map(uint8_t *out_map) {
    key_feedback_semantic_read_count++;
    memcpy(out_map, fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
}

void key_feedback_tap_branch_map(uint8_t *out_map) {
    key_feedback_tap_branch_read_count++;
    memcpy(out_map, fake_key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
}

void key_feedback_broad_owner_map(uint8_t *out_map) {
    key_feedback_broad_owner_read_count++;
    memcpy(out_map, fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
}

void combo_feedback_underlay_bitmap(uint8_t *out_bitmap) {
    combo_underlay_read_count++;
    key_origin_bitmap_copy(out_bitmap, fake_combo_underlay_bitmap);
}

void combo_feedback_overlay_bitmap(uint8_t *out_bitmap) {
    combo_overlay_read_count++;
    key_origin_bitmap_copy(out_bitmap, fake_combo_overlay_bitmap);
}

void combo_feedback_bitmaps(uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    combo_underlay_read_count++;
    combo_overlay_read_count++;
    key_origin_bitmap_copy(out_underlay_bitmap, fake_combo_underlay_bitmap);
    key_origin_bitmap_copy(out_overlay_bitmap, fake_combo_overlay_bitmap);
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
    if (rpc_inject_during_registration) {
        split_runtime_sync_remote_t before = split_runtime_sync_remote;
        split_runtime_lighting_facts_packet_t lighting = {.sequence=77u,.chunk=SPLIT_RUNTIME_SYNC_FACTS_COMMIT};
        split_runtime_base_sync_packet_t base = {.active_mode_id=0u};
        callback(transaction_id == PUT_SPLIT_LIGHTING_FACTS_SYNC ? sizeof(lighting) : sizeof(base),
                 transaction_id == PUT_SPLIT_LIGHTING_FACTS_SYNC ? (const void *)&lighting : (const void *)&base,0u,NULL);
        CHECK(memcmp(&before,&split_runtime_sync_remote,sizeof(before)) == 0);
    }
}

bool transaction_rpc_send(int8_t transaction_id, uint8_t initiator2target_buffer_size, const void *initiator2target_buffer) {
    bool result = fake_rpc_send_result;

    CHECK(rpc_send_count < RPC_SEND_SCRIPT_CAPACITY);
    rpc_send_times[rpc_send_count] = fake_time32;
    rpc_send_ids[rpc_send_count]   = transaction_id;
    rpc_send_count++;
    rpc_last_send_id   = transaction_id;
    rpc_last_send_size = initiator2target_buffer_size;

    if (transaction_id == PUT_SPLIT_RUNTIME_BASE_SYNC) {
        rpc_send_count_base++;
        CHECK(initiator2target_buffer_size == sizeof(split_runtime_base_sync_packet_t));
        memcpy(&rpc_last_base_packet, initiator2target_buffer, sizeof(rpc_last_base_packet));
    } else if (transaction_id == PUT_SPLIT_LIGHTING_FACTS_SYNC) {
        const split_runtime_lighting_facts_packet_t *chunk = initiator2target_buffer;
        uint8_t                                      index = (uint8_t)(chunk->chunk & SPLIT_RUNTIME_SYNC_FACTS_INDEX_MASK);
        size_t                                       at    = (size_t)index * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES;

        CHECK(initiator2target_buffer_size == sizeof(split_runtime_lighting_facts_packet_t));
        CHECK(index < CHUNKS && rpc_send_count_lighting < 32u);
        rpc_lighting_chunks[rpc_send_count_lighting++] = *chunk;
        memcpy(&rpc_lighting_frame[at], chunk->bytes, FRAME_BYTES - at < SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES ? FRAME_BYTES - at : SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES);
    } else {
        CHECK(false);
    }

    if (rpc_send_script_index < rpc_send_script_count) {
        result = rpc_send_script[rpc_send_script_index++];
    }

    return result;
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
    rpc_send_count          = 0;
    rpc_send_count_base     = 0;
    rpc_send_count_lighting = 0;
    memset(rpc_send_times, 0, sizeof(rpc_send_times));
    memset(rpc_send_ids, -1, sizeof(rpc_send_ids));
}

static void test_set_rpc_send_script(const bool *results, uint8_t count) {
    CHECK(count <= RPC_SEND_SCRIPT_CAPACITY);
    if (count > 0u) {
        CHECK(results != NULL);
        memcpy(rpc_send_script, results, count * sizeof(results[0]));
    }
    rpc_send_script_count = count;
    rpc_send_script_index = 0u;
}

static void test_reset_timer_counts(void) {
    timer_read_count                 = 0u;
    timer_elapsed_count              = 0u;
    timer_read32_count               = 0u;
    timer_elapsed32_count            = 0u;
    auto_mouse_elapsed_read_count    = 0u;
    auto_mouse_elapsed_at_read_count = 0u;
    auto_mouse_elapsed_at_last_now   = 0u;
}

static void test_set_idle_runtime_state(void) {
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
}


// The frame the USB half would build from the fakes now.
static void test_expected_frame(uint8_t *frame) {
    noah_lighting_facts_build_synced(frame, true);
}

// The lighting chunks of the last pass: which indices went out, and whether
// the last one committed.
static uint16_t test_lighting_chunk_mask(void) {
    uint16_t mask = 0u;
    for (uint8_t i = 0u; i < rpc_send_count_lighting; i++) mask |= (uint16_t)(1u << (rpc_lighting_chunks[i].chunk & SPLIT_RUNTIME_SYNC_FACTS_INDEX_MASK));
    return mask;
}

static bool test_last_chunk_commits(void) {
    if (!rpc_send_count_lighting) return false;
    for (uint8_t i = 0u; i + 1u < rpc_send_count_lighting; i++) {
        if (rpc_lighting_chunks[i].chunk & SPLIT_RUNTIME_SYNC_FACTS_COMMIT) return false;
    }
    return (rpc_lighting_chunks[rpc_send_count_lighting - 1u].chunk & SPLIT_RUNTIME_SYNC_FACTS_COMMIT) != 0u;
}

// Replays the chunks the master sent into the other half's RPC handler.
static void test_deliver_lighting_chunks(slave_callback_t callback) {
    (void)callback;
    for (uint8_t i = 0u; i < rpc_send_count_lighting; i++) test_split_peer_deliver(&rpc_lighting_chunks[i]);
}

// Which chunks hold bytes that differ between two frames.
static uint16_t test_changed_chunk_mask(const uint8_t *before, const uint8_t *after) {
    uint16_t mask = 0u;
    for (uint16_t i = 0u; i < FRAME_BYTES; i++) {
        if (before[i] != after[i]) mask |= (uint16_t)(1u << (i / SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES));
    }
    return mask;
}

#define ALL_CHUNKS ((uint16_t)((1u << CHUNKS) - 1u))

// Sends one pass's chunk to the other half's handler.
static void test_deliver_chunk(uint8_t sequence, uint8_t index, bool commit, const uint8_t *frame) {
    split_runtime_lighting_facts_packet_t packet = {.sequence = sequence, .chunk = (uint8_t)(index | (commit ? SPLIT_RUNTIME_SYNC_FACTS_COMMIT : 0u))};
    size_t                                at     = (size_t)index * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES;

    memcpy(packet.bytes, &frame[at], FRAME_BYTES - at < SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES ? FRAME_BYTES - at : SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES);
    test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC)(sizeof(packet), &packet, 0u, NULL);
}

static void test_deliver_frame(uint8_t sequence, const uint8_t *frame) {
    for (uint8_t index = 0u; index < CHUNKS; index++) test_deliver_chunk(sequence, index, index + 1u == CHUNKS, frame);
}

static void test_fill_frame(uint8_t *frame, uint8_t seed) {
    for (uint16_t i = 0u; i < FRAME_BYTES; i++)
        frame[i] = (uint8_t)(seed + i * 7u);
}

static void test_init_registers_rpcs_and_sends_initial_packets_on_master(void) {
    uint8_t expected[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(sizeof(split_runtime_base_sync_packet_t) == (size_t)(5u + KEY_ORIGIN_BITMAP_SIZE));
#else
    CHECK(sizeof(split_runtime_base_sync_packet_t) == 4u);
#endif
    CHECK(sizeof(split_runtime_lighting_facts_packet_t) == 32u);
    // The stub matrix is 8x8; the frame still spans several chunks.
    CHECK(CHUNKS == (FRAME_BYTES + SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES - 1u) / SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES && CHUNKS >= 3u);

    split_runtime_sync_init();

    CHECK(rpc_register_count == 2u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC) != NULL);
    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_ids[0] == PUT_SPLIT_RUNTIME_BASE_SYNC);
    CHECK(rpc_last_base_packet.automouse_progress == 60u);
    CHECK(rpc_last_base_packet.active_mode_id == pd_mode_id_from_mask(fake_pd_active_flags));
    CHECK(rpc_last_base_packet.locked_mode_id == pd_mode_id_from_mask(fake_pd_locked_flags));
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    CHECK(rpc_last_base_packet.pd_mode_owner_sides == fake_pd_owner_sides);
    CHECK(memcmp(rpc_last_base_packet.pd_mode_owner_bitmap, fake_pd_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
#endif
    // One pass: every chunk, in order, under one sequence, the last committing.
    CHECK(rpc_send_count_lighting == CHUNKS);
    CHECK(test_lighting_chunk_mask() == ALL_CHUNKS);
    CHECK(test_last_chunk_commits());
    for (uint8_t i = 0u; i < CHUNKS; i++) {
        CHECK((rpc_lighting_chunks[i].chunk & SPLIT_RUNTIME_SYNC_FACTS_INDEX_MASK) == i);
        CHECK(rpc_lighting_chunks[i].sequence == rpc_lighting_chunks[0].sequence);
    }
    test_expected_frame(expected);
    CHECK(memcmp(rpc_lighting_frame, expected, FRAME_BYTES) == 0);
    CHECK(rpc_lighting_frame[FRAME_PREVIEW] == fake_key_preview_layer);
    CHECK(memcmp(&rpc_lighting_frame[FRAME_COMBOS + KEY_ORIGIN_BITMAP_SIZE], fake_combo_overlay_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    CHECK(memcmp(&rpc_lighting_frame[FRAME_SEMANTIC], fake_key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE) == 0);
    CHECK(memcmp(&rpc_lighting_frame[FRAME_OWNERS], fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE) == 0);
    CHECK(memcmp(&rpc_lighting_frame[FRAME_VISIBLE], fake_key_feedback_flash_visibility_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
    // The USB half never reads its own link.
    CHECK(!test_read_lighting(read));
}

static void test_init_registers_rpcs_without_sending_on_slave(void) {
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();

    CHECK(rpc_register_count == 2u);
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);
    CHECK(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC) != NULL);
    CHECK(rpc_send_count == 0u);
    CHECK(!test_read_lighting(read));
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
    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(rpc_send_count_base == 1u);
    CHECK(test_lighting_chunk_mask() == ALL_CHUNKS);
    CHECK(test_last_chunk_commits());
}

static void test_tick_samples_one_shared_timestamp(void) {
    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_timer_counts();

    split_runtime_sync_tick();

    CHECK(timer_read32_count == 1u);
    CHECK(timer_elapsed32_count == 0u);
    CHECK(timer_read_count == 0u);
    CHECK(timer_elapsed_count == 0u);
    CHECK(auto_mouse_elapsed_read_count == 0u);
    CHECK(auto_mouse_elapsed_at_read_count == 1u);
    CHECK(auto_mouse_elapsed_at_last_now == (uint16_t)fake_time32);
}

static void test_force_sync_samples_one_shared_timestamp(void) {
    split_runtime_sync_debug_clock_t clock;

    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_timer_counts();

    split_runtime_sync();

    CHECK(timer_read32_count == 1u);
    CHECK(timer_elapsed32_count == 0u);
    CHECK(timer_read_count == 0u);
    CHECK(timer_elapsed_count == 0u);
    CHECK(auto_mouse_elapsed_read_count == 0u);
    CHECK(auto_mouse_elapsed_at_read_count == 1u);
    CHECK(rpc_send_count >= 1u + CHUNKS);
    split_runtime_sync_debug_clock_snapshot(&clock);
    CHECK(clock.base_last_send == fake_time32);
    CHECK(clock.lighting_facts_last_send == fake_time32);
}

// The timeout fade is exactly the window where the pointer is idle: QMK
// recomputes is_auto_mouse_active() from each mouse report, so it goes false as
// soon as movement stops while the activity timer keeps counting toward the
// timeout. The slave mirrors this progress, so gating the read on "active"
// blanked the slave's fade while the master, which reads the clock directly,
// kept fading. Progress must be published for an idle pointer.
static void test_idle_auto_mouse_still_publishes_fade_progress(void) {
    test_reset_stubs();
    fake_auto_mouse_active  = false;
    fake_auto_mouse_elapsed = (uint16_t)(AUTOMOUSE_RGB_DEAD_TIME + AUTOMOUSE_RGB_SYNC_STEP * 2u);

    split_runtime_sync_init();

    CHECK(auto_mouse_elapsed_at_read_count >= 1u);
    CHECK(rpc_last_base_packet.automouse_progress != 0u);
    CHECK(rpc_last_base_packet.automouse_progress == automouse_rgb_quantize_progress(fake_auto_mouse_elapsed));
}

// A locked pd mode still suppresses the fade, and that decision stays in the
// packet builder rather than in the elapsed read.
static void test_locked_mode_suppresses_fade_progress(void) {
    test_reset_stubs();
    fake_auto_mouse_active  = false;
    fake_any_mode_locked    = true;
    fake_auto_mouse_elapsed = (uint16_t)(AUTOMOUSE_RGB_DEAD_TIME + AUTOMOUSE_RGB_SYNC_STEP * 2u);

    split_runtime_sync_init();

    CHECK(rpc_last_base_packet.automouse_progress == 0u);
}

// Auto-mouse turned off sends no fade to the other half. QMK zeroes its timer
// when disabled, which would otherwise read as a finished timeout and park the
// slave's pointer layer at the fade destination.
static void test_disabled_auto_mouse_publishes_no_fade_progress(void) {
    test_reset_stubs();
    fake_auto_mouse_enabled = false;
    fake_auto_mouse_active  = false;
    fake_auto_mouse_elapsed = AUTO_MOUSE_TIME;

    split_runtime_sync_init();

    CHECK(rpc_last_base_packet.automouse_progress == 0u);
}

static void test_active_auto_mouse_uses_shared_low16_after_wrap(void) {
    test_reset_stubs();
    fake_time32 = UINT32_MAX - 2u;
    split_runtime_sync_init();
    test_reset_timer_counts();

    fake_time32 += 5u;
    split_runtime_sync_tick();

    CHECK(timer_read32_count == 1u);
    CHECK(timer_read_count == 0u);
    CHECK(timer_elapsed_count == 0u);
    CHECK(auto_mouse_elapsed_read_count == 0u);
    CHECK(auto_mouse_elapsed_at_read_count == 1u);
    CHECK(auto_mouse_elapsed_at_last_now == 2u);
}

static void test_active_heartbeat_is_wrap_safe(void) {
    test_reset_stubs();
    fake_time32 = UINT32_MAX - 100u;
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_reset_timer_counts();

    fake_time32 += 249u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);
    CHECK(timer_read32_count == 1u);
    CHECK(timer_elapsed32_count == 0u);

    test_reset_timer_counts();
    fake_time32 += 1u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(timer_read32_count == 1u);
    CHECK(timer_elapsed32_count == 0u);
}

static void test_first_failure_stops_tick_and_force_respects_backoff(void) {
    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_reset_builder_counts();
    fake_rpc_send_result = false;

    split_runtime_sync();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_lighting == 0u);

    test_reset_rpc_send_counts();
    test_reset_builder_counts();
    test_reset_timer_counts();
    split_runtime_sync();

    CHECK(rpc_send_count == 0u);
    CHECK(timer_read32_count == 1u);
    CHECK(auto_mouse_elapsed_at_read_count == 0u);
    CHECK(pd_owner_bitmap_read_count == 0u);
    CHECK(combo_underlay_read_count == 0u);
    CHECK(combo_overlay_read_count == 0u);
    CHECK(key_feedback_semantic_read_count == 0u);
    CHECK(key_feedback_broad_owner_read_count == 0u);
    CHECK(key_feedback_tap_branch_read_count == 0u);
}

static void test_retry_schedule_caps_and_is_wrap_safe(void) {
    static const uint32_t                expected_delays[] = {50u, 100u, 200u, 400u, 800u, 1000u, 1000u};
    split_runtime_sync_debug_transport_t transport;

    test_reset_stubs();
    fake_time32 = UINT32_MAX - 20u;
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    fake_rpc_send_result = false;

    split_runtime_sync();
    CHECK(rpc_send_count == 1u);
    split_runtime_sync_debug_transport_snapshot(&transport);
    CHECK(transport.backoff_active);
    CHECK(transport.consecutive_failures == 1u);
    CHECK(transport.retry_delay_ms == expected_delays[0]);

    for (uint8_t index = 0u; index < (uint8_t)(sizeof(expected_delays) / sizeof(expected_delays[0])); index++) {
        uint8_t  next_index = index + 1u < (uint8_t)(sizeof(expected_delays) / sizeof(expected_delays[0])) ? (uint8_t)(index + 1u) : index;
        uint32_t delay      = expected_delays[index];

        test_reset_rpc_send_counts();
        fake_time32 += delay - 1u;
        split_runtime_sync_tick();
        CHECK(rpc_send_count == 0u);

        fake_time32 += 1u;
        split_runtime_sync_tick();
        CHECK(rpc_send_count == 1u);
        CHECK(rpc_send_count_base == 1u);
        CHECK(rpc_send_times[0] == fake_time32);
        split_runtime_sync_debug_transport_snapshot(&transport);
        CHECK(transport.backoff_active);
        CHECK(transport.retry_delay_ms == expected_delays[next_index]);
    }
}

// A pass that fails part-way commits nothing on the other half; once the link
// recovers, the next pass resends every chunk of the latest frame.
static void test_failed_lighting_pass_resends_the_whole_latest_frame(void) {
    static const bool                    fail_second_chunk[] = {true, true, false};
    split_runtime_sync_debug_transport_t transport;
    uint8_t                              expected[FRAME_BYTES];
    uint8_t                              read[FRAME_BYTES];

    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    fake_combo_underlay_bitmap[0] = 0x31u;
    split_runtime_sync_mark_combo_dirty();
    test_set_rpc_send_script(fail_second_chunk, (uint8_t)(sizeof(fail_second_chunk) / sizeof(fail_second_chunk[0])));

    split_runtime_sync();

    CHECK(rpc_send_count == 3u);
    CHECK(rpc_send_ids[0] == PUT_SPLIT_RUNTIME_BASE_SYNC);
    CHECK(rpc_send_ids[1] == PUT_SPLIT_LIGHTING_FACTS_SYNC);
    CHECK(rpc_send_ids[2] == PUT_SPLIT_LIGHTING_FACTS_SYNC);
    CHECK(!test_last_chunk_commits());
    // The chunk that did arrive stays staged and is never published.
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    CHECK(!test_read_lighting(read));

    fake_combo_underlay_bitmap[0] = 0x72u;
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    key_feedback_tap_branch_map_set(fake_key_feedback_tap_branch_map, (keypos_t){.row = 1, .col = 1}, 3u);
    split_runtime_sync_mark_key_feedback_dirty();
    test_reset_rpc_send_counts();
    test_set_rpc_send_script(NULL, 0u);
    fake_rpc_send_result = true;

    fake_time32 += 49u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);

    fake_time32 += 1u;
    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(rpc_send_ids[0] == PUT_SPLIT_RUNTIME_BASE_SYNC);
    CHECK(test_lighting_chunk_mask() == ALL_CHUNKS);
    CHECK(test_last_chunk_commits());
    test_expected_frame(expected);
    CHECK(memcmp(rpc_lighting_frame, expected, FRAME_BYTES) == 0);
    CHECK(rpc_lighting_frame[FRAME_COMBOS] == 0x72u);
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, expected, FRAME_BYTES) == 0);
    split_runtime_sync_debug_transport_snapshot(&transport);
    CHECK(!transport.backoff_active);
    CHECK(transport.consecutive_failures == 0u);
    CHECK(transport.retry_delay_ms == 0u);

    test_reset_rpc_send_counts();
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);
}

static void test_active_to_idle_clear_survives_outage(void) {
    uint8_t idle[FRAME_BYTES];

    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_set_idle_runtime_state();
    fake_rpc_send_result = false;

    split_runtime_sync_mark_key_feedback_dirty();
    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);

    test_reset_rpc_send_counts();
    fake_rpc_send_result = true;
    fake_time32 += 50u;
    split_runtime_sync_tick();

    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_last_base_packet.automouse_progress == 0u);
    CHECK(rpc_last_base_packet.active_mode_id == PD_MODE_ID_NONE);
    CHECK(rpc_last_base_packet.locked_mode_id == PD_MODE_ID_NONE);
    CHECK(test_last_chunk_commits());
    noah_lighting_facts_build_idle(idle);
    CHECK(memcmp(rpc_lighting_frame, idle, FRAME_BYTES) == 0);
    CHECK(rpc_lighting_frame[FRAME_PREVIEW] == UINT8_MAX);
}

static void test_role_reinitialization_clears_transport_backoff(void) {
    split_runtime_sync_debug_transport_t transport;

    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    fake_rpc_send_result = false;
    split_runtime_sync();
    split_runtime_sync_debug_transport_snapshot(&transport);
    CHECK(transport.backoff_active);

    rpc_register_count = 0u;
    fake_is_master     = false;
    split_runtime_sync_init();
    split_runtime_sync_debug_transport_snapshot(&transport);
    CHECK(!transport.backoff_active);
    CHECK(transport.consecutive_failures == 0u);
    CHECK(rpc_send_count == 1u);

    rpc_register_count   = 0u;
    fake_is_master       = true;
    fake_rpc_send_result = true;
    test_reset_rpc_send_counts();
    split_runtime_sync_init();
    CHECK(rpc_send_count == 1u + CHUNKS);
}

static void test_force_sync_sends_all_packets_even_when_unchanged(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();

    split_runtime_sync();

    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(rpc_send_count_base == 1u);
    CHECK(test_lighting_chunk_mask() == ALL_CHUNKS);
    CHECK(test_last_chunk_commits());
}

static void test_key_feedback_visibility_is_ignored_without_flashing_semantics(void) {
    test_reset_stubs();
    key_feedback_semantic_map_clear(fake_key_feedback_semantic_map);
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);
    key_origin_bitmap_add_keypos(fake_key_feedback_flash_visibility_bitmap, (keypos_t){.row = 1, .col = 1});

    split_runtime_sync_init();
    CHECK(!key_origin_bitmap_has_any(&rpc_lighting_frame[FRAME_VISIBLE]));

    test_reset_rpc_send_counts();
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);
}

static void test_key_feedback_visibility_changes_when_flashing_semantics_are_present(void) {
    test_reset_stubs();

    split_runtime_sync_init();
    CHECK(key_origin_bitmap_has_keypos(&rpc_lighting_frame[FRAME_VISIBLE], (keypos_t){.row = 0, .col = 0}));

    test_reset_rpc_send_counts();
    key_origin_bitmap_clear(fake_key_feedback_flash_visibility_bitmap);

    split_runtime_sync_tick();
    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(test_lighting_chunk_mask() == (uint16_t)(1u << (FRAME_VISIBLE / SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES)));
    CHECK(test_last_chunk_commits());
    CHECK(!key_origin_bitmap_has_any(&rpc_lighting_frame[FRAME_VISIBLE]));
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
    fake_auto_mouse_elapsed = 91u;

    split_runtime_sync_tick();

    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 1u);
    CHECK(rpc_send_count_lighting == 0u);
    CHECK(rpc_last_base_packet.automouse_progress == 70u);
}

// One change sends exactly the chunks it touched, the last committing.
static void test_tick_sends_exactly_the_changed_chunks(void (*change)(void)) {
    uint8_t before[FRAME_BYTES];
    uint8_t after[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    split_runtime_sync_init();
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    test_expected_frame(before);
    test_reset_rpc_send_counts();

    change();
    split_runtime_sync_tick();

    test_expected_frame(after);
    CHECK(test_changed_chunk_mask(before, after) != 0u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(test_lighting_chunk_mask() == test_changed_chunk_mask(before, after));
    CHECK(test_last_chunk_commits());
    CHECK(memcmp(rpc_lighting_frame, after, FRAME_BYTES) == 0);
    // The other half merges the changed chunks into the frame it holds.
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, after, FRAME_BYTES) == 0);
}

static void change_combo_underlay(void) {
    fake_combo_underlay_bitmap[0] = 0x01u;
}

static void change_preview_layer(void) {
    fake_key_preview_layer = 5u;
}

static void change_semantic(void) {
    key_feedback_semantic_map_set(fake_key_feedback_semantic_map, (keypos_t){.row = 1, .col = 1}, KEY_FEEDBACK_SEMANTIC_TAP_BRANCH_PENDING);
}

static void change_tap_branch(void) {
    key_feedback_tap_branch_map_set(fake_key_feedback_tap_branch_map, (keypos_t){.row = 7, .col = 5}, 3u);
}

static void change_broad_owner(void) {
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_GLOBAL, (keypos_t){.row = 1, .col = 1});
    key_feedback_broad_owner_map_set(fake_key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_LEFT_HALF, (keypos_t){.row = 1, .col = 1});
}

// Resolving a layer hold clears both visual substages, while the output itself
// remains held on the master. Empty bitmaps must be sent and replace the peer's
// previous footprint, even without a separate dirty notification.
static void test_quiet_combo_projection_clears_peer_feedback(void) {
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    key_origin_bitmap_add_keypos(fake_combo_underlay_bitmap, (keypos_t){.row = 0, .col = 0});
    key_origin_bitmap_add_keypos(fake_combo_overlay_bitmap, (keypos_t){.row = 4, .col = 0});
    split_runtime_sync_init();
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    CHECK(test_read_lighting(read));
    CHECK(key_origin_bitmap_has_any(&read[FRAME_COMBOS]) && key_origin_bitmap_has_any(&read[FRAME_COMBOS + KEY_ORIGIN_BITMAP_SIZE]));
    test_reset_rpc_send_counts();

    key_origin_bitmap_clear(fake_combo_underlay_bitmap);
    key_origin_bitmap_clear(fake_combo_overlay_bitmap);
    split_runtime_sync_tick();
    CHECK(rpc_send_count_lighting >= 1u);
    CHECK(test_last_chunk_commits());
    test_deliver_lighting_chunks(test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC));
    CHECK(test_read_lighting(read));
    CHECK(!key_origin_bitmap_has_any(&read[FRAME_COMBOS]) && !key_origin_bitmap_has_any(&read[FRAME_COMBOS + KEY_ORIGIN_BITMAP_SIZE]));
}

static void test_idle_combo_feedback_activation_sends_without_dirty_notification(void) {
    test_reset_stubs();
    test_set_idle_runtime_state();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_reset_builder_counts();

    fake_combo_underlay_bitmap[0] = 0x02u;

    split_runtime_sync_tick();

    // Only the combo slice is built: the behaviour slice is at rest and clean.
    CHECK(combo_underlay_read_count == 1u);
    CHECK(combo_overlay_read_count == 1u);
    CHECK(key_feedback_semantic_read_count == 0u);
    CHECK(rpc_send_count == 1u);
    CHECK(rpc_send_count_base == 0u);
    CHECK(rpc_send_count_lighting == 1u);
    CHECK(test_last_chunk_commits());
    CHECK(rpc_lighting_frame[FRAME_COMBOS] == 0x02u);
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
    CHECK(rpc_send_count_lighting == 0u);
    CHECK(memcmp(rpc_last_base_packet.pd_mode_owner_bitmap, fake_pd_owner_bitmap, KEY_ORIGIN_BITMAP_SIZE) == 0);
}
#endif

static void test_idle_packets_use_idle_heartbeat(void) {
    test_reset_stubs();
    test_set_idle_runtime_state();

    split_runtime_sync_init();
    CHECK(!key_origin_bitmap_has_any(&rpc_lighting_frame[FRAME_VISIBLE]));

    test_reset_rpc_send_counts();

    fake_time32 += 999u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 0u);

    fake_time32 += 1u;
    split_runtime_sync_tick();
    CHECK(rpc_send_count == 1u + CHUNKS);
    CHECK(rpc_send_count_base == 1u);
    CHECK(test_lighting_chunk_mask() == ALL_CHUNKS);
}

// The behaviour slice is gated: at rest and clean it is not rebuilt, so idle
// scans do not derive unchanged key-feedback maps. The ungated slices are
// built every pass.
static void test_idle_tick_keeps_combo_immediate_and_skips_key_feedback_builders_until_due_or_dirty(void) {
    test_reset_stubs();
    test_set_idle_runtime_state();

    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_reset_builder_counts();

    fake_time32 += 999u;
    split_runtime_sync_tick();

    CHECK(rpc_send_count == 0u);
    CHECK(pd_owner_bitmap_read_count == 0u);
    CHECK(combo_underlay_read_count == 1u);
    CHECK(combo_overlay_read_count == 1u);
    CHECK(key_feedback_semantic_read_count == 0u);
    CHECK(key_feedback_flash_visibility_read_count == 0u);
    CHECK(key_feedback_broad_owner_read_count == 0u);
    CHECK(key_feedback_tap_branch_read_count == 0u);

    // Dirty rebuilds the behaviour slice once.
    split_runtime_sync_mark_key_feedback_dirty();
    split_runtime_sync_tick();

    CHECK(rpc_send_count == 0u);
    CHECK(key_feedback_semantic_read_count == 1u);
    CHECK(key_feedback_flash_visibility_read_count == 1u);
    CHECK(key_feedback_broad_owner_read_count == 1u);
    CHECK(key_feedback_tap_branch_read_count == 1u);
    CHECK(combo_underlay_read_count == 2u);

    split_runtime_sync_tick();
    CHECK(key_feedback_semantic_read_count == 1u);
    CHECK(combo_underlay_read_count == 3u);
}

// While the behaviour slice is away from rest it is rebuilt every pass, so a
// change without a dirty notification still reaches the other half.
static void test_active_behaviour_slice_rebuilds_every_pass(void) {
    test_reset_stubs();
    split_runtime_sync_init();
    test_reset_rpc_send_counts();
    test_reset_builder_counts();

    split_runtime_sync_tick();
    CHECK(key_feedback_semantic_read_count == 1u);
    split_runtime_sync_tick();
    CHECK(key_feedback_semantic_read_count == 2u);
    CHECK(rpc_send_count == 0u);
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
    };
    uint8_t frame[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    test_fill_frame(frame, 0x11u);

    split_runtime_sync_init();
    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(base_packet), &base_packet, 0u, NULL);
    test_deliver_frame(9u, frame);

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
    CHECK(remote_snapshot_apply_count == 1u);
    CHECK(remote_snapshot_active == pd_mode_mask_from_id(base_packet.active_mode_id));
    CHECK(remote_snapshot_locked == pd_mode_mask_from_id(base_packet.locked_mode_id));
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, frame, FRAME_BYTES) == 0);
    noah_lighting_facts_t resolved = {0};
    CHECK(split_runtime_sync_remote_collect_lighting(&resolved));
    noah_lighting_facts_pack_synced(&resolved, read);
    CHECK(memcmp(read, frame, FRAME_BYTES) == 0);
    CHECK(!split_runtime_sync_remote_collect_lighting(&resolved));
}

static void test_slave_base_rpc_ignores_short_packets(void) {
    split_runtime_base_sync_packet_t packet = {
        .automouse_progress = 12u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ZOOM),
    };

    test_reset_stubs();
    fake_is_master = false;

    split_runtime_sync_init();
    CHECK(test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC) != NULL);

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)((uint8_t)(sizeof(packet) - 1u), &packet, 0u, NULL);

    CHECK(split_runtime_sync_remote.automouse_progress == 0u);
    CHECK(remote_snapshot_apply_count == 0u);
}

// Chunks stage until the one that commits; only then does the frame publish.
static void test_slave_publishes_a_frame_only_on_its_commit(void) {
    uint8_t first[FRAME_BYTES];
    uint8_t second[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    test_fill_frame(first, 0x20u);
    test_fill_frame(second, 0x90u);

    for (uint8_t index = 0u; index + 1u < CHUNKS; index++) test_deliver_chunk(1u, index, false, first);
    CHECK(!test_read_lighting(read));
    test_deliver_chunk(1u, CHUNKS - 1u, true, first);
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, first, FRAME_BYTES) == 0);

    // A pass of one changed chunk keeps the others as committed.
    test_deliver_chunk(2u, 0u, false, second);
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, first, FRAME_BYTES) == 0);
    test_deliver_chunk(2u, 1u, true, second);
    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, second, 2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
    CHECK(memcmp(&read[2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], &first[2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], FRAME_BYTES - 2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
}

// A new sequence abandons an uncommitted pass: its chunks never publish.
static void test_slave_drops_an_abandoned_pass(void) {
    uint8_t first[FRAME_BYTES];
    uint8_t abandoned[FRAME_BYTES];
    uint8_t third[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    test_fill_frame(first, 0x01u);
    test_fill_frame(abandoned, 0x55u);
    test_fill_frame(third, 0xA0u);
    test_deliver_frame(1u, first);

    test_deliver_chunk(2u, 0u, false, abandoned);
    test_deliver_chunk(3u, 1u, true, third);

    CHECK(test_read_lighting(read));
    CHECK(memcmp(read, first, SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
    CHECK(memcmp(&read[SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], &third[SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
}

// Before its first commit the other half holds the frame at rest, so a first
// pass of only some chunks fills the rest from rest, not from zeros.
static void test_slave_first_partial_pass_starts_from_rest(void) {
    uint8_t idle[FRAME_BYTES];
    uint8_t frame[FRAME_BYTES];
    uint8_t read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    noah_lighting_facts_build_idle(idle);
    test_fill_frame(frame, 0x33u);

    test_deliver_chunk(4u, 1u, true, frame);

    CHECK(test_read_lighting(read));
    CHECK(read[FRAME_PREVIEW] == UINT8_MAX);
    CHECK(memcmp(read, idle, SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
    CHECK(memcmp(&read[SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], &frame[SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
    CHECK(memcmp(&read[2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], &idle[2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES], FRAME_BYTES - 2u * SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES) == 0);
}

static void test_slave_ignores_malformed_lighting_chunks(void) {
    split_runtime_lighting_facts_packet_t packet = {.sequence = 1u, .chunk = (uint8_t)(0u | SPLIT_RUNTIME_SYNC_FACTS_COMMIT)};
    uint8_t                               read[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    memset(packet.bytes, 0x5A, sizeof(packet.bytes));

    test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC)((uint8_t)(sizeof(packet) - 1u), &packet, 0u, NULL);
    CHECK(!test_read_lighting(read));

    packet.chunk = (uint8_t)(CHUNKS | SPLIT_RUNTIME_SYNC_FACTS_COMMIT);
    test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC)(sizeof(packet), &packet, 0u, NULL);
    CHECK(!test_read_lighting(read));
}

// ─── Publication interleaving ───────────────────────────────────────────────
//
// Slave RPC callbacks publish from the split worker context, so these tests
// drive the publish seam to run a main-context read while a packet is only
// half stored. Every case asserts three things: the seam really sits on a
// partially stored domain, the reader refuses that in-flight generation, and
// the reader's destination still holds the complete previous packet.

typedef struct {
    uint8_t seam_count;
    bool    saw_generation_in_flight;
    bool    saw_partial_store;
    bool    read_coherent;
} publish_seam_observation_t;

static publish_seam_observation_t       publish_seam;
static split_runtime_base_sync_packet_t seam_first_base_packet;
static split_runtime_base_sync_packet_t seam_second_base_packet;
static uint8_t                          seam_first_frame[FRAME_BYTES];
static uint8_t                          seam_second_frame[FRAME_BYTES];
static uint8_t                          seam_read_frame[FRAME_BYTES];

static void test_publish_seam(split_runtime_sync_domain_t domain) {
    publish_seam.seam_count++;

    switch (domain) {
        case SPLIT_RUNTIME_SYNC_DOMAIN_BASE:
            publish_seam.saw_generation_in_flight = noah_runtime_publication_in_flight(split_runtime_sync_remote.base_generation);
            publish_seam.saw_partial_store        = split_runtime_sync_remote.active_mode_id == seam_second_base_packet.active_mode_id
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
                                             && split_runtime_sync_remote.pd_mode_owner_sides == seam_first_base_packet.pd_mode_owner_sides
#endif
                ;
            break;
        case SPLIT_RUNTIME_SYNC_DOMAIN_LIGHTING_FACTS:
            publish_seam.saw_generation_in_flight = noah_runtime_publication_in_flight(split_runtime_sync_remote.lighting_facts_generation);
            publish_seam.saw_partial_store        = memcmp(split_runtime_sync_remote.lighting_facts, seam_second_frame, FRAME_BYTES / 2u) == 0 && memcmp(&split_runtime_sync_remote.lighting_facts[FRAME_BYTES / 2u], &seam_first_frame[FRAME_BYTES / 2u], FRAME_BYTES - FRAME_BYTES / 2u) == 0;
            publish_seam.read_coherent            = test_read_lighting(seam_read_frame);
            break;
        default:
            break;
    }
}

static void test_begin_publish_seam_case(void) {
    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    publish_seam = (publish_seam_observation_t){0};
}

static void test_arm_publish_seam(void) {
    publish_seam = (publish_seam_observation_t){0};
    split_runtime_sync_test_set_publish_seam(test_publish_seam);
}

static void test_disarm_publish_seam(void) {
    split_runtime_sync_test_set_publish_seam(NULL);
}

static void test_lighting_facts_publication_is_atomic_for_readers(void) {
    test_begin_publish_seam_case();
    test_fill_frame(seam_first_frame, 0x10u);
    test_fill_frame(seam_second_frame, 0x80u);

    test_deliver_frame(1u, seam_first_frame);
    CHECK(test_read_lighting(seam_read_frame));
    CHECK(memcmp(seam_read_frame, seam_first_frame, FRAME_BYTES) == 0);

    test_arm_publish_seam();
    test_deliver_frame(2u, seam_second_frame);
    test_disarm_publish_seam();

    CHECK(publish_seam.seam_count == 1u);
    CHECK(publish_seam.saw_generation_in_flight);
    CHECK(publish_seam.saw_partial_store);
    CHECK(!publish_seam.read_coherent);
    CHECK(memcmp(seam_read_frame, seam_first_frame, FRAME_BYTES) == 0);

    CHECK(!noah_runtime_publication_in_flight(split_runtime_sync_remote.lighting_facts_generation));
    CHECK(test_read_lighting(seam_read_frame));
    CHECK(memcmp(seam_read_frame, seam_second_frame, FRAME_BYTES) == 0);
}

static void test_base_publication_marks_its_generation_in_flight(void) {
    test_begin_publish_seam_case();

    seam_first_base_packet = (split_runtime_base_sync_packet_t){
        .automouse_progress = 10u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_ZOOM),
        .locked_mode_id     = PD_MODE_ID_NONE,
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides = SPLIT_SIDE_MASK_LEFT,
#endif
    };
    seam_second_base_packet = (split_runtime_base_sync_packet_t){
        .automouse_progress = 20u,
        .active_mode_id     = pd_mode_id_from_mask(PD_MODE_VOLUME),
        .locked_mode_id     = PD_MODE_ID_NONE,
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
        .pd_mode_owner_sides = SPLIT_SIDE_MASK_RIGHT,
#endif
    };

    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(seam_first_base_packet), &seam_first_base_packet, 0u, NULL);
    CHECK(!noah_runtime_publication_in_flight(split_runtime_sync_remote.base_generation));

    test_arm_publish_seam();
    test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(seam_second_base_packet), &seam_second_base_packet, 0u, NULL);
    test_disarm_publish_seam();

    CHECK(publish_seam.seam_count == 1u);
    CHECK(publish_seam.saw_generation_in_flight);
    CHECK(publish_seam.saw_partial_store);
    CHECK(!noah_runtime_publication_in_flight(split_runtime_sync_remote.base_generation));
    CHECK(split_runtime_sync_remote.active_mode_id == seam_second_base_packet.active_mode_id);
}

// The helper advertises that a failed read leaves the caller's destination
// untouched, so the frame renders its previous coherent snapshot.
//
// This pins the reachable half of that contract: an in-flight publication, where
// the pre-check skips the copy entirely, and a half that has heard no frame yet.
// The other half -- exhausting the retry budget with a publication landing
// inside every attempt -- is not reachable at transport timing and cannot be
// forced without a hook in production code, so it is covered by construction
// (each attempt stages into a local and commits only after confirming the
// generation) rather than by this test.
static void test_failed_coherent_read_leaves_the_destination_untouched(void) {
    uint8_t frame[FRAME_BYTES];

    test_reset_stubs();
    fake_is_master = false;
    split_runtime_sync_init();
    memset(frame, 0xA5, sizeof(frame));
    // Fill the shared frame with a different pattern, so any write shows up.
    memset(split_runtime_sync_remote.lighting_facts, 0x11, FRAME_BYTES);

    noah_lighting_facts_t resolved, before;
    memset(&resolved,0xa5,sizeof(resolved)); before = resolved;
    split_runtime_sync_remote.lighting_facts_received = false;
    CHECK(!test_read_lighting(frame));
    CHECK(!split_runtime_sync_remote_collect_lighting(&resolved));
    CHECK(memcmp(&before,&resolved,sizeof(resolved)) == 0);

    // Odd generation means a publication is in flight for every attempt.
    split_runtime_sync_remote.lighting_facts_received   = true;
    split_runtime_sync_remote.lighting_facts_generation = 1u;
    CHECK(!test_read_lighting(frame));
    CHECK(!split_runtime_sync_remote_collect_lighting(&resolved));
    CHECK(memcmp(&before,&resolved,sizeof(resolved)) == 0);

    for (uint16_t index = 0; index < FRAME_BYTES; index++) {
        CHECK(frame[index] == 0xA5);
    }
    split_runtime_sync_remote = (split_runtime_sync_remote_t)SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;
}

static void test_role_storage_is_ready_before_callbacks(void) {
    for (unsigned role=0;role<2;role++) {
        test_reset_stubs();
        fake_is_master = (bool)role;
        rpc_inject_during_registration = true;
        split_runtime_sync_init();
        rpc_inject_during_registration = false;
        if (fake_is_master) {
            split_runtime_sync_remote_t before = split_runtime_sync_remote;
            // A wrong-role callback must reject before dereferencing even a
            // nominally full-sized packet. It cannot overwrite sender storage.
            test_registered_callback(PUT_SPLIT_LIGHTING_FACTS_SYNC)(sizeof(split_runtime_lighting_facts_packet_t),NULL,0,NULL);
            test_registered_callback(PUT_SPLIT_RUNTIME_BASE_SYNC)(sizeof(split_runtime_base_sync_packet_t),NULL,0,NULL);
            CHECK(memcmp(&before,&split_runtime_sync_remote,sizeof(before)) == 0);
            test_reset_rpc_send_counts();
            split_runtime_sync_tick();
            CHECK(rpc_send_count == 0u);
        }
        // Quiescent role reinit must not interpret the previous union arm.
        fake_is_master = !fake_is_master;
        rpc_register_count = 0;
        rpc_inject_during_registration = true;
        split_runtime_sync_init();
        rpc_inject_during_registration = false;
        CHECK(!split_runtime_sync_remote.lighting_facts_received);
    }
}

static void change_lighting_identity(void) {
    fake_lighting_identity = fake_lighting_identity == 1u ? 128u : 1u;
    split_runtime_sync_notify_key_feedback_dirty();
}

int main(void) {
    test_failed_coherent_read_leaves_the_destination_untouched();
    test_role_storage_is_ready_before_callbacks();
    test_init_registers_rpcs_and_sends_initial_packets_on_master();
    test_init_registers_rpcs_without_sending_on_slave();
    test_elapsed_skips_unchanged_packets_until_heartbeat();
    test_tick_samples_one_shared_timestamp();
    test_force_sync_samples_one_shared_timestamp();
    test_idle_auto_mouse_still_publishes_fade_progress();
    test_locked_mode_suppresses_fade_progress();
    test_active_heartbeat_is_wrap_safe();
    test_active_auto_mouse_uses_shared_low16_after_wrap();
    test_first_failure_stops_tick_and_force_respects_backoff();
    test_retry_schedule_caps_and_is_wrap_safe();
    test_failed_lighting_pass_resends_the_whole_latest_frame();
    test_active_to_idle_clear_survives_outage();
    test_role_reinitialization_clears_transport_backoff();
    test_force_sync_sends_all_packets_even_when_unchanged();
    test_key_feedback_visibility_is_ignored_without_flashing_semantics();
    test_key_feedback_visibility_changes_when_flashing_semantics_are_present();
    test_locked_pd_mode_zeroes_automouse_progress();
    test_disabled_auto_mouse_publishes_no_fade_progress();
    test_tick_sends_only_base_packet_when_only_automouse_changes();
    test_tick_sends_exactly_the_changed_chunks(change_combo_underlay);
    test_tick_sends_exactly_the_changed_chunks(change_preview_layer);
    test_tick_sends_exactly_the_changed_chunks(change_semantic);
    test_tick_sends_exactly_the_changed_chunks(change_tap_branch);
    test_tick_sends_exactly_the_changed_chunks(change_broad_owner);
    test_tick_sends_exactly_the_changed_chunks(change_lighting_identity);
    test_quiet_combo_projection_clears_peer_feedback();
    test_idle_combo_feedback_activation_sends_without_dirty_notification();
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    test_tick_sends_base_packet_when_only_pd_owner_bitmap_changes();
#endif
    test_idle_packets_use_idle_heartbeat();
    test_idle_tick_keeps_combo_immediate_and_skips_key_feedback_builders_until_due_or_dirty();
    test_active_behaviour_slice_rebuilds_every_pass();
    test_slave_rpcs_apply_exact_remote_state();
    test_slave_base_rpc_ignores_short_packets();
    test_slave_publishes_a_frame_only_on_its_commit();
    test_slave_drops_an_abandoned_pass();
    test_slave_first_partial_pass_starts_from_rest();
    test_slave_ignores_malformed_lighting_chunks();
    test_lighting_facts_publication_is_atomic_for_readers();
    test_base_publication_marks_its_generation_in_flight();

    puts("split_runtime_sync host tests passed");
    return 0;
}

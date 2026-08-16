// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the custom split RPC sync for runtime-visible state. The sync surface
// is intentionally split into three logical domains:
// - base runtime state (automouse / pd / preview)
// - combo RGB locality (underlay + overlay)
// - authored key-feedback truth (semantic state + broad owner state + tap branch state)
//
// The master periodically re-sends each surface as a heartbeat so a rebooted
// or rejoined half can recover even if the relevant state did not change. Heavy
// combo/key-feedback surfaces are rebuilt only while active, dirty, forced, or
// heartbeat-due so idle scans do not spend time deriving unchanged RGB packets.
// A shared transport-health gate stops a send pass after its first failure and
// suppresses packet building until a bounded recovery-probe deadline.
//
// Slave RPC callbacks run in the split worker context, not the main loop, so
// every domain is published as one publication generation and main-context
// readers copy a domain through the `split_runtime_sync_remote_read_*` helpers
// below instead of touching the fields directly.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>
#include <string.h>

#include "../key/runtime/feedback.h"
#include "../key/runtime/slot/origin_registry.h"
#include "../pointing/defs/pd_mode_flags.h"
#include "../state/shared/runtime_publication.h"

#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32u
#endif

typedef struct __attribute__((packed)) {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t pd_mode_owner_sides;
    uint8_t pd_mode_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    uint8_t key_preview_layer;
} split_runtime_base_sync_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
} split_runtime_combo_feedback_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t key_feedback_flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
} split_runtime_key_feedback_semantic_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t key_feedback_broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    uint8_t key_feedback_tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
} split_runtime_key_feedback_branch_packet_t;

typedef struct {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t pd_mode_owner_sides;
    uint8_t pd_mode_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    uint8_t key_preview_layer;
    uint8_t combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t key_feedback_flash_visibility_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
    uint8_t key_feedback_broad_owner_map[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    uint8_t key_feedback_tap_branch_map[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];
    // One publication generation per logical domain. The split worker keeps a
    // generation odd while that domain's fields are in flight.
    noah_runtime_publication_generation_t base_generation;
    noah_runtime_publication_generation_t combo_generation;
    noah_runtime_publication_generation_t key_feedback_semantic_generation;
    noah_runtime_publication_generation_t key_feedback_branch_generation;
} split_runtime_sync_remote_t;

typedef enum {
    SPLIT_RUNTIME_SYNC_DOMAIN_BASE = 0,
    SPLIT_RUNTIME_SYNC_DOMAIN_COMBO,
    SPLIT_RUNTIME_SYNC_DOMAIN_KEY_FEEDBACK_SEMANTIC,
    SPLIT_RUNTIME_SYNC_DOMAIN_KEY_FEEDBACK_BRANCH,
} split_runtime_sync_domain_t;

#ifdef SPLIT_RUNTIME_SYNC_PUBLISH_TEST_BACKEND
// Host-test seam. The registered hook runs while a domain's publication is
// still in flight, so a test can interleave a main-context read between two
// fields of the same packet deterministically.
typedef void (*split_runtime_sync_publish_seam_fn_t)(split_runtime_sync_domain_t domain);

void split_runtime_sync_test_set_publish_seam(split_runtime_sync_publish_seam_fn_t seam);
#endif

#ifdef NOAH_HOST_TEST_ENV
typedef struct {
    uint32_t base_last_send;
    uint32_t combo_last_send;
    uint32_t semantic_last_send;
    uint32_t branch_last_send;
} split_runtime_sync_debug_clock_t;

typedef struct {
    uint32_t last_failure;
    uint32_t retry_delay_ms;
    uint8_t  consecutive_failures;
    bool     backoff_active;
} split_runtime_sync_debug_transport_t;
#endif

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT                                             \
        {                                                                                    \
            .automouse_progress                   = 0,                                       \
            .active_mode_id                       = PD_MODE_ID_NONE,                         \
            .locked_mode_id                       = PD_MODE_ID_NONE,                         \
            .pd_mode_owner_sides                  = SPLIT_SIDE_MASK_NONE,                    \
            .pd_mode_owner_bitmap                 = {0},                                     \
            .key_preview_layer                    = UINT8_MAX,                               \
            .combo_underlay_bitmap                = {0},                                     \
            .combo_overlay_bitmap                 = {0},                                     \
            .key_feedback_flash_visibility_bitmap = {0},                                     \
            .key_feedback_semantic_map            = {0},                                     \
            .key_feedback_broad_owner_map         = KEY_FEEDBACK_BROAD_OWNER_MAP_EMPTY_INIT, \
            .key_feedback_tap_branch_map          = {0},                                     \
            .base_generation                      = 0,                                       \
            .combo_generation                     = 0,                                       \
            .key_feedback_semantic_generation     = 0,                                       \
            .key_feedback_branch_generation       = 0,                                       \
        }
#else
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT                                             \
        {                                                                                    \
            .automouse_progress                   = 0,                                       \
            .active_mode_id                       = PD_MODE_ID_NONE,                         \
            .locked_mode_id                       = PD_MODE_ID_NONE,                         \
            .key_preview_layer                    = UINT8_MAX,                               \
            .combo_underlay_bitmap                = {0},                                     \
            .combo_overlay_bitmap                 = {0},                                     \
            .key_feedback_flash_visibility_bitmap = {0},                                     \
            .key_feedback_semantic_map            = {0},                                     \
            .key_feedback_broad_owner_map         = KEY_FEEDBACK_BROAD_OWNER_MAP_EMPTY_INIT, \
            .key_feedback_tap_branch_map          = {0},                                     \
            .base_generation                      = 0,                                       \
            .combo_generation                     = 0,                                       \
            .key_feedback_semantic_generation     = 0,                                       \
            .key_feedback_branch_generation       = 0,                                       \
        }
#endif

_Static_assert(sizeof(split_runtime_base_sync_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_base_sync_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_combo_feedback_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_combo_feedback_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_key_feedback_semantic_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_key_feedback_semantic_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_key_feedback_branch_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_key_feedback_branch_packet_t must fit in one QMK RPC payload");

#if defined(SPLIT_TRANSACTION_IDS_USER)

extern split_runtime_sync_remote_t split_runtime_sync_remote;

void split_runtime_sync_init(void);
void split_runtime_sync_tick(void);
void split_runtime_sync_elapsed(uint16_t raw_elapsed);
void split_runtime_sync(void);
void split_runtime_sync_mark_combo_dirty(void);
void split_runtime_sync_mark_key_feedback_dirty(void);
#    ifdef NOAH_HOST_TEST_ENV
void split_runtime_sync_debug_clock_snapshot(split_runtime_sync_debug_clock_t *out);
void split_runtime_sync_debug_transport_snapshot(split_runtime_sync_debug_transport_t *out);
#    endif

static inline void split_runtime_sync_notify_combo_dirty(void) {
    split_runtime_sync_mark_combo_dirty();
}

static inline void split_runtime_sync_notify_key_feedback_dirty(void) {
    split_runtime_sync_mark_key_feedback_dirty();
}

#else

static const split_runtime_sync_remote_t split_runtime_sync_remote = SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;

static inline void split_runtime_sync_init(void) {}
static inline void split_runtime_sync_tick(void) {}
static inline void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void split_runtime_sync(void) {}
static inline void split_runtime_sync_mark_combo_dirty(void) {}
static inline void split_runtime_sync_mark_key_feedback_dirty(void) {}
#    ifdef NOAH_HOST_TEST_ENV
static inline void split_runtime_sync_debug_clock_snapshot(split_runtime_sync_debug_clock_t *out) {
    if (out) {
        *out = (split_runtime_sync_debug_clock_t){0};
    }
}
static inline void split_runtime_sync_debug_transport_snapshot(split_runtime_sync_debug_transport_t *out) {
    if (out) {
        *out = (split_runtime_sync_debug_transport_t){0};
    }
}
#    endif
static inline void split_runtime_sync_notify_combo_dirty(void) {}
static inline void split_runtime_sync_notify_key_feedback_dirty(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)

// ─── Coherent remote reads ──────────────────────────────────────────────────
//
// Main-context readers copy a whole domain through these helpers so a
// publication that preempts the copy is retried instead of committed as a
// mixture of two packets. Each attempt copies into a local staging buffer and
// only commits to the caller's destination once the generation is confirmed
// unchanged, so a helper that returns false has written nothing at all and the
// caller keeps the copy it already holds. Staging costs a few dozen bytes of
// stack on the render path, which is what makes "a failed read leaves your
// buffer alone" true rather than merely intended.

static inline bool split_runtime_sync_remote_read_combo(uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap) {
    uint8_t staged_underlay[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t staged_overlay[KEY_ORIGIN_BITMAP_SIZE];

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t generation = noah_runtime_publication_observe(&split_runtime_sync_remote.combo_generation);

        if (noah_runtime_publication_in_flight(generation)) {
            continue;
        }

        key_origin_bitmap_copy(staged_underlay, split_runtime_sync_remote.combo_underlay_bitmap);
        key_origin_bitmap_copy(staged_overlay, split_runtime_sync_remote.combo_overlay_bitmap);

        if (noah_runtime_publication_settled(&split_runtime_sync_remote.combo_generation, generation)) {
            key_origin_bitmap_copy(out_underlay_bitmap, staged_underlay);
            key_origin_bitmap_copy(out_overlay_bitmap, staged_overlay);
            return true;
        }
    }

    return false;
}

static inline bool split_runtime_sync_remote_read_key_feedback_semantic(uint8_t *out_flash_visibility_bitmap, uint8_t *out_semantic_map) {
    uint8_t staged_visibility[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t staged_semantic[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t generation = noah_runtime_publication_observe(&split_runtime_sync_remote.key_feedback_semantic_generation);

        if (noah_runtime_publication_in_flight(generation)) {
            continue;
        }

        key_origin_bitmap_copy(staged_visibility, split_runtime_sync_remote.key_feedback_flash_visibility_bitmap);
        memcpy(staged_semantic, split_runtime_sync_remote.key_feedback_semantic_map, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);

        if (noah_runtime_publication_settled(&split_runtime_sync_remote.key_feedback_semantic_generation, generation)) {
            key_origin_bitmap_copy(out_flash_visibility_bitmap, staged_visibility);
            memcpy(out_semantic_map, staged_semantic, KEY_FEEDBACK_SEMANTIC_MAP_SIZE);
            return true;
        }
    }

    return false;
}

static inline bool split_runtime_sync_remote_read_key_feedback_branch(uint8_t *out_broad_owner_map, uint8_t *out_tap_branch_map) {
    uint8_t staged_broad_owner[KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE];
    uint8_t staged_tap_branch[KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE];

    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t generation = noah_runtime_publication_observe(&split_runtime_sync_remote.key_feedback_branch_generation);

        if (noah_runtime_publication_in_flight(generation)) {
            continue;
        }

        memcpy(staged_broad_owner, split_runtime_sync_remote.key_feedback_broad_owner_map, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
        memcpy(staged_tap_branch, split_runtime_sync_remote.key_feedback_tap_branch_map, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);

        if (noah_runtime_publication_settled(&split_runtime_sync_remote.key_feedback_branch_generation, generation)) {
            memcpy(out_broad_owner_map, staged_broad_owner, KEY_FEEDBACK_BROAD_OWNER_MAP_SIZE);
            memcpy(out_tap_branch_map, staged_tap_branch, KEY_FEEDBACK_TAP_BRANCH_MAP_SIZE);
            return true;
        }
    }

    return false;
}

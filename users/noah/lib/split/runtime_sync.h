// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the custom split RPC sync for runtime-visible state, in two parts:
// - the base packet: auto-mouse fade progress and the pointing mode the other
//   half's pointing runtime mirrors
// - the lighting facts frame: the synced slices of the lighting fact frame
//   (docs/architecture/lighting-rules-v1.md, "Facts on the split link"), in
//   30-byte chunks published on the other half as one generation
//
// The master periodically re-sends each part as a heartbeat so a rebooted or
// rejoined half can recover even if the relevant state did not change. The
// gated lighting slices (key feedback) are rebuilt only while dirty, active,
// forced or heartbeat-due so idle scans do not spend time deriving unchanged
// facts. A shared transport-health gate stops a send pass after its first
// failure and suppresses work until a bounded recovery-probe deadline.
//
// Slave RPC callbacks run in the split worker context, not the main loop, so
// each part is published as one publication generation and main-context
// readers copy it through the `split_runtime_sync_remote_read_*` helpers below.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>
#include <string.h>

#include "../key/runtime/feedback.h"
#include "../rgb/lighting/lighting.h"
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
} split_runtime_base_sync_packet_t;

// One chunk of the lighting facts frame: the frame sequence, the chunk index
// (bits 0-3) with bit 7 marking the chunk that commits the pass, and 30 frame
// bytes from index * 30.
enum {
    SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES = 30u,
    SPLIT_RUNTIME_SYNC_FACTS_COMMIT      = 0x80u,
    SPLIT_RUNTIME_SYNC_FACTS_INDEX_MASK  = 0x7fu,
};

#define SPLIT_RUNTIME_SYNC_FACTS_FRAME_BYTES (NOAH_LIGHTING_SYNCED_BYTES > 0u ? NOAH_LIGHTING_SYNCED_BYTES : 1u)
#define SPLIT_RUNTIME_SYNC_FACTS_CHUNKS ((NOAH_LIGHTING_SYNCED_BYTES + SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES - 1u) / SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES)
_Static_assert(SPLIT_RUNTIME_SYNC_FACTS_CHUNKS <= SPLIT_RUNTIME_SYNC_FACTS_INDEX_MASK + 1u, "the lighting facts frame must fit 128 chunks");

typedef struct __attribute__((packed)) {
    uint8_t sequence;
    uint8_t chunk;
    uint8_t bytes[SPLIT_RUNTIME_SYNC_FACTS_CHUNK_BYTES];
} split_runtime_lighting_facts_packet_t;

typedef struct {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t pd_mode_owner_sides;
    uint8_t pd_mode_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    // Fixed boot roles share storage. Only the initialized arm may be used;
    // the callback ready/role guard protects reinitialization as well.
    union {
        struct {
            uint8_t lighting_facts[SPLIT_RUNTIME_SYNC_FACTS_FRAME_BYTES];
            uint8_t lighting_staging[SPLIT_RUNTIME_SYNC_FACTS_FRAME_BYTES];
        };
        struct {
            uint8_t built[SPLIT_RUNTIME_SYNC_FACTS_FRAME_BYTES];
            uint8_t sent[SPLIT_RUNTIME_SYNC_FACTS_FRAME_BYTES];
        } lighting_master;
    };
    // One publication generation per logical domain. The split worker keeps a
    // generation odd while that domain's fields are in flight.
    noah_runtime_publication_generation_t base_generation;
    noah_runtime_publication_generation_t lighting_facts_generation;
    // Set once the first lighting facts frame commits.
    volatile bool lighting_facts_received;
} split_runtime_sync_remote_t;

typedef enum {
    SPLIT_RUNTIME_SYNC_DOMAIN_BASE = 0,
    SPLIT_RUNTIME_SYNC_DOMAIN_LIGHTING_FACTS,
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
    uint32_t lighting_facts_last_send;
} split_runtime_sync_debug_clock_t;

typedef struct {
    uint32_t last_failure;
    uint32_t retry_delay_ms;
    uint8_t  consecutive_failures;
    bool     backoff_active;
} split_runtime_sync_debug_transport_t;
#endif

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT                \
        {                                                       \
            .automouse_progress        = 0,                     \
            .active_mode_id            = PD_MODE_ID_NONE,       \
            .locked_mode_id            = PD_MODE_ID_NONE,       \
            .pd_mode_owner_sides       = SPLIT_SIDE_MASK_NONE,  \
            .pd_mode_owner_bitmap      = {0},                   \
            .lighting_facts            = {0},                   \
            .base_generation           = 0,                     \
            .lighting_facts_generation = 0,                     \
            .lighting_facts_received   = false,                 \
        }
#else
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT                \
        {                                                       \
            .automouse_progress        = 0,                     \
            .active_mode_id            = PD_MODE_ID_NONE,       \
            .locked_mode_id            = PD_MODE_ID_NONE,       \
            .lighting_facts            = {0},                   \
            .base_generation           = 0,                     \
            .lighting_facts_generation = 0,                     \
            .lighting_facts_received   = false,                 \
        }
#endif

_Static_assert(sizeof(split_runtime_base_sync_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_base_sync_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_lighting_facts_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_lighting_facts_packet_t must fit in one QMK RPC payload");

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
// mixture of two frames. Each attempt copies into a local staging buffer and
// only commits to the caller's destination once the generation is confirmed
// unchanged, so a helper that returns false has written nothing at all and the
// caller keeps the copy it already holds.

// Internal staging copier: destination is scratch, and may change on failure.
// Public readers below commit only after this validates a complete generation.
static inline bool split_runtime_sync_remote_stage_lighting(uint8_t *staged) {
#if NOAH_LIGHTING_HAS_SYNCED_FACTS
    for (uint8_t attempt = 0; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        if (!split_runtime_sync_remote.lighting_facts_received) return false;
        uint8_t generation = noah_runtime_publication_observe(&split_runtime_sync_remote.lighting_facts_generation);
        if (noah_runtime_publication_in_flight(generation)) continue;
        memcpy(staged, split_runtime_sync_remote.lighting_facts, NOAH_LIGHTING_SYNCED_BYTES);
        if (noah_runtime_publication_settled(&split_runtime_sync_remote.lighting_facts_generation, generation)) return true;
    }
#else
    (void)staged;
#endif
    return false;
}

static inline bool split_runtime_sync_remote_read_lighting_facts(uint8_t *out_frame) {
#if NOAH_LIGHTING_HAS_SYNCED_FACTS
    uint8_t staged[NOAH_LIGHTING_SYNCED_BYTES];
    if (split_runtime_sync_remote_stage_lighting(staged)) {
        memcpy(out_frame, staged, sizeof(staged));
        return true;
    }
#else
    (void)out_frame;
#endif
    return false;
}

// Rendering commits the validated staging directly into its owned slices.
// There is still one full coherent-read staging buffer, never an unstaged
// copy into resolved facts; no second full-frame output buffer is needed.
static inline bool split_runtime_sync_remote_collect_lighting(noah_lighting_facts_t *facts) {
#if NOAH_LIGHTING_HAS_SYNCED_FACTS
    uint8_t staged[NOAH_LIGHTING_SYNCED_BYTES];
    if (split_runtime_sync_remote_stage_lighting(staged)) return noah_lighting_facts_unpack_synced_changed(facts, staged);
#else
    (void)facts;
#endif
    return false;
}

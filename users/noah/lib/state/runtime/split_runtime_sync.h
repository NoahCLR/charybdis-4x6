// ────────────────────────────────────────────────────────────────────────────
// Split Runtime Sync
// ────────────────────────────────────────────────────────────────────────────
//
// Owns the custom split RPC sync for runtime-visible state. The sync surface
// is intentionally split into three packets:
// - base runtime state (automouse / pd / preview)
// - combo RGB locality (underlay + overlay)
// - authored key-feedback semantic truth
//
// The master periodically re-sends each surface as a heartbeat so a rebooted
// or rejoined half can recover even if the relevant state did not change.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdint.h>

#include "../../key/runtime/feedback.h"
#include "../../key/runtime/origin_registry.h"
#include "../../pointing/defs/pd_mode_flags.h"

#ifndef RPC_M2S_BUFFER_SIZE
#    define RPC_M2S_BUFFER_SIZE 32u
#endif

typedef struct __attribute__((packed)) {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t      pd_mode_owner_sides;
    uint8_t      pd_mode_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    uint8_t      key_preview_layer;
} split_runtime_base_sync_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
} split_runtime_combo_feedback_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t key_feedback_flash_meta;
    uint8_t key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
} split_runtime_key_feedback_packet_t;

typedef struct {
    uint16_t     automouse_progress;
    pd_mode_id_t active_mode_id;
    pd_mode_id_t locked_mode_id;
#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
    uint8_t      pd_mode_owner_sides;
    uint8_t      pd_mode_owner_bitmap[KEY_ORIGIN_BITMAP_SIZE];
#endif
    uint8_t      key_preview_layer;
    uint8_t      combo_underlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t      combo_overlay_bitmap[KEY_ORIGIN_BITMAP_SIZE];
    uint8_t      key_feedback_flash_meta;
    uint8_t      key_feedback_semantic_map[KEY_FEEDBACK_SEMANTIC_MAP_SIZE];
} split_runtime_sync_remote_t;

#ifdef RGB_PD_MODE_ACTIVE_HALF_ENABLE
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT \
        {                                        \
            .automouse_progress    = 0,          \
            .active_mode_id        = PD_MODE_ID_NONE, \
            .locked_mode_id        = PD_MODE_ID_NONE, \
            .pd_mode_owner_sides   = SPLIT_SIDE_MASK_NONE, \
            .pd_mode_owner_bitmap  = {0},        \
            .key_preview_layer     = UINT8_MAX,  \
            .combo_underlay_bitmap = {0},        \
            .combo_overlay_bitmap  = {0},        \
            .key_feedback_flash_meta = 0,        \
            .key_feedback_semantic_map = {0},    \
        }
#else
#    define SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT \
        {                                        \
            .automouse_progress    = 0,          \
            .active_mode_id        = PD_MODE_ID_NONE, \
            .locked_mode_id        = PD_MODE_ID_NONE, \
            .key_preview_layer     = UINT8_MAX,  \
            .combo_underlay_bitmap = {0},        \
            .combo_overlay_bitmap  = {0},        \
            .key_feedback_flash_meta = 0,        \
            .key_feedback_semantic_map = {0},    \
        }
#endif

_Static_assert(sizeof(split_runtime_base_sync_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_base_sync_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_combo_feedback_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_combo_feedback_packet_t must fit in one QMK RPC payload");
_Static_assert(sizeof(split_runtime_key_feedback_packet_t) <= RPC_M2S_BUFFER_SIZE, "split_runtime_key_feedback_packet_t must fit in one QMK RPC payload");

#if defined(SPLIT_TRANSACTION_IDS_USER)

extern split_runtime_sync_remote_t split_runtime_sync_remote;

void split_runtime_sync_init(void);
void split_runtime_sync_tick(void);
void split_runtime_sync_elapsed(uint16_t raw_elapsed);
void split_runtime_sync(void);

#else

static const split_runtime_sync_remote_t split_runtime_sync_remote = SPLIT_RUNTIME_SYNC_REMOTE_EMPTY_INIT;

static inline void split_runtime_sync_init(void) {}
static inline void split_runtime_sync_tick(void) {}
static inline void split_runtime_sync_elapsed(uint16_t raw_elapsed) {
    (void)raw_elapsed;
}
static inline void split_runtime_sync(void) {}

#endif // defined(SPLIT_TRANSACTION_IDS_USER)

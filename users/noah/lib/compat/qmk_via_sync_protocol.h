// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Reconciliation Wire Protocol
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_QMK_VIA_SYNC_PROTOCOL_VERSION  = 1u,
    NOAH_QMK_VIA_SYNC_FRAME_SIZE        = 32u,
    NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX = 14u,
};

typedef enum {
    NOAH_QMK_VIA_SYNC_MESSAGE_METADATA        = 1,
    NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_BEGIN  = 2,
    NOAH_QMK_VIA_SYNC_MESSAGE_PUSH_CHUNK      = 3,
    NOAH_QMK_VIA_SYNC_MESSAGE_PULL_CHUNK      = 4,
    NOAH_QMK_VIA_SYNC_MESSAGE_SNAPSHOT_COMMIT = 5,
    NOAH_QMK_VIA_SYNC_MESSAGE_ACK             = 6,
    NOAH_QMK_VIA_SYNC_MESSAGE_ERROR           = 7,
} noah_qmk_via_sync_message_kind_t;

typedef enum {
    NOAH_QMK_VIA_SYNC_REGION_NONE       = 0,
    NOAH_QMK_VIA_SYNC_REGION_VIA_CONFIG = 1,
    NOAH_QMK_VIA_SYNC_REGION_KEYMAP     = 2,
    NOAH_QMK_VIA_SYNC_REGION_ENCODER    = 3,
    NOAH_QMK_VIA_SYNC_REGION_MACRO      = 4,
} noah_qmk_via_sync_region_t;

typedef enum {
    NOAH_QMK_VIA_SYNC_STATUS_OK                = 0,
    NOAH_QMK_VIA_SYNC_STATUS_INVALID_FRAME     = 1,
    NOAH_QMK_VIA_SYNC_STATUS_SCHEMA_MISMATCH   = 2,
    NOAH_QMK_VIA_SYNC_STATUS_SNAPSHOT_REQUIRED = 3,
    NOAH_QMK_VIA_SYNC_STATUS_RANGE_ERROR       = 4,
    NOAH_QMK_VIA_SYNC_STATUS_DIGEST_MISMATCH   = 5,
    NOAH_QMK_VIA_SYNC_STATUS_STORAGE_ERROR     = 6,
    NOAH_QMK_VIA_SYNC_STATUS_BUSY              = 7,
} noah_qmk_via_sync_status_t;

typedef struct {
    noah_qmk_via_sync_message_kind_t kind;
    noah_qmk_via_sync_status_t       status;
    noah_qmk_via_sync_region_t       region;
    uint32_t                         generation;
    uint16_t                         offset;
    uint16_t                         region_length;
    uint32_t                         digest;
    uint8_t                          payload_length;
    uint8_t                          payload[NOAH_QMK_VIA_SYNC_FRAME_PAYLOAD_MAX];
} noah_qmk_via_sync_frame_t;

bool noah_qmk_via_sync_frame_encode(const noah_qmk_via_sync_frame_t *frame, uint8_t out[NOAH_QMK_VIA_SYNC_FRAME_SIZE]);
bool noah_qmk_via_sync_frame_decode(const uint8_t *wire, uint8_t length, noah_qmk_via_sync_frame_t *out);

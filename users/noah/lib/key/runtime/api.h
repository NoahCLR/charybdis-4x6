// ────────────────────────────────────────────────────────────────────────────
// Key Runtime API
// ────────────────────────────────────────────────────────────────────────────
//
// Narrow cross-module entry surface for key-runtime-owned behavior.
// Production code outside the key-runtime owner layer should stay on this
// header instead of depending on slot storage or process internals.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

typedef struct {
    uint8_t press_token_count;
    uint8_t tap_series_count;
    uint8_t lease_count;
    uint8_t pending_release_count;
    uint8_t persistent_intent_count;
} noah_key_runtime_activity_snapshot_t;

void noah_key_runtime_scan(void);
bool noah_key_runtime_settle_pending_fallback_hold(void);
void noah_key_runtime_activity_snapshot(noah_key_runtime_activity_snapshot_t *out);

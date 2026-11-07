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

#include "types.h"

typedef struct {
    uint8_t press_token_count;
    uint8_t tap_series_count;
    uint8_t lease_count;
    uint8_t pending_release_count;
    uint8_t persistent_intent_count;
} noah_key_runtime_activity_snapshot_t;

void noah_key_runtime_scan(void);
// Registers every fallback hold that is due, as an output goes out: a key held
// as a modifier modifies it. `emitting_key_pos` is the output's own key, whose
// pending press it leaves alone, so a key's taps never turn its own press into
// a hold; an output of no key passes NOAH_KEY_RUNTIME_NO_KEY.
bool noah_key_runtime_settle_pending_fallback_hold(keypos_t emitting_key_pos);
// A physical press about to be resolved, of a key without a behaviour, ends
// other keys' multi-tap windows: settle their pending layer-changing taps
// (layer locks) first, so the press resolves on the layers they leave on.
// True if the layers may have changed.
bool noah_key_runtime_settle_layer_taps_before_press(keyrecord_t *record);
void noah_key_runtime_activity_snapshot(noah_key_runtime_activity_snapshot_t *out);

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

void noah_key_runtime_scan(void);
bool noah_key_runtime_settle_pending_fallback_hold(void);

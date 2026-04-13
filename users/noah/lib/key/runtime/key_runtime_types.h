// ────────────────────────────────────────────────────────────────────────────
// Key Runtime Types
// ────────────────────────────────────────────────────────────────────────────
//
// Small leaf runtime types shared across the interaction resolver and runtime
// storage layers.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

typedef enum {
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_DEFAULT = 0,
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_IMPLICIT,
    KEY_RUNTIME_SLOT_HOLD_STRATEGY_FALLBACK,
} key_runtime_slot_hold_strategy_t;

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

typedef enum {
    KEY_RUNTIME_SLOT_PHASE_IDLE = 0,
    KEY_RUNTIME_SLOT_PHASE_TAP_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_PRESS_HELD_WINDOW,
    KEY_RUNTIME_SLOT_PHASE_RELEASE_HOLD_PENDING,
    KEY_RUNTIME_SLOT_PHASE_HOLD_TIER_ACTIVE,
    KEY_RUNTIME_SLOT_PHASE_HOLD_COMPLETE,
} key_runtime_slot_phase_t;

# Key runtime

The engine that decides every handled key. How an event reaches it and where
its effects go is [key-press.md](../../../../../docs/architecture/key-press.md);
this file is what the engine itself owns.

## Layout

- `reducer/`: the one state owner, `key_runtime_core_state_t`, with its
  transitions (`runtime.c`), lease and lock mechanics and shadow projection
  (`ownership_state.c`) and read-only queries (`state_query.c`).
- `planning/`: release, scan and tap-series decisions, and the effect plan.
- `projection/`: applies effects to the ledgers; snapshots for debug.
- `queue/`: the pending-release queue's mechanics over core storage.
- `slot/`: packed key positions, origin bitmaps, the materialized interaction
  contract a token caches.
- Top level: the QMK-facing integration (`process.c`, `preflight.c`,
  `press.c`, `release.c`, `scan.c`), the plan executor (`transition.c`), the
  deferred-release adapter, delayed-action replay, feedback queries, trace and
  debug.

## What the reducer owns

New runtime state belongs in `key_runtime_core_state_t`, unless it is an
external ledger or a stateless behaviour helper. A new multi-tap rule extends
the reducer and its release and scan planning, never a second state machine.

- `press_token_t`: one physical press, by position: its identity, phase,
  cached interaction contract and physical press and release times.
- `tap_series_t`: a pending tap series, which outlives the press. The reducer
  fills it in one place (`key_runtime_core_tap_series_fill_from_token`): a
  handled press joins it; a first tap's release decision starts it; a later
  tap's decision preserves or ends it; a hold's release ends its pending hold.
  The release planner returns a settlement, never a copy of the series.
- `lease_t`: temporary ownership a press holds (layer, modifiers, held action,
  repeat, pointing mode, pointer anchor).
- `persistent_intent_t`: what outlives a press: layer locks, the one-shot
  layer, pointing locks, the pointer toggle.
- `pending_release_slot_t`: dispatches waiting for another key's live tap,
  linked by array index with explicit head and tail, so order never depends
  on uptime. Nothing drains while a blocker remains; a record whose owner is
  still down stays in place while later eligible records go; one drain takes
  at most four (`KEY_RUNTIME_DEFERRED_RELEASE_DRAIN_BATCH_CAPACITY`) and
  ignores re-entry.
- `key_runtime_core_shadow_projection_t`: the reducer's view of layers,
  modifiers and pointing state, for blocking queries and overlap reasoning.

**Token IDs.** Zero means no owner. Allocation wraps from the maximum to 1 and
skips any ID still named by an active press, a token awaiting release
emission, a lease or a pending release (`key_runtime_core_token_id_is_reserved`).
It runs before the press changes any state. Exhausted, it counts
`token_allocation_failure_count` and the handled press is consumed with no
effects. A new owner-bearing store joins that scan and its host test.

## Authority map

| Fact | Authoritative | Projected or adapted by |
| --- | --- | --- |
| Press identity and phase | `press_token_t` | debug and trace snapshots |
| Pending tap series | `tap_series_t` | `planning/tap_series_flush.c` flushes, `planning/scan_planner.c` resolves thresholds, `planning/release_planner.c` decides releases |
| Release semantics (quick release, fallback suppression, buffered base tap, active and pending-series releases) | `planning/release_planner.c` | `deferred_release.c` queues blocked dispatches without re-deciding |
| Held ownership (held actions, repeats, momentary layers, managed modifiers, pointing holds, pointer anchor) | `lease_t` | `projection/projection.c` writes out to `held_action.c`, `held_repeat.c`, `layer_ownership.c`, `keyboard_mod_ownership.c`, the pointing runtime |
| Locks and the one-shot layer | `persistent_intent_t` | `layer_ownership.c` and `pointing_mode_state.c` apply them, then report back through `key_runtime_core_layer_lock_set`, `key_runtime_core_layer_oneshot_set` and `pointing_mode_key_runtime_bridge_observe_local_lock_state` |
| Pointing mode state | `pointing_mode_state.c` | the reducer requests changes as effects and observes locks through the bridge |
| Feedback pulses | core pulse fields | `projection/feedback_projection.c` queues; `feedback.c`, lighting and split render |
| Inspection | `reducer/state_query.c` reads only | `debug.c`, `feedback.c`, preflight, deferred release |

A change that touches the reducer and a ledger plans an effect and projects
it. A new write-back into the reducer is architecture work: it joins the
bridge allowlists in `run_feature_gate_compile_tests.sh`.

## Effect plans

One plan of 16 effects (`KEY_RUNTIME_CORE_EFFECT_PLAN_CAPACITY`) lives on the
stack per transition. A **buffered** plan (the handled press and release)
keeps every effect for its caller, so the release can defer dispatches, and
drops what does not fit. A **streaming** plan (the moments before a press and
a release, the scan, layer-tap settlement) projects when it fills and between
a moment's steps, so it carries any number of effects. Fallback-hold
settlement (`api.c`) holds no plan: it runs beneath another plan's projection,
decides every due key at once, then projects each in key order.

## Modifier masks

Lease and shadow masks are 8-bit report masks. QMK's `MT()` field is a 5-bit
encoding (low nibble Ctrl/Shift/Alt/GUI, bit 4 the right-hand bank), so
`MT(MOD_RCTL, KC_Q)` encodes `0x11` and holds report bit `0x10`;
`noah_qmk_mods_to_report_mask()` in `compat/qmk_mod_contract.h` converts it
and leaves Magic remapping to QMK.

An `LT` or `MT` held past its tap / hold threshold takes a layer or modifier
lease only when its hold at that tap count is the keycode's own, held until
release (`key_runtime_core_press_token_holds_keycode_hold`, D-F27). A row whose
hold is something else holds what that hold registers. A mod-tap's hold is a
managed owner, never a physical modifier.
`run_key_runtime_physical_ownership_integration_tests.sh` compares the shadow
masks with the applied ledger for every modifier subset;
`run_qmk_contract_checks.sh` pins the `MT` encoding to the BK commit.

## Debugging

Read state through `key_runtime_core_press_token_at()`,
`key_runtime_core_tap_series_at()`, `key_runtime_core_shadow_projection()` and
`key_runtime_core_projection_snapshot_capture()`. A missing view is added to
that surface.

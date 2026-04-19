# Runtime V2 Preservation Matrix

Date: 2026-04-19  
Thread: authored-key overlap wedge / single-authority runtime redesign

## Purpose

This matrix freezes the behavior that the single-authority runtime redesign must preserve while the internal ownership model changes. The preservation oracle is:

- `docs/INTERACTION_MODEL.md`
- `docs/KEYMAP.md`
- real-profile integration scenarios under `tests/host/`

This matrix does **not** preserve incidental slot/preflight/release white-box behavior from the legacy runtime unless it is required by those sources of truth.

## Acceptance Matrix

| Area | Preserved user-visible contract | Primary source | Executable enforcement |
| --- | --- | --- | --- |
| Tap / hold timing | Tap, hold, and long-hold outcomes remain driven by authored timing contracts and `tap_hold_term` thresholds. | `docs/INTERACTION_MODEL.md`, `docs/KEYMAP.md` | `tests/host/run_key_runtime_release_matrix_tests.sh`, `tests/host/run_key_runtime_transition_tests.sh` |
| Multi-tap | Multi-tap series keep their authored tap chain, pending-hold, and delayed-action behavior. | `docs/INTERACTION_MODEL.md` | `tests/host/run_key_runtime_slot_tests.sh`, `tests/host/run_key_runtime_scenario_tests.sh` |
| Transparent inheritance | `KC_TRNS` continues resolving through the active authored layer stack, including delayed and buffered paths. | `docs/KEYMAP.md` | `tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `tests/host/run_real_profile_validation_tests.sh` |
| Default `LT()` fallback | Raw `LT()` and authored momentary-layer keys continue to fall back to their tap behavior when the hold contract does not commit. | `docs/INTERACTION_MODEL.md` | `tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| `KC_LEFT_GUI` second-tap hold | The second tap of authored `KC_LEFT_GUI` still promotes to held `KC_LEFT_ALT` and remains usable across overlapping nav behavior. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| `KC_RIGHT_ALT` arrow mode | Tap still locks arrow mode; held `KC_RIGHT_ALT` still behaves like normal `Alt` until the authored lock path commits. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `tests/host/run_pd_mode_tests.sh` |
| Nav arrows | Nav left/right taps, medium holds, and long holds keep their authored immediate behavior under nav parents. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Nav -> `DRAGSCROLL` | Raw/authored nav entry into `DRAGSCROLL` continues to activate dragscroll without orphaning layer or pointer state. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Thumb tap / hold / lock | Thumb layer behaviors keep their current tap, hold, and double-tap lock semantics. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Pd-mode tap / hold / lock | `DRAGSCROLL`, `PINCH_MODE`, `VOLUME_MODE`, and `BRIGHTNESS_MODE` retain their authored tap/hold/lock patterns. | `docs/KEYMAP.md`, `docs/ADDING_PD_MODE.md` | `tests/host/run_pd_mode_tests.sh`, `tests/host/run_pd_runtime_tests.sh`, `tests/host/run_pd_mode_key_runtime_integration_tests.sh` |
| Pointer-layer anchoring | Pointer-layer anchoring remains derived from authored state, not from accidental overlap side effects. | `docs/INTERACTION_MODEL.md` | `tests/host/run_pointer_layer_policy_tests.sh`, `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Quiescent release | After all releases, authored overlaps unwind cleanly: no stale slot owner, pending multi-tap, deferred release, layer hold, pd-mode hold, or managed modifier remains live unless backed by a real lock intent. | Shared review thread requirement | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `tests/host/run_runtime_debug_tests.sh` |

## Shadow-Parity Contract

The first v2 landing pass adds a shadow-validation contract before any cutover:

- normalized inputs are recorded as `runtime_event_t`
- the current runtime emits projection checkpoints into the shared runtime trace ring
- host replay decodes those normalized inputs and re-runs the same scenario
- parity requires:
  - identical normalized input stream
  - identical structured runtime trace output
  - identical `projection_snapshot_t` end state

## First Landed Shadow Scenarios

- Raw `LAYER_NAV` -> `DRAGSCROLL` overlap replay parity
- `KC_LEFT_GUI` second-tap hold to `KC_LEFT_ALT` with repeated nav-arrow taps replay parity

These are intentionally the first shadow scenarios because they are the strongest currently open repro families for the authored-key wedge.

## Non-Goals For This Pass

- Do not cut production hooks over to the v2 reducer yet.
- Do not delete the legacy preflight/slot/release path yet.
- Do not reinterpret legacy white-box state transitions as preserved API.

## Next Migration Domains

1. Press/release identity and immutable press-token resolution
2. Tap-series state split from active presses
3. Lease-backed layer and modifier ownership
4. Pd-mode and pointer-anchor ownership under the v2 reducer
5. Shadow parity against captured hardware traces before cutover

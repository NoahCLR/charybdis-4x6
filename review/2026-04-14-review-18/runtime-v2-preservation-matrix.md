# Runtime V2 Preservation Matrix

Date: 2026-04-20  
Thread: authored-key overlap wedge / single-authority runtime redesign

## Purpose

This matrix records the behavior that the completed `runtime_v2` cutover must
continue to preserve.

The preservation oracle is:

- `docs/INTERACTION_MODEL.md`
- `docs/KEYMAP.md`
- current real-profile and runtime integration suites under `tests/host/`

Legacy slot/index white-box behavior is no longer a preserved contract. The
reducer-owned state and debug surfaces are now canonical.

## Acceptance Matrix

| Area | Preserved user-visible contract | Primary source | Executable enforcement |
| --- | --- | --- | --- |
| Tap / hold timing | Tap, hold, and long-hold outcomes still follow authored timing and threshold contracts. | `docs/INTERACTION_MODEL.md`, `docs/KEYMAP.md` | `tests/host/run_key_runtime_release_matrix_tests.sh`, `tests/host/run_key_runtime_scenario_tests.sh` |
| Multi-tap | Multi-tap series preserve authored tap chains, delayed action flush, pending-hold promotion, threshold/long-hold behavior, and per-key pending-chain independence across unrelated presses. | `docs/INTERACTION_MODEL.md` | `tests/host/run_key_runtime_release_matrix_tests.sh`, `tests/host/run_key_runtime_scenario_tests.sh`, `tests/host/run_runtime_debug_tests.sh` |
| Transparent inheritance | `KC_TRNS` still resolves through the live authored layer stack, including delayed and buffered paths. | `docs/KEYMAP.md` | `tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `tests/host/run_real_profile_validation_tests.sh` |
| Default `LT()` fallback | Raw `LT()` and authored momentary-layer keys still fall back to their tap behavior when the hold contract does not commit. | `docs/INTERACTION_MODEL.md` | `tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| `KC_LEFT_GUI` second-tap hold | The second tap of authored `KC_LEFT_GUI` still promotes to held `KC_LEFT_ALT` and remains usable across overlap. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| `KC_RIGHT_ALT` arrow mode | Tap still locks arrow mode; held `KC_RIGHT_ALT` still behaves like normal `Alt` until the authored lock path commits. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `tests/host/run_pd_mode_tests.sh` |
| Nav arrows | Nav taps, medium holds, and long holds keep their authored immediate behavior under nav parents. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Nav -> `DRAGSCROLL` | Raw/authored nav entry into `DRAGSCROLL` still activates dragscroll without orphaning layer or pointer state. | `docs/KEYMAP.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `tests/host/run_pd_mode_key_runtime_integration_tests.sh` |
| Thumb tap / hold / lock | Thumb layer behaviors keep their current tap, hold, and double-tap lock semantics, and left/right thumb pending tap windows can coexist until they resolve independently. | `docs/KEYMAP.md`, `docs/INTERACTION_MODEL.md` | `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Pd-mode tap / hold / lock | `DRAGSCROLL`, `PINCH_MODE`, `VOLUME_MODE`, and `BRIGHTNESS_MODE` retain their authored tap/hold/lock patterns. | `docs/KEYMAP.md`, `docs/ADDING_PD_MODE.md` | `tests/host/run_pd_mode_tests.sh`, `tests/host/run_pd_runtime_tests.sh`, `tests/host/run_pd_mode_key_runtime_integration_tests.sh` |
| Pointer-layer anchoring | Pointer-layer anchoring remains derived from authored state, not from accidental overlap side effects. | `docs/INTERACTION_MODEL.md` | `tests/host/run_pointer_layer_policy_tests.sh`, `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |
| Quiescent release | After releases settle, no stale press token, tap series, lease, pending release, layer hold, pd-mode hold, or managed modifier remains live unless backed by an active persistent intent. | Thread closure bar | `tests/host/run_runtime_debug_tests.sh`, `tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh` |

## Current Production Contract

The cutover is complete:

- production hooks now run the v2 reducer
- legacy slot/index reducers are deleted
- release, scan, and non-handled ownership cleanup all retire v2-owned leases
- host/runtime suites now link the real reducer surface instead of the deleted
  observer stub path

## Post-Cutover Validation

The remaining work for this thread is validation and closure, not more runtime
replacement:

1. Keep replaying the original wedge families through host and on-device traces.
2. Close the review thread only after the active review notes, full host suite,
   and firmware build all remain green on the v2-only tree.

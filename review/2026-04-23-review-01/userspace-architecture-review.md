# Userspace Architecture Review

## Scope

This is a new full-codebase architecture pass opened on 2026-04-23 using
`prompts/initial-architecture-review.md`.

The previous active review,
`review/2026-04-22-review-01/`, remains coherent and is scoped to the
combo-origin, RGB feedback, split-sync, and runtime memory-compaction thread.
This pass is separate because the requested review covers the whole userspace
architecture rather than follow-up remediation for that thread.

## Findings

### must-fix: Effect-plan overflow truncates runtime side effects

References:

- `users/noah/lib/key/runtime/transition.h:19`
- `users/noah/lib/key/runtime/transition.c:12`
- `users/noah/lib/key/runtime/transition.c:74`
- `users/noah/lib/key/runtime/core/runtime.c:184`
- `users/noah/lib/key/runtime/core/runtime.c:3020`
- `users/noah/lib/key/runtime/core/runtime.c:3078`
- `users/noah/lib/key/runtime/core/runtime.c:3102`

The key runtime correctly keeps stack-backed effect plans size-bounded, but
overflow currently means "drop later effects and set a flag." Projection still
executes only the retained prefix and does not fail, recover, or retry. Several
fan-out paths iterate over board-sized state and may need more effects than the
fixed queue can carry:

- active-key interruption can emit fallback-hold activations for many active
  keys
- multi-tap flush can emit one delayed action per pending tap series
- active-key flush can emit release, unregister, or dispatch effects across
  many active keys

That makes overflow a state-corruption risk rather than a diagnostic-only
condition. Losing one unregister, layer release, repeat stop, or delayed tap is
enough to leave the live QMK state and the reducer's shadow projection out of
sync.

Recommendation:

- Treat overflow as a hard contract failure in host tests.
- Add a production recovery path before projection, such as chunked draining,
  immediate fail-safe reset of owned state, or loop-local projection that does
  not require one queue to hold all effects.
- Add stress coverage for maximum simultaneous active tokens, pending tap
  series, held actions, repeats, and layer/pd leases.

### should-fix: Authored-profile validation remains log-only in firmware

References:

- `users/noah/runtime_init.c:65`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:61`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:70`
- `users/noah/lib/key/interaction/key_behavior_lookup.c:92`
- `users/noah/lib/key/interaction/keymap_validation.c:20`
- `users/noah/lib/key/interaction/keymap_validation.c:31`
- `users/noah/lib/key/interaction/keymap_validation.c:98`
- `users/noah/lib/key/interaction/keymap_validation.c:147`

The validation layer knows about important authored-data hazards: duplicate
`key_behaviors[]` rows, unsupported raw layer actions, invalid combo outputs,
ambiguous combo members, and unreachable behavior rows. In firmware, those
validators only print with `uprintf()` under `CONSOLE_ENABLE` and return
`void`; `keyboard_post_init_user()` continues booting no matter how many
validation errors were found.

The host tests make the current profile safe when they are run, but the runtime
contract itself is still soft. That is a weak fit for this repo's stated
"production firmware" stance because a bad keymap can compile, flash, and run
with behavior the runtime explicitly says is unsupported.

Recommendation:

- Change validation APIs to return an error count or boolean.
- Make the real-profile validation host test assert the hard result, not just
  empty logs.
- Decide the production response for validation failure: latch a diagnostic RGB
  state, disable only the invalid surfaces, or stop selected runtime init after
  preserving safe base QMK behavior.

### should-fix: PD-mode state is scalar while the key runtime can model multiple owners

References:

- `users/noah/lib/pointing/runtime/pd_mode_state.c:261`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:295`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:305`
- `users/noah/lib/pointing/runtime/pd_mode_state.c:381`
- `users/noah/lib/key/runtime/core/runtime.c:1546`

The key-runtime reducer can create one `LEASE_KIND_PD_MODE` lease per owning
token, and it already removes only different-mode leases when a new mode is
pressed. The PD runtime itself is still a scalar active/locked mode plus one
owner key/side.

The mismatch matters when the same mode is entered by more than one owner, for
example from two physical keys, a combo and a key, or a future profile that
duplicates a mode for ergonomics. Pressing a second key for the already-active
mode is treated as handled, but owner side is only refreshed when
`pd_mode_apply_activate_mode()` reports a state change. Releasing any unlocked
same-mode key can deactivate the scalar mode even if another reducer lease for
the same mode is still alive.

That edge case is not exercised by the current authored layout, but it is a
real extensibility trap because the reducer and PD runtime present different
ownership models.

Recommendation:

- Either make PD mode activation/refcount ownership reducer-derived, or keep a
  PD-side owner/refcount table keyed by owner token/key position.
- Add host coverage for two simultaneous same-mode owners, including release
  order and trigger-half RGB ownership.

### optional cleanup: The deferred-release blocker observer is now dead API

References:

- `users/noah/lib/key/runtime/core/runtime.h:356`
- `users/noah/lib/key/runtime/core/runtime.c:3628`

`key_runtime_core_observe_deferred_release_blocker_profile(...)` remains in the
core API but is an intentional no-op after deferred release blockers became
derived from press tokens. Keeping a public no-op observer makes future changes
harder to reason about because callers can believe they are feeding an
authoritative blocker table when no storage exists behind the call.

Recommendation:

- Remove the observer if no callers remain, or rename/comment it as a legacy
  compatibility no-op and gate new callers away from it.

## Remediation Status

Reconciliation note: the findings above were the audit-time snapshot from this
review. Remediation landed in the same thread, so the current tree should be
read with the status below.

- Effect-plan overflow: resolved. Fan-out transition paths now stream core
  effects into the transition layer and drain full chunks before appending more
  effects, so cleanup effects are not truncated by one fixed-size plan. Covered
  by `tests/host/runtime_debug_test.c` stress coverage and
  `sh tests/host/run_runtime_debug_tests.sh`.
- Authored-profile validation: resolved. Key behavior, keymap, and hardcoded
  macro validators now return error counts; `users/noah/rules.mk` runs
  `sh tests/host/run_real_profile_validation_tests.sh` as part of the firmware
  build and fails `qmk compile` on invalid authored data. Runtime post-init no
  longer repeats authored-profile validation, since invalid profiles should not
  reach flashing. Covered by validation runners, macro dispatch tests, runtime
  init-order tests, the feature-gate check for the build hook, and `qmk compile`.
- PD-mode multi-owner mismatch: resolved. PD runtime state now tracks
  transient key owners by key position while keeping the single active/locked
  mode invariant. Same-mode release order and trigger-half ownership are
  covered in `tests/host/pd_mode_test.c`.
- Deferred-release blocker observer: resolved. The no-op public observer was
  removed from `users/noah/lib/key/runtime/core/runtime.h` and
  `users/noah/lib/key/runtime/core/runtime.c`.

Final verification for the remediation pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_behavior_validation_tests.sh`
- `sh tests/host/run_keymap_validation_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_key_runtime_integration_harness_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_release_matrix_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Solid Areas

- Header ownership is mechanically enforced. `run_feature_gate_compile_tests.sh`
  scans production and host-test boundaries for umbrella/runtime-internal
  include regressions.
- Source ownership is clear. `users/noah/source_manifest.mk` is the shared
  firmware/host source inventory, and the keymap directory is mostly authored
  data.
- QMK and fork-specific assumptions are well localized under
  `users/noah/lib/compat/`.
- The current docs are unusually strong for firmware code. `README.md`,
  `docs/KEY_RUNTIME.md`, `docs/INTERACTION_MODEL.md`,
  `docs/POINTER_MODES.md`, `docs/RGB_CONFIG.md`, and
  `docs/HOOK_OVERRIDES.md` match the broad runtime structure.
- Test coverage is broad and targeted. The host suite has meaningful coverage
  for hook chaining, QMK contracts, key runtime scenarios, PD mode behavior,
  split sync, RGB rendering, profile validation, macro dispatch, and boundary
  compile gates.

## Current Architecture Assessment

The userspace is in good overall shape. The codebase has moved away from a
scratch keymap into a real firmware userspace with separated authored data,
runtime policy, compatibility shims, focused test runners, and profile
validation that is mechanically enforced before firmware flashing.

The remediation pass closed the main risks found by this review: fan-out
effects are no longer truncated by a single transition buffer, invalid authored
profiles hard-fail the firmware build, same-mode PD owners release
independently, and the stale blocker observer API is gone.

## Closure Verification

Prompt sources used:

- `prompts/follow-up-architecture-audit.md`
- `prompts/closure-verification-review.md`

Findings: no must-fix, should-fix, or optional-cleanup findings remain open for
this review thread.

### Prior Finding Status

- Effect-plan overflow: resolved. Fan-out transition paths now stream core
  effects through the transition sink and drain full chunks before appending
  more effects. Code references:
  `users/noah/lib/key/runtime/transition.c:29`,
  `users/noah/lib/key/runtime/transition.c:38`,
  `users/noah/lib/key/runtime/transition.c:92`,
  `users/noah/lib/key/runtime/core/runtime.c:194`, and
  `users/noah/lib/key/runtime/core/runtime.h:32`. Enforcement references:
  `tests/host/runtime_debug_test.c:1190`,
  `tests/host/runtime_debug_test.c:1997`. Verification commands passed:
  `sh tests/host/run_runtime_debug_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- Authored-profile validation: resolved. Key behavior, keymap, and hardcoded
  macro validators return error counts, and the firmware build hard-fails
  invalid authored data through `users/noah/rules.mk:7`. Runtime post-init no
  longer repeats authored-profile validation; it only runs runtime init stages
  in `users/noah/runtime_init.c:65`. Enforcement references:
  `tests/host/real_profile_validation_test.c:169`,
  `tests/host/real_profile_validation_test.c:170`,
  `tests/host/run_real_profile_validation_tests.sh:32`, and the build-gate
  presence check in `tests/host/run_feature_gate_compile_tests.sh`. Verification
  commands passed: `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_runtime_init_order_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- PD-mode multi-owner mismatch: resolved. PD mode runtime state now stores
  transient same-mode key owners by key position and aggregates owner sides
  before deactivating a mode. Code references:
  `users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h:14`,
  `users/noah/lib/pointing/runtime/pd_mode_state.c:79`,
  `users/noah/lib/pointing/runtime/pd_mode_state.c:120`,
  `users/noah/lib/pointing/runtime/pd_mode_state.c:148`, and
  `users/noah/lib/pointing/runtime/pd_mode_state.c:458`. Enforcement
  references: `tests/host/pd_mode_test.c:382`,
  `tests/host/pd_mode_test.c:803`. Verification commands passed:
  `sh tests/host/run_pd_mode_tests.sh`,
  `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- Deferred-release blocker observer: resolved. The public no-op observer was
  removed from the key-runtime core API and implementation; the remaining
  release-dispatch observers are the concrete deferred/drained APIs in
  `users/noah/lib/key/runtime/core/runtime.h:364` and
  `users/noah/lib/key/runtime/core/runtime.c:3616`. Enforcement references:
  the full host suite and firmware build compile the current public API surface
  and would fail on remaining callers. Verification commands passed:
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

Closure Verdict: close thread.

Remaining Open Findings: none.

## Recommended Follow-Up

No current remediation items remain open for this review.

1. Keep adding stress coverage whenever a new fan-out effect source is added to
   the key runtime.
2. Add key-runtime integration coverage if future authored data introduces
   duplicate physical or combo entry points for the same momentary PD mode.

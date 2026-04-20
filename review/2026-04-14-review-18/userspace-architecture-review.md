# Userspace Architecture Review

Date: 2026-04-20  
Prompt used: `prompts/closure-verification-review.md`  
Scope: `users/noah/` runtime ownership, key-runtime architecture, host/build enforcement, and the authored-key wedge remediation thread.

## Findings

### must-fix

- None in the current tree.

### should-fix

- None in the current tree.

### optional cleanup

- Registry-style single sources of truth remain the main maintainability risk.
  `users/noah/lib/action/action_kind_registry_list.h` and
  `users/noah/lib/pointing/defs/pd_mode_manifest.h` still encode multiple
  semantic fields in positional macro rows. That is acceptable for this
  closed thread, but it is still the next obvious cleanup topic if those
  registries start changing again.
- The keymap materialization macros remain the least inspectable part of the
  authored surface. `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
  and `users/noah/keymap_materialize.h` are workable, but future authored
  surfaces should prefer explicit `const` tables over more macro expansion.
- `users/noah/lib/pointing/runtime/pd_runtime.c` still mixes init defaults,
  mouse-record classification, idle suppression, mode dispatch, and sniping
  handoff. It is stable now, but it is still the obvious future split point
  for pointing work.

## Prior Finding Status

- `resolved`: the broad key-runtime internal seam is gone. The deleted
  `users/noah/lib/key/runtime/key_runtime_internal.h`,
  `users/noah/lib/key/runtime/key_runtime_index_internal.h`, and
  `users/noah/lib/key/runtime/key_runtime_shared_state.h` are replaced by the
  reducer-owned surface in `users/noah/lib/key/runtime/core/runtime.h` and the
  thin orchestration layer in `users/noah/lib/key/runtime/process.c`,
  `users/noah/lib/key/runtime/release.c`,
  `users/noah/lib/key/runtime/scan.c`, and
  `users/noah/lib/key/runtime/transition.c`. Enforcement comes from
  `tests/host/run_feature_gate_compile_tests.sh`, the core-runtime host suites,
  the full host suite, and the firmware build. Verified with:
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_runtime_debug_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

- `resolved`: the authored-key overlap wedge is now mechanically covered in the
  current tree. The production runtime is single-authority under
  `users/noah/lib/key/runtime/core/runtime.h` and `.c`, and the original overlap
  repro families now have regression-specific coverage in
  `tests/host/pd_mode_key_runtime_integration_test.c`,
  `tests/host/real_profile_thumb_layer_lock_integration_test.c`, and
  `tests/host/runtime_debug_test.c`. Those suites assert that `NAV ->
  DRAGSCROLL`, `KC_LEFT_GUI` second-tap-hold overlap, and quiescent-release
  cleanup all unwind without stale state. Verified with:
  `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`,
  `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`,
  `sh tests/host/run_runtime_debug_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

- `open`: registry-style single sources of truth remain a maintainability risk
  in `users/noah/lib/action/action_kind_registry_list.h` and
  `users/noah/lib/pointing/defs/pd_mode_manifest.h`, but they are now
  explicitly deferred as optional cleanup outside this thread's closure bar.

- `open`: the keymap materialization path in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` and
  `users/noah/keymap_materialize.h` remains the least inspectable authored
  surface, but it is now deferred as optional cleanup outside this thread's
  closure bar.

- `open`: `users/noah/lib/pointing/runtime/pd_runtime.c` still owns multiple
  pointing-runtime responsibilities, but it is now deferred as optional
  cleanup outside this thread's closure bar.

## Solid Areas

- The key runtime now has one authority. `key_runtime_core` owns press identity,
  multi-tap lifetime, reducer-owned leases, persistent intents, pending
  release transport, and debug/projection snapshots.
- The production runtime path is narrower, not broader. The permanent runtime
  tree now lives under `users/noah/lib/key/runtime/`, with reducer authority in
  `users/noah/lib/key/runtime/core/` and orchestration/effect transport in the
  surrounding integration files instead of parallel state mutation.
- The original wedge repro families are no longer only “believed fixed”.
  The current integration suites exercise those overlap families directly and
  assert runtime quiescence after release.
- Pending multi-tap lifetime is now position-owned even across unrelated
  foreign presses. The overlap policy remains conservative for active-key
  interruption and release blockers, but pending tap-series state is no longer
  globally collapsed just because another key started its own tap window.
- The build/test surface matches the current architecture. The source manifest,
  compile gate, authored-profile validation, runtime-debug suite, integration
  suites, full host suite, and firmware build all target the current
  core-owned runtime tree.

## Closure Verdict

`close thread`

The authored-key wedge / `runtime_v2` cutover thread is ready to close. The
current code matches the intended single-authority design, the regression
families that previously blocked closure are now covered by regression-specific
host suites, the docs/review notes match the live tree, and both
`sh tests/host/run_all_host_tests.sh` and
`qmk compile -kb bastardkb/charybdis/4x6 -km noah` passed in this closure pass.

## Remaining Open Findings

- Registry-style single-source lists in
  `users/noah/lib/action/action_kind_registry_list.h` and
  `users/noah/lib/pointing/defs/pd_mode_manifest.h` remain optional cleanup.
- The macro-heavy authored materialization seam in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` and
  `users/noah/keymap_materialize.h` remains optional cleanup.
- `users/noah/lib/pointing/runtime/pd_runtime.c` still has mixed
  responsibilities and remains optional cleanup.

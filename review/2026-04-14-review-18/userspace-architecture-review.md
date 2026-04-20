# Userspace Architecture Review

Date: 2026-04-20  
Prompt used: `prompts/follow-up-architecture-audit.md`  
Scope: `users/noah/` runtime ownership, key-runtime architecture, host/build enforcement, and the active authored-key wedge remediation thread.

## Findings

### must-fix

- None in the current tree. The legacy key-runtime slot/index path is gone, the
  production runtime is reducer-owned under `runtime_v2`, and the cutover is
  mechanically exercised by the current host/build surface.

### should-fix

- Registry-style single sources of truth remain the main maintainability risk.
  `users/noah/lib/action/action_kind_registry_list.h` and
  `users/noah/lib/pointing/defs/pd_mode_manifest.h` still encode multiple
  semantic fields in positional macro rows. That keeps single-source authoring,
  but it is still easy to make a syntactically valid change that silently
  changes dispatch or lifecycle semantics.

### optional cleanup

- The keymap materialization macros remain the least inspectable part of the
  authored surface. `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
  and `users/noah/keymap_materialize.h` are still workable, but future authored
  surfaces should prefer explicit `const` tables over more macro expansion.
- `users/noah/lib/pointing/runtime/pd_runtime.c` still mixes init defaults,
  mouse-record classification, idle suppression, mode dispatch, and sniping
  handoff. This is not blocking, but it is still the obvious future split
  point for pointing work.

## Prior Finding Status

- `resolved`: the broad key-runtime internal seam is gone. The deleted
  `users/noah/lib/key/runtime/key_runtime_internal.h`,
  `users/noah/lib/key/runtime/key_runtime_index_internal.h`, and
  `users/noah/lib/key/runtime/key_runtime_shared_state.h` are replaced by the
  reducer-owned surface in `users/noah/lib/runtime_v2/runtime_v2.h` and the
  thin orchestration layer in `users/noah/lib/key/runtime/key_runtime_process.c`,
  `users/noah/lib/key/runtime/key_runtime_release.c`, and
  `users/noah/lib/key/runtime/key_runtime_transition.c`. Enforcement comes from
  `tests/host/run_feature_gate_compile_tests.sh`, which now rejects the removed
  headers and validates the current source manifest. Verified with:
  `sh tests/host/run_runtime_debug_tests.sh`,
  `sh tests/host/run_key_runtime_release_matrix_tests.sh`,
  `sh tests/host/run_key_runtime_scenario_tests.sh`,
  `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`,
  `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`,
  `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`,
  `sh tests/host/run_runtime_trace_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, and
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

- `open`: registry-style single sources of truth remain the main
  maintainability risk in `users/noah/lib/action/action_kind_registry_list.h`
  and `users/noah/lib/pointing/defs/pd_mode_manifest.h`.

- `open`: the keymap materialization path in
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` and
  `users/noah/keymap_materialize.h` remains the least inspectable authored
  surface.

- `open`: `users/noah/lib/pointing/runtime/pd_runtime.c` still owns multiple
  pointing-runtime responsibilities.

## Solid Areas

- The key runtime now has one authority. `runtime_v2` owns press identity,
  multi-tap lifetime, reducer-owned leases, persistent intents, pending release
  transport, and debug/projection snapshots.
- The production runtime path is narrower, not broader. `users/noah/lib/key/runtime/`
  is now orchestration and effect transport around reducer entry points instead
  of parallel state mutation.
- The build/test surface matches the current architecture. The source manifest,
  compile gate, runtime-debug suite, integration harness, and full host suite
  all target the v2-only tree.

## Current Conclusion

The authored-key wedge thread is now on the architecture it was aiming for:
single-authority reducer state under `runtime_v2`, no production fallback to
legacy slot/index storage, and no legacy slot reducer files left in the tree.
The active review thread stays open only for the older maintainability findings
outside the cutover itself, not because the runtime still has a mixed-state
design.

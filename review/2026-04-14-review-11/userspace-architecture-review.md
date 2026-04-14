# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up audit plus remediation of the recently landed runtime-sealing
and action-policy refactor work.

Scope:

- runtime reset/debug public seams
- pd-mode runtime encapsulation
- host-test seam quality after strict runtime sealing
- maintainer docs and review integrity after the remediation pass

## Findings

### No must-fix correctness regressions found in the current tree

The code now matches the intended direction of the refactor materially better
than it did at the start of this review. I did not find a concrete firmware
behavior regression in the reviewed areas after the remediation pass landed.

### Resolved: the public runtime reset seam is strict again

References:

- [runtime_reset.h](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/state/runtime/runtime_reset.h:1)
- [runtime_shared_state.c](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/state/runtime/runtime_shared_state.c:1)
- [host_runtime_fixture.h](/Users/noah/dev/charybdis/charybdis-4x6/tests/host/include/host_runtime_fixture.h:1)

What changed:

- `noah_runtime_reset_for_test()` now lives behind its own public header.
- The production runtime owner no longer ships weak host-only fallbacks for
  `layer_state`, `clear_*mods()`, or `send_keyboard_report()`.
- Host runners that need reset now provide the required QMK stubs explicitly,
  either through the shared fixture helpers or local test seams.

Why this is materially better:

- Missing reset hooks now fail at link time instead of silently degrading to
  partial reset behavior.
- The production runtime layer no longer carries host-accommodation code.
- The reset seam now has one meaning again.

### Resolved: the pd-mode raw storage leak is sealed

References:

- [pd_mode_runtime_shared_state_internal.h](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/pointing/runtime/pd_mode_runtime_shared_state_internal.h:1)
- [pd_mode_state.c](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/pointing/runtime/pd_mode_state.c:1)
- [pd_mode_snapshot.c](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/pointing/runtime/pd_mode_snapshot.c:1)

What changed:

- The former public `pd_mode_runtime_shared_state.h` surface is gone.
- Raw pd-mode storage is now internal to the pd/runtime owner layer.
- Public/read callers stay on `pd_mode_snapshot()` and the
  `pd_mode_*_snapshot()` query family.

Why this is materially better:

- Repo-owned code outside the pd/runtime owner layer can no longer mutate
  runtime-owned pd fields directly.
- The remaining pd public API is semantic instead of storage-shaped.

### Resolved: `runtime_debug.h` is no longer a whole-runtime storage dump

References:

- [runtime_debug.h](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/state/runtime/runtime_debug.h:1)
- [runtime_debug.c](/Users/noah/dev/charybdis/charybdis-4x6/users/noah/lib/state/runtime/runtime_debug.c:1)
- [runtime_debug_test.c](/Users/noah/dev/charybdis/charybdis-4x6/tests/host/runtime_debug_test.c:1)

What changed:

- `runtime_debug.h` is now a key-runtime observation surface only.
- The public snapshot no longer embeds pd, layer-ownership, held-action,
  held-repeat, keyboard-mod-ownership, or trace storage layouts.
- The aggregate runtime-debug tests now use module-owned debug/query seams for
  non-key-runtime assertions.

Why this is materially better:

- Public debug callers no longer depend on one giant cross-subsystem snapshot
  struct.
- Internal ownership-module storage can change without forcing that churn
  through `runtime_debug.h`.

Residual maintainability note:

- Some module-owned debug snapshots are still storage-shaped, but that is now a
  local testing concern rather than a global runtime-debug API problem.

### Resolved: maintainer docs and active review notes now match the code

References:

- [KEY_RUNTIME.md](/Users/noah/dev/charybdis/charybdis-4x6/docs/KEY_RUNTIME.md:1)
- [progress.md](/Users/noah/dev/charybdis/charybdis-4x6/review/2026-04-14-review-11/progress.md:1)

What changed:

- `docs/KEY_RUNTIME.md` no longer points at deleted runtime aggregate headers or
  `handled_key.c`.
- The doc now points maintainers at `runtime_reset.h`, the narrowed
  `runtime_debug.h`, the pd read APIs, and the split handled-key files that
  actually exist.
- Review-11 now carries the corrected post-remediation status instead of only
  the pre-fix audit findings.

## Solid Areas

- The action-kind registry still looks coherent after the follow-up work. I did
  not find a mismatch between action identity, classification, and dispatch.
- The compile gate is stronger now: it blocks removed runtime headers, internal
  runtime headers from the wrong layers, and the removed public pd runtime
  storage header.
- The runtime-sealing work now lands cleanly enough that it should stop being
  treated as an active architecture debt item.

## Remaining Architecture Debt

- The main remaining structural debt is still hook/stage orchestration in
  `key_runtime_process.c`, `runtime_init.c`, and `rgb_runtime.c`.
- Authored keymap structure in `keymap.c` is still a secondary maintainability
  issue, but it is lower priority than hook/stage registration.

## Verification

Commands run for this remediation pass:

- `git status --short`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_runtime_trace_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
- `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

Current conclusion:

- review-11 follow-up findings are resolved in the tree as it exists now
- no sibling workspace folders were edited

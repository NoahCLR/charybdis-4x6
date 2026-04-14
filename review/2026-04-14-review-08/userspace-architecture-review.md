# Userspace Architecture Review

Date: 2026-04-14

Status: follow-up quality audit of the refactor work landed through
`review/2026-04-14-review-07/`.

Scope:

- correctness and regression risk after the review-07 shipping hardening pass
- abstraction quality and interface cleanliness in the current landed tree
- test quality, code organization, and review integrity for the refactored
  userspace runtime

Out of scope:

- fresh architecture brainstorming
- hardware changes
- upstream QMK redesign outside this repo

## Findings

### No Must-Fix Issues Found

I did not find a new production regression that looks clearly broken on-device
today. The review-07 pass did close the specific stale-signature, runtime-debug
helper, and action-kind coverage issues it was targeting, and the full host
suite plus firmware build are green.

### Should-Fix

#### 1. The shared integration harness still hides incomplete subsystem linkage behind weak debug/reset shims, so higher-level integration runners can quietly test against zeroed runtime surfaces

References:

- `tests/host/key_runtime_integration_harness.c:25-67`
- `users/noah/lib/state/runtime/runtime_debug.c:29-34`
- `users/noah/lib/state/runtime/runtime_debug.c:120-126`
- `tests/host/run_key_runtime_modifier_hold_integration_tests.sh:20-21`
- `tests/host/run_key_runtime_modifier_hold_integration_tests.sh:47-51`

Why this matters:

- The aggregate runtime-debug seam is now real, but the shared integration
  harness still provides weak zeroed implementations for
  `layer_ownership_debug_snapshot(...)`, `held_action_debug_snapshot(...)`,
  `held_repeat_debug_snapshot(...)`, `keyboard_mod_ownership_debug_snapshot(...)`,
  and the matching `*_reset_for_test()` hooks.
- `runtime_debug.c` calls those hooks unconditionally when building snapshots
  and resetting state.
- That means a harness-based runner can omit a real subsystem module and still
  compile cleanly against a zeroed/no-op replacement.

Why this is still a quality risk:

- This is the same class of problem the review chain has been chasing: the
  production seam is cleaner than the test seam.
- A future runner change can accidentally stop linking a subsystem and still
  get green output, because the harness silently supplies “empty world”
  fallbacks instead of forcing a link failure.
- `run_key_runtime_modifier_hold_integration_tests.sh` is a concrete example:
  it links the shared harness and `runtime_debug.c`, but not
  `layer_ownership.c`, so the harness fallback remains part of the effective
  contract for that runner.

Recommended direction:

- Keep weak harness defaults only for the truly minimal process/keymap hooks.
- Move debug/reset provider fallbacks out of the shared harness and localize
  any remaining stubbed subsystem surfaces to the specific runner that needs
  them.
- Prefer link failures over zeroed aggregate runtime surfaces for integration
  harnesses.

#### 2. The runtime-debug cleanup still exports raw slot layout through `noah_runtime_debug_slot_copy(...)`, so the new semantic seam is only half-finished

References:

- `users/noah/lib/state/runtime/runtime_debug.h:22-39`
- `users/noah/lib/state/runtime/runtime_debug.c:37-73`
- `tests/host/key_runtime_scenario_harness.c:231-279`

Why this matters:

- `runtime_debug.h` now has good semantic helpers for common key-runtime
  questions, but it also publicly exposes `noah_runtime_debug_slot_copy(...)`
  returning the full `active_key_state_t`.
- The scenario harness immediately uses that raw slot copy and then reads
  `slot.owner.keycode`, `slot.lifecycle.held_action_keycode`,
  `key_runtime_slot_has_pending_multi_tap(&slot)`, and
  `key_runtime_slot_hold_is_complete(&slot)` itself.

Why this is still a maintainability problem:

- The cleanup removed direct `snapshot.core.key.slots_by_position[...]`
  indexing from the higher-level harnesses, but replaced it with a public API
  that still exports the same internal slot shape.
- That means future slot-layout changes still propagate into higher-level test
  helpers through `active_key_state_t`, even though the runtime-debug layer now
  already owns semantic queries for most of the interesting state.

Recommended direction:

- Keep the aggregate raw snapshot for low-level debug/testing, but stop using
  `noah_runtime_debug_slot_copy(...)` as a higher-level harness seam.
- Add the remaining missing semantic queries and let the scenario harness query
  runtime-debug directly instead of materializing raw slot structs.

#### 3. The new action-kind guard improved coverage for defined kinds, but the descriptor contract is still inconsistent for invalid `kind` values and degrades silently in normal non-console firmware builds

References:

- `users/noah/lib/action/action_dispatch.h:91-98`
- `users/noah/lib/action/action_kind.c:133-147`
- `users/noah/lib/action/action_kind_dispatch.c:234-255`
- `tests/host/action_lifecycle_test.c:232-236`

Why this matters:

- `noah_action_desc_t` is still a public struct, so callers can construct it
  directly.
- Metadata queries normalize invalid `desc.kind` values back to the literal
  row via `noah_action_kind_def(...)`.
- Dispatch does something different: invalid or uncovered kinds go through the
  host-fail/no-op fallback in `action_kind_dispatch.c`.

Why this is still a maintainability problem:

- The action-kind boundary is cleaner than it was before, but it is not fully
  coherent yet: the same malformed descriptor can look like a literal action to
  metadata/predicate callers and a failed/no-op action to dispatch.
- The new host coverage only asserts that the currently defined
  `0..NOAH_ACTION_KIND_COUNT-1` rows are populated. It does not exercise the
  invalid-kind contract.
- In non-host builds without `CONSOLE_ENABLE`, the fallback path is also
  silent: the code degrades to a noop dispatch with no diagnostic output.

Recommended direction:

- Pick one invalid-kind policy and apply it consistently across metadata and
  dispatch: either normalize everywhere or reject everywhere.
- If the production policy remains “reject/noop,” make the non-host fallback
  explicitly observable even when `CONSOLE_ENABLE` is off.
- Add one focused host test for the chosen invalid-kind policy instead of only
  testing current-table completeness.

## Areas Assessed As Solid

- The review-07 shipping pass did materially improve the code:
  - stale `noah_dispatch_synthetic_record(...)` signatures in host suites are
    gone
  - the main harnesses now link the real `runtime_debug.c` seam
  - the action-kind metadata/dispatch tables now have explicit coverage checks
    for all current enum rows
- The mutation-maintained key-runtime index still looks solid.
- The handled-key authored lookup -> materialize -> runtime interaction seam
  remains much cleaner than it was earlier in the review chain.

## Verification

Commands run for this audit:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Results:

- full host suite passed
- firmware build passed and produced `.build/bastardkb_charybdis_4x6_noah.uf2`

Workspace scope:

- no sibling workspace folders were edited

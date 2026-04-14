# Refactor Follow-Up Review

Date: 2026-04-14

Status: follow-up quality audit of the landed runtime-sealing, runtime-debug,
and host-fixture refactor work after `review/2026-04-14-review-12/`.

Scope:

- key-runtime sealing after the `review-11` and `review-12` cleanup passes
- orchestration quality in key runtime, runtime init, and RGB runtime
- host-test seam quality after the fixture/debug refactor
- docs and review integrity against the code that exists now

## Landed Structure Update

The follow-up remediation pass that landed after this audit now uses these
seams:

- `users/noah/lib/key/runtime/key_runtime_api.h` is the only cross-module
  production entry surface for key-runtime-owned behavior.
- `users/noah/lib/key/runtime/key_runtime_internal.h`,
  `users/noah/lib/key/runtime/key_runtime_process_internal.h`, and
  `users/noah/lib/key/runtime/key_runtime_shared_state.h` are internal-only
  headers, with compile-gate allowlists limited to key-runtime owner modules,
  `runtime_shared_state_internal.h`, and the low-level white-box host suites.
- `users/noah/lib/state/runtime/runtime_debug.h` stays public and stable, but
  its implementation now lives in
  `users/noah/lib/key/runtime/key_runtime_debug.c`.
- Higher-level integration and scenario harnesses now drive the runtime
  through `noah_process_record_user()`, `key_runtime_api.h`,
  `runtime_reset.h`, and `runtime_debug.h`.
- `users/noah/runtime_init.c`, `users/noah/lib/key/runtime/key_runtime_process.c`,
  and `users/noah/lib/rgb/core/rgb_runtime.c` now express their stage order
  through explicit local stage tables, with host tests pinning runtime-init
  order and RGB render precedence.

## Findings

### No must-fix correctness regressions found in the current tree

The current tree passed the full host suite and firmware compile, and I did not
find a concrete behavior regression in the reviewed runtime, pd, debug, or RGB
surfaces.

### Should-fix: key-runtime sealing is still incomplete because `key_runtime_internal.h` remains the de facto cross-module API

References:

- `users/noah/lib/key/runtime/key_runtime_internal.h:5`
- `users/noah/runtime_init.c:14`
- `users/noah/lib/action/action_dispatch.c:9`
- `users/noah/lib/key/runtime/key_runtime_debug.c:7`
- `tests/host/key_runtime_scenario_harness.c:9`
- `tests/host/run_feature_gate_compile_tests.sh:30`

Reasoning:

- `key_runtime_internal.h` explicitly says it should stay key-runtime-local, but
  non-key-runtime production modules still include it for cross-domain entry
  points such as `noah_key_runtime_scan()` and
  `key_runtime_activate_pending_fallback_hold()`.
- That header also exports `active_key_state_t` storage layout plus many slot
  mutation helpers, so slot-layout churn still has an easy path to leak across
  the repo even though the refactor is now described as sealed.
- The compile gate blocks removed aggregate headers and internal runtime
  headers, but it does not block new cross-module dependencies on
  `key_runtime_internal.h` or direct includes of `key_runtime_shared_state.h`.

Why this matters:

- the refactor removed one broad leak but left a new wide compatibility seam in
  place
- future slot/storage changes can still ripple into unrelated modules and
  higher-level harnesses
- boundary drift is not mechanically prevented today

### Should-fix: the refactor did not converge orchestration; it left three different order-sensitive stage dialects

References:

- `users/noah/lib/key/runtime/key_runtime_process.c:17`
- `users/noah/runtime_init.c:28`
- `users/noah/lib/rgb/core/rgb_runtime.c:23`

Reasoning:

- `key_runtime_process.c` uses a local tri-state stage table and mutable
  process context.
- `runtime_init.c` still uses one-off ordered call lists for init and scan
  sequencing.
- `rgb_runtime.c` has its own compile-time-gated stage ordering with inline
  comments documenting why order matters.
- All three surfaces are order-sensitive, but the order lives as ad hoc code in
  each file instead of one coherent contract.

Why this matters:

- the refactor moved coordination complexity into orchestration files instead of
  really removing it
- changing order or adding a stage still requires bespoke control-flow edits
- there is no single seam that mechanically protects these ordering contracts

### Should-fix: higher-level host harnesses still depend on private key-runtime headers instead of the narrowed semantic seams

References:

- `users/noah/lib/key/runtime/key_runtime_process_internal.h:5`
- `users/noah/lib/key/runtime/key_runtime_internal.h:8`
- `tests/host/key_runtime_integration_harness.c:3`
- `tests/host/key_runtime_scenario_harness.c:9`
- `tests/host/pd_mode_key_runtime_integration_test.c:12`
- `tests/host/real_profile_thumb_layer_lock_integration_test.c:12`

Reasoning:

- `key_runtime_process_internal.h` is documented as a private orchestration surface and
  `key_runtime_internal.h` as key-runtime-local, but scenario and integration
  helpers still include them directly.
- That means higher-level tests remain coupled to private process and slot APIs
  for scanning and handled-key entry points instead of staying on
  `noah_process_record_user()`, `runtime_reset.h`, `runtime_debug.h`, and
  module-owned semantic readers.
- The behavioral coverage is still useful, but the seam choice keeps the
  refactor easy to bypass in future test additions.

Why this matters:

- it weakens the claim that the host-test seam cleanup fully landed
- private-header churn can still force broad high-level test churn even when
  public behavior stays stable
- it makes it harder to distinguish behavior regressions from harness-coupling
  fallout

### Optional cleanup: `docs/KEY_RUNTIME.md` still points maintainers at helper names that do not exist

References:

- `docs/KEY_RUNTIME.md:248`
- `users/noah/lib/key/runtime/key_runtime_interaction.h:155`

Reasoning:

- The doc tells maintainers to prefer `key_runtime_slot_materialize(...)` and
  `key_runtime_slot_interaction_from_resolution(...)`.
- The current header exposes `key_runtime_slot_interaction_from_materialized(...)`
  instead.
- This is small, but it undercuts the doc set in exactly the area the refactor
  was supposed to clarify.

## Solid Areas

- I did not find a concrete semantic regression from removing
  `noah_runtime_debug_snapshot_t`; the live query surface in
  `users/noah/lib/state/runtime/runtime_debug.h` still matches the behaviors
  exercised by the integration and scenario suites.
- The host fixture split is materially better than the old umbrella header.
  `tests/host/include/host_runtime_reset_fixture.h` and
  `tests/host/include/host_pd_fixture.h` are clearer, and
  `tests/host/run_feature_gate_compile_tests.sh` now enforces the removed
  umbrella-header boundary.
- The runtime reset seam looks sound after the hard-cut cleanup.
  `noah_runtime_reset_for_test()` plus the full host suite and firmware build
  both passed in the current tree.

## Verification

Commands run in this audit pass:

- `git status --short`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_index_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Current conclusion:

- no must-fix correctness regressions found in the reviewed refactor
- the strongest remaining debt is not aggregate runtime sealing anymore; it is
  the incomplete cleanup of key-runtime public/private boundaries and the
  still-fragmented orchestration model
- docs and tests are materially better than before, but the refactor has not
  fully achieved the "minimal, sealed, coherent seams" goal it now claims

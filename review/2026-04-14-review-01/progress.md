# Implementation Progress

This file tracks the architecture review captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Architecture review pass

Completed in this pass:

- Started with `git status --short` and confirmed the worktree was clean.
- Confirmed the current review lineage under `review/` and opened the next
  sortable review folder:
  `review/2026-04-14-review-01/`.
- Re-read the newest prior review in
  `review/2026-04-13-review-07/` before auditing the live code.
- Reviewed the current userspace structure across:
  - hook entry points and runtime init
  - action classification and lifecycle dispatch
  - handled-key resolution, transparent-source lookup, and slot
    materialization
  - key-runtime preflight, transition planning, scans, feedback, and slot
    storage
  - pd-mode manifest, registry, state, sync, and policy helpers
  - macro dispatch, payload IR, VIA defaults, and live VIA playback
  - runtime debug, runtime trace, and the host test surface
- Wrote a new architecture review focused on long-term extensibility for the
  fixed hardware and the current software boundaries.

Key findings recorded in this review:

- high priority: key-behavior semantics are still spread across handled-key
  policy derivation, release-contract materialization, and feedback logic, so
  new hold styles still require cross-module edits
- high priority: handled-key contextual resolution still hides global
  dependencies on `layer_state` and keymap introspection behind lookup helpers
- high priority: key-runtime cross-key coordination still relies on repeated
  whole-table sweeps and broadcast interrupts rather than explicit registries
- high priority: pd-mode split sync still transports flag masks and reconstructs
  effective display identity from registry order
- medium priority: the macro subsystem shares one IR, but hardcoded, VIA
  default, and live VIA-backed sources still do not share one source/cache
  abstraction
- medium priority: a few core files are becoming mixed-responsibility
  navigation bottlenecks even though the directory layout remains strong

Areas assessed as strong in this pass:

- the authored keymap/runtime split remains real and well documented
- the slot/effect-plan runtime architecture is still the right core model
- pd mode has a legitimate command/state layer rather than scattered hook
  branching
- build wiring through `source_manifest.mk` and `rules.mk` remains explicit
- host tests plus `runtime_debug` and `runtime_trace` provide strong refactor
  support

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed
- final `git status --short` showed only the new review folder as an untracked
  change

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-01/`

Next steps:

- move hold semantics behind one materialized behavior contract so new
  interaction kinds do not require edits in handled-key, release, and feedback
  code at the same time
- make handled-key contextual resolution explicit instead of reading layer
  stack and physical lookup state implicitly inside lookup helpers
- add explicit active/pending registries to the key runtime so cross-key policy
  does not keep growing through whole-table sweeps
- move pd-mode split sync toward explicit effective mode identity instead of
  reconstructing display state from flag masks and registry order
- add a shared macro source/provider abstraction over hardcoded, VIA-default,
  and live VIA-backed macro slots

### Implementation passes 1-5

Completed in this pass:

- Landed phase 1 by adding `handled_key_hold_semantics_t` and
  `handled_key_behavior_contract_t`, then moved key-runtime interaction,
  feedback, and pending-multi-tap release/scan logic onto the cached handled
  key contract instead of re-deriving hold semantics at each call site.
- Landed phase 2 by adding `handled_key_resolution_ctx_t`,
  `handled_key_materialized_t`, and `handled_key_materialize(...)`, then moved
  layer-stack transparency and same-position fallback resolution into the
  materializer and split the old handled-key implementation into
  `handled_key_defaults.c`, `handled_key_transparency.c`, and
  `handled_key_materialize.c`.
- Landed phase 3 by adding `key_runtime_index_state_t` plus registry rebuild
  helpers for active slots, pending multi-tap slots, preview ownership, and
  pending fallback ownership, then moved preflight, transition, feedback, and
  fallback activation off whole-table sweeps and onto the registry state.
- Landed phase 4 by introducing `pd_mode_id_t`, moving split runtime sync from
  flag masks to `active_mode_id` and `locked_mode_id`, removing registry-order
  remote display selection, and making the remote-apply path consume explicit
  pd-mode identity.
- Landed phase 5 by introducing `macro_slot_provider_t` and shared slot-cache
  helpers, moving hardcoded macro dispatch, VIA default validation/seeding, and
  live VIA playback onto provider-backed loaders, and splitting
  `macro_payload_decode_qmk_stream(...)` out of `macro_payload_run.c` into
  `macro_payload_decode_qmk.c`.
- Updated `users/noah/source_manifest.mk` and the affected host runners in the
  same passes so the build, compile gates, and focused suites all consumed the
  same refactored source surface.

Contracts and boundaries touched:

- handled-key contract/materialization:
  `handled_key_hold_semantics_t`, `handled_key_behavior_contract_t`,
  `handled_key_resolution_ctx_t`, `handled_key_materialized_t`,
  `handled_key_materialize(...)`
- key-runtime registry surface:
  `key_runtime_index_state_t`
- split runtime sync packet:
  `split_runtime_sync_packet_t.active_mode_id`,
  `split_runtime_sync_packet_t.locked_mode_id`
- macro provider/cache surface:
  `macro_slot_provider_t`, shared provider cache/load/invalidate helpers,
  `via_macro_provider_*`

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_key_behavior_lookup_tests.sh`
- `sh tests/host/run_key_runtime_slot_tests.sh`
- `sh tests/host/run_key_runtime_feedback_tests.sh`
- `sh tests/host/run_key_runtime_transition_tests.sh`
- `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
- `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
- `sh tests/host/run_key_runtime_preflight_tests.sh`
- `sh tests/host/run_runtime_debug_tests.sh`
- `sh tests/host/run_key_runtime_admission_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_layer_ownership_tests.sh`
- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_qmk_contract_checks.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- all phase-targeted host suites passed
- feature-gate compile checks passed after the new handled-key and macro source
  splits
- the full host suite passed
- the firmware build passed and produced
  `bastardkb_charybdis_4x6_noah.uf2`

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all implementation changes are confined to `charybdis-4x6/`

Next steps:

- no open implementation work remains for this review roadmap
- future follow-up, if needed, should add narrower provider-level tests around
  explicit cache invalidation for direct storage mutation paths

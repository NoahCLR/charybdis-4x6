# Userspace Architecture Review

Date: 2026-04-14
Prompt used: `prompts/initial-architecture-review.md`
Scope: `users/noah/` runtime, `keyboards/bastardkb/charybdis/4x6/keymaps/noah/` authoring surface, and the host/build enforcement around those seams.

## Findings

### must-fix

- None in the current tree. The main architecture boundaries are coherent and mechanically enforced by the split authoring/runtime entry surfaces in `users/noah/noah_keymap.h:1-17` and `users/noah/noah_runtime.h:1-25`, plus the compile-gate checks in `tests/host/run_feature_gate_compile_tests.sh:55-169`. This pass also verified the baseline with `sh tests/host/run_all_host_tests.sh` and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`, both passing.

### should-fix

- The registry-style single sources of truth are now the main maintainability risk. `users/noah/lib/action/action_kind_registry_list.h:12-105` and `users/noah/lib/pointing/defs/pd_mode_manifest.h:12-74` encode a lot of policy in long positional macro rows, and those rows are expanded into multiple independent surfaces through `users/noah/noah_keymap_ids.h:75-107` and `users/noah/lib/pointing/runtime/pd_mode_registry.c:74-75`. The current design is compact, but a misplaced argument or copy/paste error can stay syntactically valid while silently changing dispatch, lifecycle, or capability semantics. Keep the single-source property, but move these registries toward named initializer tables or narrower helper wrappers so each field is explicit and reviewable.

- `users/noah/lib/key/runtime/key_runtime_internal.h:15-68` is still an overly broad internal seam. It exposes slot lookup, slot lifecycle queries, pending multi-tap mutation, preview hints, repeat binding mutation, and hold-state mutation through one header. `tests/host/run_feature_gate_compile_tests.sh:128-169` already treats this header family as privileged, which is good evidence that the repo recognizes the seam as important. The next improvement should be to split this internal surface into smaller slice headers so future slot-model work does not require friend-style access to most of the key runtime at once.

### optional cleanup

- The keymap is correctly data-owned, but the materialization path is still the least inspectable part of the authoring surface. `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:62-140` defines macro-slot and combo data through preprocessor tables, and `users/noah/keymap_materialize.h:6-32` turns those into exported runtime symbols. This is workable today, but it will keep getting harder to review and tool if more authored surfaces are added as macro DSLs instead of typed arrays. If this area changes again, migrate one surface at a time toward explicit `const` tables rather than extending the macro layer further.

- `users/noah/lib/pointing/runtime/pd_runtime.c:60-126` is a small but mixed-responsibility bridge. It currently owns pointing init defaults, mouse-record classification, idle-noise suppression, active-mode handler dispatch, and layer-state sniping handoff. The file is still readable and not urgent, but it is the natural place where future pointing work will accumulate unrelated concerns. Split it only when the next pointing feature lands; there is no need for a speculative rewrite now.

## Solid Areas

- The authoring/runtime boundary is explicit and disciplined. `users/noah/noah_keymap.h:1-17` stays keymap-facing, `users/noah/noah_runtime.h:1-25` stays hook/runtime-facing, and `tests/host/run_feature_gate_compile_tests.sh:55-169` actively enforces that split.

- Source ownership is centralized instead of mirrored. `users/noah/source_manifest.mk:1-94` is the single source inventory for userspace build participation, and the feature-gate compile runner consumes that manifest directly.

- Runtime entry ordering is readable and testable. `users/noah/runtime_init.c:28-63` makes init/scan/post-init staging explicit, and `users/noah/lib/key/runtime/key_runtime_process.c:129-162` makes `process_record_user()` flow explicit instead of burying policy in scattered hook code.

## Current Architecture Assessment

The current userspace is in good architectural shape. The hard parts are mostly right: authored profile data remains keymap-owned, shared runtime policy lives under `users/noah/`, hook integration is centralized, and the host/build enforcement is stronger than typical personal firmware repos. The main remaining risk is not missing tests or uncontrolled sprawl. It is the maintainability cost of the macro-driven registry surfaces and the breadth of one internal key-runtime seam.

## Reconciliation Note

Reliability remediation landed after this initial review snapshot without changing the main architecture assessment. The original `should-fix` findings remain open, but two runtime seams are now intentionally broader than described earlier:

- the shared runtime hook surface now includes `housekeeping_task_user()` via `users/noah/noah_runtime.h`, `users/noah/hooks.c`, and `users/noah/runtime_init.c` so time-driven repeat dispatch can run after QMK event processing instead of from `matrix_scan_user()`;
- the shared output seam in `users/noah/lib/action/action_dispatch.h` and `users/noah/lib/action/action_dispatch.c` now owns masked synthetic-QMK taps, and `users/noah/lib/pointing/modes/pd_mode_arrow.c` consumes that helper instead of open-coding modifier suspend/apply logic.

This is an intentional strengthening of the existing “centralized hooks and centralized emit policy” design, not a boundary regression. The remediation was covered by `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_runtime_init_order_tests.sh`, `sh tests/host/run_action_dispatch_tests.sh`, `sh tests/host/run_pd_mode_handlers_tests.sh`, `sh tests/host/run_held_action_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

## Follow-Up Audit

Date: 2026-04-14  
Prompt used: `prompts/follow-up-architecture-audit.md`

### Findings

#### must-fix

- None in the landed remediation. The new post-event repeat scheduling and masked synthetic-emit path match the intended design and are mechanically covered by the added host checks.

#### should-fix

- None introduced by this remediation pass. The landed changes strengthen the existing hook/output seams rather than introducing a new cross-boundary runtime shortcut.

#### optional cleanup

- `users/noah/lib/action/action_dispatch.h:185-191` now exposes both the generic `noah_emit_policy_t` helpers and one specialized masked synthetic-tap helper. That split is coherent for one caller and well-tested in `tests/host/action_dispatch_test.c:454-520`, but if another masked-output case appears the emit surface should be revisited before helper proliferation turns into a second policy DSL.

## Prior Finding Status

- `open`: registry-style single sources of truth remain the main maintainability risk. `users/noah/lib/action/action_kind_registry_list.h:12-105` and `users/noah/lib/pointing/defs/pd_mode_manifest.h:12-74` are unchanged by this remediation.
- `open`: `users/noah/lib/key/runtime/key_runtime_internal.h:15-68` is still too broad for an internal seam. This remediation avoided making that seam wider, but it did not narrow it.
- `open`: the keymap materialization macros in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:62-140` and `users/noah/keymap_materialize.h:6-32` remain the least inspectable part of the authoring surface.
- `open`: `users/noah/lib/pointing/runtime/pd_runtime.c:60-126` still mixes pointing init defaults, mouse-record classification, idle-noise suppression, mode dispatch, and sniping handoff.

## Solid Areas

- The hook integration remained coherent after adding the new post-event seam. `users/noah/noah_runtime.h:16-26`, `users/noah/hooks.c:19-29`, and `users/noah/runtime_init.c:40-60` keep the userspace-owned QMK hook surface centralized, and `tests/host/hook_chaining_test.c:264-345` plus `sh tests/host/run_hook_chaining_tests.sh` mechanically cover both weak-default and chained-override behavior.
- The repeat fix improved correctness without smearing policy into unrelated modules. `users/noah/runtime_init.c:52-60` moved repeat ticking into housekeeping, `users/noah/lib/key/runtime/key_runtime_scan.c:14-20` stopped mixing repeat dispatch into matrix-scan orchestration, and `tests/host/runtime_init_order_test.c:107-147` plus `tests/host/held_action_test.c:281-303` cover both the new staging and the dropped-backlog timing policy.
- The masked synthetic emit landed on the intended shared output seam. `users/noah/lib/action/action_dispatch.c:54-70` now owns “settle, filter mods, emit, restore post-settlement state,” `users/noah/lib/pointing/modes/pd_mode_arrow.c:29-30` uses that seam instead of open-coded mod suspension, and `tests/host/action_dispatch_test.c:474-497` plus `tests/host/pd_mode_handlers_test.c:543-570` verify the contract.
- Review and user-facing docs now match the current tree. `docs/KEY_RUNTIME.md:154-158` documents post-event repeat scheduling, `docs/KEY_RUNTIME.md:268-274` documents the masked emit helper, and `docs/ADDING_PD_MODE.md:195-200` now points PD-mode authors at the shared seam instead of manual mod-state choreography.

## Current Conclusion

The remediation landed cleanly. It fixes the identified runtime hazards by strengthening existing architecture boundaries rather than bypassing them: time-driven repeat work now runs in a post-event hook, and temporary modifier filtering now lives in the centralized emit layer. The active review thread remains open because the original maintainability findings are still unresolved, not because this remediation introduced a new boundary problem.

## Remaining Open Findings

- Move the registry-style action and pd-mode manifests toward more explicit row shapes or named initializer tables.
- Split `users/noah/lib/key/runtime/key_runtime_internal.h` into narrower internal slice headers.
- Revisit the authored keymap materialization macros if more authored surfaces are added.
- Split `users/noah/lib/pointing/runtime/pd_runtime.c` when the next pointing feature lands instead of letting it accumulate more responsibilities.

## Recommended Next Refactor Sequence

1. Refactor `NOAH_ACTION_KIND_REGISTRY` first. It has the highest architecture leverage and is already covered by the action dispatch, action lifecycle, key runtime, and full host suites.
2. Split `key_runtime_internal.h` into smaller internal slice headers without changing behavior. Keep `tests/host/run_feature_gate_compile_tests.sh` strict while narrowing the allowlists.
3. Only after those two steps land, decide whether `NOAH_PD_MODE_LIST` or `MATERIALIZE_KEYMAP_DATA()` need further cleanup. Do not rewrite every macro-driven surface in one pass.

## Verification Baseline

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Sibling workspace folders touched: none

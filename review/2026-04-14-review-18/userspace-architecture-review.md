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

Additional pd-mode remediation also landed after that snapshot without changing the overall assessment:

- the private pd-mode hook surface in `users/noah/lib/pointing/runtime/pd_mode_registry_internal.h`, `users/noah/lib/pointing/runtime/pd_mode_registry.c`, and `users/noah/lib/pointing/modes/pd_mode_pinch.c` now owns buffered single-tap replay masking for mode-owned real modifiers;
- `users/noah/lib/key/interaction/multi_tap_engine.c` still owns the actual delayed-tap snapshot, but it now queries pd-mode policy through `pd_mode_buffered_tap_masked_real_mods(...)` instead of embedding pinch-specific behavior;
- `users/noah/lib/state/ownership/keyboard_mod_ownership.c` provides the generic `keyboard_mod_ownership_managed_only_mask(...)` helper so buffered taps strip only managed-only modifiers and preserve physically held GUI state.

This keeps the policy with the pd mode that owns the modifier while leaving the generic key runtime responsible only for snapshot timing. The change was covered by `sh tests/host/run_keyboard_mod_ownership_tests.sh`, `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_real_profile_validation_tests.sh`, `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `sh tests/host/run_key_runtime_transition_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

Another pd-mode/runtime remediation landed after that buffered-tap work without changing the broader architecture assessment:

- the shared QMK hook surface in `users/noah/noah_runtime.h`, `users/noah/hooks.c`, and `users/noah/lib/key/runtime/key_runtime_process.c` now includes `pre_process_record_user()` and `post_process_record_user()` so userspace can track physically pressed modifiers before `process_record_user()` and restore any temporarily masked mode-owned real modifiers after the event;
- the private pd-mode hook surface now also owns concurrent keyboard-event masking policy through `users/noah/lib/pointing/runtime/pd_mode_keyboard_event_internal.h`, `users/noah/lib/pointing/runtime/pd_mode_registry_internal.h`, and `users/noah/lib/pointing/runtime/pd_mode_registry.c`, while `users/noah/lib/key/runtime/key_runtime_process.c` remains the only generic runtime caller;
- `users/noah/lib/pointing/modes/pd_mode_pinch.c` now reuses the same managed-only `GUI` policy for both buffered tap replay and concurrent keyboard-event masking, and the transient keyboard-event mask state lives in `users/noah/lib/key/runtime/key_runtime_shared_state.h` instead of a process-local static.

This is an intentional strengthening of the existing centralized-hook and mode-owned-policy design. It keeps the mode-owned modifier scoped to pointing behavior without stripping a physically held user modifier, and it removes a redundant pinch-specific static latch that was only compensating for behavior the pd-mode lifecycle already guarantees. The change was covered by `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

One more runtime follow-up landed after that hook expansion without changing the broader architecture assessment:

- the shared hook surface in `users/noah/noah_runtime.h`, `users/noah/hooks.c`, and `users/noah/lib/key/runtime/key_runtime_process.c` now includes an explicit `noah_process_record_user_finalize(...)` helper so temporary keyboard-event masking is finalized on the final QMK result instead of the helper-local `noah_process_record_user(...)` result;
- the common integration/scenario host harnesses in `tests/host/key_runtime_integration_harness.c` and `tests/host/key_runtime_scenario_harness.c` now model real QMK `pre/process/finalize-or-post` flow instead of calling `noah_process_record_user()` directly;
- the strong chaining contract is now explicit: overrides may still narrow the shared result, but if they return `false` after chaining they must finalize the shared runtime first, and `tests/host/hook_chaining_test.c` now covers that exact false-after-chain pattern.

The later follow-up review gaps in that cleanup are now closed as well. `users/noah/noah_runtime.h:8-11` now documents the broader rule that any chained override returning a final `false` must finalize first, not only `&&`-style narrowing wrappers; `tests/host/hook_chaining_test.c:218-246,482-499` now also covers an explicit pass-through `if (!noah_process_record_user(...)) return false;` shape; and the default weak `post` behavior in `tests/host/key_runtime_integration_harness.c:37-38` now matches production by calling finalize on the `true` path. That harness behavior is mechanically enforced by `tests/host/key_runtime_integration_harness_test.c:1-83`, `tests/host/run_key_runtime_integration_harness_tests.sh`, `tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

This keeps the QMK-shaped `pre/process/post` hooks intact while giving the shared runtime one repo-local seam for final outcome cleanup. That is cleaner than pushing the fix into upstream `process_record_kb()` or relying on keymap-local overrides to remember post-hook semantics that QMK itself will skip on final `false`. The change was covered by `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_key_runtime_scenario_tests.sh`, `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

I reran the follow-up audit after the two remediation-specific should-fix items were addressed. No new findings were introduced by that last cleanup. `users/noah/noah_runtime.h:8-12` now states the correct final-false contract, `tests/host/hook_chaining_test.c:224-250,499-518` covers both the `&&` narrowing shape and the pass-through false shape, `tests/host/key_runtime_integration_harness.c:31-39` now gives the weak default `post` path the same finalize-on-true behavior as production, and `tests/host/key_runtime_integration_harness_test.c:1-91` plus `tests/host/run_all_host_tests.sh:37-40` mechanically enforce that harness behavior in the full host suite. The original thread-level maintainability findings remain open, but the remediation-specific review gaps for this hook/harness fix are closed.

Another narrow handled-key runtime adjustment landed later on `arrowmodefix2` without changing the broader architecture assessment. The earlier emit-policy theory turned out not to be the durable seam. The current tree now does the narrower thing instead:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` maps authored release-time pd-mode lock taps straight onto the native `KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP` effect instead of representing them as a generic dispatched action;
- this lines `KC_RIGHT_ALT` tap behavior up with the existing quick-tap lock path for real pd-mode keys, so release-time lock toggles no longer depend on the generic action emit wrapper at all;
- the strengthened host harness now links `users/noah/lib/pointing/runtime/pd_runtime.c` and `users/noah/lib/pointing/policy/pointer_layer_policy.c`, so the right-alt integration test exercises the real `auto_mouse_layer_off() -> layer_off() -> noah_layer_state_set_user()` activation stack instead of a direct layer-bit clear stub.

Inference from the current code and tests: the relevant distinction was not split sync and not the generic pd-mode core, but the fact that authored release taps to lock actions were taking a different runtime effect path than native pd-mode quick-lock releases. The current mapping removes that divergence while preserving the overlap-remediation behavior for ordinary tap actions.

Another handled-key follow-up landed after that right-alt work when the broader `layer_interrupted` model introduced during overlap remediation proved unstable on hardware. The current tree now does the narrower thing instead:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h`, `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c`, and `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` now treat interruption as a momentary-layer-tap-only concern rather than a generic authored-hold quick-release suppression bit;
- the current tree also keeps a separate foreign-overlap lifecycle fact for immediate-hold keys, so overlap-aware quick-release suppression on those paths no longer borrows the momentary-layer latch and instead only blocks reopening a quick tap or first-tap chain after the key was actually used as a hold;
- `users/noah/lib/key/runtime/key_runtime_transition.c` no longer classifies deferred-release blockers by feeding live slots through the generic release resolver; it uses a separate phase/ownership helper so held or repeating owners stop blocking once they have committed their owned state;
- `users/noah/lib/key/runtime/key_runtime_transition.c` also now treats active slots that are still carrying a pending multi-tap hold as non-blockers, because those slots resolve through the dedicated pending-multi-tap reducer with saved mods rather than through the ordinary tap-release overlap path;
- `users/noah/lib/state/runtime/runtime_debug.h`, `users/noah/lib/key/runtime/key_runtime_debug.c`, and the updated host suites now expose and assert slot phase plus momentary-layer interrupt state directly, which closes the earlier blind spot where output-only assertions could miss poisoned slot state.

Inference from the current code and tests: the failed theory was not “all authored quick-release taps should share one interrupt latch,” but the opposite. Momentary-layer tap cancellation is a specific rule, immediate-hold overlap is a separate rule, and pd-mode quick-lock taps plus deferred-release blocking each need their own narrower contract.

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

## Follow-Up Audit

Date: 2026-04-15  
Prompt used: `prompts/follow-up-architecture-audit.md`

### Findings

#### must-fix

- None in the landed buffered-tap remediation. The implementation moved policy out of the generic key runtime and into mode-owned pd hooks without regressing the verified runtime behavior.

#### should-fix

- None introduced by the seam narrowing pass. The buffered-tap policy query no longer leaks through the public pd-mode header.

#### optional cleanup

- None introduced by the seam narrowing pass. The follow-up coverage cleanup now exercises the authored transparent replay path directly inside the pd-mode integration harness.

## Prior Finding Status

- `open`: registry-style single sources of truth remain the main maintainability risk. `users/noah/lib/action/action_kind_registry_list.h:12-105` and `users/noah/lib/pointing/defs/pd_mode_manifest.h:12-74` are still positional registry DSLs, unchanged by this remediation.
- `resolved`: the buffered-tap policy query no longer leaks through the public pd-mode API. The public declaration was removed from `users/noah/lib/pointing/defs/pd_modes.h:80-97`, the dedicated internal seam now lives in `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h:1-13`, and the only repo-owned production callers are `users/noah/lib/pointing/runtime/pd_mode_registry.c:10,148-150` and `users/noah/lib/key/interaction/multi_tap_engine.c:10-15,79-80`. This is mechanically enforced by `tests/host/run_feature_gate_compile_tests.sh:170-191`, which restricts the new internal header to pd runtime owner modules plus `multi_tap_engine.c` in production and to `tests/host/pd_mode_test.c` in host tests. Verified with `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- `open`: `users/noah/lib/key/runtime/key_runtime_internal.h:15-68` is still too broad for an internal seam. This remediation avoided widening that seam, but it did not narrow it.
- `open`: the keymap materialization macros in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:62-140` and `users/noah/keymap_materialize.h:6-32` remain the least inspectable part of the authoring surface.
- `open`: `users/noah/lib/pointing/runtime/pd_runtime.c:60-126` still mixes pointing init defaults, mouse-record classification, idle-noise suppression, mode dispatch, and sniping handoff.

## Solid Areas

- The remediation did improve separation of concerns versus the earlier tactical fix. Mode-owned buffered tap policy now lives with `PINCH_MODE` in `users/noah/lib/pointing/modes/pd_mode_pinch.c:16-22,49-56`, while `users/noah/lib/key/interaction/multi_tap_engine.c:60-80` remains responsible only for delayed tap snapshot timing.
- The narrowed internal seam now matches the intended ownership story. `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h:1-13` is the only exported declaration for the query, `users/noah/lib/key/interaction/multi_tap_engine.c:10-15,79-80` uses that narrow seam, and the broader public pd-mode header no longer carries buffered-tap replay policy.
- The modifier filtering logic was factored into a genuinely reusable ownership helper instead of another pinch-specific branch. `users/noah/lib/state/ownership/keyboard_mod_ownership.c:198-218` answers the generic “managed-only bits” question and is directly covered by `tests/host/keyboard_mod_ownership_test.c:121-164` and `sh tests/host/run_keyboard_mod_ownership_tests.sh`.
- The new behavior claims are mechanically covered at both the pd-mode seam and the runtime integration seam. `tests/host/pd_mode_test.c:539-548` checks the mode-owned query itself, and `tests/host/pd_mode_key_runtime_integration_test.c:543-632` checks both “managed-only GUI masked” and “physically held GUI preserved.” This was verified with `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- Review notes and user-facing docs now describe the landed structure instead of the discarded tactical fix. `docs/ADDING_PD_MODE.md:36-42,353-367` points mode authors at mode-owned lifecycle hooks for buffered tap replay policy, and the active review note’s reconciliation section now matches the current tree.

## Current Conclusion

The implementation is materially cleaner than the tactical pinch fix, and the follow-up seam narrowing finished the architecture cleanup that the prior audit asked for. Policy now lives with the pd mode that owns the modifier, the generic key runtime only applies that policy at the correct snapshot point, the buffered-tap query is now sealed behind a dedicated internal header with compile-gate enforcement, and the pinch integration test now exercises the authored transparent replay path directly.

## Remaining Open Findings

- Move the registry-style action and pd-mode manifests toward more explicit row shapes or named initializer tables.
- Split `users/noah/lib/key/runtime/key_runtime_internal.h` into narrower internal slice headers.
- Revisit the authored keymap materialization macros if more authored surfaces are added.
- Split `users/noah/lib/pointing/runtime/pd_runtime.c` when the next pointing feature lands instead of letting it accumulate more responsibilities.

## Closure Verification Review

Date: 2026-04-15  
Prompt used: `prompts/closure-verification-review.md`

### Findings

#### must-fix

- None. The active review folder, the user-facing pd-mode docs, the compile-gate allowlists, and the current buffered-tap implementation all describe the same landed structure. The thread is not blocked by a correctness regression or documentation mismatch; it stays open because earlier architecture findings are still unresolved.

#### should-fix

- The thread is not ready to close while the positional registry DSLs remain open. `users/noah/lib/action/action_kind_registry_list.h:12-22` and `users/noah/lib/pointing/defs/pd_mode_manifest.h:68-74` still pack multiple semantic fields into long positional macro rows that are expanded into dispatch, metadata, and identity surfaces elsewhere. This was the main maintainability finding from the initial review, and no later remediation has narrowed or mechanically enforced a safer row shape.

- The thread is not ready to close while `users/noah/lib/key/runtime/key_runtime_internal.h:15-68` remains the same broad internal seam called out in the initial review. `tests/host/run_feature_gate_compile_tests.sh:128-169` still correctly treats that header family as privileged, but the seam itself still aggregates slot lookup, interaction queries, pending multi-tap mutation, preview hints, repeat binding mutation, and hold-state mutation through one include.

#### optional cleanup

- The authoring materialization macros and the mixed-responsibility pointing bridge remain acceptable but unchanged growth hotspots. `users/noah/keymap_materialize.h:27-32` still expands authored macro and combo tables through preprocessor materialization, and `users/noah/lib/pointing/runtime/pd_runtime.c:60-126` still combines pointing init defaults, mouse-record classification, idle-noise suppression, mode dispatch, and sniping handoff in one bridge module.

## Prior Finding Status

- `open`: registry-style single sources of truth remain the main maintainability risk. `users/noah/lib/action/action_kind_registry_list.h:12-22` and `users/noah/lib/pointing/defs/pd_mode_manifest.h:68-74` are still positional registry DSLs, unchanged by the later remediation passes.
- `resolved`: the buffered-tap policy query no longer leaks through the public pd-mode API. The public declaration is gone from `users/noah/lib/pointing/defs/pd_modes.h:74-97`, the dedicated internal seam now lives in `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h:1-13`, and the only repo-owned production includes are `users/noah/lib/pointing/runtime/pd_mode_registry.c:10` and `users/noah/lib/key/interaction/multi_tap_engine.c:9-15`. This is mechanically enforced by `tests/host/run_feature_gate_compile_tests.sh:172-191`, and behavior remains covered by `tests/host/pd_mode_test.c:540-548` and `tests/host/pd_mode_key_runtime_integration_test.c:543-632`. Verified by the already-passed commands `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`, `sh tests/host/run_feature_gate_compile_tests.sh`, `sh tests/host/run_all_host_tests.sh`, and `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
- `open`: `users/noah/lib/key/runtime/key_runtime_internal.h:15-68` is still too broad for an internal seam. Later remediation avoided widening it, but the header has not been split.
- `open`: the keymap materialization path in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:62-140` and `users/noah/keymap_materialize.h:27-32` remains the least inspectable part of the authoring surface.
- `open`: `users/noah/lib/pointing/runtime/pd_runtime.c:60-126` still mixes pointing init defaults, mouse-record classification, idle-noise suppression, mode dispatch, and sniping handoff.

## Solid Areas

- The buffered-tap remediation and the seam narrowing both remain mechanically enforced. `users/noah/lib/key/interaction/multi_tap_engine.c:9-15,60-80` still applies mode-owned policy only at delayed-tap snapshot time, and `tests/host/run_feature_gate_compile_tests.sh:172-191` still prevents that query from leaking back into broader production or host-test surfaces.
- The pinch regression coverage now exercises the real transparent replay contract instead of a synthetic tap stand-in. `tests/host/pd_mode_key_runtime_integration_test.c:48-52` now keeps `PINCH_MODE` authored as `TAP_SENDS(KC_TRNS)`, `tests/host/pd_mode_key_runtime_integration_test.c:90-111` provides a local keymap-backed transparent source path with a lower raw `LT(..., KC_J)` key, and `tests/host/pd_mode_key_runtime_integration_test.c:580-628` verifies that delayed replay resolves to `KC_J` while still masking only managed-only GUI state.

- The active review folder is internally coherent after the follow-up audits. The current review notes, `docs/ADDING_PD_MODE.md:36-42,353-367`, and the landed pd-mode runtime files all describe the same ownership model: pd modes own buffered-tap replay policy, while the generic key runtime owns snapshot timing.

- The closure bar evidence for the resolved buffered-tap seam work is present and specific. The latest green baseline in `review/2026-04-14-review-18/progress.md` now includes both the seam-narrowing verification and the later transparent-path cleanup rerun of the targeted integration suite, full host suite, and firmware compile, so there is no missing verification evidence for that resolved finding.

## Verification Integrity

- The closure-verification assessment itself did not run new commands at review time, but the later transparent-path cleanup revalidated the tree with the current required baseline recorded in `review/2026-04-14-review-18/progress.md`.
- Current closure evidence includes:
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Current Conclusion

The buffered-tap work itself is in good shape and the active notes no longer contain contradictory claims about that seam. The direct transparent-path coverage gap is now closed as well. This thread should still remain open, because the original architecture findings that were explicitly kept as `should-fix` work are still open: the positional registry DSLs have not been made more explicit, and `key_runtime_internal.h` has not been narrowed.

## Closure Verdict

`keep thread open`

I reran the closure verification after the later hook/harness cleanup. The verdict does not change. The remediation-specific findings for the final-outcome cleanup are now closed, but the thread still fails the closure bar because the original open `should-fix` items are still present: the registry DSLs remain positional and `users/noah/lib/key/runtime/key_runtime_internal.h` is still a broad internal seam. No new correctness or review-integrity blockers were found in the rerun.

## Remaining Open Findings

- Move the registry-style action and pd-mode manifests toward more explicit row shapes or named initializer tables.
- Split `users/noah/lib/key/runtime/key_runtime_internal.h` into narrower internal slice headers.
- Revisit the authored keymap materialization macros if more authored surfaces are added.
- Split `users/noah/lib/pointing/runtime/pd_runtime.c` when the next pointing feature lands instead of letting it accumulate more responsibilities.

## 2026-04-19 Hardware Regression Reconciliation Note

The earlier audit and closure-verification snapshots above should now be read as audit-time baselines, not the current truth of the hardware. A new hardware-visible regression is open again: authored-key overlaps can wedge the runtime after layer, pd-mode, or modifier transitions. The detailed capture lives in `review/2026-04-14-review-18/authored-key-overlap-wedge-regression.md`.

This new evidence changes the current correctness assessment:

- `must-fix`: open again. The tree has a real hardware regression even though the present host matrix can still pass.
- The most likely failure class is a deterministic cleanup leak between pre-userspace QMK side effects and userspace key-runtime ownership cleanup after the live layer/pointer/pd/modifier context diverges from the original press identity.
- The host harness was initially missing at least one real hardware seam. The current follow-up work now models press-time release identity across layer changes and the pre-userspace auto-mouse ownership path more faithfully, but host green should still not be treated as sufficient evidence that the overlap/release path is closed until the board is revalidated.

One concrete remediation landed during that follow-up: `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c` had been double-owning auto-mouse state for some anchored pd modes by combining a synthetic lifecycle anchor with either the normal held-key auto-mouse path or a lock-owned `auto_mouse_toggle()` path. The lifecycle code now limits the synthetic anchor to locked non-toggle modes, and the host matrix locks that contract in `tests/host/pd_mode_test.c` plus the real-profile overlap harness.

This note reconciles the older "no correctness blocker found" audit wording with the current hardware state so the active review folder stays internally coherent.

## 2026-04-19 Runtime V2 Foundation Note

The first implementation pass of the single-authority runtime redesign is now landed in parallel with the legacy runtime. This does **not** cut production behavior over yet, and it does **not** close the authored-key wedge by itself. What landed is the new internal structure and validation substrate that the redesign plan required before cutover:

- `users/noah/lib/runtime_v2/runtime_v2.h`, `runtime_v2.c`, and `runtime_v2_trace.c` define the new typed v2 model: immutable normalized input events, press-token/tap-series/lease/persistent-intent records, and a projection snapshot contract.
- `users/noah/lib/state/runtime/runtime_trace.h` and `runtime_trace.c` now carry a dedicated `NOAH_TRACE_RUNTIME_V2` kind plus a console dump path for snapshot inspection under runtime trace flags.
- `tests/host/key_runtime_integration_harness.c` now records normalized input events from the real hook path and emits projection checkpoints after semantic events.
- `tests/host/real_profile_thumb_layer_lock_integration_test.c` now proves trace-capture and replay parity for the two most important open repro families: raw nav into `DRAGSCROLL`, and `KC_LEFT_GUI` second-tap hold to `KC_LEFT_ALT` with repeated nav-arrow taps.

This foundation changes the architecture direction in one important way: the review thread now has an explicit migration path away from "release re-derives meaning from the live world" toward "press resolves once, leases own state, projection recomputes from owned records." That is the right direction for the wedge class.

It does **not** yet resolve the earlier open finding about `users/noah/lib/key/runtime/key_runtime_internal.h` being too broad, because the live production behavior still runs through the legacy runtime. The v2 subtree reduces the need to widen that seam again, but the closure bar for that finding stays open until production behavior migrates off it and the old seam can be narrowed or deleted.

## 2026-04-19 Runtime V2 Identity Shadow Note

The next runtime-v2 pass is now landed as shadow behavior, still without production cutover:

- `users/noah/lib/runtime_v2/runtime_v2.c` now owns a real shadow reducer for immutable press identity, release-by-position, basic hold promotion timing, and tap-series lifetime instead of acting as a projection-only container.
- `tests/host/key_runtime_integration_harness.c` can now feed normalized events into runtime-v2 through an explicit adapter seam, and `tests/host/key_runtime_integration_runtime_v2_adapter.c` enables that path only in suites that are intentionally exercising the shadow reducer.
- `tests/host/runtime_debug_test.c` now locks the first reducer invariants directly: release-keycode mismatch must still retire the press bound to the physical key position, timer/scan advancement must not rewrite press identity, and tap-series state must remain independent from active press-token storage.
- That new white-box coverage immediately found one real shadow-runtime bug: pending-hold tap series were expiring on elapsed tap time even while the matching second press was still active. `users/noah/lib/runtime_v2/runtime_v2.c` now keeps a pending-hold series alive until the owning press resolves, which is exactly the kind of state isolation the redesign needs.

Inference from the current tree: the right migration shape is still the reducer/lease model, but the validation strategy also matters. The shadow runtime is now strong enough to catch architectural leaks inside the replacement core before any production cutover. That is progress, but it still does **not** close the authored-key wedge or the broader internal-seam finding yet.

## 2026-04-19 Runtime V2 Layer/Modifier Lease Note

The next shadow-runtime pass is now also landed:

- `users/noah/lib/runtime_v2/runtime_v2.c` now owns real token-backed leases for the first shared state domains instead of only tracking token/tap-series timing:
  - raw `MO(layer)` presses create immediate layer leases,
  - raw `LT(layer, key)` presses promote into layer leases only on hold,
  - raw physical modifier keys create physical modifier leases on press, and
  - raw `MT(mod, key)` presses promote into managed modifier leases on hold.
- The v2 state now keeps a reducer-owned shadow projection for layer and modifier state plus a simple persistent layer-lock intent, so release and cancellation cleanup is already happening in the right shape: remove owned records, then recompute projection.
- `tests/host/runtime_debug_test.c` now locks the first ownership-projection invariants directly, including token replacement cancelling old owned leases immediately instead of leaving stale layer state behind.

Inference from the current tree: the replacement core has now crossed the line from "observes timing" to "owns real shared state" for layer/mod domains. That is the first meaningful proof that the lease model can replace the old branchy cleanup style. It still does **not** cover pd-mode or pointer-anchor ownership yet, so the wedge-prone pointer/layer overlap class remains open until those domains migrate too.

## 2026-04-19 Runtime V2 Pd-Mode/Pointer Ownership Note

The next shadow-runtime pass is now landed as well:

- `users/noah/lib/runtime_v2/runtime_v2.c` now owns token-backed active pd-mode leases for raw pd-mode keys, token-backed pointer-anchor leases for momentary anchored modes, persistent pd-mode lock intents, and persistent pointer-toggle intents for lock-owned auto-mouse-toggle modes.
- The shadow projection now recomputes pd-mode active/locked state plus pointer anchor/toggle/prefer-typing policy from those owned records instead of from imperative cleanup sequencing.
- The shadow reducer also preserves the production exclusivity rule: a newly activated raw pd-mode key clears foreign active pd-mode leases and foreign lock/toggle intents before becoming authoritative.
- `tests/host/runtime_debug_test.c` now locks those rules directly, including the `DRAGSCROLL` lock-owned toggle shape and foreign-mode supersession cleanup.

Inference from the current tree: the replacement core now owns the same general state family as the original `NAV -> DRAGSCROLL` wedge path, not just the safer layer/mod domains. That is the first meaningful runtime-v2 pass that can speak to the suspected pointer/pd ownership leak class directly. It still does **not** observe emitted authored `*_LOCK` actions yet, so authored lock paths are not fully inside the shadow model until that next seam lands.

## 2026-04-19 Runtime V2 Live Lock Observation Note

The next shadow-runtime seam is now landed too:

- `users/noah/lib/state/ownership/layer_ownership.c` now feeds successful `layer_ownership_set_lock_state(...)` mutations into `runtime_v2_layer_lock_set(...)`.
- `users/noah/lib/pointing/runtime/pd_mode_state.c` now feeds successful `pd_mode_set_lock_state(...)` mutations into `runtime_v2_pd_mode_lock_set(...)`.
- That means the shadow reducer no longer depends on direct test-only lock APIs to see persistent lock state changes. Real authored lock actions and native key-runtime lock taps now enter the shadow model through the same production setters that own the authoritative lock state.
- `tests/host/runtime_debug_test.c` now locks those setter-driven observation paths directly instead of only asserting the direct runtime-v2 helper calls.
- Focused host runners that intentionally keep the full reducer out of process now share one central `tests/host/runtime_v2_observer_stub.c` instead of each growing their own local runtime-v2 stubs.

Inference from the current tree: the earlier review note about authored `*_LOCK` actions sitting completely outside the shadow model is no longer true for persistent layer/pd lock mutations. The remaining gap is further downstream: production still runs the legacy runtime for the actual release/deferred-emission logic, so the cleanup decisions that can wedge the board are still being made outside the reducer-owned lease model.

## 2026-04-19 Runtime V2 Pending Release Ownership Note

The next release-path seam is now landed as well:

- `users/noah/lib/key/runtime/key_runtime_shared_state.h` now stores deferred release entries with `key_pos`, `action`, and saved mod snapshot instead of only `action + mods`.
- `users/noah/lib/key/runtime/key_runtime_release.c` now passes that real owner key position into the deferred queue and into runtime-v2 observer hooks on both enqueue and drain.
- `users/noah/lib/runtime_v2/runtime_v2.c` now keeps a separate `pending_release_t` record family plus a dedicated `v2_pending_release_count`, instead of pretending deferred release work can always stay attached to the single active/released token slot for a key position.
- `tests/host/key_runtime_release_matrix_test.c` now mechanically proves the production deferred queue retains the releasing key position, and `tests/host/runtime_debug_test.c` now proves a v2 pending-release record survives a same-key re-press until the deferred action actually drains.

Inference from the current tree: deferred release is no longer an anonymous action FIFO in either the legacy debug surface or the v2 shadow model. That is a real architectural step forward. The deeper problem still remains open, though: the decision about whether a deferred release is blocked still comes from `key_runtime_transition_has_foreign_tap_release_slot_except(...)` and `key_runtime_transition_has_any_tap_release_slot(...)`, which re-derive blocker state by scanning the live slot world instead of consulting reducer-owned blocker records.

## 2026-04-19 Runtime V2 Native Blocker Derivation Note

The blocker seam has now crossed the next architectural boundary inside the shadow reducer:

- `users/noah/lib/runtime_v2/runtime_v2.c` now derives blocker semantics natively from reducer-owned state instead of mirroring blocker truth from legacy slot/index ownership.
- `runtime_v2_press_token_begin(...)` now resolves handled-key materialization once on press against shadow layer state, stores blocker-relevant contract fields on the immutable press token, and latches foreign-key interruption on already-active tokens inside the reducer.
- `runtime_v2_deferred_release_blocker_count_for_keypos(...)` and projection capture now compute blocker counts by asking whether live press tokens currently block deferred release based on:
  - press-time handled-key contract,
  - token interruption latches,
  - token timing,
  - pending-hold tap-series state, and
  - whether the token has already crossed into owned hold state.
- `runtime_v2_observe_deferred_release_blocker_profile(...)` remains present only as a compatibility hook while the mixed architecture still exists; blocker semantics inside v2 no longer depend on observer-fed blocker profiles.
- `tests/host/runtime_debug_test.c` now proves native authored blocker shapes instead of synthetic mirrored profiles, including:
  - an interrupted momentary-layer tap that blocks only before hold ownership settles,
  - a plain handled tap that still blocks after `tap_hold_term`, and
  - reducer-owned interruption clearing a first token's quick-tap blocker when a foreign press arrives.

Reconciliation Note:

- The earlier "Runtime V2 Blocker Observation Note" below is now an audit-time snapshot only.
- It still describes the intermediate step correctly, but it is no longer the current architecture for blocker semantics inside `runtime_v2`.
- The current tree now derives blocker truth natively in the reducer and uses the observer hook only for compatibility during migration.

Inference from the current tree: blocker timing and blocker semantics are now both modeled inside `runtime_v2`. That is a meaningful reduction in split-brain risk. The remaining open architecture gap is production ownership: the real release/deferred-emission path still runs through the legacy runtime, so the board can still wedge until the production blocker decision and release cleanup migrate onto the single-authority reducer path.

## 2026-04-19 Runtime V2 Production Blocker Bridge Note

The blocker seam is now partially cut over in the production path:

- `users/noah/lib/key/runtime/key_runtime_process.c` now feeds normalized non-synthetic physical key down/up events into runtime-v2 from the real `noah_process_record_user(...)` path.
- `users/noah/lib/key/runtime/key_runtime_scan.c` now feeds normalized scan events into runtime-v2 from the real key-runtime scan path.
- `users/noah/lib/runtime_v2/runtime_v2.c` now tracks whether it has observed the real normalized input stream and exposes native blocker query helpers:
  - any blocker
  - foreign blocker except a key position
- `users/noah/lib/key/runtime/key_runtime_transition.c` now consults those native v2 blocker queries when the runtime-v2 input stream is authoritative, and falls back to the legacy blocker index only in low-level slot/unit surfaces that still mutate legacy state directly without feeding v2.
- `tests/host/key_runtime_integration_harness.c` now suppresses manual key/scan shadow injection when the linked real userspace already feeds runtime-v2 itself, which keeps the integration/parity suites on a single production-shaped input stream instead of double-driving the reducer.
- `tests/host/runtime_debug_test.c` now proves the production process hook feeds blocker queries and that scan-time observation clears the quick-tap blocker once hold ownership settles.

Reconciliation Note:

- The earlier note above that said "the real release/deferred-emission path still runs through the legacy runtime" remains true for effect execution and cleanup ownership, but it is no longer fully true for blocker *decision*.
- The blocker decision has now crossed into a hybrid state:
  - blocker semantics are native in runtime-v2,
  - production blocker queries use runtime-v2 when that normalized stream is authoritative,
  - legacy blocker index ownership remains only as a fallback for low-level direct-slot surfaces that have not been migrated yet.

Inference from the current tree: the production runtime is no longer merely shadow-validating blocker state. The live deferred-release gate can already consult the single-authority reducer path on the default firmware flow. The remaining architecture gap is the release/deferred-emission cleanup itself: effect queues, slot retirement, and owned-state unwind still execute in the legacy runtime, so the wedge class is not closed until that cleanup ownership moves over too.

## 2026-04-19 Runtime V2 Pending Release Drain Ownership Note

The deferred-release cleanup seam has now moved one step further into reducer ownership:

- `users/noah/lib/runtime_v2/runtime_v2.c` now gives each pending release a stable sequence number and exposes a reducer-owned take path that drains pending releases in enqueue order.
- That take path also clears the owning token's `pending_release_emission` and release-pending phase inside runtime-v2, so token cleanup no longer depends on the legacy runtime calling back into observer hooks one drained item at a time.
- `users/noah/lib/key/runtime/key_runtime_release.c` now uses that reducer-owned take path when:
  - runtime-v2 has observed the real normalized input stream, and
  - the mirrored legacy deferred-release queue count matches the reducer-owned pending-release count.
- In that authoritative branch, the legacy queue is now mirror/transport state only; ordering and reducer-side cleanup are owned by runtime-v2, while the legacy release module only dispatches the already-drained actions.
- The old per-item observer drain path remains as compatibility fallback for low-level direct-slot surfaces and any mismatch case while the mixed architecture still exists.
- `tests/host/runtime_debug_test.c` now proves the reducer-owned drain preserves enqueue order and clears token pending-release state after the drain.

Inference from the current tree: production blocker gating and pending-release cleanup are now partially on the same reducer-owned path. That is a real architectural reduction in split-brain state. The remaining gap is still the legacy release/action execution path itself: slot release resolution, effect planning, and owned-state unwind are not yet generated directly from reducer state, so the old slot/release machinery can still poison the board before or around that final dispatch layer.

## 2026-04-19 Runtime V2 Owned-State Lease Observation Note

The reducer now observes one more critical piece of the live production world: held-action and repeat ownership.

- `users/noah/lib/runtime_v2/runtime_v2.c` now tracks reducer-owned held-action and repeat leases by physical key position.
- Those lease records now also keep `owner_key_pos`, so runtime-owned state is no longer tied only to token id. The reducer can clear held/repeat ownership by the same physical key identity that production release cleanup uses, even if token reuse has already happened on that position.
- New runtime-v2 observer hooks now mirror the production execute-plan seams for:
  - held-action register,
  - held-action unregister,
  - repeat start, and
  - release-owned-state cleanup by key.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now calls those hooks from the real `key_runtime_transition_execute_plan(...)` path, so the reducer sees actual held/repeat ownership changes instead of inferring them only from timing/contract state.
- `runtime_v2_press_token_owned_state_active(...)` now consults those reducer-observed held/repeat leases before falling back to threshold-based inference, which tightens blocker semantics around the real owned-state path.
- `runtime_v2_release_owned_state_by_key(...)` is now used from the production release-owned-state effect branch before the legacy held-action/repeat owner runs, so reducer-side unwind of held/repeat lease state is no longer purely synthetic and no longer depends on the legacy owner being the only source of truth for that cleanup.
- `tests/host/runtime_debug_test.c` now proves both the direct lease observation path and the production `key_runtime_transition_execute_plan(...)` path updating and clearing reducer-owned held/repeat lease state.

Inference from the current tree at that stage: blocker gating, pending-release drain, and runtime-owned held/repeat cleanup were now all partially on the reducer side. That was the strongest reduction so far in “split brain about owned state after release,” but active release settlement still lived in the legacy slot release resolver.

## 2026-04-19 Runtime V2 Active Release Settlement Note

The active-slot release decision has now crossed into the reducer-owned path as well:

- `users/noah/lib/runtime_v2/runtime_v2.h` now stores the immutable press-resolved `key_runtime_slot_interaction_t` on each press token, plus a reducer-owned `slot_phase` that tracks release semantics separately from the coarse token lifecycle phase.
- `users/noah/lib/runtime_v2/runtime_v2.c` now advances that reducer-owned release phase only from scan/effect progression:
  - scan events can promote a token from tap window into release-hold-pending or committed hold phases,
  - held-action/repeat observer hooks can commit hold phase when production effect execution does so outside the scan loop, and
  - key-up itself no longer reinterprets release semantics just because time elapsed.
- Held-action and repeat leases now survive physical key-up until `RELEASE_OWNED_STATE_BY_KEY`, which is required for reducer-owned release settlement to see the same live owned-state picture as the production cleanup seam.
- `users/noah/lib/runtime_v2/runtime_v2_release_internal.h` now exposes `runtime_v2_resolve_active_release(...)`, which resolves active release outcome from:
  - the immutable token interaction snapshot,
  - reducer-owned release phase,
  - reducer-owned held/repeat leases,
  - reducer-owned interruption latches, and
  - the release-time elapsed interval captured on the token.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` now uses that reducer-owned release planner when runtime-v2 has observed the real normalized input stream, while retaining the legacy slot-owned resolver as fallback for non-authoritative or narrowed surfaces.
- `tests/host/runtime_debug_test.c` now proves two crucial invariants of this seam:
  - key-up after `tap_hold_term` does not advance the release phase by itself, preserving current scan-sensitive behavior, and
  - scan-time threshold promotion changes reducer-owned release outcome the same way the production slot scan path does.
- The release matrix, scenario suite, modifier-hold integration suite, pd-mode key-runtime integration suite, feature-gate compile suite, and the real-profile thumb/nav suite all stayed green after the cutover, which is the enforcement bar that the migrated active-release planner preserved intended runtime behavior.

Inference from the current tree at that stage: the default active-slot release caller no longer decided tap vs hold vs long-hold vs pd-lock by re-reading mutable live slot state at release time. That decision now came from the reducer-owned press-token snapshot, but pending multi-tap release still used the shared legacy resolver.

## 2026-04-19 Runtime V2 Pending Multi-Tap Release Settlement Note

The second handled-release caller now uses the same reducer-owned authority for release outcome:

- `users/noah/lib/runtime_v2/runtime_v2_release_internal.h` and `users/noah/lib/runtime_v2/runtime_v2.c` now expose `runtime_v2_resolve_pending_multi_tap_release(...)`.
- That API resolves pending multi-tap release from:
  - the released press token's immutable interaction snapshot,
  - reducer-owned release timing captured on the token,
  - the resolved tap payload returned by `multi_tap_resolve_hold(...)`, and
  - whether the multi-tap chain remains active and should be preserved.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now uses that reducer-owned planner when runtime-v2 has observed the real normalized input stream, while leaving the delayed-action mod snapshot and repeat payload on the slot-owned multi-tap storage for this pass.
- That is an intentional narrow migration cut: the reducer now owns the pending multi-tap release *decision*, but the slot still transports the saved-mod snapshot into the final delayed-action effects.
- `tests/host/runtime_debug_test.c` now proves the reducer API directly for both:
  - quick release preserving the chain, and
  - release after the tap-hold term dispatching the held release action.
- The release matrix, transition suite, scenario suite, modifier-hold integration suite, pd-mode key-runtime integration suite, feature-gate compile suite, and the real-profile thumb/nav suite all stayed green after the cutover, which is the enforcement bar that pending multi-tap release preserved current behavior.

Inference from the current tree at that stage: both handled-release callers now took their outcome from reducer-owned release planning rather than from slot-local release reinterpretation. The remaining multi-tap gap was lifecycle ownership, not release outcome: pending multi-tap scan-time hold promotion and chain expiry still lived in the legacy slot/multi-tap reducer, and the effect-planning / slot-retirement layer after the decision was still legacy-owned.

## 2026-04-19 Runtime V2 Pending Multi-Tap Lifecycle Note

The next multi-tap lifecycle seam has now crossed into the reducer too:

- `users/noah/lib/runtime_v2/runtime_v2.h` now stores richer authored tap-series payload:
  - first-tap action,
  - current tap action/repeat payload,
  - authored hold/long-hold contract for the current tap count,
  - per-series tap-hold term, and
  - per-series multi-tap term.
- `users/noah/lib/runtime_v2/runtime_v2.c` now exposes `runtime_v2_resolve_pending_multi_tap_scan(...)`, which resolves:
  - second-tap hold-threshold promotion,
  - late-scan long-hold promotion, and
  - expired-chain flush,
  from reducer-owned press-token/tap-series state instead of from slot-local timer state alone.
- The reducer now keeps authored multi-tap chains alive past generic time refresh until that dedicated pending-multi-tap scan resolver settles them, while plain non-authored tap-series state can still expire independently.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now uses that reducer-owned scan resolver when runtime-v2 is authoritative, but still delegates actual effect building and delayed-action mod transport to the legacy slot/effect layer for this pass.
- `users/noah/lib/key/runtime/key_runtime_trace.h` and `.c` now distinguish:
  - scan hold-threshold promotion,
  - scan long-hold promotion, and
  - scan expired-chain flush,
  which keeps the trace surface aligned with the new reducer-owned lifecycle decisions.
- `tests/host/runtime_debug_test.c` now directly proves reducer-owned pending multi-tap lifecycle for:
  - threshold promotion,
  - long-hold promotion,
  - expired-chain flush, and
  - the narrower `pending_hold` contract that only authored hold-capable chains use that latch.

Reconciliation Note:

- The inference paragraph above is now an audit-time snapshot only.
- Release outcome plus scan-time lifecycle decision are now both reducer-owned for pending multi-tap paths.
- The remaining multi-tap split is narrower:
  - explicit foreign-chain flush/reset still clears legacy slot storage directly,
  - saved delayed-action mod payload is still transported through slot-owned multi-tap storage, and
  - final effect planning / slot retirement after the reducer decision is still legacy-owned.

Inference from the current tree: authored pending multi-tap timing is substantially less split-brained than it was. Both handled release settlement and scan-time pending-hold / expiry settlement now come from reducer-owned token/tap-series state. The remaining wedge risk is no longer "which timer owns multi-tap lifecycle," but the downstream legacy effect-planning and reset seams that still consume those decisions.

## 2026-04-19 Runtime V2 Slot Retirement And Explicit Flush Note

The next reset/retirement seam has now crossed into reducer-aware cleanup too:

- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now expose:
  - `runtime_v2_take_pending_multi_tap_flush(...)`,
  - `runtime_v2_reset_pending_multi_tap(...)`, and
  - `runtime_v2_retire_press_token(...)`.
- `runtime_v2_take_pending_multi_tap_flush(...)` resolves explicit pending-multi-tap flush action/repeat payload from reducer-owned tap-series state and clears that reducer-owned series when the production flush path consumes it.
- `runtime_v2_reset_pending_multi_tap(...)` lets direct legacy pending-multi-tap resets clear reducer-owned tap-series state at the same moment, instead of leaving v2 waiting for a later scan or key event.
- `runtime_v2_retire_press_token(...)` cancels an active reducer-owned press token when the production slot world forcibly flushes that key before physical key-up, releasing leases and recomputing the shadow projection immediately.
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now bridges the legacy slot helpers into those reducer APIs:
  - `key_runtime_slot_take_pending_multi_tap_flush(...)` prefers reducer-owned tap-series payload when available,
  - `key_runtime_slot_reset_pending_multi_tap(...)` clears reducer-owned tap-series state when the legacy pending chain is reset, and
  - `key_runtime_slot_reset(...)` retires the reducer-owned active token plus any reducer-owned tap-series state before the legacy slot is zeroed.
- `tests/host/runtime_debug_test.c` now proves the two production-shaped seams directly:
  - foreign pending-multi-tap flush clears the reducer-owned tap series, and
  - forced active-slot flush retires the reducer-owned active press token.

Inference from the current tree: the reducer now sees the same lifetime end points the production slot world sees for three separate cases:

- handled release settlement,
- scan-time pending-multi-tap settlement, and
- explicit slot/tap-series flush/reset.

That is a meaningful reduction in architecture risk, because shadow state is less able to survive after the legacy slot world has already declared a key path dead. The remaining split is now mostly in the last step after the decision: production still uses the legacy plan/effect/slot-retirement machinery to emit the final actions and unwind the world, rather than having runtime-v2 own that whole aftermath end-to-end.

## 2026-04-19 Runtime V2 Release Effect Planning Note

The next release-aftermath seam has now crossed into reducer-owned planning too:

- `users/noah/lib/runtime_v2/runtime_v2_release_internal.h` now defines:
  - `runtime_v2_release_effect_plan_t`,
  - `runtime_v2_pending_multi_tap_seed_t`, and
  - explicit settlement instructions for handled release callers.
- `users/noah/lib/runtime_v2/runtime_v2.c` now exposes:
  - `runtime_v2_plan_active_release_effects(...)`, and
  - `runtime_v2_plan_pending_multi_tap_release_effects(...)`,
  which map reducer-owned handled-release decisions into:
  - concrete effect queue items,
  - pending multi-tap seed payload, and
  - slot-settlement instructions.
- That mapping now includes reducer-owned handling for pd-mode lock taps, momentary-layer release effects, release-owned-state cleanup effects, held-lifecycle effects, and delayed-action effects with saved modifier payload.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` now consumes the reducer-owned active-release effect plan on the authoritative path, uses the returned settlement to reset legacy slot state, seeds legacy pending multi-tap storage from reducer-owned payload, and appends the reducer-owned effect queue into the existing slot result.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now does the same for pending multi-tap release, using reducer-owned settlement to either preserve the chain or reset the slot before appending the emitted effects.
- `tests/host/runtime_debug_test.c` now directly proves:
  - active release buffering a pending multi-tap seed without immediate effects,
  - active interrupted layer release emitting layer-release plus tap-action effects,
  - pending multi-tap quick release preserving the chain without immediate effects, and
  - pending multi-tap delayed-action release preserving the saved delayed-mod payload.
- The slot suite, transition suite, release matrix, scenario suite, runtime-debug suite, and the real-profile thumb/nav overlap suite all stayed green after this bridge, which is the enforcement bar that reducer-owned effect planning preserved intended outward behavior.

Inference from the current tree: handled-release effect *selection* is no longer legacy-owned. Once runtime-v2 decides what the release means, runtime-v2 now also decides which effects should exist and whether the slot resets or preserves a pending multi-tap chain. The remaining split is now downstream transport and execution:

- legacy slot storage still carries the pending multi-tap seed and effect queue through the old result path, and
- legacy transition execution still applies those side effects to the world.

That is a materially smaller split-brain surface than before, but it still means the final emitted-effect execution contract is not yet end-to-end reducer-owned.

## 2026-04-19 Runtime V2 Effect Projection Note

The next downstream seam has now crossed into the reducer-owned projector layer:

- `users/noah/lib/runtime_v2/runtime_v2_projection.h` now defines the runtime-v2 projector surface for:
  - `runtime_v2_project_effect(...)`, and
  - `runtime_v2_project_pending_release_dispatch(...)`.
- `users/noah/lib/runtime_v2/runtime_v2.c` now owns the actual side-effect projection for handled-key runtime effects:
  - action taps,
  - held-action register/unregister,
  - release-owned-state cleanup,
  - repeat start,
  - layer press/release,
  - feedback pulse,
  - pd-mode lock tap, and
  - delayed-action repeats.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now reduces `key_runtime_transition_execute_plan(...)` to an execution shell that:
  - emits trace records, then
  - hands each effect to `runtime_v2_project_effect(...)`.
- `users/noah/lib/key/runtime/key_runtime_release.c` now routes both the reducer-owned drained pending-release path and the legacy deferred-release queue fallback through `runtime_v2_project_pending_release_dispatch(...)` instead of calling delayed-action execution directly.
- `tests/host/runtime_debug_test.c` now directly proves the projector seam by checking:
  - direct effect projection for an authored action tap and a pd-mode lock tap, and
  - direct deferred delayed-action projection with preserved saved-mod payload.
- The transition suite, runtime-debug suite, modifier-hold integration suite, pd-mode key-runtime integration suite, and feature-gate compile suite stayed green after this cut, which is the enforcement bar that the new projector seam preserved intended behavior.

Inference from the current tree: handled release meaning, effect selection, and effect application are now all on reducer-owned seams. The remaining split is transport:

- legacy slot/result state still carries reducer-owned effect queues and pending multi-tap seed payload through the old handled-key transport, and
- legacy transition/release orchestration still owns queue assembly and drain ordering for those transported effects.

That is the smallest split-brain surface this thread has reached so far. It is still not the clean-slate end state, but the remaining ownership gap is now much narrower and more mechanical than the earlier “multiple subsystems decide what release means” bug class.

## 2026-04-20 Runtime V2 Authoritative Release Transport Note

The next transport seam has now crossed into the reducer-owned path too:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.h` and `.c` now expose `key_runtime_slot_take_v2_active_release_plan(...)`.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.h` and `.c` now expose `key_runtime_slot_take_v2_pending_multi_tap_release_plan(...)`.
- Those helpers sit exactly at the intended release boundary:
  - runtime-v2 has already observed the physical key-up,
  - the legacy slot is still live and has not yet been settled,
  - the helper resolves reducer-owned release semantics,
  - mutates the live slot according to reducer-owned settlement, and
  - returns the reducer-owned effect plan without serializing through `key_runtime_slot_result_t`.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now uses those helpers when runtime-v2 has authoritative normalized input:
  - pending multi-tap release bypasses `key_runtime_slot_step(...)`,
  - active release bypasses `key_runtime_slot_step(...)`,
  - reducer-owned effect plans append directly into `key_runtime_transition_plan_t`, and
  - unmatched handled releases append layer-release / owned-state cleanup effects directly into the transition plan rather than going through slot-result transport.
- The old slot-result release reducers remain as compatibility fallback for non-authoritative or narrowed surfaces, so this is a narrow production migration rather than a big-bang delete.
- `tests/host/runtime_debug_test.c` now directly proves the new boundary contract for both helper entrypoints, and the transition, release matrix, scenario, real-profile overlap, modifier-hold integration, pd-mode key-runtime integration, key-runtime harness, runtime-debug, and feature-gate compile suites all stayed green after this cut.

Inference from the current tree: authoritative handled release no longer depends on the legacy slot-result transport at all. For release paths, runtime-v2 now owns:

- release meaning,
- release effect selection,
- release effect projection, and
- authoritative release transport into the transition plan.

The remaining split is now outside authoritative handled release:

- press, scan, interrupt, and flush still use the legacy slot-result transport,
- deferred-release queue assembly and drain orchestration still live in the legacy release/transition layer, and
- compatibility fallback still routes narrowed or non-authoritative release surfaces through the old slot reducers.

That is a real architecture milestone. The remaining work is now mostly mechanical transport/orchestration cleanup rather than “which subsystem decides what this release means.”

## 2026-04-20 Runtime V2 Flush And Interrupt Transport Note

The next non-release transport seams have now moved off the old slot-result wrapper too:

- `users/noah/lib/key/runtime/key_runtime_transition.c` now translates pending-chain flush directly into delayed-action effects for:
  - `key_runtime_transition_flush_multi_tap(...)`, and
  - `key_runtime_transition_flush_foreign_multi_tap(...)`.
- The same file now translates slot-policy builders directly into transition-plan effects for:
  - `key_runtime_transition_flush_active_keys_except(...)`, and
  - `key_runtime_transition_interrupt_active_keys_on_other_press(...)`.
- Those paths no longer route through:
  - `key_runtime_slot_step(...)`,
  - `KEY_RUNTIME_SLOT_EVENT_PENDING_MULTI_TAP_FLUSH`,
  - `KEY_RUNTIME_SLOT_EVENT_INTERRUPT`, or
  - temporary `key_runtime_slot_result_t` wrapping
  on the normal transition path.
- The underlying state mutation is intentionally unchanged:
  - pending-chain flush still comes from `key_runtime_slot_take_pending_multi_tap_flush(...)`, and
  - active-slot interrupt/flush still comes from slot-policy helpers.
- `tests/host/key_runtime_transition_test.c` now adds dedicated coverage for `key_runtime_transition_flush_foreign_multi_tap(...)`, while the transition, release matrix, scenario, runtime-debug, real-profile overlap, modifier-hold integration, pd-mode key-runtime integration, key-runtime harness, and feature-gate compile suites all stayed green after the move.

Inference from the current tree: the old slot-result transport is no longer on the hot path for:

- authoritative handled release,
- pending multi-tap flush,
- foreign pending multi-tap flush,
- active-key flush, and
- active-key interrupt.

The remaining mixed transport is now narrower:

- handled press still uses slot-step/result transport,
- active scan and pending multi-tap scan still use slot-step/result transport, and
- deferred-release queue assembly/drain orchestration still lives in the legacy release/transition layer.

That is the right shape for the next passes. The remaining work is now concentrated in press/scan/orchestration seams rather than spread across every event type.

## 2026-04-20 Runtime V2 Scan Transport Note

The next scan-time transport seam has now crossed into the direct-plan path too:

- Added `users/noah/lib/key/runtime/slot/key_runtime_slot_direct_plan.h` as the narrow transport surface for migrated slot reducers that should:
  - mutate live slot state, and
  - return emitted `key_runtime_effect_t` items directly
  without round-tripping through `key_runtime_slot_result_t`.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_scan_reduce.h` and `.c` now expose `key_runtime_slot_take_active_scan_plan(...)`, which keeps the existing active-slot scan mutation intact while returning emitted threshold/long-hold/fallback-hold effects directly.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.h` and `.c` now expose `key_runtime_slot_take_pending_multi_tap_scan_plan(...)`, which does the same for pending multi-tap scan settlement:
  - pending-hold threshold promotion,
  - pending long-hold promotion, and
  - expired-chain delayed-action flush.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now uses those direct-plan helpers in `key_runtime_transition_scan(...)` instead of calling through slot-step/result transport on the default path.
- The cut stays intentionally narrow:
  - state mutation is unchanged,
  - legacy slot-result reducers still exist as compatibility wrappers,
  - but normal active scan and pending multi-tap scan no longer depend on `key_runtime_slot_result_t` as a transport envelope.
- `tests/host/key_runtime_transition_test.c` now includes a dedicated mixed-scan regression proving one scan tick can merge an active-slot threshold effect plus an independent pending-chain expiry flush into one transition plan, and the transition, runtime-debug, release matrix, slot, scenario, real-profile overlap, modifier-hold integration, pd-mode key-runtime integration, key-runtime harness, feature-gate compile, and full host suites all stayed green after the move.

Inference from the current tree: the old slot-result transport is no longer on the default hot path for:

- authoritative handled release,
- pending multi-tap flush,
- foreign pending multi-tap flush,
- active-key flush,
- active-key interrupt,
- active scan, and
- pending multi-tap scan.

The remaining mixed transport is narrower again:

- handled press still uses slot-step/result transport, and
- deferred-release queue assembly/drain orchestration still lives in the legacy release/transition layer.

That is now a much more realistic endgame. The remaining work is no longer “most of the runtime”; it is the last press/orchestration seams plus deletion of the compatibility wrappers once the default path no longer depends on them.

## 2026-04-20 Runtime V2 Press Transport Note

Handled press has now crossed into the same direct-plan transport model:

- `users/noah/lib/key/runtime/slot/key_runtime_slot_press_reduce.h` and `.c` now expose `key_runtime_slot_take_handled_press_plan(...)`.
- That helper keeps the existing handled-press reducer semantics intact for:
  - pending multi-tap reuse,
  - pending-chain flush before a fresh begin,
  - reclaiming an occupied active slot, and
  - fresh press begin,
  but emits a `key_runtime_slot_direct_plan_t` directly instead of serializing through `key_runtime_slot_result_t`.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_direct_plan.h` now also covers the remaining press-only effect shapes needed by that reducer:
  - direct dispatch-action emission, and
  - direct layer-press emission.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now uses that helper in `key_runtime_transition_handled_key_press(...)`, so the default handled-press path no longer goes through:
  - `key_runtime_slot_step(...)`, or
  - slot-result transport.
- The compatibility boundary remains intentionally narrow:
  - `key_runtime_slot_reduce_handled_press(...)` still exists as a wrapper for focused slot tests and narrowed callers, and
  - slot-step/result transport still exists where the migration has not deleted it yet,
  - but the normal handled-press path is now direct-plan based.
- `tests/host/key_runtime_transition_test.c` now adds dedicated press transport regressions proving:
  - reclaiming a live tap slot emits the old tap before the new immediate-hold registration, and
  - flushing a foreign pending chain emits the delayed action before a new layer-press request.
- The transition, slot, runtime-debug, release matrix, scenario, modifier-hold integration, pd-mode key-runtime integration, real-profile overlap, key-runtime harness, feature-gate compile, and full host suites stayed green after this cut.

Inference from the current tree: the default hot path no longer depends on old slot-result transport for:

- handled press,
- authoritative handled release,
- active scan,
- pending multi-tap scan,
- pending multi-tap flush,
- foreign pending multi-tap flush,
- active-key flush, and
- active-key interrupt.

The remaining mixed architecture is now concentrated in one real production seam:

- deferred-release queue assembly and drain orchestration still live in the legacy release/transition layer.

Compatibility wrappers still exist, but they are no longer the default runtime authority. That is the first point in this thread where the default handled-key event flow is mostly reducer/direct-plan driven end to end, with deferred release orchestration standing out as the main remaining legacy owner.

## 2026-04-19 Runtime V2 Blocker Observation Note

The blocker seam moved one step further toward the intended shadow-reducer model:

- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now keep blocker records per physical key position, keyed to the current press token id for that key position.
- The v2 blocker record stores the same timed blocker profile as the legacy owner:
  - blocks before `tap_hold_term`
  - blocks after `tap_hold_term`
- `runtime_v2` now evaluates the threshold locally from the owning press token's `pressed_at` and `hold_term_ms`, so time-boundary blocker activation is no longer borrowed from legacy scan-time blocker refresh.
- `users/noah/lib/key/runtime/key_runtime_index.c` now feeds the blocker profile into `runtime_v2_observe_deferred_release_blocker_profile(...)` from the real blocker owner seam.
- `runtime_v2_projection_snapshot_capture()` now records both legacy blocker counts and v2 blocker counts, so the parity surface can compare blocker ownership explicitly instead of leaving it invisible.
- `tests/host/runtime_debug_test.c` now proves the v2 shadow blocker record changes state correctly across `tap_hold_term` in both directions.

Inference from the current tree: blocker timing is now modeled inside `runtime_v2`, but blocker semantics are still observer-fed from the legacy owner. That is a meaningful reduction in architecture risk, but it is not yet the final clean state. The remaining step is to derive blocker ownership natively from authored press-token semantics inside the reducer instead of mirroring the legacy blocker profile.

## 2026-04-19 Deferred Release Timed Blocker Ownership Note

The next blocker-path refinement is now landed too:

- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now stores an explicit deferred-release blocker profile on each slot:
  - blocks before `tap_hold_term`
  - blocks after `tap_hold_term`
- `users/noah/lib/key/runtime/key_runtime_index.c` now maintains both the effective blocker subset and a dedicated timed subset whose membership only changes when `tap_hold_term` is crossed.
- Timed blocker refresh no longer rescans the full active-slot set and reruns blocker classification. It now consults immutable press-resolved slot interaction plus slot-owned lifecycle latches, then updates only the threshold-tracked subset.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c` now re-syncs the slot index immediately when foreign-key interruption latches change, which was required once blocker membership stopped being a live recomputation.
- `tests/host/key_runtime_index_test.c` now proves the two time-boundary contracts directly:
  - a quick-tap blocker expires after `tap_hold_term` without an active-slot resync, and
  - an interrupted layer-tap blocker with nonquick tap behavior becomes active after `tap_hold_term` without rescanning the active set.

Inference from the current tree at that stage: blocker ownership was materially cleaner, but the timed blocker profile still lived entirely in legacy slot/index state. The later runtime-v2 blocker observation pass above moved that profile into the shadow reducer and localised threshold timing there, but the blocker semantics are still mirrored from the legacy owner rather than derived natively from authored token state.

## 2026-04-19 Deferred Release Blocker Index Note

The next blocker-path seam is now landed too:

- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now owns the blocker predicate in `key_runtime_slot_blocks_deferred_release_dispatch(...)` instead of leaving that logic embedded in `key_runtime_transition.c`.
- `users/noah/lib/key/runtime/key_runtime_index.c` now maintains a dedicated deferred-release blocker subset derived from the active-slot index, and transition-layer blocker queries now ask the index instead of rescanning active slots and re-deriving the blocker rule themselves.
- `tests/host/key_runtime_index_test.c` now locks the low-level ownership contract directly, including immediate blocker-index updates on slot mutations and the interrupted-layer-tap exclusion.
- The release matrix, modifier-hold, pd-mode integration, and real-profile overlap suites stayed green after this move, so the new owner layer preserved the existing outward behavior.

Inference from the current tree at that stage: blocker ownership had moved in the right direction, but it was still not a fully reducer-owned fact because the index still refreshed blocker membership by rescanning the active-slot set as time passed. The later timed-blocker ownership pass above narrowed that seam further, but blocker state is still legacy-owned rather than v2-owned.

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

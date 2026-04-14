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

## Recommended Next Refactor Sequence

1. Refactor `NOAH_ACTION_KIND_REGISTRY` first. It has the highest architecture leverage and is already covered by the action dispatch, action lifecycle, key runtime, and full host suites.
2. Split `key_runtime_internal.h` into smaller internal slice headers without changing behavior. Keep `tests/host/run_feature_gate_compile_tests.sh` strict while narrowing the allowlists.
3. Only after those two steps land, decide whether `NOAH_PD_MODE_LIST` or `MATERIALIZE_KEYMAP_DATA()` need further cleanup. Do not rewrite every macro-driven surface in one pass.

## Verification Baseline

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Sibling workspace folders touched: none

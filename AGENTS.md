# Repo Agent Instructions

Use this repo like production firmware, not a scratch keymap.

## Start Here

- Start every task with `git status --short`.
- Assume the worktree may already be dirty. Never revert or overwrite unrelated user changes.
- This repo is one folder in a multi-root VS Code workspace. The workspace also includes sibling directories such as `../bastardkb-qmk` and `../builds`.
- Treat `charybdis-4x6` as the default write target. Do not edit sibling workspace folders unless the task explicitly requires it and the user wants that scope.
- In general, prefer changes in this repo over changes in sibling workspace folders. `../builds` is output/artifact space, not source.
- If the task depends on upstream QMK behavior or build wiring, inspect the relevant files in `../bastardkb-qmk` instead of guessing how upstream behaves.
- `README.md` and the files under `docs/` are the main human-facing documentation set for this repo.
- For doc fixes, doc updates, doc audits, or user-facing explanation work, check `README.md` and the relevant files under `docs/` first.
- Review folders under `review/` are internal architecture/planning notes, not the default target for normal documentation requests.
- For refactors or runtime architecture work, read the newest review folder under `review/` first. The newest review folder is the primary source of truth for that architecture work.
- If the newest review folder is internally contradictory, reconcile it before using it as the source of truth for follow-up work on the same thread.
- Name new review folders with a sortable ISO date prefix. For distinct reviews opened on the same day, append a zero-padded review sequence such as `review/2026-04-11-review-01/`, `review/2026-04-11-review-02/`, and `review/2026-04-11-review-03/` so "newest review" is unambiguous.
- If work belongs to an existing review, continue in that folder instead of creating a same-day duplicate with a different naming pattern.
- Each new review lives in its own folder under `review/` and must include:
  - `userspace-architecture-review.md`: current architecture decisions, tradeoffs, and intended structure
  - `progress.md`: completed work, in-flight work, verification, and next steps
- Do not recreate deleted root-level review files if the active review has moved into a subfolder.

## Review Prompt Templates

- Review prompt templates live under `prompts/`, when using one of these files always mention it in the chat!!.
- Use `prompts/initial-architecture-review.md` for the first architecture pass on a new thread.
- Use `prompts/follow-up-architecture-audit.md` for critical audits of landed refactor work.
- Use `prompts/closure-verification-review.md` to decide whether an active review thread is actually ready to close.
- Prefer referencing these files by path in requests instead of pasting prompt text into the chat.

## Repo Boundaries

- Keep `keyboards/bastardkb/charybdis/4x6/keymaps/noah/` mainly data-driven. That path should primarily hold authored profile data such as layers, combos, `key_behaviors[]`, RGB tables, macros, and other keymap configuration.
- Put shared runtime policy, reusable engine logic, and QMK/fork integration under `users/noah/`.
- Treat `users/noah/` as the local userspace/runtime surface for this repo and `../bastardkb-qmk/` as the upstream firmware tree. Keep ownership and changes explicit when work crosses that boundary.
- Runtime modules under `users/noah/` must not include `noah_keymap.h`.
- Keymap-owned translation units under `keyboards/.../keymaps/noah/` must not include `noah_runtime.h`.
- If a keymap overrides a weak QMK hook, chain back into the matching `noah_*` helper unless the task explicitly replaces the shared behavior and documents that choice.
- Keep fork-specific QMK or VIA contracts centralized in `users/noah/lib/compat/` instead of scattering new compatibility assumptions through unrelated modules.
- Do not introduce raw QMK layer-action outputs in authored data when they bypass userspace layer ownership.

Required verification workflow:

1. Treat tests and compile checks as required checkpoints throughout the work, not a one-time end-of-task ceremony.
2. Run targeted host tests repeatedly while working on a subsystem. Prefer the concrete runners in `tests/host/` over guessing from `run_<area>_tests.sh`.
3. Use the subsystem runners that match the files and behavior you touched:
   - key runtime: `sh tests/host/run_key_runtime_admission_tests.sh`, `sh tests/host/run_key_runtime_index_tests.sh`, `sh tests/host/run_key_runtime_slot_tests.sh`, `sh tests/host/run_key_runtime_release_matrix_tests.sh`, `sh tests/host/run_key_runtime_preflight_tests.sh`, `sh tests/host/run_key_runtime_transition_tests.sh`, `sh tests/host/run_key_runtime_feedback_tests.sh`, `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `sh tests/host/run_key_runtime_scenario_tests.sh`
   - pointing / pd mode: `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_handlers_tests.sh`, `sh tests/host/run_pd_runtime_tests.sh`, `sh tests/host/run_pointer_layer_policy_tests.sh`, `sh tests/host/run_split_runtime_sync_tests.sh`
   - authored profile validation: `sh tests/host/run_key_behavior_lookup_tests.sh`, `sh tests/host/run_key_behavior_validation_tests.sh`, `sh tests/host/run_keymap_validation_tests.sh`, `sh tests/host/run_real_profile_validation_tests.sh`
   - RGB: `sh tests/host/run_rgb_validation_tests.sh`, `sh tests/host/run_rgb_layer_render_tests.sh`
   - hooks / ownership: `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_keyboard_mod_ownership_tests.sh`, `sh tests/host/run_owned_keycode_tests.sh`, `sh tests/host/run_held_action_tests.sh`, `sh tests/host/run_layer_ownership_tests.sh`
   - macro / VIA / QMK-contract work: `sh tests/host/run_qmk_contract_checks.sh`, `sh tests/host/run_action_lifecycle_tests.sh`, `sh tests/host/run_macro_dispatch_tests.sh`, `sh tests/host/run_macro_payload_tests.sh`, `sh tests/host/run_via_macro_defaults_tests.sh`, `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
   - shared runtime / tracing: `sh tests/host/run_runtime_init_order_tests.sh`, `sh tests/host/run_runtime_debug_tests.sh`, `sh tests/host/run_runtime_trace_tests.sh`
4. If authored keymap, combo, macro, or RGB data changed, also run:
   `sh tests/host/run_real_profile_validation_tests.sh`
5. If runtime wiring, source lists, compat surfaces, or header boundaries changed, also run:
   `sh tests/host/run_feature_gate_compile_tests.sh`
6. Before handing work back, run the full host suite:
   `sh tests/host/run_all_host_tests.sh`
7. Run the firmware build only after all required host tests for that pass are green:
   `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Required failure handling:

- If a test fails, stop and find the root cause. Fix the code or wiring unless the test is genuinely asserting the wrong contract now.
- Do not change, weaken, or delete a failing test just to get green output. Update a test only when behavior intentionally changed and the old expectation is no longer correct.
- When behavior, invariants, or module boundaries change, add or extend tests in the same pass. Do not leave new logic uncovered.
- Treat a failing compile gate as a real boundary or wiring regression, not optional cleanup.

Required reporting:

- Do not claim success without naming the verification commands that actually ran.
- If any required check was skipped or failed, say so explicitly and explain why.
- For architecture, compatibility, or header-boundary work, name the contracts you touched and which tests or compile gates covered them.
- If work crossed into a sibling workspace folder such as `../bastardkb-qmk`, say so explicitly.

Repo-specific guardrails:

- When adding a new firmware source file that should participate in the userspace build, wire it into `users/noah/source_manifest.mk` in the same pass. If host compile gates or test runners mirror that build surface, update them too.
- Prefer small, local changes over generic runtime rewrites unless the task explicitly requires runtime architecture work.
- If behavior, workflows, setup steps, or user-facing capabilities changed, update `README.md` and the relevant files under `docs/` in the same pass.
- If any authored input to `tools/profile_introspect.py` changes, regenerate the introspection outputs in the same pass with `python3 tools/profile_introspect.py --write` and verify them with `python3 tools/profile_introspect.py --check`. Current authored inputs are `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`, `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`, `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`, and the shared pd-mode manifest `users/noah/lib/pointing/defs/pd_mode_manifest.h`.
- If architectural work lands, update the active review folder's `progress.md` in the same pass.
- Keep `progress.md` structured so it reads as clear history.
- Always include next steps.
- If the intended structure or tradeoffs changed, update `userspace-architecture-review.md` in the same pass.

## Active Review Thread

- For one architecture/refactor thread, keep one active review folder until the thread is closed.
- Routine remediation, re-audit, and closure verification for the same thread belong in the active review folder, not a new same-day folder.
- Create a new review folder only when:
  - the architecture topic is materially different, or
  - the previous thread is explicitly closed and a new thread is starting.
- If you create a new review folder, state in `progress.md` why the active folder was not continued.

## Review Integrity Rules

- A review folder must be internally coherent for the tree it describes.
- If later remediation lands after an audit, either:
  - update the active review folder so its findings and landed state agree, or
  - add an explicit `Reconciliation Note` that labels older findings as audit-time snapshot only.
- Never leave a review folder containing both:
  - “this has landed”, and
  - stale open findings about the same issue,
  without an explicit reconciliation note.

## Required Review Structure

- For follow-up architecture reviews, include a `Prior Finding Status` section.
- For each major prior finding, mark it as:
  - `open`
  - `partially resolved`
  - `resolved`
  - `regressed`
- When marking a finding `resolved`, include:
  - code references
  - enforcement references such as compile gates or tests
  - verification commands that passed

## Finding Lifecycle

- Track each major finding across follow-up passes as one of:
  - `open`
  - `partially resolved`
  - `resolved`
  - `regressed`
- Do not mark a finding `resolved` because the code looks cleaner. Mark it `resolved` only when the closure bar below is met.

## Closure Bar

- Do not call architecture work resolved because it looks cleaner.
- A seam/boundary/API finding is only resolved when:
  - the current code matches the intended design
  - compile gates or tests mechanically enforce the claim
  - docs and the active review note match the current tree
  - `sh tests/host/run_all_host_tests.sh` passes
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes

## Review Before New Review

- If the newest review folder is contradictory, reconcile it before opening another follow-up review on the same thread.

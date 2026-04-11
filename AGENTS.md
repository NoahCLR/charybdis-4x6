# Repo Agent Instructions

Use this repo like production firmware, not a scratch keymap.

## Start Here

- Start every task with `git status --short`.
- Assume the worktree may already be dirty. Never revert or overwrite unrelated user changes.
- This repo is one folder in a multi-root VS Code workspace. The workspace also includes sibling directories such as `../bastardkb-qmk` and `../builds`.
- Treat `charybdis-4x6` as the default write target. Do not edit sibling workspace folders unless the task explicitly requires it and the user wants that scope.
- In general, prefer changes in this repo over changes in sibling workspace folders. `../builds` is output/artifact space, not source.
- If the task depends on upstream QMK behavior or build wiring, inspect the relevant files in `../bastardkb-qmk` instead of guessing how upstream behaves.
- For refactors or runtime architecture work, read the newest review folder under `review/` first. The newest review folder is the primary source of truth.
- Name new review folders with a sortable ISO date prefix such as `review/2026-04-11-phase-4/` so "newest review" is unambiguous.
- Each new review lives in its own folder under `review/` and must include:
  - `userspace-architecture-review.md`: current architecture decisions, tradeoffs, and intended structure
  - `progress.md`: completed work, in-flight work, verification, and next steps
- Do not recreate deleted root-level review files if the active review has moved into a subfolder.

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
   - key runtime: `sh tests/host/run_key_runtime_slot_tests.sh`, `sh tests/host/run_key_runtime_preflight_tests.sh`, `sh tests/host/run_key_runtime_transition_tests.sh`, `sh tests/host/run_key_runtime_feedback_tests.sh`, `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
   - pointing / pd mode: `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_handlers_tests.sh`, `sh tests/host/run_pointer_layer_policy_tests.sh`, `sh tests/host/run_split_runtime_sync_tests.sh`
   - authored profile validation: `sh tests/host/run_key_behavior_lookup_tests.sh`, `sh tests/host/run_key_behavior_validation_tests.sh`, `sh tests/host/run_keymap_validation_tests.sh`, `sh tests/host/run_real_profile_validation_tests.sh`
   - RGB: `sh tests/host/run_rgb_validation_tests.sh`, `sh tests/host/run_rgb_layer_render_tests.sh`
   - hooks / ownership: `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_keyboard_mod_ownership_tests.sh`, `sh tests/host/run_owned_keycode_tests.sh`, `sh tests/host/run_held_action_tests.sh`, `sh tests/host/run_layer_ownership_tests.sh`
   - macro / VIA / QMK-contract work: `sh tests/host/run_qmk_contract_checks.sh`, `sh tests/host/run_action_lifecycle_tests.sh`, `sh tests/host/run_macro_payload_tests.sh`, `sh tests/host/run_via_macro_defaults_tests.sh`, `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
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

- When adding a new firmware source file that should participate in the userspace build, wire it into `users/noah/rules.mk` in the same pass. If host compile gates or test runners mirror that build surface, update them too.
- Prefer small, local changes over generic runtime rewrites unless the task explicitly requires runtime architecture work.
- If architectural work lands, update the active review folder's `progress.md` in the same pass. If the intended structure or tradeoffs changed, update `userspace-architecture-review.md` too.

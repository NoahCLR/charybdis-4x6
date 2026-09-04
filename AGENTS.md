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
- For refactors or runtime architecture work, read `docs/LIVE_EDIT_APP_DIRECTION.md` first. It carries the current direction, the decisions behind it, and what is deliberately left undesigned.
- The durable specs live under `docs/architecture/`: the Profile Wire and split protocols, the authority state tables, the storage and resource baseline, the field classification, and the known-issue notes. Treat those as the contract; change them deliberately.
- This repo no longer keeps dated review folders or finding registers. Record decisions in the doc they govern, next to the thing they constrain.

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
   - key runtime: `sh tests/host/run_key_runtime_release_matrix_tests.sh`, `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`, `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`, `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`, `sh tests/host/run_key_runtime_scenario_tests.sh`, `sh tests/host/run_key_runtime_integration_harness_tests.sh`
   - pointing / pd mode: `sh tests/host/run_pd_mode_tests.sh`, `sh tests/host/run_pd_mode_handlers_tests.sh`, `sh tests/host/run_pd_runtime_tests.sh`, `sh tests/host/run_pointer_layer_policy_tests.sh`, `sh tests/host/run_split_runtime_sync_tests.sh`
   - authored profile validation: `sh tests/host/run_key_behavior_lookup_tests.sh`, `sh tests/host/run_key_behavior_validation_tests.sh`, `sh tests/host/run_keymap_validation_tests.sh`, `sh tests/host/run_real_profile_validation_tests.sh`
   - RGB: `sh tests/host/run_rgb_validation_tests.sh`, `sh tests/host/run_rgb_layer_render_tests.sh`
   - hooks / ownership: `sh tests/host/run_hook_chaining_tests.sh`, `sh tests/host/run_keyboard_mod_ownership_tests.sh`, `sh tests/host/run_owned_keycode_tests.sh`, `sh tests/host/run_held_action_tests.sh`, `sh tests/host/run_layer_ownership_tests.sh`
   - macro / VIA / QMK-contract work: `sh tests/host/run_qmk_contract_checks.sh`, `sh tests/host/run_action_lifecycle_tests.sh`, `sh tests/host/run_macro_dispatch_tests.sh`, `sh tests/host/run_macro_payload_tests.sh`, `sh tests/host/run_via_macro_defaults_tests.sh`, `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
   - shared runtime / tracing: `sh tests/host/run_runtime_init_order_tests.sh`, `sh tests/host/run_runtime_debug_tests.sh`, `sh tests/host/run_runtime_trace_tests.sh`
   - Profile Studio extension/UI: from `tools/charybdis-profile-studio/`, run `npm run check` and `npm run screenshots`
     - For Profile Studio hover/tooltip changes, `npm run screenshots` is not enough because it captures resting page states. Generate a kept harness with `npm run screenshots -- --keep-harness`, serve that harness over localhost, then drive a real mouse move against the target with Chrome/CDP `Input.dispatchMouseEvent` or an equivalent browser action and capture the hover state. Do not rely on file URLs in the in-app browser or read-only DOM synthetic events for hover verification; they can be blocked or fail to create the actual tooltip state. Stop any temporary localhost server before handing work back.
     - When editing regex literals inside the generated Profile Studio client script returned by `getClientScript()`, remember the code lives inside an outer JavaScript template string. Escape regex backslashes for the generated script, for example `/\\b(?:VIA_MACRO|MACRO)_\\d+\\b/g`, otherwise hover-only checks can miss broken parsing that normal screenshots do not exercise.
4. If authored keymap, combo, macro, or RGB data changed, also run:
   `sh tests/host/run_real_profile_validation_tests.sh`
5. If runtime wiring, source lists, compat surfaces, or header boundaries changed, also run:
   `sh tests/host/run_feature_gate_compile_tests.sh`
6. Before handing work back, run the full host suite:
   `sh tests/host/run_all_host_tests.sh`
7. Run the firmware build only after all required host tests for that pass are green:
   `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Narrow verification exceptions:

- Docs, prompt-template, review-note, or `AGENTS.md`-only changes may skip host
  tests and firmware compile. Still run `git diff --check`, and explicitly
  report that runtime/build behavior was not changed and those checks were
  intentionally skipped.
- Python-only tooling changes do not require `qmk compile` unless they alter
  generated firmware inputs, source manifests, build wiring, or authored
  profile data. Run the relevant Python/tool checks for the changed files, run
  `git diff --check`, and explicitly report that firmware compile was skipped
  under this exception.

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
- If any authored input to `tools/profile_introspect.py` changes, regenerate the introspection outputs in the same pass with `python3 tools/profile_introspect.py --write` and verify them with `python3 tools/profile_introspect.py --check`. Current authored inputs are `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`, `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`, `users/noah/config.h`, `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`, and the shared pd-mode manifest `users/noah/lib/pointing/defs/pd_mode_manifest.h`.
- If architectural work lands, update the active open review folder's
  `progress.md` in the same pass. If the related review thread is closed, open
  the next sortable review folder and update that new folder instead.
- Keep `progress.md` structured so it reads as clear history.
- Always include next steps.
- If the intended structure or tradeoffs changed, update `userspace-architecture-review.md` in the same pass.

## RP2040 Resource Truth

- Read `docs/architecture/memory-budgets.md` before interpreting or reporting
  firmware RAM, allocator, EEPROM-cache, or stack values.
- Report resources per keyboard half. Each half has its own RP2040 and its own
  270,336 bytes of physical SRAM: 262,144 bytes in SRAM0–3 plus 4,096 bytes
  each in SRAM4 and SRAM5. The overlapping `ram7` boot window is part of SRAM5,
  not extra memory.
- Treat the `.data + .bss` threshold and the BSS threshold as regression
  policies, not physical-RAM limits. Never describe their remaining policy
  margin as total RAM headroom.
- Describe `__heap_end__ - __heap_base__` as the SRAM0–3 linker/core-memory
  span at boot. It backs the ChibiOS core allocator and linked newlib
  allocation path; it is not guaranteed unused memory or a runtime high-water
  measurement.
- Keep reviewed-path stack policy margin separate from the physical stack
  boundary. A stack-gate PASS covers the named manifest paths only and does not
  prove a global or interrupt-stack maximum.
- Do not add GNU `size`'s aggregate BSS number to separately reported `.data`,
  `.bss`, core-memory-span, or stack figures; the aggregate includes NOLOAD
  reservations.
- Any claim that a RAM representation is required or impossible must name the
  physical/linker bank, fresh linked accounting, the applicable policy, and
  runtime high-water evidence. Preserve conservative designs when useful, but
  do not justify them with policy values presented as hardware capacity.

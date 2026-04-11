# Repo Agent Instructions

Use this repo like production firmware, not a scratch keymap.

- Start by checking `git status --short`. Assume the worktree may already be dirty. Never revert or overwrite unrelated user changes.
- For refactors or runtime architecture work, read `review/userspace-architecture-review.md` and `review/progress.md` first so changes stay aligned with the current plan.
- Do not treat compile checks and tests as one final end-of-task ceremony. They are required checkpoints throughout the work.

Required verification workflow:

1. Run targeted host tests repeatedly while working on a subsystem:
   `sh tests/host/run_<area>_tests.sh`
2. If authored keymap or RGB data changed, also run:
   `sh tests/host/run_real_profile_validation_tests.sh`
3. If runtime wiring, source lists, or header boundaries changed, also run:
   `sh tests/host/run_feature_gate_compile_tests.sh`
4. Before handing work back, run the full host suite:
   `sh tests/host/run_all_host_tests.sh`
5. Run the firmware build only after all required tests for that pass are green:
   `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Required failure handling:

- If a test fails, stop and find the root cause. Fix the code or wiring unless the test is genuinely asserting the wrong contract now.
- Do not change, weaken, or delete a failing test just to get green output. Update a test only when behavior intentionally changed and the old expectation is no longer correct.
- When behavior, invariants, or module boundaries change, add or extend tests in the same pass. Do not leave new logic uncovered.

Required reporting:

- Do not claim success without naming the verification commands that actually ran.
- If any required check was skipped or failed, say so explicitly and explain why.

Repo-specific guardrails:

- Keep the authoring/runtime boundary intact: runtime modules under `users/noah/` must not include `noah_keymap.h`, and keymap-owned translation units under `keyboards/.../keymaps/noah/` must not include `noah_runtime.h`.
- Keep `keyboards/.../keymaps/noah/` mainly data-driven. That path should primarily hold authored profile data such as layers, combos, `key_behaviors[]`, RGB tables, macros, and other keymap configuration, not shared runtime policy or reusable engine logic.
- When adding a new firmware source file that should participate in the userspace build, wire it into `users/noah/rules.mk` in the same pass. If host compile gates or test runners mirror that build surface, update them too.
- Prefer small, local changes over generic runtime rewrites unless the task explicitly requires runtime architecture work.
- If architectural work lands, update the relevant docs and `review/progress.md` in the same pass.

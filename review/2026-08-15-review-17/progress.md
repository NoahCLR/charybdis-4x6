# Combined Firmware Closure Review Progress

## Why This Review Exists

Review 16 is closed and remains immutable. The accumulated remediation work
needed one combined closure-focused audit before declaring that only hardware
testing remained. This is post-closure architecture work, so it uses Review 17
instead of changing the historical Review 16 record.

The repository's `prompts/closure-verification-review.md` template guided the
review.

## 2026-08-15 — Baseline and Scope

- Started from a clean `sol` branch at `5a56146d` (`Bound key runtime hot
  paths`).
- Reviewed the current `Sol Findings` roadmap and implementation record.
- Reconciled the latest closed review with the two already-open hardware
  obligations: Finding 05 split persistence and Finding 10 pointing feel.
- Audited the integrated key runtime, ownership, macro, split, RGB, pointing,
  VIA, memory, and stack surfaces.
- Made no firmware source changes and no sibling workspace source changes.

## Findings Recorded

1. **Must fix / Finding 08 regressed:** pre-process physical ownership is not
   rolled back when the handled-key pipeline consumes the event. Authored
   shifted-symbol and Shift+Enter holds can suppress their own base key-down;
   a handled-modifier interleaving can retain a false report owner.
2. **Must fix / Finding 15 partially resolved:** split worker callbacks publish
   multi-field remote display state directly while main-context RGB/PD readers
   copy it without a coherent-generation contract.
3. **Should fix / Finding 10 partially resolved:** dormant arrow-axis backlog
   can survive a dominant-axis change and emit on later unrelated motion.
4. **Should fix / new bounded-state issue:** dragscroll/pinch residual addition
   and absolute-value handling have signed-overflow paths.
5. **Optional:** a future trace-enabled build needs an explicit split-worker
   writer contract for the shared trace ring.

## Verification Passed

Focused suites:

- `sh tests/host/run_owned_keycode_tests.sh`
- `sh tests/host/run_held_action_tests.sh`
- `sh tests/host/run_key_runtime_scenario_tests.sh`
- `sh tests/host/run_keyboard_mod_ownership_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`
- `sh tests/host/run_rgb_layer_render_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`

Whole-tree and target gates:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`

Target resource results:

- 25,524 B static BSS, below the 26,000 B limit.
- 212,352 B linker heap, above the 204,800 B minimum.
- 599 B named macro storage, below the 8,192 B limit.
- 1,912/1,920 B worst reviewed main-process path.
- 336/768 B worst reviewed split-worker path.

The existing suites pass because they test synchronous final states, direct
ownership ledgers, unhandled physical overlap, and per-axis bounds. They do not
exercise the consumed/default-processing distinction, forced worker/main
publication interleaving, cross-axis residual lifecycle, or near-`int32_t`
dragscroll accumulation.

## Closure Verdict

The review remains **open**. The current branch is buildable and resource-safe
under its enforced gates, but it is not correct enough to reduce the remaining
work to hardware testing only.

## Next Steps

1. Implement provisional physical ownership with finalize-time commit or
   rollback, plus the missing handled-key overlap tests.
2. Implement coherent split remote-state publication and deterministic
   interleaving tests.
3. Define and test arrow dominant-axis transition semantics.
4. Bound dragscroll/pinch residual accumulation and test the numeric edges.
5. Re-run focused tests, feature gates, the full host suite, ordinary target
   compile, memory gate, and fresh linked stack gate.
6. Only after software closure, perform the Finding 05 and Finding 10 physical
   matrices.

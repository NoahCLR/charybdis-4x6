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

## 2026-08-16 — Software Remediation Landed

All four software findings from this review are now implemented and enforced.
The two hardware-verification obligations are unchanged and still open.

### Finding 08 — report ownership settles on the final event result

The audit called this "pre-process records physical owners". Reproducing it
first showed the user-visible severity was higher than recorded: a host test
driving the real pre-process/process/finalize seam proved that holding `KC_1`
registered Shift and never registered `KC_1`, so every authored shifted-symbol
hold and Shift+Enter emitted a bare modifier. The regression entered with
`753f73bf`, the original Finding 08 remediation.

Implementing the fix exposed a distinction the audit did not name. The physical
refcounts answer two different questions, and only one of them was wrong:

- **"a physical modifier key is down"** — what masking policy needs.
  `pd_mode_pinch.c` deliberately reads it to keep a user-held GUI visible while
  hiding the mode-owned GUI from concurrent plain keys. This stays in
  pre-process, unchanged.
- **"QMK's default handler put this in the report"** — what teardown needs, and
  only knowable once the event result is final.

`owned_keycode`'s counts only ever gated report decisions, so they moved whole
to the finalize hook, gated by a per-position committed bitmap in
`key_runtime_core_state_t` so a press and its release stay balanced even when
preflight consumes the release. `keyboard_mod_ownership` keeps its physical
counts and gains separate report counts, used solely by the `del_mods` teardown
in `unregister_mods()`. `tests/host/key_runtime_physical_ownership_integration_test.c`
covers the shifted-symbol hold, Shift+Enter, a default-processed physical key
overlapping a managed owner of the same usage, and a consumed handled modifier
interleaved with a managed modifier owner.

### Finding 15 — coherent publication across the worker/main boundary

`users/noah/lib/state/shared/runtime_publication.h` adds a single-writer
publication generation with two shapes: an in-place seqlock for the three split
feedback domains that are too large to double buffer, and slot publication for
pd-mode's mirrored identity. Readers re-check the generation around their copy
and retry a bounded number of times, keeping their previous coherent copy rather
than blocking, because the `SlaveThread` runs at `HIGHPRIO` and must never be
delayed by rendering work. A critical section was rejected for that reason, and
the transport's own `split_shared_memory` mutex is held across blocking serial
I/O so main-context readers must not take it.

Two seams were closed beyond the audit's description:

1. `rgb_runtime_pd_mode_stage_render()` took mirrored identity and the owner
   bitmap from two separate accessors. Each was individually coherent, but a
   publication landing between the two calls still paired identity from one
   generation with an owner bitmap from the next.
   `pd_mode_snapshot_with_owner_bitmap()` now serves both from one generation.
2. `pd_mode_snapshot()` initially copied the whole published slot, pulling the
   owner bitmap onto a bounded main-loop stack path that does not render owner
   keys. It now takes identity only, which is exactly as coherent under slot
   publication.

Interleaving is proven deterministically rather than by timing: a publish seam
compiled in only under a test backend fires between two field stores of the same
publication, and the test performs a real reader call from inside it. Each case
asserts the raw fields at the seam really are half-new, that the reader still
returns the complete previous packet, and that a later read returns the complete
new one.

### Findings 10 and 11 — pointing mode bounded state

Arrow mode cancels the inactive axis at an actual dominant-axis transition
rather than on the next report that carries motion on the old axis, which a
purely vertical report never does. Because only the dominant axis accumulates,
the invariant is now "the inactive axis is empty", so the previous cross-axis
resets were removed rather than kept alongside. Dragscroll and pinch accumulate
saturating at a documented residual cap, and absolute-value handling is defined
across the whole `int32_t` domain. This is robustness, not a reachable defect:
the drain path leaves less than one divisor behind and an 80 ms pause discards
the buffer, so overflow needs hours of continuous motion that never resolves
into an axis lock.

### Resource result

The reviewed main-process stack path improved from 1,912 B to **1,872 B**
against its 1,920 B budget. Making `pd_mode_snapshot_fill_view()` write through
a pointer instead of returning a view by value more than paid for the coherence
cost; the reviewed-path function list in `tools/firmware_stack_budget.json` was
updated for the rename. Added static state is about 40 B, visible as linker heap
moving from 212,352 B to 212,312 B against its 204,800 B minimum. Static BSS
span is unchanged at 25,524 B because the runtime singleton and split remote
struct sit outside the measured `__bss_base__`/`__bss_end__` span.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0, no failures
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

One correction to this review's own Verification section: the full host suite
could not have passed as recorded on 2026-08-15. `run_owned_keycode_tests.sh`,
`run_macro_payload_engine_tests.sh`, and `run_feature_gate_compile_tests.sh`
shell out to `rg`, which was not installed on the machine, so the first aborted
the suite and the other two would have passed vacuously. Ripgrep was installed
on 2026-08-16 and all three now genuinely enforce. The first real run of the
owned-keycode guard immediately caught a new raw-report-caller match, in a code
comment added during this remediation.

## Next Steps

1. Flash and run the Finding 05 two-half persistence, power-cycle, reconnect,
   and USB-role-swap matrix.
2. Flash and run the Finding 10 arrow timing/feel matrix, now that inactive-axis
   semantics are settled.
3. Consider two follow-ups this pass deliberately did not take:
   - `split_runtime_sync_remote.pd_mode_owner_sides` and `.pd_mode_owner_bitmap`
     are written by the base RPC and never read in production, since RGB takes
     ownership from the pd-mode copy. That looks like dead published state.
   - `NOAH_RUNTIME_TRACE_ENABLE` remains main-context-only by assumption. If
     trace-enabled hardware diagnostics are ever wanted, the split worker's
     writer contract for the shared trace ring needs to be made explicit first.

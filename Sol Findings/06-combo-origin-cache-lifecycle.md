# Finding 06: Bound the Combo-Origin Pending Cache Lifecycle

## Plan metadata

- **Severity:** Should-fix (P1 correctness and bounded-resource risk)
- **Status:** Verified on 2026-08-15
- **Affected surfaces:** QMK combo compatibility, combo-origin attribution, RGB/key feedback, split combo snapshots, authored overlapping combos
- **Primary files:** [qmk_combo_origin.c](../users/noah/lib/compat/qmk_combo_origin.c), [feedback.c](../users/noah/lib/key/runtime/feedback.c), [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- **Prerequisites:** Confirm the exact combo-buffer timing and state transitions in the pinned QMK fork before choosing the expiry bound
- **Recommended phase:** Phase 2; independent of macro playback and safe to run alongside it after ownership seams are understood

## Problem statement

The compatibility layer records every physically complete combo as a possible future QMK combo output. Some physically complete combos never emit because QMK later suppresses them in favor of a longer overlapping combo. Those candidates currently have no terminal path unless a matching combo release event is eventually emitted.

That leaves stale physical-origin bitmaps in a four-entry cache. The stale entries can both consume all candidate capacity and make unrelated keys look like active combo members, which suppresses unresolved-tap feedback.

Clearing candidates as soon as a physical member is released is not correct. QMK can legitimately delay a combo output until after the physical release, including tap-only and buffered cases. The fix therefore needs an explicit candidate lifecycle that distinguishes delayed legitimate output from a combo that QMK has definitively dropped.

## Current evidence and failure scenario

- users/noah/lib/compat/qmk_combo_origin.c:42-47 sizes both combo caches from COMBO_BUFFER_LENGTH, whose default is four.
- users/noah/lib/compat/qmk_combo_origin.c:318-333 records every combo that can be constructed from the currently pressed physical keys.
- users/noah/lib/compat/qmk_combo_origin.c:531-550 calls that recorder immediately after every physical press.
- users/noah/lib/compat/qmk_combo_origin.c:553-555 clears only the physical-key state on release. It intentionally leaves pending candidates intact.
- users/noah/lib/compat/qmk_combo_origin.c:568-585 consumes a pending candidate if a delayed COMBO_EVENT press arrives.
- users/noah/lib/compat/qmk_combo_origin.c:588-596 clears pending state only after a matching emitted combo release.
- users/noah/lib/compat/qmk_combo_origin.c:620-626 ORs every active pending candidate into the pressed-combo bitmap.
- users/noah/lib/key/runtime/feedback.c:573-577 removes unresolved-tap feedback for every key present in that bitmap.
- keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:189-196 contains real overlapping chords, including the two-key N+M combo and the longer N+M+comma combo.
- In the pinned QMK fork, quantum/process_keycode/process_combo.c:469-493 disables the shorter buffered combo when a longer overlapping combo wins. No matching combo output event is generated for the dropped candidate.

Concrete failure:

1. Press the members of a short combo.
2. Before QMK applies it, complete a longer overlapping combo.
3. The compatibility layer has already cached both physical completions.
4. QMK disables the shorter candidate and emits only the longer combo.
5. The shorter pending entry never receives a press/release pair and remains active.
6. Repeating this with suppressed candidates can occupy all four slots; their bitmaps also continue hiding unresolved-tap feedback.

## Required invariants

1. Every pending candidate reaches exactly one terminal state: promoted to emitted/active, explicitly suppressed, expired after the legal output window, or cleared by reset.
2. A candidate that can still legally produce a delayed QMK output remains available for origin normalization after its physical members are released.
3. A candidate QMK has marked disabled/dropped is removed before it contributes to feedback or consumes reusable capacity.
4. Candidate identity includes at least combo index and a physical-completion generation; a later activation of the same combo must not consume an older candidate accidentally.
5. Cache replacement never evicts a still-valid candidate when an expired or suppressed entry is available.
6. A full cache fails conservatively: origin may fall back, but no unrelated bitmap is attributed to an output.
7. Timer comparisons remain correct across uint16_t timer wrap.
8. Active combo release attribution continues to use the bitmap captured for the matching emitted press.

## Scope

This pass should:

- model pending-candidate states explicitly;
- reconcile candidates with QMK active/disabled state after combo processing;
- retain delayed-output candidates for a bounded, contract-derived window;
- make cache overflow and expiry observable in host tests and optional diagnostics;
- preserve dynamic/VIA-resolved keycode matching and existing origin fallback behavior;
- cover the authored overlapping-combo shapes.

## Non-goals

- Reimplementing QMK's combo engine.
- Changing authored combo precedence, COMBO_TERM, or chord membership.
- Increasing COMBO_BUFFER_LENGTH as the primary fix.
- Clearing every candidate on physical release.
- Moving general combo behavior out of the compatibility module.

## Detailed implementation plan

### Step 1: Lock down the QMK timing contract

1. Inspect the pinned fork's process_combo.c paths for normal, must-tap, must-hold, timer-expired, disabled, and overlap-dropped candidates.
2. Record the latest point at which a COMBO_EVENT press can legally arrive after physical completion or release under this repository's feature flags.
3. Express that contract through one compatibility helper rather than duplicating QMK constants throughout the module.
4. Add compile-time guards for any fork fields used directly, including the EXTRA_SHORT_COMBOS representation.

The expiry window must be based on the fork contract. Do not introduce an arbitrary wall-clock timeout merely because it clears the leak in the common case.

### Step 2: Give pending candidates an explicit identity and state

Extend combo_origin_pending_output_entry_t with the minimum state needed to represent:

- empty;
- awaiting QMK resolution;
- confirmed output/consumed;
- suppressed or expired, immediately reusable.

Store a monotonically increasing physical-completion generation in addition to combo_index, keycode, owner position, complete_at, and bitmap. If the generation counter wraps, equality must remain safe because no more than COMBO_BUFFER_LENGTH generations can be live; alternatively use a nonzero generation plus an explicit active flag.

Make candidate insertion idempotent for the same combo generation. Re-observing the same fully pressed chord must refresh neither age nor eviction priority.

### Step 3: Add post-QMK reconciliation

Add a narrowly named compatibility helper that runs after QMK has processed the physical record or at the first scan boundary after it. That helper should:

1. inspect each awaiting candidate's matching combo_t state;
2. retire candidates QMK has marked disabled;
3. preserve candidates that are active or still within the legal buffered-output window;
4. expire released, inactive candidates only after QMK has had its final legal opportunity to emit them;
5. clear any expired candidate before pressed_combo_bitmap is built.

The hook ordering is part of the contract. Add or extend a hook-order/compile test so the reconciliation helper cannot silently move ahead of QMK combo processing.

If the fork does not expose enough state to distinguish buffered from dropped candidates reliably, add a small compat-owned resolution marker at the existing combo hook seam. Do not infer success from physical state alone.

### Step 4: Correlate emitted press and release events

On COMBO_EVENT press:

1. prefer the newest compatible candidate for the exact combo index;
2. promote/copy its origin into the active cache;
3. retire that exact candidate generation;
4. leave unrelated candidates with the same output keycode untouched.

On COMBO_EVENT release:

1. resolve against the active cache first;
2. clear only the matching active generation;
3. tolerate a missing candidate by using the existing conservative fallback without clearing other entries by keycode.

This removes the current ambiguity where two combos sharing an output keycode can clear one another's pending state.

### Step 5: Define deterministic capacity behavior

When storing a candidate:

1. reuse an empty, suppressed, or expired slot first;
2. then reuse the oldest terminal slot;
3. if every slot is still legally awaiting output, refuse the new candidate and increment a diagnostic counter;
4. never overwrite an awaiting entry silently.

Origin fallback for the refused candidate should remain conservative. The fallback must not add the refused candidate's bitmap to pressed_combo_bitmap.

### Step 6: Keep feedback and split state honest

1. Ensure noah_qmk_combo_origin_pressed_combo_bitmap includes only physically complete live combos and valid awaiting candidates.
2. Notify split combo state dirty when reconciliation removes the last contribution of a candidate.
3. Verify unresolved-tap feedback returns immediately after a suppressed/expired entry retires.
4. Keep cache state local; synchronize only the derived bitmap/state already owned by split runtime sync.

## Test plan

### New focused cases

Extend tests/host/qmk_combo_origin_test.c and its existing runner with:

- a short overlapping combo that is cached and then disabled when a longer combo wins;
- four suppressed candidates followed by a valid combo, proving capacity is recovered;
- a legitimate output that arrives after all physical members are released but before the legal deadline;
- expiry one tick before, at, and one tick after the deadline;
- uint16_t timer wrap while a delayed candidate is pending;
- two combo indices with the same output keycode, proving generation/index correlation;
- cache-full behavior with no live entry overwritten;
- pressed_combo_bitmap and unresolved-feedback behavior before and after retirement;
- reset clearing every lifecycle state and diagnostic counter as specified.

### Targeted runners during implementation

~~~sh
sh tests/host/run_qmk_combo_origin_tests.sh
sh tests/host/run_key_runtime_scenario_tests.sh
sh tests/host/run_key_runtime_integration_harness_tests.sh
sh tests/host/run_hook_chaining_tests.sh
sh tests/host/run_split_runtime_sync_tests.sh
sh tests/host/run_real_profile_validation_tests.sh
~~~

If hook wiring, compat headers, or source manifests change, also run:

~~~sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_qmk_contract_checks.sh
~~~

### Closure gates

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

Under the existing diagnostics/console gates, expose:

- pending-candidate high-water mark;
- suppressed-candidate retirement count;
- deadline-expiry count;
- cache-full refusal count;
- unmatched delayed-output count.

Host tests should assert counters. Production logging should be edge-triggered or snapshot-based so combo processing does not become a noisy hot path.

## Risks, tradeoffs, and fallback

- **Expiry too short:** breaks legitimate delayed output attribution. Mitigate by deriving it from the pinned QMK paths and testing delayed release emission.
- **Expiry too long:** retains stale feedback and capacity longer than needed. Suppressed-state reconciliation should be the primary retirement mechanism; expiry is a safety net.
- **Hook-order dependence:** a pre-QMK reconciliation pass can observe stale state. Enforce post-processing order mechanically.
- **Shared keycodes:** keycode-only matching can consume the wrong candidate. Use combo index and generation whenever QMK supplies them.
- **Fork drift:** direct combo_t flag assumptions can change upstream. Keep them in users/noah/lib/compat and cover both compact/noncompact representations with compile gates.
- **Fallback if state is not observable:** add a compat-local notification at the QMK hook boundary; do not fall back to immediate release clearing.

## Documentation and review-note updates

When the implementation lands:

- update the combo-origin comments in keymap.c if lifecycle or fallback semantics change;
- update the relevant README/docs explanation if users can observe changed combo feedback timing;
- update the active runtime review's progress.md with code references, tests, and measured cache behavior;
- if the relevant runtime review is already closed, open the next sortable review folder instead of editing closed history;
- record the pinned QMK combo contract in the review note so later fork upgrades can re-audit it.

## Acceptance checklist

- [x] Suppressed overlapping combos retire without an emitted combo event.
- [x] A legitimate delayed output after physical release still receives the correct origin bitmap.
- [x] All four slots can be recovered after repeated suppressed candidates.
- [x] Same-keycode combos cannot consume or clear one another's generations.
- [x] Stale candidates no longer suppress unresolved-tap feedback.
- [x] Cache-full behavior is conservative and observable.
- [x] Timer wrap and deadline boundaries are tested.
- [x] Hook order and fork-specific combo fields have mechanical coverage.
- [x] All targeted runners pass.
- [x] The full host suite passes.
- [x] The Charybdis firmware compile passes.
- [x] Documentation and the active review note match the landed tree.

## Implementation checkpoint — 2026-08-15

- Pending and active origins are exact `(combo_index, generation)` entries.
  Nonzero generations are allocated away from every live entry, including
  counter wrap.
- The scan boundary reconciles the pinned QMK state before key-runtime
  projection. Disabled candidates retire immediately; inactive candidates
  survive the first scan beyond the profile-wide legal wait so the following
  `combo_task()` gets its final opportunity, then expire on the next scan.
- Full pending storage refuses the new candidate instead of overwriting a live
  origin. Snapshot diagnostics expose current counts, high-water, suppression,
  expiry, refusal, and unmatched output.
- Same-keycode output presses promote one exact origin and releases select the
  matching active footprint from the triggering physical release member.
- Focused coverage passed for suppression, delayed output, timer boundaries and
  wrap, capacity refusal/recovery, same-output identity, reset, diagnostics,
  normal/compact QMK layouts, timerless compilation, runtime scan ordering,
  hook integration, split sync, and the real profile.
- Full host, ordinary firmware, and the fresh reviewed-path target gate pass.
  The new combo retirement path is 280 B. The worst reviewed main path is
  1,816 B of 1,920 B; split remains 328 B of 768 B.
- The final instrumented image is 144,172 B text and 245,592 B total BSS. The
  exact lifecycle symbols occupy 96 B pending cache, 96 B active cache, 12 B
  diagnostics, and 4 B generation state. Relative to the prior Finding 08
  instrumented image, text increased 648 B while total BSS stayed constant as
  the fixed RAM layout reduced heap space.
- No sibling source was edited. The pinned QMK fork was inspected and its
  generated build artifacts were refreshed for target evidence only.

## Next action

Finding 06 is closed. Continue Phase 2 with Finding 07's nonblocking macro
playback scheduler, reusing the owner-scoped literal-key leases from Finding 08.

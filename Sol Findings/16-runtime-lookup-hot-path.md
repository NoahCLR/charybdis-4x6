# Finding 16: Bound Runtime Lookup and Active-State Hot Paths

## Plan metadata

- Severity: medium optimization
- Status: planned
- Recommended phase: Phase 6 profile-guided optimization, after correctness-critical identity and ownership fixes
- Affected surfaces:
  - users/noah/lib/key/behavior/key_behavior_lookup.c
  - users/noah/lib/key/behavior/key_behavior_lookup.h
  - users/noah/lib/key/behavior/handled_key_lookup.c
  - users/noah/lib/key/runtime/reducer/runtime.c
  - users/noah/lib/key/runtime/reducer/runtime.h
  - users/noah/lib/key/runtime/reducer/state_query.c
  - users/noah/lib/key/runtime/scan.c
  - users/noah/lib/key/runtime/feedback.c
  - users/noah/lib/key/runtime/projection/feedback_projection.c
  - users/noah/lib/split/runtime_sync_dirty.c
- Prerequisites:
  - Correctness fixes for token and pending-release rollover must land first.
  - Add test-only counters before selecting an index or active-set representation.
  - Coordinate feedback projection generations with [Finding 15](15-rgb-render-work.md).

## Problem statement

Authored behavior lookup is a linear scan, and a single resolution can invoke that scan more than once. The event pipeline can repeat resolutions in observation, preflight, handled-stage, and fallback paths. At the audited profile size of 35 behavior rows, an absent keydown can perform roughly six full scans, or about 210 keycode comparisons.

The runtime then repeatedly traverses matrix-sized press-token and tap-series arrays. Feedback projection traverses them again, and an active runtime marks split key feedback dirty on every scan even when no visible semantic changed. These costs are bounded today, but they occur in latency-sensitive firmware paths and scale with authored data and matrix capacity rather than actual active keys.

## Current evidence and workload

- users/noah/lib/key/behavior/key_behavior_lookup.c:16-20 linearly scans key_behaviors for one keycode.
- users/noah/lib/key/behavior/key_behavior_lookup.c:159-168 calls that lookup independently for step, more-taps, and future-path queries.
- users/noah/lib/key/behavior/handled_key_lookup.c:7-10 obtains a behavior view, then may call key_behavior_step_lookup and always calls key_behavior_has_more_taps, causing repeated config searches.
- users/noah/lib/key/runtime/reducer/runtime.c:302-319 resolves the authored behavior when beginning a press.
- users/noah/lib/key/runtime/reducer/runtime.h:27-30 sizes press tokens and tap series to MATRIX_ROWS × MATRIX_COLS; the audited target has 60 slots.
- users/noah/lib/key/runtime/reducer/runtime.c:249-269 refreshes all press-token positions and corresponding tap-series expiry, then scans all press slots again for scan phase work at runtime.c:888-910.
- users/noah/lib/key/runtime/scan.c:19-33 first queries whether any core work exists and then unconditionally marks key feedback dirty whenever core work exists.
- users/noah/lib/key/runtime/feedback.c:496-535 scans all tap-series and press-token slots for broad owners.
- users/noah/lib/key/runtime/feedback.c:537-579 scans those arrays again for semantic output.
- users/noah/lib/key/runtime/feedback.c:581-597 scans tap series again for branch output.

Representative scenario:

1. Press a key absent from key_behaviors.
2. Multiple pipeline stages each prove absence with a full 35-row scan.
3. While one unrelated handled key remains active, each matrix scan traverses 60 press slots several times.
4. Feedback is marked dirty even when no phase, deadline, or semantic changes.
5. Split packet builders and RGB rendering may rebuild maps that are byte-identical to the prior projection.

## Required invariants

1. Behavior resolution remains identical for every keycode, tap count, layer state, and authored row.
2. Duplicate and unreachable behavior validation remains authoritative.
3. A physical press performs at most one authored-row search in the normal pipeline.
4. A matched release uses the interaction cached in its press token; it does not re-resolve authored data.
5. Unmatched releases retain safe cleanup fallback behavior.
6. Active-set acceleration cannot lose, duplicate, or reorder runtime transitions.
7. Counts, bitmaps, or indexes remain consistent through cancellation, deferred release, reset, and capacity exhaustion.
8. Split feedback becomes dirty only when a visible projection changes or a visible time deadline is crossed.
9. Optimization choices are justified by counters from the real profile, not asymptotic preference alone.
10. Stack and BSS changes are measured on the target.

## Scope

- Eliminate duplicate config scans within one behavior resolution.
- Add a small authored-profile lookup index if measurements still justify it.
- Propagate resolved behavior through pipeline stages.
- Replace repeated full active-slot scans with active indexes, bitmaps, or due-deadline tracking where proven valuable.
- Make feedback dirtiness event/deadline driven.
- Add deterministic operation-count budgets to host tests.

## Non-goals

- Do not generically rewrite the key runtime.
- Do not change authored key behavior structure or semantics solely for speed.
- Do not remove validation to save startup work.
- Do not add a 65,536-entry direct keycode table.
- Do not optimize validation-only O(N²) duplicate checks unless measurement shows startup impact.
- Do not cache layer-dependent materialization beyond the layer state for which it was resolved.

## Implementation plan

### Phase 1: Instrument the real hot paths

1. Add test-only counters for:
   - config rows compared
   - behavior resolutions requested
   - press-token slots visited
   - tap-series slots visited
   - semantic, broad-owner, and branch projection builds
   - split feedback dirty notifications
2. Exercise present and absent authored keycodes, first and later taps, normal presses/releases, overlapping active keys, and idle scans.
3. Record counts for the current real profile and synthetic profiles with 0, 1, 35, and maximum supported rows.
4. Keep wall-clock microbenchmarks secondary; operation counts are more stable across host machines.

### Phase 2: Resolve one config pointer once

1. Add internal helpers that accept const key_behavior_t pointers for step, more-taps, future-path, and view construction.
2. Make handled_key_lookup_tap_count call key_behavior_config_lookup once and derive all fields from that pointer.
3. Return or retain a complete handled_key_resolution_t so downstream stages do not ask the authored table again.
4. Audit observation, transparency, preflight, press, release, and unmatched-release paths for redundant resolution.
5. Preserve a single explicit fallback lookup only where no press token exists.
6. Add a host budget: one config search for a normal press resolution and zero for its matched release.

### Phase 3: Choose a profile-sized index

1. Re-measure after removing duplicate calls. Stop if the remaining linear search is below the agreed budget.
2. If an index is still justified, compare:
   - a sorted uint8_t row-index array plus binary search
   - a small fixed open-addressed table sized from key_behavior_count
   - generated sorted authored data, only if authoring order is not semantically significant
3. Prefer the sorted row-index array unless measurements show otherwise. It adds roughly one byte per authored row, preserves the source table order, and avoids hash sizing/tombstone policy.
4. Build and validate the index during runtime initialization, or generate it with compile-time authored materialization if initialization ordering becomes awkward.
5. Fail validation deterministically on duplicate keycodes; never let index construction silently pick a different row.
6. Test empty, one-row, duplicate, unsorted-source, present, absent, low/high keycodes, and maximum-row profiles.

### Phase 4: Track active and due runtime entries

1. Measure which full-array loops dominate after lookup improvement.
2. Introduce active bitmaps or compact index sets for press tokens and tap series. Matrix-position-indexed storage can remain the source of truth.
3. Centralize all activate/deactivate mutations so array state, count, and bitmap update atomically.
4. Iterate set bits for interrupt, refresh, scan, and feedback paths where ordering does not alter behavior.
5. For time-based work, maintain the nearest relevant deadline or a due set so scans can skip state whose phase cannot change yet.
6. Keep a debug verifier that recomputes active bits/counts from arrays and asserts equality in host tests.
7. Explicitly test token cancellation, repeated press on an occupied slot, multi-tap expiry, deferred release, reset, and rollover-adjacent paths.

### Phase 5: Make projection dirtiness semantic

1. Remove the unconditional dirty notification at users/noah/lib/key/runtime/scan.c:31 only after all visible transitions have explicit invalidation.
2. Have reducer effects or projection code increment a feedback generation when any of these changes:
   - semantic kind
   - branch selection
   - broad owner
   - combo suppression input
   - flash visibility phase
   - preview layer
3. Track the next flash/confirmation deadline and invalidate at that crossing, not every scan while a key is active.
4. Let split sync and RGB snapshots consume generation changes.
5. Test that identical active scans produce no dirty notification while every visible threshold still produces exactly one update.

### Phase 6: Add budgets and target measurements

1. Set operation-count budgets from the improved implementation:
   - one authored search per normal press
   - zero on matched release
   - no full behavior-table scan when an index is enabled
   - active-slot visits proportional to active/due entries
   - zero feedback dirty notifications for a scan with no visible change
2. Measure target text, BSS, and stack changes.
3. Reject an index that saves small lookup time at disproportionate code/RAM complexity.

## Test and verification plan

Add focused cases for lookup result parity, comparison budgets, resolution propagation, matched/unmatched releases, active-index consistency, deadline transitions, and dirty-generation behavior.

Run targeted checks repeatedly:

1. sh tests/host/run_key_behavior_lookup_tests.sh
2. sh tests/host/run_key_behavior_validation_tests.sh
3. sh tests/host/run_key_runtime_scenario_tests.sh
4. sh tests/host/run_key_runtime_integration_harness_tests.sh
5. sh tests/host/run_key_runtime_release_matrix_tests.sh
6. sh tests/host/run_split_runtime_sync_tests.sh
7. sh tests/host/run_rgb_layer_render_tests.sh
8. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah

## Observability and measurements

- Comparisons per present and absent key resolution.
- Slot visits per idle scan and per 1, 2, and 10 active keys.
- Projection builds and dirty notifications over a held key with no threshold crossing.
- Target scan-loop timing for representative key and RGB workloads.
- Text, BSS, and stack deltas for the chosen index and active-set structures.
- Debug consistency-verifier coverage of every active-state mutation path.

## Risks, tradeoffs, and fallbacks

- An active bitmap can become a second source of truth. Centralized mutation and debug recomputation are mandatory.
- Binary search requires an index lifecycle; lookup before initialization must remain safe.
- Caching a materialized behavior across a layer change can be wrong. Cache the authored config/resolution contract, and keep layer-dependent materialization scoped.
- Deadline-driven dirtiness can miss an animation transition if one mutation path is overlooked.
- If active-set bookkeeping produces too much churn, retain fixed arrays and land only one-lookup resolution plus semantic dirty generations.

## Documentation and review-note updates

- Document the lookup index, active-set invariants, and feedback-generation ownership in the active runtime architecture note.
- Update the active open review progress.md during implementation, or open the next sortable folder if the relevant review is closed.
- Record baseline/final counters and target memory/timing results.
- Update developer documentation if new profiling counters or commands are retained.

## Acceptance checklist

- [ ] Normal press resolution searches authored rows at most once.
- [ ] Matched release performs no authored lookup.
- [ ] Lookup results match the linear reference for all tested profiles/keycodes.
- [ ] Active-index/count invariants survive all lifecycle paths.
- [ ] Idle or unchanged active scans avoid unnecessary slot/projection work.
- [ ] Visible deadlines still invalidate feedback exactly when required.
- [ ] Operation-count budgets are enforced in host tests.
- [ ] Targeted lookup, runtime, split, RGB, and feature-gate checks pass.
- [ ] The full host suite passes.
- [ ] The target QMK compile passes.
- [ ] Target stack/BSS/text measurements and architecture notes are current.

## Next action

Add test-only comparison, slot-visit, and dirty-notification counters. First remove repeated config lookup inside handled_key_lookup_tap_count; only then decide whether the remaining real-profile search merits a sorted index.

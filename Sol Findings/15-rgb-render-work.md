# Finding 15: Eliminate Repeated RGB Render Projection Work

## Plan metadata

- Severity: medium optimization
- Status: planned
- Recommended phase: Phase 5 RGB performance, after preview parity and coordinated with runtime projection invalidation
- Affected surfaces:
  - users/noah/lib/rgb/core/rgb_runtime.c
  - users/noah/lib/rgb/core/rgb_helpers.h
  - users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c
  - users/noah/lib/rgb/stages/rgb_key_feedback_stage.c
  - users/noah/lib/key/runtime/feedback.c
  - users/noah/lib/split/runtime_sync.c
  - tests/host/rgb_layer_render_test.c
  - tests/host/split_runtime_sync_test.c
- Prerequisites:
  - Land [Finding 13](13-rgb-preview-parity.md) so performance work has a single correct layer-render contract.
  - Coordinate projection invalidation with [Finding 16](16-runtime-lookup-hot-path.md).
  - Establish instrumentation before choosing a frame-cache lifecycle.

## Problem statement

QMK invokes the advanced RGB indicator callback in LED chunks. The current callback reruns every enabled stage for every chunk, even when the current physical half cannot intersect that chunk and even when the stage's source projection is unchanged.

Several stages also rebuild the same projections within one invocation. Key feedback creates a semantic map, then asks a wrapper to rebuild the semantic map while deriving flash visibility. Combo underlay and overlay each rebuild both partition bitmaps through wrapper APIs. Key-feedback suppression combines those partitions again. This multiplies scans of runtime slots, combo state, and matrix positions without changing the resulting colors.

## Current evidence and quantified workload

- users/noah/config.h:76-87 defines 58 global LEDs split 29 and 29.
- The audited build processes 12 LEDs per callback, producing five chunks for 58 LEDs.
- On MASTER_RIGHT, the first two projected chunks do not intersect the local 29-LED half; audited calls arrived as empty/inverted local ranges equivalent to (29, 12) and (29, 24).
- users/noah/lib/rgb/core/rgb_runtime.c:114-139 runs diagnostics and all enabled base/underlay/preview/pointing/overlay/key-feedback stages without a top-level empty-range fast exit.
- users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:463-479 builds semantic, tap-branch, flash, and broad-owner maps for every chunk.
- users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:149-155 calls key_feedback_flash_visibility_bitmap on the master.
- users/noah/lib/key/runtime/feedback.c:471-479 shows that wrapper rebuilding the semantic map before deriving flash visibility, even though feedback.c:449-469 already provides key_feedback_flash_visibility_bitmap_for_semantic_map.
- users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c:37-52 fetches underlay or overlay separately.
- users/noah/lib/key/runtime/feedback.c:599-631 shows each single-bitmap wrapper builds both partitions and discards one.
- users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:104-119 invokes both single-bitmap wrappers again and combines them for suppression.
- users/noah/lib/split/runtime_sync.c:99-103 already demonstrates the combined combo_feedback_bitmaps API that obtains both partitions in one projection.
- The audit estimated roughly ten semantic-map builds and twenty combo partitions across five chunks, including idle frames.

## Required invariants

1. Optimized rendering is per-LED color-equivalent to the current correct renderer for every enabled stage ordering.
2. A source snapshot is internally coherent: semantic, flash, broad-owner, tap-branch, combo underlay, and combo overlay fields describe the same logical generation.
3. No stage does projection work for a chunk that cannot intersect the current physical half.
4. Each expensive logical projection is built at most once per render snapshot, not once per stage and chunk.
5. Time-dependent effects refresh at their actual deadline; caching must not freeze flashing or gradients.
6. Remote split packet updates invalidate the relevant snapshot.
7. Local key-runtime, combo, layer, pointing, and configuration transitions invalidate only the projections they affect.
8. Chunked output matches one-shot full-range output.
9. RAM used by snapshots is bounded and included in the target memory report.

## Scope

- Add a top-level current-half/chunk intersection guard.
- Reuse a semantic map when deriving flash visibility.
- Build combo underlay and overlay together and reuse them across consumers.
- Introduce a bounded render-source snapshot or generation-keyed cache.
- Add counters and performance budgets to host tests.

## Non-goals

- Do not change authored RGB colors, locality, or stage order.
- Do not merge every RGB stage into one generic renderer.
- Do not optimize before tests prove exact color parity.
- Do not rely on a cache lifetime inferred from undocumented QMK callback ordering without an invalidation fallback.
- Do not fold the optional private WS2812 driver-symbol/half-wire-time cleanup into this pass; track it separately after projection work is measured.

## Implementation plan

### Phase 1: Instrument and lock color parity

1. Add host counters around:
   - semantic-map builds
   - flash-visibility derivations
   - broad-owner builds
   - tap-branch builds
   - combo partition builds
   - stage invocations
2. Add a test renderer that captures all 58 final colors and painted return values.
3. Compare rendering as one full range with rendering as 12-LED chunks.
4. Run fixtures for idle, each feedback semantic, combo underlay/overlay, preview, pointing overlay, flashing visibility, and master/secondary roles.
5. Record the current projection counts as the failing performance baseline.

### Phase 2: Add the physical-half fast exit

1. Centralize a helper that intersects the QMK global chunk with the LEDs physically driven by the current half.
2. Normalize or reject empty/inverted ranges before any stage runs.
3. At entry to noah_rgb_matrix_indicators_advanced_user, return false for a chunk with no locally addressable LEDs.
4. Test left master, right master, each boundary-crossing chunk, zero-length ranges, and ranges outside RGB_MATRIX_LED_COUNT.
5. Do not change the global LED indices consumed by authored groups or row/column mapping.

### Phase 3: Remove duplicate work within one callback

1. In key feedback:
   - build the semantic map once
   - pass it to key_feedback_flash_visibility_bitmap_for_semantic_map
   - retain the existing current tap-branch and broad-owner snapshots
2. In combo feedback:
   - call combo_feedback_bitmaps once to obtain both partitions
   - pass the appropriate const bitmap to underlay and overlay renderers
   - build the combined bitmap once for unresolved-semantic suppression
3. Add narrow stage APIs that accept prebuilt const snapshots. Keep convenience wrappers only for callers that truly lack a shared context.
4. Update split packet builders to continue using the same combined projection APIs.
5. Confirm there are no stack regressions from duplicating large snapshot structs in nested stage calls.

### Phase 4: Define a reliable render snapshot lifecycle

1. Introduce an rgb_runtime_render_snapshot_t containing only expensive source projections, not necessarily 58 final colors.
2. Choose one of these lifecycles based on QMK integration evidence:
   - explicit begin-frame hook or frame token, if available and mechanically tested
   - generation-keyed cache invalidated by local/remote changes and time deadlines
3. Do not use only led_min decreasing as a frame boundary unless every feature-gate and split ordering is proven; skipped or half-filtered chunks can make that heuristic fragile.
4. Build each projection lazily on first consumer access so disabled or inactive stages pay nothing.
5. Store a source generation for:
   - key feedback state
   - combo origin state
   - remote split packet state
   - layer/preview state
6. Add a next-time-dependent-deadline for flashing/gradient state. Expiry invalidates only the affected projection.
7. Keep snapshot construction outside deeply nested render calls to control stack use.

### Phase 5: Enforce a work budget

1. For an idle five-chunk frame, require:
   - zero projection builds for stages proven inactive, or one lazy snapshot build if activity detection requires it
   - no builds for nonintersecting local chunks
2. For an active key-feedback frame, require at most one semantic, flash, tap-branch, and broad-owner projection per generation.
3. For an active combo frame, require at most one combined underlay/overlay partition per generation.
4. Add an upper bound on snapshot BSS and stack use.
5. Preserve the full-color parity test as a mandatory oracle whenever the budget changes.

## Test and verification plan

Extend RGB host fixtures with:

- Full-range versus five-chunk color parity.
- Left- and right-master physical-range filtering.
- Boundary chunk at LED 29 and final partial chunk.
- Idle projection counters.
- Active key-feedback projection counters.
- Combo underlay and overlay are built together once.
- Semantic map is reused for flash visibility.
- Local state invalidation.
- Remote split packet invalidation.
- Flash deadline invalidation without unrelated state changes.
- Multiple chunks use one coherent snapshot.
- A new generation between chunks cannot mix old and new fields; either defer to the next frame or atomically rebuild according to the chosen contract.
- All feature-gated stage combinations compile.

Run targeted checks repeatedly:

1. sh tests/host/run_rgb_layer_render_tests.sh
2. sh tests/host/run_rgb_validation_tests.sh
3. sh tests/host/run_split_runtime_sync_tests.sh
4. sh tests/host/run_key_runtime_scenario_tests.sh
5. sh tests/host/run_runtime_trace_tests.sh
6. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah

## Observability and measurements

- Projection-build counters per full RGB cycle, not just per callback.
- Stage invocation and early-exit counts for both physical halves.
- Host timing for idle and representative active frames.
- Target scan-time or RGB-task timing with diagnostics compiled in for measurement.
- Snapshot BSS and maximum relevant stack frames from the target build.
- Final LED color hash for each parity fixture to make accidental visual changes obvious.
- Before/after firmware size.

## Risks, tradeoffs, and fallbacks

- Frame lifecycle inference can be wrong when QMK changes process limits or callback order.
- Generation invalidation can be missed if ownership is scattered. Add the cache only after all mutation sites are enumerated.
- Reusing one snapshot across chunks intentionally stabilizes a frame, but introduces up to one-frame latency for changes mid-frame; document and test that choice.
- A large snapshot can move work from CPU to RAM. Measure it against the macro-cache RAM work.
- New const-snapshot APIs can expand module seams. Keep them specific and update feature-gate tests.
- If robust cross-chunk caching is too risky, land the current-half guard and within-callback deduplication first. Those steps are independently valuable and easier to prove.

## Documentation and review-note updates

- Document the snapshot lifetime, generation sources, and time-based invalidation in the runtime architecture documentation.
- Update the active open runtime review progress.md during implementation, or open the next sortable review folder if the relevant review is closed.
- Record baseline and final projection counts, frame timing, stack/BSS change, and color-parity commands.
- Note explicitly that authored RGB appearance and stage order did not change.

## Acceptance checklist

- [ ] Nonintersecting current-half chunks exit before stage work.
- [ ] Key feedback reuses one semantic map for flash derivation.
- [ ] Combo partitions are built together and reused.
- [ ] Expensive projections are at most once per generation/frame contract.
- [ ] Local, remote, and time-deadline invalidation is complete.
- [ ] Full-range and chunked output are color-identical.
- [ ] Left/right role and feature-gate cases are covered.
- [ ] Snapshot RAM and stack costs are measured and bounded.
- [ ] Targeted RGB, split, key-runtime, trace, and compile-gate checks pass.
- [ ] The full host suite passes.
- [ ] The target QMK compile passes.
- [ ] Architecture documentation and review notes match the cache lifecycle.

## Next action

Add projection counters and full-range-versus-five-chunk color comparisons to rgb_layer_render_test.c. Then land the physical-half guard and within-callback semantic/combo reuse before deciding whether a cross-chunk generation cache is justified.

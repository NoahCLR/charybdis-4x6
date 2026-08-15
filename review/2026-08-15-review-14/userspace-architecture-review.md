# RGB Render-Work Architecture Review

Review 13 is closed as the immutable Finding 13 preview-parity snapshot.
Finding 15 is a distinct performance and frame-coherence topic, so this work
uses the next sortable review folder instead of changing Review 13.

## Findings

### Should-fix — Chunk callbacks rebuild identical feedback projections

QMK invokes the advanced RGB indicator callback in chunks. The current RGB
runtime asks combo and key-feedback stages to reconstruct their runtime source
maps for every chunk. On the master, the combo underlay, combo overlay, and
key-feedback suppression paths each call single-partition wrappers that build
both combo partitions. Key feedback also builds a semantic map and then asks a
flash wrapper to build it again.

This repeated work cannot change the intended colors within one RGB frame, but
it repeatedly scans runtime slots, combo state, and matrix positions.

### Should-fix — Nonlocal split chunks still enter every RGB stage

The right half receives the first global chunks as empty or inverted local
ranges, while the left half receives later chunks outside its physical LEDs.
`noah_rgb_matrix_indicators_advanced_user()` currently enters diagnostics and
every enabled stage before those stages independently discover that they have
nothing to paint.

## Prior Finding Status

| Prior finding | Status | Preservation requirement |
| --- | --- | --- |
| Finding 05: VIA split persistence | partially resolved | Software and target gates pass; the physical two-half matrix remains independent and pending. |
| Finding 10: pointing backlog bounds | partially resolved | Software and target gates pass; the flashed timing and feel check remains independent and pending. |
| Finding 13: RGB preview parity | resolved | Review 13 is closed; its normal-versus-preview parity fixture remains the color oracle. |
| Finding 15: RGB render work | resolved | `rgb_runtime.c` owns the exact-frame snapshot and physical filter; the 58-LED work/color fixture, feature gates, full host suite, target build, resource measurement, and fresh stack gate pass. |

## Reconciliation Note

The findings above are the audit-time baseline. The current tree removes
per-chunk source ownership from the combo and key-feedback painters, adds the
exact QMK frame snapshot, and rejects nonlocal chunks before stage work. The
work-count and target evidence below describe the landed state.

## QMK Frame Contract

The pinned QMK tree computes each callback range with
`rgb_matrix_get_limits(params->iter - 1)`. Therefore the exact result of
`rgb_matrix_get_limits(0)` is a mechanically identifiable first callback for
each RGB frame. This remains true on the right half, where the first range can
be inverted after split clamping.

The runtime will invalidate its source snapshot when the incoming range equals
that exact first-frame range. It will perform this check before rejecting a
nonintersecting physical range, so an inverted right-half first callback still
starts a fresh frame.

## Intended Structure

- Keep stage order and authored RGB behavior unchanged.
- At callback entry, identify the exact QMK frame start and invalidate a small
  runtime-owned source snapshot.
- Intersect the callback with the current physical half and return before stage
  work when the normalized range is empty.
- Build combo underlay and overlay together, lazily, at the first local
  consumer in a frame.
- Build key semantic, tap-branch, flash-visibility, and broad-owner projections
  together, lazily, reusing the semantic map for flash derivation and the
  already-built combo partitions for unresolved-semantic suppression.
- Pass immutable projection views into the stage painters. Keep rendering and
  source-projection ownership separate.

The snapshot deliberately stabilizes one RGB frame. A local event, remote
packet, or flash-time change after the first local chunk appears on the next
frame rather than mixing old and new fields across chunks. Since every QMK RGB
frame invalidates the snapshot, no additional scattered mutation hooks or
deadline bookkeeping are required.

## Resource Policy

The snapshot contains only packed source projections, never a second 58-LED
color frame. It is statically bounded. Host counters must prove each expensive
projection is built at most once per RGB frame, and the target build plus stack
gate must measure the final BSS, text, and reviewed stack maxima.

## Verification Strategy

- Add an independently runnable 58-LED, five-chunk fixture.
- Compare full-range and five-chunk colors for both physical sides.
- Enforce one semantic, flash, tap-branch, broad-owner, and combined-combo
  projection build per active frame.
- Prove nonintersecting, empty, inverted, and out-of-range callbacks never
  enter the stage pipeline.
- Mutate local and remote sources between chunks and prove the current frame is
  coherent while the next exact frame boundary refreshes the snapshot.
- Preserve Review 13's preview parity scenarios and all existing stage-order
  fixtures.

## Closure Evidence

- Code authority: `users/noah/lib/rgb/core/rgb_runtime.c` owns exact frame
  invalidation, physical normalization, and the lazy source snapshot.
- Painter seams: `rgb_combo_feedback_stage.c` and
  `rgb_key_feedback_stage.c` consume immutable source maps.
- Mechanical enforcement: the 58-LED workload variant in
  `tests/host/rgb_layer_render_test.c` plus Review 13's layer parity variant.
- Work budget: semantic 10→1, combined combo projection 20→1, and stage
  pipeline 5→3 per physical half.
- Resource bound: 80 B linked snapshot, 96 B compile ceiling, unchanged
  245,592 B linked BSS, and unchanged reviewed stack maxima.
- Verification commands are recorded in `progress.md`.

## Current Architecture Assessment

Finding 15 is **resolved**. One exact QMK RGB frame owns one coherent packed
source snapshot. Nonlocal ranges cannot enter the stage pipeline, expensive
projections are built once, time and split changes refresh on the next frame,
and authored colors plus stage order remain unchanged.

## Recommended Next Refactor Sequence

1. Preserve the workload and layer-parity fixtures for future RGB changes.
2. Keep Finding 16 lookup optimization independent of this frame lifecycle.
3. Reopen RGB architecture only if the pinned QMK frame-limit contract changes.

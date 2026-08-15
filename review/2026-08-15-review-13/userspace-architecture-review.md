# RGB Preview-Parity Architecture Review

Review 12 remains open only for Finding 10's flashed timing and feel check.
RGB preview parity is a materially different rendering-contract topic, so this
work uses the next sortable review folder instead of changing Review 12.

## Findings

### Should-fix — Preview has a second, incomplete layer-group renderer

`users/noah/lib/rgb/stages/rgb_preview_stage.c` independently paints the
preview layer and its groups instead of consuming the normal selected-layer
contract from `rgb_layer_stage.c`.

The preview implementation currently:

- rejects a layer without a solid base before considering explicit groups;
- accepts only rows whose layer exactly matches the preview layer, omitting
  `RGB_LAYER_GROUP_ALL`;
- converts `HSV(0, 0, 0)` directly to black instead of treating it as the
  inherit sentinel.

Normal layer rendering already has the intended group applicability,
inheritance, authored ordering, chunk bounds, and painted-mask behavior. The
two paths can therefore disagree as soon as authored layer groups are enabled.

### Optional cleanup — Preview-only direct LED helpers duplicate frame logic

`rgb_runtime_layer_stage_paint_layer()` and the preview-specific group
intersection helper bypass the frame abstraction used by normal layers. Once
preview renders through the selected-layer frame contract, these narrow
duplicates should be removed if no caller remains.

## Prior Finding Status

| Prior finding | Status | Preservation requirement |
| --- | --- | --- |
| Finding 05: VIA split persistence | partially resolved | Software and target gates pass; the physical two-half matrix remains independent and pending. |
| Finding 10: pointing backlog bounds | partially resolved | Software and target gates pass; Review 12 remains open only for flashed timing and feel. |
| Finding 13: RGB preview parity | resolved | `rgb_layer_stage.c` owns the shared selection renderer; `rgb_preview_stage.c` consumes it through the runtime-owned frame. Five parity scenarios, RGB/split/feature gates, the full host suite, target compile, and fresh stack gate pass. |

## Reconciliation Note

The findings above are the audit-time baseline. The current tree removes the
second preview renderer and its direct layer/group helpers. Normal and preview
now share one two-phase frame contract, and the dedicated host variant
mechanically enforces all audited parity cases.

## Areas That Are Solid

- `rgb_runtime_layer_stage_render_frame()` already preserves base-layer order,
  authored group-row order, later-row overrides, and per-chunk painted masks.
- `rgb_runtime_layer_stage_group_color()` already implements the documented
  inherit sentinel and correctly skips inheritance without a solid base.
- The runtime stage order is explicit: combo underlay, preview, pointing mode,
  combo overlay, then key feedback.
- The existing RGB host harness covers master/secondary preview sources,
  chunk-aware LED writes, and overlay ordering.

## Intended Structure

Keep `rgb_layer_stage.c` authoritative for rendering a set of layers. Both
normal rendering and preview rendering should use one internal two-phase
algorithm:

1. paint eligible solid bases in layer order;
2. scan authored group rows in order and resolve each eligible group through
   the same inheritance helper.

Normal rendering selects every effectively active layer. Preview rendering
selects exactly one valid preview layer, regardless of active state. Preview
uses the runtime's existing reusable frame buffer, clears only the requested
chunk, and applies only entries marked painted. This avoids both stack growth
and a second RGB-sized BSS buffer.

## Verification Strategy

- Add a layer-group test variant with a solid selected layer and a base-less
  selected layer.
- Preserve red evidence for universal-group omission, inherit-as-black,
  explicit-group suppression, and later-row ordering.
- Compare normal selected-layer output with preview output over full and split
  chunks, including painted return values.
- Preserve master/secondary preview sourcing and stage-order tests.
- Run targeted RGB, split, and feature-gate checks, then the full host suite,
  target compile, and target resource/stack checks.

## Closure Evidence

- Code authority: `users/noah/lib/rgb/stages/rgb_layer_stage.c` and its narrow
  `rgb_runtime_layer_stage_render_selected_frame()` seam.
- Preview integration: `users/noah/lib/rgb/stages/rgb_preview_stage.c` and the
  runtime-owned frame passed from `users/noah/lib/rgb/core/rgb_runtime.c`.
- Mechanical enforcement: the synthetic layer-group variant in
  `tests/host/rgb_layer_render_test.c` plus its scan counter and runner cases.
- Passed commands:
  - `sh tests/host/run_rgb_layer_render_tests.sh`
  - `sh tests/host/run_rgb_validation_tests.sh`
  - `sh tests/host/run_split_runtime_sync_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `PYTHONPYCACHEPREFIX=/tmp/noah-stack-pycache PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh`
  - `git diff --check`

## Current Architecture Assessment

Finding 13 is **resolved**. Normal activation selects all effectively active
layers and preview selects one layer, but both execute the same base phase,
group phase, inheritance resolver, authored row order, painted mask, and chunk
contract. Preview reuses the existing runtime frame, so the resolution does not
trade correctness for stack or frame-sized BSS growth.

## Recommended Next Refactor Sequence

1. Keep the Finding 13 parity variant as Finding 15's byte-for-byte color
   oracle.
2. Instrument RGB projection work before adding any cross-chunk cache.
3. Land physical-half early exits and within-callback reuse first.

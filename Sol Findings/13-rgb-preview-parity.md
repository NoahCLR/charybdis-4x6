# Finding 13: Make RGB Preview Match Normal Layer Rendering

## Plan metadata

- Severity: medium
- Status: verified
- Recommended phase: Phase 5 RGB correctness, before [Finding 15](15-rgb-render-work.md)
- Affected surfaces:
  - users/noah/lib/rgb/stages/rgb_preview_stage.c
  - users/noah/lib/rgb/stages/rgb_layer_stage.c
  - users/noah/lib/rgb/stages/rgb_layer_stage.h
  - users/noah/lib/rgb/core/rgb_helpers.h
  - tests/host/rgb_layer_render_test.c
  - tests/host/rgb_base_underlay_test.c
  - tests/host/run_rgb_layer_render_tests.sh
- Prerequisites:
  - Preserve stage ordering in users/noah/lib/rgb/core/rgb_runtime.c.
  - Define parity as rendering the preview layer with the same base-color and group rules used when that layer is normally active.
  - Land correctness before [Finding 15](15-rgb-render-work.md) caches or reuses frame snapshots.

## Problem statement

The preview stage independently reimplements only part of the layer renderer. It does not honor universal layer groups, treats the inherit-color sentinel as literal black, and refuses to render any preview when the layer has no solid base color even if that layer has explicit nonblack groups.

The current authored profile has no active layer-group rows, so the defect is latent. The public data model and comments nevertheless promise these group semantics, and a future profile can silently render different colors in preview and normal activation.

## Current evidence and failure scenarios

- users/noah/lib/rgb/stages/rgb_preview_stage.c:30-33 returns early unless the selected preview layer has a solid base color.
- users/noah/lib/rgb/stages/rgb_preview_stage.c:38-41 accepts only groups whose layer exactly equals the preview layer, skipping RGB_LAYER_GROUP_ALL.
- users/noah/lib/rgb/stages/rgb_preview_stage.c:43-46 always calls hsv_to_rgb, so HSV(0, 0, 0), the inherit sentinel, becomes black.
- users/noah/lib/rgb/stages/rgb_layer_stage.c:120-136 contains the correct inherit-color resolution.
- users/noah/lib/rgb/stages/rgb_layer_stage.c:173-204 applies solid layers, explicit layer groups, and RGB_LAYER_GROUP_ALL through the normal frame path.
- users/noah/lib/rgb/core/rgb_helpers.h:84 and 87-89 define the universal-layer sentinel and inherit-color sentinel.
- keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:109-118 documents RGB_LAYER_GROUP_ALL and contains the currently empty group table.

Failure scenarios:

1. Add an RGB_LAYER_GROUP_ALL row and preview any layer. Normal activation paints the group; preview omits it.
2. Add a layer-specific group with HSV(0, 0, 0). Normal activation inherits the layer color; preview paints black.
3. Configure no solid color for a layer but add an explicit colored group. Normal activation can paint the group; preview returns before considering it.

## Required invariants

1. Given the same layer and LED chunk, normal-layer and preview-layer resolution produce identical base and group colors.
2. RGB_LAYER_GROUP_ALL applies during preview exactly as it applies during normal rendering.
3. HSV(0, 0, 0) means inherit; it is never interpreted as explicit black by one path and inherit by another.
4. An absent solid base does not suppress explicit non-inherit groups.
5. An inherit group with no available base color paints nothing, matching the normal stage.
6. Group ordering and later-row override behavior are identical in both paths.
7. LED chunk bounds are respected and the return value reports whether at least one LED in the chunk was painted.
8. Preview remains an overlay at its current point in the stage order.

## Scope

- Extract or expose one shared selected-layer rendering primitive.
- Route preview base and group resolution through that primitive.
- Add parity tests for normal and preview paths.
- Preserve current authored RGB data and stage ordering.

## Non-goals

- Do not redesign all RGB stages.
- Do not change the meaning of RGB_LAYER_GROUP_ALL or the inherit sentinel.
- Do not add active authored groups merely to exercise the code.
- Do not combine this correctness fix with per-frame caching or driver changes.
- Do not make black an explicit group color without first defining a separate representation; HSV(0, 0, 0) is already reserved for inherit.

## Implementation plan

### Phase 1: Specify parity with failing tests

1. Add a compact layer-group fixture containing:
   - one layer-specific inherit group
   - one layer-specific explicit-color group
   - one RGB_LAYER_GROUP_ALL explicit-color group
   - an ordering case where a later group overrides an earlier color on the same LED
2. Add a preview layer with a solid base and one without a solid base.
3. Render the same selected layer through the normal frame path and preview path over full and partial chunks.
4. Compare per-LED results and painted return values.
5. Ensure each of the three audited failures is independently visible before implementation.

### Phase 2: Extract a shared selected-layer resolver

1. Keep the authoritative group-color resolver in rgb_layer_stage.c.
2. Introduce a primitive that paints one selected layer into a supplied frame or overlay target:
   - paint the base only when the layer has a solid color
   - scan the group table in authored order
   - accept rows for the selected layer or RGB_LAYER_GROUP_ALL
   - resolve inherit through the selected layer's base color
   - skip inherit groups when no base color exists
   - permit explicit groups even without a base
3. Prefer a frame-based primitive so color resolution and painted bookkeeping remain testable before applying to QMK LEDs.
4. If the normal renderer still needs effective-layer-state filtering, keep that filtering outside the selected-layer primitive.
5. Export the narrowest possible API from rgb_layer_stage.h; do not expose internal tables or mutable caches.

### Phase 3: Route both renderers through the shared contract

1. Update rgb_runtime_layer_stage_render_frame to call the selected-layer primitive for each effectively active layer.
2. Update rgb_runtime_preview_stage_render to:
   - validate the preview layer
   - invoke the same selected-layer primitive for only that layer
   - apply the resulting overlay for the requested LED chunk
3. Remove preview-specific group intersection and direct hsv_to_rgb logic once no callers need it.
4. Confirm the preview stage remains between combo underlay and pointing/combo/key-feedback overlays in rgb_runtime.c:121-137.
5. Avoid clearing colors painted by earlier stages outside the requested overlay semantics. If a shared frame is cleared internally, constrain the apply operation to frame entries marked painted.

### Phase 4: Enforce configuration and feature variants

1. Cover zero groups, universal-only groups, and group arrays compiled out or empty.
2. Test both KEYS_MAPPED_ON_THIS_LAYER_ONLY and full-layer base modes.
3. Compile with RGB preview feedback disabled and with layer groups empty.
4. Confirm no new keymap-owned dependency leaks into userspace runtime modules.

## Test and verification plan

Extend tests/host/rgb_layer_render_test.c and, if the base frame helper is the better unit seam, tests/host/rgb_base_underlay_test.c with:

- RGB_LAYER_GROUP_ALL parity.
- Inherited group-color parity.
- Explicit group on a layer with no solid base.
- Inherit group on a layer with no solid base paints nothing.
- Multiple group ordering and override parity.
- Full range and chunked range parity.
- No-preview and invalid-preview layer behavior.
- Master-local and secondary-remote preview-layer sources.

Run targeted checks repeatedly:

1. sh tests/host/run_rgb_layer_render_tests.sh
2. sh tests/host/run_rgb_validation_tests.sh
3. sh tests/host/run_split_runtime_sync_tests.sh
4. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah

Because the implementation changes a runtime header seam, the feature-gate compile is required. Do not mark the finding resolved without a passing target compile.

## Observability and measurements

- Use deterministic per-LED frame comparisons rather than screenshots as the primary oracle.
- Count selected-layer group scans in the host fixture to ensure the shared helper does not accidentally double-render groups.
- Record firmware size before and after; extraction should not create two retained implementations through inlining.
- If Finding 15 later adds render counters, retain the parity fixture as the color-correctness baseline.

## Implementation result — 2026-08-15

- `rgb_layer_stage.c` now owns one selection-aware two-phase frame renderer:
  solid bases render in layer order, then eligible group rows render in authored
  order through the existing inheritance resolver.
- Normal rendering selects every effectively active layer. Preview rendering
  selects exactly one valid preview layer and otherwise consumes the same
  algorithm.
- Preview reuses `rgb_runtime.c`'s existing primary frame after the base stage
  has applied it. It clears only the requested chunk and applies only painted
  entries, adding neither a frame-sized stack object nor a second RGB BSS
  buffer.
- The preview-only direct layer painter, group intersection test, and direct
  `hsv_to_rgb()` group path were removed.
- A host-only scan counter proves normal rendering scans the group table once
  and preview adds exactly one selected-layer scan.
- A dedicated synthetic-group variant preserves independent red/green evidence
  for inheritance, universal rows, a base-less explicit group, later-row
  overrides, and full-versus-two-chunk parity. Authored profile data remains
  unchanged.

Target evidence:

- ordinary linked image: 150,896 B text, 0 B data, 245,592 B BSS;
- compared with Finding 10: -12 B text and +8 B BSS from linked layout, with no
  new production frame or diagnostic allocation;
- reviewed stack maxima remain 1,904/1,920 B main and 336/768 B split.

## Risks, tradeoffs, and fallbacks

- A helper that clears a full frame can erase prior-stage output when used as an overlay. Painted-mask semantics must be explicit.
- Moving normal rendering to a shared primitive can change group ordering if the existing layer loop and group loop are collapsed carelessly.
- A public helper that exposes too much internal state would enlarge the RGB API surface.
- If unifying the whole frame path is too invasive, first extract only group applicability and color resolution, then make both paths call those pure helpers. The tests must still compare final LED output.

## Documentation and review-note updates

- Update the relevant RGB documentation to state that preview uses normal selected-layer base/group semantics.
- Keep the authored table comments in rgb_config.c aligned with the tested behavior.
- Update the active open runtime architecture review progress.md during implementation, or open the next sortable review folder if the relevant review is closed.
- Record the three parity cases and the exact RGB test commands in the review note.

## Acceptance checklist

- [x] Preview applies RGB_LAYER_GROUP_ALL.
- [x] Preview resolves inherit colors through the selected layer base.
- [x] Explicit groups render when the selected layer has no solid base.
- [x] Inherit groups without a base remain unpainted.
- [x] Normal and preview output match per LED for full and chunked ranges.
- [x] Stage ordering and later-group override behavior remain stable.
- [x] Targeted RGB, split, and feature-gate checks pass.
- [x] The full host suite passes.
- [x] The target QMK compile passes.
- [x] RGB docs and the active review note describe the landed contract.

## Next action

Finding 13 is closed. Retain its parity fixture as the color oracle while
Finding 15 instruments and streamlines RGB render work.

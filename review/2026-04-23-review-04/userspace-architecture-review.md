# RGB Runtime Architecture Review

## Scope

This review records an initial architecture pass over the RGB runtime and
authored RGB profile as of 2026-04-23.

The pass used `prompts/initial-architecture-review.md`. It did not continue
`review/2026-04-23-review-03/` because that folder is closed history for a
whole-userspace file-map review. This is a separate post-closure RGB runtime
and profile review.

The initial pass was review-only. Follow-up implementation in this same active
thread added fixed left/right key-feedback placement modes and updated the RGB
authoring docs.

## Findings

### Must-Fix

None.

### Should-Fix

None.

### Optional Cleanup

- The runtime and upstream keyboard support normal QMK RGB Matrix effects, but
  the authored keymap does not place RGB Matrix control keycodes anywhere in
  the visible layer data at
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c:414`. By contrast,
  the upstream default 4x6 keymap exposes `RM_NEXT`, `RM_TOGG`, and `RM_PREV`
  at `../bastardkb-qmk/keyboards/bastardkb/charybdis/4x6/keymaps/default/keymap.c:49`.
  If "expected RGB modes" includes user-facing effect/toggle controls, the
  firmware has the underlying QMK modes but this profile lacks an authored
  physical path. This can remain a conscious VIA-only choice, but it should be
  documented or a small control cluster should be added.
- Key-feedback paint priority is currently encoded by enum ordinal ordering:
  `KEY_FEEDBACK_SEMANTIC_MULTI_TAP_PENDING` is last in
  `users/noah/lib/key/runtime/feedback.h:30`, and the RGB renderer selects the
  greatest semantic at `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:140`
  and `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c:159`. Current
  behavior is tested and coherent, but future feedback states would be safer
  with an explicit priority helper or static assertions that document the
  intended ordering.

## Resolved During Follow-Up

- PD-mode RGB authoring comments now match the profile: `pd_mode_colors[]` uses
  `PD_COLOR_MODE_RIGHT_HALF` for every pointing mode, and the user-facing
  `rgb_config.c` comment describes that pointer-half policy without mentioning
  backend debug defines. Verification: `python3 tools/profile_introspect.py
  --check`, `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`, `qmk compile -kb
  bastardkb/charybdis/4x6 -km noah`, and `git diff --check`.
- Key-behavior feedback now has fixed-half placement options in addition to
  both halves, key half, and exact key. Code references:
  `users/noah/lib/rgb/core/rgb_helpers.h`,
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`,
  `users/noah/lib/rgb/core/rgb_validation.c`, and
  `tests/host/rgb_layer_render_test.c`.

## Non-Findings

- The RGB runtime shape is coherent. `users/noah/lib/rgb/core/rgb_runtime.c:122`
  keeps top-level rendering as a stage pipeline: base/layer state first, then
  combo underlay, preview, pointing-mode overlay, combo overlay, and
  key-feedback overlay. That ordering matches the docs and the host render
  tests.
- The runtime is coherent with the tap engine. RGB does not re-interpret
  `key_behaviors[]` directly; it consumes semantic outputs from
  `users/noah/lib/key/runtime/feedback.c:147` and
  `users/noah/lib/key/runtime/feedback.c:240`. The tap engine owns preview
  layer selection, pending multi-tap truth, hold/long-hold semantics, pulses,
  and combo locality. RGB only broadens or narrows that truth at paint time.
- Split behavior is correctly layered. The master computes preview, combo, and
  key-feedback state, while `split_runtime_sync` transports those surfaces to
  the slave. The slave renderer uses the mirrored semantic map instead of
  trying to duplicate key-runtime decisions.
- The authored RGB mode surface is broad enough for this firmware: layer all
  keys vs mapped-only, layer LED groups, auto-mouse fade destination modes,
  pointing-mode left/right/both/trigger-half overlays, pointing-mode LED
  groups, combo both/half/keys/fixed-side placement, key-feedback
  both/fixed-half/key-half/key placement, and diagnostic override color are all
  present.
- All six registered pointing modes have profile colors:
  `DRAGSCROLL`, `VOLUME_MODE`, `BRIGHTNESS_MODE`, `ARROW_MODE`, `PINCH_MODE`,
  and `ZOOM_MODE`.

## Prior Finding Status

No prior RGB-runtime findings were open. The newest review folder before this
pass, `review/2026-04-23-review-03/`, was already closed and remains immutable
history.

## Current Architecture Assessment

The RGB runtime is in good shape architecturally. It is staged, testable, and
mostly data-driven. The tap-engine relationship is especially strong: RGB
renders semantic state exported by the key runtime instead of duplicating tap,
hold, longer-hold, multi-tap, combo-origin, or PD ownership logic.

The current `.mode` authoring surface is coherent for the main feedback
surfaces. PD and combo feedback support fixed left/right placement, and
key-behavior feedback now does as well.

## Recommended Next Refactor Sequence

1. If ordinary RGB Matrix controls are expected on-board, add a small authored
   path for `RM_TOGG`, `RM_NEXT`, and `RM_PREV` or document that VIA remapping
   is the intended control surface.
2. If new key-feedback semantic states are added later, first make feedback
   priority explicit instead of relying on enum ordering.
3. Keep the current staged RGB runtime. No rewrite is warranted from this pass.

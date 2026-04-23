# RGB Runtime Architecture Review

## Scope

This review records an initial architecture pass over the RGB runtime and
authored RGB profile as of 2026-04-23.

The pass used `prompts/initial-architecture-review.md`. It did not continue
`review/2026-04-23-review-03/` because that folder is closed history for a
whole-userspace file-map review. This is a separate post-closure RGB runtime
and profile review.

No runtime, keymap, generated introspection, or user-facing documentation files
were changed in this pass.

## Findings

### Must-Fix

None.

### Should-Fix

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:146` says the
  profile uses `PD_COLOR_MODE_TRIGGER_HALF` for every pointing mode, but every
  actual `pd_mode_colors[]` row uses `PD_COLOR_MODE_RIGHT_HALF` at
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:155`,
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:160`,
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:165`,
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:170`,
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:175`, and
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c:180`.
  `users/noah/config.h:50` also enables `RGB_PD_MODE_ACTIVE_HALF_ENABLE`,
  while its local comment says that feature is required only when a profile row
  uses trigger-half placement. This is not a render bug, but it is an
  authoring-intent mismatch: the profile either wants pointer-mode feedback to
  follow the triggering half, or it wants every mode pinned to the pointer half.
  Pick one and make the comment, config flag, generated overview, and profile
  data agree.

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
  both/half/key placement, and diagnostic override color are all present.
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

The main issue is not missing runtime capability. It is profile intent. The
profile currently reads like it wants dynamic trigger-half PD feedback, but it
actually renders every PD mode on the right half. That should be resolved
before treating the RGB profile as polished.

## Recommended Next Refactor Sequence

1. Decide the PD overlay locality policy:
   - choose `PD_COLOR_MODE_TRIGGER_HALF` for each PD color row if feedback
     should follow the hand/combo that activated the mode, or
   - keep `PD_COLOR_MODE_RIGHT_HALF`, update the stale `rgb_config.c` comment,
     and disable `RGB_PD_MODE_ACTIVE_HALF_ENABLE` if no profile row needs it.
2. If ordinary RGB Matrix controls are expected on-board, add a small authored
   path for `RM_TOGG`, `RM_NEXT`, and `RM_PREV` or document that VIA remapping
   is the intended control surface.
3. If new key-feedback semantic states are added later, first make feedback
   priority explicit instead of relying on enum ordering.
4. Keep the current staged RGB runtime. No rewrite is warranted from this pass.

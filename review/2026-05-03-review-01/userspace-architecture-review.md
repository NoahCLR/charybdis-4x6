# RGB LED Group Inheritance And All-Target Contract

## Scope

This review tracks the RGB LED group authoring contract for layer, pointing-mode,
combo-feedback, and key-behavior feedback groups.

The prior newest review folder, `review/2026-04-29-review-01/`, covers
PD-runtime semantics and closure criteria for that thread. This review is
separate because the work here changes RGB group render and authoring semantics.

## Intended Contract

- `HSV(0, 0, 0)` in an LED group row means "inherit the active color for this
  RGB stage." It must not paint black.
- A nonzero LED group `HSV(...)` remains an explicit override for that group.
- Layer LED groups can use `RGB_LAYER_GROUP_ALL` to apply one row to every
  active layer. When paired with `HSV(0, 0, 0)`, each layer instance uses that
  layer's resolved color.
- PD-mode LED groups can use `RGB_PD_MODE_GROUP_ALL` to apply one row to every
  active pointing mode. When paired with `HSV(0, 0, 0)`, the row uses the active
  pointing-mode color.
- `KEY_FEEDBACK_GROUP_ALL` follows the same color rule: zero HSV inherits the
  active feedback semantic color, and nonzero HSV overrides all matching
  feedback semantics.
- Combo-feedback LED groups use the same zero-HSV inheritance rule against the
  active combo-feedback color.

## Touched Contracts

- Runtime helper API: `users/noah/lib/rgb/core/rgb_helpers.h`
- RGB render stages:
  - `users/noah/lib/rgb/stages/rgb_layer_stage.c`
  - `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`
  - `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`
  - `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`
- RGB config validation: `users/noah/lib/rgb/core/rgb_validation.c`
- Authored profile surface:
  `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`
- Editor and generated documentation:
  - `tools/charybdis-profile-studio/extension.js`
  - `tools/profile_introspect.py`
  - `docs/RGB_CONFIG.md`
  - `docs/tooling/PROFILE_STUDIO.md`
  - `docs/KEYMAP-OVERVIEW.md`

## Enforcement

- RGB render tests cover inherited group colors for layer, PD, combo, and
  key-behavior feedback groups.
- RGB validation tests accept the all-target layer and PD selectors while still
  rejecting invalid owners.
- Profile introspection renders inherited groups as inherited stage colors
  instead of black swatches.


# RGB Configuration

Most authored RGB configuration in this repo lives in
[`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).

Use this doc when you want to change colors, LED groups, auto-mouse fade
visuals, combo feedback, or key-behavior feedback colors. If you only want to
see what the current profile looks like, start with
[KEYMAP-OVERVIEW.md](./KEYMAP-OVERVIEW.md).

For the cross-system runtime ownership and RGB render flow, see
[architecture/README.md](./architecture/README.md) and
[architecture/runtime-flow.md](./architecture/runtime-flow.md).

The interaction feedback stages are individually gated from the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- `RGB_PD_MODE_FEEDBACK_ENABLE` controls the pointing-device mode overlay and
  its LED groups
- `RGB_COMBO_FEEDBACK_ENABLE` controls combo feedback and its LED groups
- `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` controls key-behavior feedback and its
  LED groups; the preview-layer overlay remains internal to this path
- `RGB_AUTOMOUSE_GRADIENT_ENABLE` controls the auto-mouse timeout fade

When one of those flags is off, the runtime skips that render stage instead of
calling an empty stage.

`rgb_config.c` is the main authored RGB surface:

- layer colors
- per-layer LED highlights
- the auto-mouse timeout gradient
- pointing-device mode colors
- per-mode LED highlights
- combo feedback color
- key-behavior feedback colors

If you want to change how the current profile looks, start there.

If you want to change how RGB is rendered, look at:

- [`users/noah/lib/rgb/core/rgb_runtime.c`](../users/noah/lib/rgb/core/rgb_runtime.c)
- [`users/noah/lib/rgb/stages/rgb_layer_stage.c`](../users/noah/lib/rgb/stages/rgb_layer_stage.c)
- [`users/noah/lib/rgb/automouse/rgb_automouse_stage.c`](../users/noah/lib/rgb/automouse/rgb_automouse_stage.c)
- [`users/noah/lib/rgb/stages/rgb_preview_stage.c`](../users/noah/lib/rgb/stages/rgb_preview_stage.c)
- [`users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`](../users/noah/lib/rgb/stages/rgb_pd_mode_stage.c)
- [`users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`](../users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c)
- [`users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`](../users/noah/lib/rgb/stages/rgb_key_feedback_stage.c)
- [`users/noah/lib/rgb/automouse/rgb_automouse.c`](../users/noah/lib/rgb/automouse/rgb_automouse.c)
- [`users/noah/lib/rgb/core/rgb_helpers.h`](../users/noah/lib/rgb/core/rgb_helpers.h)

`users/noah/lib/rgb/` is organized by edit surface:

- `core/` for runtime orchestration, shared authored-config helpers, and validation
- `automouse/` for the auto-mouse state and its gradient renderer
- `stages/` for the remaining ordered render stages

If you want to change the small helper surface used by `rgb_config.c`,
look at:

- [`users/noah/lib/rgb/core/rgb_config_helpers.h`](../users/noah/lib/rgb/core/rgb_config_helpers.h)

If you want to change what authored RGB data is considered valid, also look at:

- [`users/noah/lib/rgb/core/rgb_validation.c`](../users/noah/lib/rgb/core/rgb_validation.c)

If you want to change what the key-behavior overlay means instead of how it is
painted, also look at:

- [`users/noah/lib/key/runtime/feedback.c`](../users/noah/lib/key/runtime/feedback.c)
- [`users/noah/lib/key/runtime/scan.c`](../users/noah/lib/key/runtime/scan.c)

## HSV Quick Reference

The color values in [`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) are authored with the `HSV(h, s, v)` helper. Use this
quick reference when picking hue values:

![HSV quick reference](./media/hsv_colors.jpg)

## What `rgb_config.c` Controls

The file reads best in render order:

1. base layer render surfaces
2. the auto-mouse base-stage transition
3. later overlay surfaces, in order:
   enabled combo underlay, preview, pd-mode colors, combo overlay, then
   key-behavior feedback

### `layer_colors[]`

`layer_colors[]` is indexed by the layer enum values from the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

Each row is a `layer_color_config_t` with a color and per-layer render mode:

```c
[LAYER_NUM] = {
    .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .mode = KEYS_MAPPED_ON_THIS_LAYER_ONLY,
},
```

The available modes are:

- `ALL_KEYS`: paint the whole layer as a solid color wash
- `KEYS_MAPPED_ON_THIS_LAYER_ONLY`: paint only LEDs whose key position has a
  non-`KC_TRNS`, non-`KC_NO` keycode on that layer

Mapped-only layers compose cleanly with overlap: lower active colored layers
stay visible wherever the higher layer is transparent. `LAYER_BASE` is treated
as the persistent underlay for the layer scene, so a non-black base color
paints both when no higher layer is active and underneath higher mapped-only
layers. The runtime resolves that against the effective keymap, so VIA dynamic
keymap edits are reflected after the runtime refreshes its cached LED
coverage.

`HSV(0, 0, 0)` means "do not paint a solid layer color here." That is
useful for:

- `LAYER_BASE`, when you want the base scene to fall through to the normal RGB Matrix effect
- any layer you intentionally want to stay colorless in the layer stack

### LED Groups

The LED map lives near the top of
[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
Reusable physical groups are defined directly below that map with
`#define RGB_LED_GROUP_* RGB_LED_GROUP(...)`. Stage-specific LED group tables
use `RGB_LED_GROUP_TABLE(...)` and then set `.led_group = RGB_LED_GROUP_*` in
each row, so you do not need separate physical LED arrays or visible
placeholder rows.

You can still use `.led_group = RGB_LED_GROUP(...)` directly for a one-off
cluster, but named groups are clearer when the same LEDs may be used by more
than one stage.

The optional stage-specific LED group tables decide when a cluster lights and
which color it uses:

- `Layer LED Groups`
- `Pointing-Device Mode LED Groups`
- `Combo Feedback LED Groups`
- `Key-Behavior Feedback LED Groups`

### `layer_led_groups`

`layer_led_groups` lets a layer highlight specific LEDs instead of, or in
addition to, a full-board color.

Rows keyed to `LAYER_BASE` follow the same persistent-underlay rule as the
base layer color: they are treated as base-scene accents unless a later layer
or later overlay repaints those LEDs.

In `rgb_config.c`, use one of these helper forms:

- leave all row entries commented out when no per-layer LED groups are enabled
- uncomment or add rows inside `layer_led_groups_data`
- keep the table wrapped in `RGB_LED_GROUP_TABLE(...)`; `MATERIALIZE_RGB_CONFIG()`
  at the bottom exports it

Each row contains:

- the layer id
- an HSV color
- a `.led_group` value naming the LEDs to repaint

Use rows like:

```c
{ .layer = LAYER_NAV, .color = HSV(0, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_RIGHT_THUMB },
```

This is useful for things like:

- highlighting thumb keys
- marking navigation modifiers
- accenting a small part of a layer without repainting the full board

The LED map comment in [`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) is the reference for the standard matrix
indices on this board.

### `automouse_fade_end_config`

`automouse_fade_end_config` defines only the auto-mouse fade destination.

In the shared RGB runtime:

- the configured auto-mouse layer now uses the full authored layer-rendered
  start state, including any active layer LED groups, as the timeout fade
  start state
- the default destination is the real layer-rendered state that remains after
  the auto-mouse layer drops out
- `FOLLOW_REAL_DESTINATION` lands on that real rendered destination
- `END_COLOR_WHERE_BASE_EFFECT_WOULD_SHOW` keeps the real destination where
  layers paint, but uses `end_color` where the base RGB effect would otherwise
  show through
- `END_COLOR_ON_ALL_KEYS` uses `end_color` as the destination on every key
  while the automouse renderer is active

The timeout fade does not animate during the entire timeout. The first
`AUTOMOUSE_RGB_DEAD_TIME` milliseconds are dead time, and only the remaining
span animates. `AUTOMOUSE_RGB_DEAD_TIME` is configurable in the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h), so a
profile can trade off smoother animation against less flicker while the
trackball is still actively being used.

The configured destination is not a persistent board state. Once the automouse
renderer stops, the next frame falls back to ordinary layer rendering and then
later overlays such as pd-mode color or key feedback still paint on top.

### `pd_mode_colors[]`

`pd_mode_colors[]` defines the overlay color and locality for each active
pointing-device mode.

In `rgb_config.c`, declare `pd_mode_colors[]`; `MATERIALIZE_RGB_CONFIG()` at
the bottom derives `pd_mode_color_count`.

Each row is keyed by a `PD_MODE_*` flag rather than by array index. That means
the color mapping follows the pointing mode itself, not the order of
`pd_modes[]`.

Each row also chooses where the overlay paints through `.locality`:

- `RGB_BOTH_HALVES`: mirror the overlay across both halves
- `RGB_LEFT_HALF`: always paint the left half
- `RGB_RIGHT_HALF`: always paint the right half
- `RGB_KEY_HALF`: paint the half or halves containing the key footprint that
  triggered the currently effective PD mode.
  If the triggering combo footprint spans both halves, the overlay paints both
  halves instead of guessing one side.
- `RGB_KEYS_ONLY`: paint only the key footprint that triggered the currently
  effective PD mode. If a combo triggered the mode, every combo key in the
  footprint is painted.

Key-local placement relies on the backend PD ownership tracking in
[`users/noah/config.h`](../users/noah/config.h).

Use rows like:

```c
{ .pointing_mode = PD_MODE_ARROW, .color = HSV(127, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .locality = RGB_KEY_HALF },
{ .pointing_mode = PD_MODE_ZOOM, .color = HSV(70, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .locality = RGB_KEYS_ONLY },
```

Use this table when you want `ARROW_MODE`, `VOLUME_MODE`, `PINCH_MODE`, and the
other pd modes to have distinct overlay colors and placement.

### `pd_mode_led_groups`

`pd_mode_led_groups` is the same idea as `layer_led_groups`, but keyed by
pointing-device mode instead of layer.

Use this when one mode should highlight a very specific LED or cluster, such as
the trackball LED or one side of the board.

As above, leave row entries commented out when no per-mode LED groups are
enabled and keep the table wrapped in `RGB_LED_GROUP_TABLE(...)`.

Use rows like:

```c
{ .pointing_mode = PD_MODE_VOLUME, .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
```

### `combo_feedback_colors`

In `rgb_config.c`, declare `combo_feedback_colors` directly:

```c
const combo_feedback_color_config_t combo_feedback_colors = {
    .color    = HSV(191, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality = RGB_KEYS_ONLY,
};
```

This is a persistent combo identity layer:

- if a combo chord is active, its combo color stays active
- key-behavior feedback can still repaint above it
- combos that currently own preview or PD state are routed underneath those
  state indicators
- unrelated combos stay above preview and PD

The `locality` field controls where the combo layer paints:

- `RGB_BOTH_HALVES`: repaint both halves while any combo is active
- `RGB_LEFT_HALF`: always repaint the left half
- `RGB_RIGHT_HALF`: always repaint the right half
- `RGB_KEY_HALF`: repaint the half or halves touched by the live combo
  footprint
- `RGB_KEYS_ONLY`: repaint only the exact combo keys

This layer is intentionally steady while held. That keeps combo identity
visible underneath later flashing key-behavior overlays instead of competing
with them.

### `combo_feedback_led_groups`

`combo_feedback_led_groups` adds custom-color LED accents to the live combo
feedback stage. Groups render after `combo_feedback_colors.locality` inside the
current combo substage, so they are compatible with every combo locality:
both halves, fixed halves, key half, or keys only.

The same group table is used by both combo substages:

- preview- or PD-owning combos use the combo underlay path, so their groups
  remain below preview and PD indicators
- unrelated combos use the combo overlay path, so their groups repaint above
  preview and PD indicators
- key-behavior feedback still renders later and can repaint above combo groups

Use rows like:

```c
static const combo_feedback_led_group_t combo_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    { .color = HSV(191, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
);
```

### `key_behavior_feedback_colors`

In `rgb_config.c`, declare `key_behavior_feedback_colors` directly:

```c
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    .tap_pending_color = HSV(0, 0, 150),

    RGB_TAP_BRANCH_COLORS(
        HSV(235, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // tap index 0
        HSV(200, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // tap index 1
        HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // tap index 2
        HSV(143, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // tap index 3
        HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),  // tap index 4
    ),

    .tap_committed_color     = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .tap_commit_mode         = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
    .hold_active_color       = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .long_hold_active_color  = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality                = RGB_KEY_HALF,
};
```

Those rows populate the shared
`key_behavior_feedback_colors` config object:

- `tap_pending_color`
- `RGB_TAP_BRANCH_COLORS(...)`
- `tap_committed_color`
- `tap_commit_mode`
- `hold_active_color`
- `long_hold_active_color`
- `locality`

The `tap_pending_color` field is the neutral unresolved multi-tap color for
double-tap and higher branches while the runtime is still waiting to know
which tap index wins. The base single-tap candidate stays quiet during that
same pending window.

The `RGB_TAP_BRANCH_COLORS(...)` macro declares the confirmation colors used
while a committed double-tap or higher branch is being held in the model-level
branch-confirm window. The color table is authored in zero-based tap-index
order, matching `key_behaviors[].tap_counts[]`. The base single-tap index is
normally quiet, while double-tap and higher committed indexes use their matching
entry and clamp to the last configured branch color if they exceed the table.
Inline C comments next to those `HSV(...)` arguments are allowed and are
ignored by the profile introspector.

The `tap_commit_mode` field controls which committed tap branches pulse with
`tap_committed_color`:

- `KEY_FEEDBACK_TAP_COMMIT_OFF`: disable tap-commit pulses
- `KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS`: pulse only for double-tap and
  higher tap branches; the base single-tap branch stays quiet
- `KEY_FEEDBACK_TAP_COMMIT_ALL_TAPS`: pulse for every committed tap branch

The `locality` field controls where the overlay paints:

- `RGB_BOTH_HALVES`: repaint both halves from the newest active feedback owner
- `RGB_LEFT_HALF`: repaint the left half from the newest active feedback owner
- `RGB_RIGHT_HALF`: repaint the right half from the newest active feedback owner
- `RGB_KEY_HALF`: repaint only the half that owns the feedback-driving key or
  active tap series. Combo-driven feedback can expand this to both halves when
  the combo footprint spans both sides.
- `RGB_KEYS_ONLY`: repaint only the feedback-driving key footprint. For combo
  outputs, that means every key that formed the combo.

Like PD key-local placement, key-behavior feedback locality is driven from
the live runtime footprint, not a static guess from authored combo comments.

In the shared runtime, those colors are used for these categories:

- multi-tap pending: the engine has not resolved the winning double-tap or
  higher branch yet; the base single-tap candidate stays quiet
- tap branch committed: the winning double-tap or higher branch is known and
  the model is in the branch-confirm window before firing that branch's action
- tap committed: an authored tap branch has resolved and emitted output
- hold pending: a hold path exists, but the final action is not resolved yet
- hold trigger: a hold-tier action has just fired
- long-hold trigger: a longer-hold tier action has just fired
- held non-layer action active: a `PRESS_AND_HOLD_UNTIL_RELEASE(...)` action is
  still registered and should stay visibly active

The tier decides the color:

- tap commits allowed by `tap_commit_mode` use `tap_committed_color`, except
  layer and PD-mode state actions stay quiet because their layer/PD overlays
  are the persistent feedback
- `.hold` surfaces use `hold_active_color`
- `.long_hold` surfaces use `long_hold_active_color`

Pending momentary-layer previews are a separate overlay path. They reuse the
previewed layer's authored layer color and any matching layer LED groups
instead of these feedback colors.

The helper decides the RGB behavior shape:

- `TAP_AT_HOLD_THRESHOLD(...)`: pulse once when that tier commits
- `TAP_ON_RELEASE_AFTER_HOLD(...)`: stay steadily lit while that tier is
  pending release; if release happens during branch confirmation, the hold-tier
  action feedback follows after confirmation completes so both states get a
  visible window
- `PRESS_AND_HOLD_UNTIL_RELEASE(...)` and `REPEAT_WHILE_HELD(...)`: flash while
  that tier remains active; each key starts with a visible flash window when
  its held/repeat feedback activates, then alternates on its own cadence

When a higher hold tier actually commits, such as a `.long_hold` threshold
action, that action feedback replaces older lower-tier feedback. RGB should
not continue showing the lower-tier state after the runtime has already
committed the higher-tier behavior.

Held layer-switch actions are intentionally a special case: they get the short
preview color before activation and the real layer color after activation, but
they do not use the feedback overlay colors or trigger pulse. Once the layer
is on, the layer color itself is the main feedback.

The overlay is enabled by `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` in the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h). Its flash cadence is controlled by
`RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS`.

The runtime now keeps truthful per-key semantic state, per-key flash
visibility, and a broad-surface owner map. Owner ordering uses a 32-bit runtime
feedback activation sequence, while flash visibility still uses each owner's
activation timer for its own on/off phase. `RGB_KEYS_ONLY` renders each key
directly from that per-key truth. Broader localities choose the newest active
feedback owner for the painted surface first, then use that owner's real
visibility phase; if that owner is in its off window, the surface stays off
instead of falling back to another offset key. When the newest owner is
released, the next-newest still-active owner takes over without restarting its
flash phase. On split boards, the slave receives the packed semantic map,
tap-branch map, flash visibility bitmap, and broad owner map through
[`split_runtime_sync`](../users/noah/lib/split/runtime_sync.c).

### `key_behavior_feedback_led_groups`

`key_behavior_feedback_led_groups` adds custom-color LED accents to the
key-behavior feedback stage. Groups render after
`key_behavior_feedback_colors.locality`, so they work with every feedback
locality while still remaining within the key-behavior feedback stage.

Each row chooses a semantic category:

- `KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH`: visible while double-tap or
  higher multi-tap resolution is pending
- `KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED`: visible while a committed
  double-tap or higher branch confirmation pulse is active
- `KEY_FEEDBACK_GROUP_TAP_COMMITTED`: visible while the tap-commit pulse is
  active
- `KEY_FEEDBACK_GROUP_HOLD_ACTIVE`: visible for hold pending, hold commit
  pulses, and flashing hold states
- `KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE`: visible for steady and flashing
  long-hold states

Flashing categories follow the same flash visibility as the main
key-behavior feedback color. Later group rows can repaint LEDs painted by
earlier group rows.

Use rows like:

```c
static const key_behavior_feedback_led_group_t key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
    { .semantic = KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH, .color = HSV(0, 0, 150), .led_group = RGB_LED_GROUP_TRACKBALL },
    { .semantic = KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED, .color = HSV(169, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
    { .semantic = KEY_FEEDBACK_GROUP_TAP_COMMITTED, .color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
    { .semantic = KEY_FEEDBACK_GROUP_HOLD_ACTIVE, .color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
    { .semantic = KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE, .color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .led_group = RGB_LED_GROUP_TRACKBALL },
);
```

## Render Order

[`rgb_runtime.c`](../users/noah/lib/rgb/core/rgb_runtime.c) applies RGB in a deliberate order:

1. active non-base layers compose from low to high:
   full-board layer colors wash the whole board, and mapped-only layers paint
   only the LEDs owned by that layer's non-transparent keys
2. if the auto-mouse layer is active, that layer stage is blended toward its
   destination state instead of being painted as a fixed separate gradient
3. if `RGB_COMBO_FEEDBACK_ENABLE` is on, combo underlay for combos that
   currently own preview and/or PD state,
   including any matching combo feedback LED groups
4. per-layer preview overlay for a pending momentary-layer hold, if
   `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` is on and that previewed layer has a
   nonzero solid color
5. if `RGB_PD_MODE_FEEDBACK_ENABLE` is on, the active pointing-device mode
   color using the authored PD locality
6. if `RGB_PD_MODE_FEEDBACK_ENABLE` is on, per-mode LED groups
7. if `RGB_COMBO_FEEDBACK_ENABLE` is on, combo overlay for all other active
   combos, including any matching combo
   feedback LED groups
8. if `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` is on, the key-behavior feedback
   overlay on both halves, only the key half, or only the specific key, then
   any matching key-behavior feedback LED groups

That order matters.

Examples:

- a per-layer LED group can sit on top of a solid layer color
- a preview- or PD-owning combo can stay underneath the preview or PD overlay
- an unrelated active combo can repaint above preview or PD if it uses the
  combo overlay path
- a combo feedback LED group repaints after its combo locality render, but
  still stays within the combo underlay or overlay substage that owns it
- a pd-mode overlay can repaint its authored locality after the base scene and
  any combo underlay
- a pd-mode LED group can then repaint selected LEDs on top of the mode overlay
- the key-behavior overlay can still repaint last, either on both halves, only
  the key half, or only the specific key footprint depending on `locality`
- a key-behavior feedback LED group repaints after that feedback locality
  render when its semantic category is currently visible

## The Helper Types

[`users/noah/lib/rgb/core/rgb_helpers.h`](../users/noah/lib/rgb/core/rgb_helpers.h) defines the small config structs used by
[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c):

- `pd_mode_color_t`
- `layer_color_config_t`
- `automouse_fade_end_config_t`
- `combo_feedback_color_config_t`
- `rgb_led_group_t`
- `combo_feedback_led_group_t`
- `key_behavior_feedback_color_config_t`
- `key_behavior_feedback_group_semantic_t`
- `key_behavior_feedback_led_group_t`
- `layer_led_group_t`
- `pd_mode_led_group_t`

[`users/noah/lib/rgb/core/rgb_config_helpers.h`](../users/noah/lib/rgb/core/rgb_config_helpers.h)
defines `HSV(...)`, `RGB_LED_GROUP(...)`, `RGB_LED_GROUP_TABLE(...)`, and
`MATERIALIZE_RGB_CONFIG()` for authored RGB tables and the small runtime
bridges derived from RGB feedback config.

`rgb_helpers.h` also provides split-safe helper functions such as:

- `rgb_set_led()`
- `rgb_set_led_group()`
- `rgb_set_left_half()`
- `rgb_set_right_half()`
- `rgb_set_both_halves()`

Those helpers are for runtime rendering code. They are not where you usually
edit colors.

The main point of those helpers is that `rgb_matrix_indicators_advanced_user()`
runs in LED chunks. The helpers let the runtime use global LED indices without
having to manually clamp every write to `led_min` and `led_max`.

## Common Changes

### Change a layer color

Edit the relevant row in `layer_colors[]`.

### Change a pd-mode overlay color

Edit the matching row in `pd_mode_colors[]`.

### Change where a pd-mode overlay paints

Edit the matching row in `pd_mode_colors[]` and change its `.locality`.

### Disable the pd-mode overlay

Comment out `RGB_PD_MODE_FEEDBACK_ENABLE` in the active keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).
That disables both `pd_mode_colors[]` and `pd_mode_led_groups`.

### Add a small highlight to one layer

1. Find the target LEDs in the LED map at the top of `rgb_config.c`.
2. Use an existing `RGB_LED_GROUP_*` name, or define a new one under the LED map.
3. Uncomment or add a row in `layer_led_groups_data` with `.led_group = RGB_LED_GROUP_*`.

### Add a small highlight to one pd mode

1. Find the target LEDs in the LED map at the top of `rgb_config.c`.
2. Use an existing `RGB_LED_GROUP_*` name, or define a new one under the LED map.
3. Uncomment or add a row in `pd_mode_led_groups_data` with `.led_group = RGB_LED_GROUP_*`.

### Add a small highlight to combo feedback

1. Find the target LEDs in the LED map at the top of `rgb_config.c`.
2. Uncomment or add a row in `combo_feedback_led_groups_data` with
   `.led_group = RGB_LED_GROUP_*`.

### Add a small highlight to key-behavior feedback

1. Find the target LEDs in the LED map at the top of `rgb_config.c`.
2. Uncomment or add rows in `key_behavior_feedback_led_groups_data` with
   `.led_group = RGB_LED_GROUP_*` for the semantic categories you want to
   accent.

### Change the auto-mouse timeout fade

Edit the `LAYER_POINTER` row in `layer_colors[]` to change the main start
color. If the auto-mouse layer or overlapping layers use LED groups, those are
also part of the visible start render. Edit `automouse_fade_end_config` if you
want the timeout destination to follow the real post-timeout layer state, use
`end_color` where the base effect would show, or use `end_color` on every key.

If you want to change the timing model instead of just the colors, look at:

- `AUTO_MOUSE_TIME` and `AUTOMOUSE_RGB_DEAD_TIME` in the active keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)

`AUTOMOUSE_RGB_DEAD_TIME` must stay below `AUTO_MOUSE_TIME`. The build now
checks that at compile time.

### Change the key-behavior feedback colors

Edit the `HSV(...)` values in `key_behavior_feedback_colors` in
[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c),
including any `RGB_TAP_BRANCH_COLORS(...)` entries that should change.

### Disable combo feedback

Comment out `RGB_COMBO_FEEDBACK_ENABLE` in the active keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).
That disables both `combo_feedback_colors` and `combo_feedback_led_groups`.

### Disable the key-behavior overlay

Comment out `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` in the active keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).
That also disables the preview-layer overlay used for pending momentary-layer
holds.

### Disable the auto-mouse gradient

Comment out `RGB_AUTOMOUSE_GRADIENT_ENABLE` in the active keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

## Verification

RGB authoring changes affect generated profile docs, profile validation, and
the RGB render tests. For `rgb_config.c` changes, use:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
sh tests/host/run_profile_introspection_checks.sh
sh tests/host/run_rgb_validation_tests.sh
sh tests/host/run_rgb_layer_render_tests.sh
sh tests/host/run_real_profile_validation_tests.sh
```

Before handing back firmware-behavior changes, also run the full host suite and
firmware compile from the repo root.

## What This File Does Not Do

[`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) does not decide:

- which layer becomes the auto-mouse layer
- how long auto-mouse stays active
- which key-behavior states count as pending, active, or pulsed feedback
- when a pointing-device mode becomes active or locked
- how split sync transports auto-mouse or key-feedback state

Those behaviors live in the active keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) and
the runtime files under `users/noah/lib/`.

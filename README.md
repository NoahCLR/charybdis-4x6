# Noah's Charybdis Userspace

This is the shared userspace for my `noah` Charybdis 4x6 keymaps.

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this board and this
layout.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the behavior I use every day centralized
and editable, so someone changing
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) or the
keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
can adjust the board without having to rework the runtime.

> **Porting note:** Most board assumptions now live in the keymap
> [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
> and [`users/noah/config.h`](./users/noah/config.h). If you move this
> userspace to a different QMK base or Charybdis variant, start there before
> changing the runtime modules.

This repo is built around the Charybdis from [BastardKB](https://bastardkb.com/),
designed by Quentin. His work, and the community built around this board, have given me
hundreds of hours of fun optimizing both the hardware and the firmware, and I
am very thankful for that. If you are considering buying the hardware, buy it
from him rather than from a knockoff seller.

## What This Userspace Is For

This userspace is built around a small set of systems that make the board easy
to understand and easy to change:

- [`key_behaviors[]`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  in [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  is the main customization table: one authored row can give a key different
  tap, hold, and longer-hold actions at each tap count, plus per-key timing
  overrides. Actions can be plain keycodes, macros, layer locks, pointer-mode
  locks, QMK behavior keycodes like `MO()` or `TG()`, or keymap-local custom
  keycodes
- pointer modes are a core part of what makes this userspace different: the
  trackball can become dragscroll, pinch, zoom, arrows, volume, or brightness,
  and those mode keys use the same authored tap, hold, and lock model as the
  rest of the board
- `AUTO_MOUSE` brings up the pointer layer when the trackball moves and clears
  it again after the configured timeout
- RGB is functional feedback, not decoration.
  [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
  defines layer colors, pointer-mode colors, and LED group highlights. On top
  of that, two optional overlays — an auto-mouse countdown gradient and
  key-behavior engine feedback — can be independently toggled
- the userspace hooks into QMK through weak defaults in
  [`hooks.c`](./users/noah/hooks.c). A keymap can override any QMK hook and
  call the matching `noah_*` helper to keep the shared behavior, or replace it
  entirely (see [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md))
- on split builds, `runtime_shared_state` syncs pointing-device mode flags,
  auto-mouse progress, and key-feedback flags from master to slave so both
  halves render consistently

If you want to understand what makes this userspace special, start with
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c). That
file shows the physical layout, the authored `key_behaviors[]` table, combos,
the VIA macro defaults, the hardcoded macro table, and how the pointer-mode
keys are configured.

There is also a small VIA bridge in
[`via layouts/via_to_qmk_layout.py`](./via%20layouts/via_to_qmk_layout.py).
That script is useful when you want to experiment quickly in VIA without
giving up a readable, source-controlled
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c): it
converts VIA's exported layer data, macro slots, and custom keycodes back into
the authored `keymap.c` tables this repo uses.

## Where To Change Things

If you want to adapt this layout, these are the main files to touch first:

| File | What You Change There |
| --- | --- |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | physical layout, combos, keymap-local custom keycodes, `VIA_MACROS(MACRO)`, `HARDCODED_MACROS(MACRO)`, and the authored `key_behaviors[]` table |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pointer-mode colors, LED groups, auto-mouse gradient endpoints, and key-behavior feedback colors |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | tap/hold timing, multi-tap timing, RGB overlay toggles (`RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`, `RGB_AUTOMOUSE_GRADIENT_ENABLE`), auto-mouse target layer and timeout, auto-sniping, dragscroll feel, and other keymap-facing behavior |
| [`users/noah/noah_keymap.h`](./users/noah/noah_keymap.h) | shared custom keycode ranges (macros, pd-mode keycodes, layer locks) and the `NOAH_KEYMAP_SAFE_RANGE` boundary for keymap-local keycodes |
| [`users/noah/config.h`](./users/noah/config.h) | split transport settings, RGB geometry, pointing-device polling, sensor/report settings, and low-level QMK overrides |

In other words:

- if you want to change what a key does, start in [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- if you want to change how the board looks, start in [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- if you want to change how the keyboard feels, start in the keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- if you want to add a layer, update the layer enum in the keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h); `LAYER_COUNT` is the sentinel last value and should stay last
- if you want to add a shared custom keycode, start in [`noah_keymap.h`](./users/noah/noah_keymap.h)
- if you want to change board plumbing, start in [`users/noah/config.h`](./users/noah/config.h)

## Layers And VIA

This keymap defines five layers, in this order:

- `0 = LAYER_BASE`: default typing layer
- `1 = LAYER_NUM`: numpad layer
- `2 = LAYER_SYM`: symbols, brackets, and DPI controls
- `3 = LAYER_NAV`: navigation, media, macros, mouse buttons, and the auto-sniping layer
- `4 = LAYER_POINTER`: dedicated pointer layout and the default auto-mouse target layer

That layer order matters in a few places:

- when VIA is enabled, `DYNAMIC_KEYMAP_LAYER_COUNT` is derived from
  `LAYER_COUNT` in [`users/noah/config.h`](./users/noah/config.h)
- `AUTO_MOUSE_DEFAULT_LAYER` in the keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) is set to the layer enum value QMK expects (`LAYER_POINTER` here, which resolves to that layer index)
- the authored layer names in [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) and the VIA conversion script need to stay in sync

If you change the layout in VIA and want to bring it back into source, use
[`via_to_qmk_layout.py`](./via%20layouts/via_to_qmk_layout.py) in
[`via layouts`](./via%20layouts). It reads a VIA export, maps VIA's layer
indices and custom keycodes back to this keymap, and can print or rewrite both
`keymaps[][]` and `VIA_MACROS(MACRO)` in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c). The
separate `HARDCODED_MACROS(MACRO)` block stays authored in source.

## Macros

This keymap uses two macro surfaces on purpose:

- `VIA_MACRO_n` is the VIA/QMK dynamic macro keycode range. Defaults for those
  slots are authored in `VIA_MACROS(MACRO)` in
  [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c), and
  the VIA conversion script can sync them from an exported `macros[]` block.
- `MACRO_n` is the repo's hardcoded custom macro range. Those payloads live in
  `HARDCODED_MACROS(MACRO)` in
  [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) and
  do not come from VIA exports.

That split keeps VIA-editable defaults and firmware-owned macros separate while
still letting both kinds of macro keycodes appear in layers, combos, and
`key_behaviors[]`.

## Combos

Combos are authored directly in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c). They
are separate from the custom key-behavior engine: a combo is just a
simultaneous chord that emits one keycode or action.

Combo timing is configured in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
through `COMBO_TERM`. If a combo emits a keycode that also appears in
`key_behaviors[]`, the emitted key can still reuse the same custom behavior
handling after the combo resolves.

So the split is:

- use combos for simultaneous chords
- use `key_behaviors[]` for tap / hold / longer-hold / multi-tap behavior
  attached to an authored keycode

## Key Behavior

The richer custom tap / hold / multi-tap behavior is authored in
`key_behaviors[]`. Plain keys without a row keep their normal QMK behavior.

Keys with authored behavior rows can distinguish between:

- tap
- hold
- longer hold
- multi-tap sequences

This is what makes this layout possible:

- number-row symbols on hold
- thumb keys that combine momentary layer access, layer locks, and media
- nav arrows that cover character, word, and line movement
- pointer keys that can do more than one thing without inventing their own
  timing rules

### Actions

An action in a `key_behaviors[]` row can be:

- a plain keycode (`KC_MPLY`, `S(KC_1)`)
- a hardcoded or VIA macro (`MACRO_0`, `VIA_MACRO_6`)
- a layer lock (`LOCK_LAYER(LAYER_NAV)`)
- a pointer-mode lock (`LOCK_PD_MODE(ARROW_MODE)`)
- a QMK behavior keycode such as `MO()`, `TG()`, `TO()`, `TT()`, `OSL()`,
  `LT()`, or `MT()` — the engine dispatches these through QMK's
  `process_action()` path so they work correctly as hold or tap actions
- a keymap-local custom keycode declared in `keymap.c`

That last category is how the thumb keys work: `LEFT_THUMB` and `RIGHT_THUMB`
are keymap-local custom keycodes whose behavior is entirely defined by their
`key_behaviors[]` rows, including `PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))`
for momentary layer access on hold.

### Timing

Default timing lives in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- `TAPPING_TERM = 200`
- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`

There is one important nuance: built-in QMK dual-role keys like `LT()` and
`MT()` still use `TAPPING_TERM`. Inside `key_behaviors[]`, an omitted
`.tap_hold_term` also falls back to `TAPPING_TERM` for `LT()` rows, but to
`CUSTOM_TAP_HOLD_TERM` for other custom rows.
That split is intentional for typing feel: I want the built-in dual-role keys
to stay comfortable during fast typing, while the fully custom rows can still
use a shorter threshold where that helps.

Timing can also be customized per key. A `key_behaviors[]` row may set:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

If one of those fields is omitted, C zero-initializes it. A value of `0` means
"use the default timing for this row."

### Hold Tiers

The custom keys support three different hold styles:

- hold-and-keep-held
- fire-on-threshold
- fire-on-release-after-hold

That is what lets one key behave differently in different contexts. For
example, the nav arrows can cover character movement on tap, word movement on a
medium hold, and line movement on a longer hold.

### Multi-Tap

Multi-tap is part of the same model, not a separate feature. A key can define
different behavior for the first tap, second tap, third tap, and so on.

That is why the thumb keys and some pointer keys can keep their normal role on
hold while still exposing locks, media, or alternate actions on additional
presses.

For the full interaction model, see
[`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md).

## Pointing-Device Modes

The trackball modes use the same timing language as the rest of the board.

Current modes:

- `DRAGSCROLL`: converts ball movement into scrolling
- `PINCH_MODE`: dragscroll with `Cmd` held for
  [BetterMouse](https://better-mouse.com/)-backed pinch emulation on macOS
- `ZOOM_MODE`: converts vertical movement into `Cmd+-` and `Cmd+=`
- `ARROW_MODE`: converts ball movement into arrow keys and repurposes mouse
  buttons for editing
- `VOLUME_MODE`: converts vertical movement into volume control
- `BRIGHTNESS_MODE`: converts vertical movement into display brightness control

This keymap also enables Charybdis sniping automatically while
`LAYER_NAV` is active. That is configured in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
through `CHARYBDIS_AUTO_SNIPING_ENABLE` and
`CHARYBDIS_AUTO_SNIPING_LAYER`, so the sniping layer is easy to change. The
same file also exposes both pointer DPI ladders: `CHARYBDIS_MINIMUM_DEFAULT_DPI` and
`CHARYBDIS_DEFAULT_DPI_CONFIG_STEP` for normal cursor movement, plus
`CHARYBDIS_MINIMUM_SNIPING_DPI` and `CHARYBDIS_SNIPING_DPI_CONFIG_STEP` for
sniping mode.

These modes are not all entered the same way from the physical keys. That is
because the physical mode keys are authored through `key_behaviors[]` in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c), not
hard-wired to one fixed gesture. These entry patterns are fully customizable.

PD-mode taps are authored explicitly. If a `[0]` tap override is omitted, a
quick single tap sends nothing; single hold still defaults to momentary mode
activation.

- `ARROW_MODE`: hold for momentary arrow mode, double-tap hold to lock
- `DRAGSCROLL`: single tap `.`, hold for momentary scrolling, quick double tap
  to lock
- `VOLUME_MODE`: single tap `N`, hold for volume control, quick double tap for
  mute
- `BRIGHTNESS_MODE`: single tap `H`, hold for brightness control
- `PINCH_MODE`: single tap `J`, hold for `PINCH_MODE`, second quick tap sends
  `VIA_MACRO_6` (authored by default as Accessibility Zoom), second hold for
  `ZOOM_MODE`

For the user-facing pointer behavior, see
[`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md).

## Auto-Mouse

`LAYER_POINTER` is the default auto-mouse layer. Moving the
trackball brings the configured auto-mouse layer up automatically, and it
clears after `AUTO_MOUSE_TIME` unless pointer activity or an active mode keeps
it alive. That behavior is configured in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
through `POINTING_DEVICE_AUTO_MOUSE_ENABLE`, `AUTO_MOUSE_DEFAULT_LAYER`, and
`AUTO_MOUSE_TIME`.

## RGB

RGB is used as feedback, not decoration. All visual configuration lives in
[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
That file defines layer colors, pointer-mode colors, per-layer and per-mode LED
group highlights, auto-mouse gradient endpoints, and key-behavior feedback
colors. You can edit colors and LED groups there without touching any runtime
code.

The base RGB layer is always active:

- each layer can have a solid color
- each layer can highlight specific LEDs through LED groups
- each pointing-device mode can paint the right half a mode color
- each pointing-device mode can highlight specific LEDs through mode LED groups

On top of that, two optional overlays add dynamic feedback. Both can be
independently toggled in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- **Auto-mouse gradient** (`RGB_AUTOMOUSE_GRADIENT_ENABLE`): the configured
  auto-mouse layer uses a white-to-red countdown instead of a fixed solid
  color, so you can see how much timeout remains before the pointer layer
  clears
- **Key-behavior feedback** (`RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`): the
  key-behavior engine projects its state into the RGB overlay on both halves —
  multi-tap pending, hold pending, trigger pulses, and active held non-layer
  actions. Held layer-switch actions pulse once when they activate, then let
  the layer color take over

Both overlays are purely additive. With both disabled, `rgb_config.c` still
provides full layer and pointer-mode color feedback. With both enabled, the
render order is: layer color, auto-mouse gradient, layer LED groups,
pointer-mode color, mode LED groups, key-behavior overlay.

On split builds, the master half computes all feedback state and syncs it to
the slave through `runtime_shared_state`, so both halves render consistently.

For the full RGB authoring model, render order, and how to change feedback
colors, see [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md).

## If You Want To Go Deeper

The main implementation lives under [`users/noah/`](./users/noah/), but most
customization does not need low-level changes.

These docs are the next place to look:

- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): detailed tap,
  hold, and multi-tap behavior
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): pointer-layer and
  trackball mode behavior
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): RGB colors, key-behavior
  feedback, LED groups, and auto-mouse gradient configuration
- [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md): how the weak-hook
  model works and how to override QMK hooks in your keymap
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): how to add a new
  pointing-device mode safely

## A Little Show-Off Of My Build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

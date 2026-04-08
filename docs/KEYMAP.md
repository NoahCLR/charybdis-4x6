# Current Keymap

This document is the personal profile doc for the current `noah` Charybdis 4x6
keymap.

The top-level [README](../README.md) and
[INTERACTION_MODEL.md](./INTERACTION_MODEL.md) explain what the shared
userspace can do. This file shows what I currently do with it.

These are current choices, not guarantees of the shared runtime.

## Profile Shape

The current profile keeps the base layer close to plain QWERTY, then pushes the
extra behavior into a few consistent places:

- home-row and thumb layer access instead of many dedicated layer keys
- number-row and punctuation holds for shifted symbols
- a right-hand-heavy navigation and media surface
- a pointer layer that turns the trackball area into a mode-entry cluster
- a small set of signature keys that stack multiple useful outcomes on one
  switch

In practice, this means typing stays fairly normal, while editing, media, and
trackball control get packed into the right side and the thumbs.

## Current Layer Stack

| Layer | Current role | Current highlights |
| --- | --- | --- |
| `LAYER_BASE` | default typing layer | QWERTY, home-row layer access, custom thumbs, number-row symbol holds, signature `Esc`, `Enter`, `Shift`, and `Right Alt` behaviors |
| `LAYER_NUM` | right-hand numpad layer | numpad on the right half, `MO(LAYER_NAV)` retained on the left side for fast access back into navigation |
| `LAYER_SYM` | symbols and DPI layer | DPI controls on the left, bracket and quote families on the right, a few GUI-based shortcuts on the lower left |
| `LAYER_NAV` | navigation, media, system control, mouse buttons | arrow cluster, media, volume, brightness, GUI shortcuts, mouse buttons, `DRAGSCROLL`, and current auto-sniping |
| `LAYER_POINTER` | auto-mouse pointer surface | `BRIGHTNESS_MODE`, `PINCH_MODE`, `VOLUME_MODE`, `DRAGSCROLL`, mouse buttons, and `LT(LAYER_NUM, KC_SPC)` on the thumb cluster |

The current keymap config sets:

- `LAYER_NAV` as the auto-sniping layer
- `LAYER_POINTER` as the auto-mouse target layer

Those settings live in
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

## Current Timing Defaults

The current keymap sets these defaults in
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- `TAPPING_TERM = 200`
- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`
- `COMBO_TERM = 50`

There are also current per-key exceptions:

- `RIGHT_THUMB` uses `.tap_hold_term = 100`
- `LT(LAYER_NAV, KC_SLSH)` uses `.tap_hold_term = 100`

## Base Layer Highlights

The base layer is still recognizably a normal typing layer, but it carries a
lot of the profile's structure:

- `LT(LAYER_NAV, KC_F)` puts nav access on the left home row
- `LT(LAYER_SYM, KC_J)` puts symbol access on the right home row
- `LT(LAYER_SYM, KC_Z)` adds a second symbol entry point on the lower left
- `LT(LAYER_NAV, KC_SLSH)` gives a second nav entry point on the lower right
- `LEFT_THUMB` and `RIGHT_THUMB` are custom keymap-local keys, not plain QMK
  mod-taps
- `KC_RIGHT_ALT` is a profile-specific dual-use key: tap toggles
  `ARROW_MODE`, hold stays normal right `Alt`

The current combo set is intentionally small:

- `KC_D` + `LT(LAYER_NAV, KC_F)` -> `KC_TAB`

That keeps the layout readable while still giving one easy chorded `Tab`.

## Signature Behaviors

### Number Row And Punctuation

The current number row is not just numbers.

Single holds on `1` through `0` expose the shifted symbol family:

- `1` -> `!`
- `2` -> `@`
- `3` -> `#`
- `4` -> `$`
- `5` -> `%`
- `6` -> `^`
- `7` -> `&`
- `8` -> `*`
- `9` -> `(`
- `0` -> `)`

Some keys then grow higher-tap behavior:

- double tap `6` -> play/pause
- double tap `7` -> next track
- hold the second tap of `7` -> keep next-track held
- double tap `8` -> previous track
- hold the second tap of `8` -> keep previous-track held

The punctuation row follows the same pattern on hold:

- `-` -> `_`
- `=` -> `+`
- `[` -> `{`
- `]` -> `}`
- `\` -> `|`
- `` ` `` -> `~`
- `;` -> `:`
- `'` -> `"`
- `,` -> `<`
- `.` -> `>`

### Thumbs

`LEFT_THUMB` and `RIGHT_THUMB` are the most obviously custom keys in the
profile.

`LEFT_THUMB` currently does this:

- hold -> `MO(LAYER_SYM)`
- single tap -> `LOCK_LAYER(LAYER_SYM)`
- double tap -> play/pause
- double-tap longer hold -> `LOCK_LAYER(LAYER_NUM)`
- triple tap -> next track
- triple-tap longer hold -> keep next-track held
- quadruple tap -> previous track
- quadruple-tap longer hold -> keep previous-track held

`RIGHT_THUMB` mirrors the same media pattern, but its base layer action is nav:

- hold -> `MO(LAYER_NAV)`
- single tap -> `LOCK_LAYER(LAYER_NAV)`
- double tap -> play/pause
- double-tap longer hold -> `LOCK_LAYER(LAYER_NUM)`
- triple tap -> next track
- triple-tap longer hold -> keep next-track held
- quadruple tap -> previous track
- quadruple-tap longer hold -> keep previous-track held

The current profile also gives `RIGHT_THUMB` a shorter `100 ms` tap-hold term
so the nav thumb feels snappier than the general default.

### Slash / Nav Key

`LT(LAYER_NAV, KC_SLSH)` currently does three useful things:

- tap -> `/`
- hold -> `LAYER_NAV`
- double-tap hold -> `LOCK_LAYER(LAYER_NAV)`

This key also keeps a shorter `100 ms` tap-hold term so slash still feels close
to a normal slash key when typing quickly.

### Escape, Enter, Shift, And Right Alt

These are small profile-specific quality-of-life keys:

- `KC_ESC`: tap `Esc`, double tap `~`, long hold `Alt+Cmd+Esc`
- `KC_ENT`: hold `Shift+Enter`
- `KC_LEFT_SHIFT`: tap `Caps Lock`, hold normal left `Shift`
- `KC_RIGHT_ALT`: tap `LOCK_PD_MODE(ARROW_MODE)`, hold normal right `Alt`

`KC_RIGHT_ALT` is a good example of the profile using a plain key's default held
path while still stealing its tap for something more specialized.

### Nav Arrows

`KC_LEFT` and `KC_RIGHT` on `LAYER_NAV` currently compress character, word, and
line movement onto two keys:

`Left Arrow`:

- tap -> `Left`
- hold past `150 ms`, release before `400 ms` -> `Option+Left`
- hold past `400 ms` -> `Cmd+Left`

`Right Arrow`:

- tap -> `Right`
- hold past `150 ms`, release before `400 ms` -> `Option+Right`
- hold past `400 ms` -> `Cmd+Right`

That gives the current profile character, word, and line movement on the same
two switches.

## Layer Walkthrough

### `LAYER_BASE`

This is the main typing surface:

- plain QWERTY alpha layout
- number row and punctuation keys rely on `key_behaviors[]` for shifted holds
- nav access on `F` and `/`
- symbol access on `J` and `Z`
- custom thumb keys instead of plain layer-taps
- `KC_RIGHT_ALT` as the dedicated arrow-mode lock tap

### `LAYER_NUM`

This is a compact right-hand numpad layer:

- `P7 P8 P9` on the upper right
- `P4 P5 P6` on the home row
- `P1 P2 P3` on the lower row
- `P0` on the right thumb cluster

It also keeps a direct `MO(LAYER_NAV)` entry on the left side, so the numpad
surface does not strand navigation behind another layer jump.

### `LAYER_SYM`

This layer mixes symbols and board-control tools:

- DPI controls on the left number row
- `(` `)` on the upper right
- `[` `]` on the right home row
- `{` `}` on the lower right
- quote variants on the right edge
- a few GUI-based shortcuts and `VIA_MACRO_5` on the lower left

This makes it feel like a symbols layer first, but with a small system-control
cluster attached.

### `LAYER_NAV`

This is the densest current layer. It combines:

- media playback
- volume and brightness
- an arrow cluster
- GUI-based shortcuts
- `MS_BTN1` and `MS_BTN2`
- `DRAGSCROLL`
- three VIA macros

It is also the current auto-sniping layer, so entering nav changes both keys
and trackball feel.

### `LAYER_POINTER`

This is the current auto-mouse layer and trackball-mode surface.

It is intentionally sparse. Most of the layer is transparent, but the right
side becomes a focused cluster for:

- `BRIGHTNESS_MODE`
- `PINCH_MODE`
- `VOLUME_MODE`
- `DRAGSCROLL`
- `MS_BTN1`
- `MS_BTN2`
- `MS_BTN3`

The pointer layer also changes the thumb cluster slightly by putting
`LT(LAYER_NUM, KC_SPC)` on space, so the pointer surface can still chain into
the numpad layer without fully dropping back out.

## Pointer Workflow

The current profile uses several different pointing paths, not one single
"mouse layer":

- `LAYER_POINTER` is the auto-mouse layer
- `LAYER_NAV` is the auto-sniping layer
- `KC_RIGHT_ALT` on base is the dedicated `ARROW_MODE` lock tap
- `DRAGSCROLL` is available directly on both `LAYER_NAV` and `LAYER_POINTER`

The currently placed mode keys behave like this:

- `VOLUME_MODE`: single tap `N`, quick double tap `Mute`, hold for volume control
- `BRIGHTNESS_MODE`: single tap `H`, hold for brightness control
- `PINCH_MODE`: single tap `J`, second quick tap sends `VIA_MACRO_6`, second hold enters `ZOOM_MODE`
- `DRAGSCROLL`: single tap `.`, hold for momentary scrolling, double-tap hold locks

One important current detail:

- the key-behavior row for `ARROW_MODE` exists, but there is no plain
  `ARROW_MODE` key physically placed in `keymaps[][]` right now
- in the current profile, arrow mode is exposed through the `KC_RIGHT_ALT` tap
  lock action instead

So the current pointing setup is not "every mode gets a visible physical mode
key." It is more opinionated than that.

## Macro And Shortcut Surfaces

The current profile uses VIA defaults more than hardcoded firmware macros:

- all `MACRO_0` through `MACRO_15` hardcoded slots are currently empty
- `VIA_MACRO_0` through `VIA_MACRO_6` currently have defaults

Current VIA defaults:

| Slot | Current payload |
| --- | --- |
| `VIA_MACRO_0` | `{KC_LGUI,KC_SPC}` |
| `VIA_MACRO_1` | `{KC_LALT,KC_SPC}` |
| `VIA_MACRO_2` | `{KC_LALT,KC_LGUI,KC_SPC}` |
| `VIA_MACRO_3` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_C}` |
| `VIA_MACRO_4` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_X}` |
| `VIA_MACRO_5` | `{KC_LCTL,KC_LGUI,KC_SPC}` |
| `VIA_MACRO_6` | `{KC_LALT,KC_LGUI,KC_8}` |

The layer surfaces also use many direct modded keycodes instead of routing
everything through macros, especially on `LAYER_NAV` and `LAYER_SYM`.

## Related Files

- [`keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):
  current authored layout, combos, macros, and `key_behaviors[]`
- [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):
  current timing values, layer enum, auto-mouse, sniping, and pointer tuning
- [`rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c):
  current colors and LED highlights
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md): shared interaction semantics
- [POINTER_MODES.md](./POINTER_MODES.md): raw pointing-device behavior after a
  mode is active

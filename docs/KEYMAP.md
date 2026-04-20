# Current Keymap

This document is the personal profile doc for the current `noah` Charybdis 4x6
keymap.

The top-level [README](../README.md) and
[INTERACTION_MODEL.md](./INTERACTION_MODEL.md) explain what the shared
userspace can do. This file shows what I currently do with it.

These are current choices, not guarantees of the shared runtime.

This keymap was made for my personal macOS use. Many current choices assume
Command-based shortcuts, macOS system conventions, and a Mac-first editing and
window-management workflow rather than a cross-platform default.

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
| `LAYER_SYM` | symbols and DPI layer | DPI controls on the left, bracket and quote families on the right, and a lower-left shortcut cluster anchored by five VIA macros |
| `LAYER_NAV` | navigation, media, system control, mouse buttons | arrow cluster, media, volume, brightness, direct macOS shortcuts, mouse buttons, `DRAGSCROLL`, and current auto-sniping |
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

- `LT(LAYER_NAV, KC_SLSH)` uses `.tap_hold_term = 100`
- `CLICK_SPAM` uses `.tap_hold_term = 1`

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
  `ARROW_MODE_LOCK`, hold stays normal right `Alt`

The current combo set is intentionally small:

- `KC_D` + `LT(LAYER_NAV, KC_F)` -> `KC_TAB`
- `MS_BTN1` + `MS_BTN2` -> `CLICK_SPAM`

That keeps the layout readable while still giving one easy chorded `Tab` and a
single pointer-specific utility chord.

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

The base-layer punctuation keys follow the same pattern on hold:

- `-` -> `_`
- `[` -> `{`
- `]` -> `}`
- `\` -> `|`
- `;` -> `:`
- `'` -> `"`
- `,` -> `<`
- `.` -> `>`

`~` is currently on `KC_ESC` double tap rather than on a dedicated grave key.

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

Both thumbs currently keep the default `150 ms` tap-hold term.

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
- `KC_RIGHT_ALT`: tap `ARROW_MODE_LOCK`, hold normal right `Alt`

`KC_RIGHT_ALT` is a good example of the profile using a plain key's default held
path while still stealing its tap for something more specialized. That matters
for `ARROW_MODE`, because the mode emits real arrow taps. Holding `Right Alt`
while using horizontal arrow-mode motion still gives the usual
`Option+Left` / `Option+Right` word-jump behavior. Vertical arrow-mode taps
intentionally mask `Alt` so up/down stays plain.

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

### Click Spam

`CLICK_SPAM` is a keymap-local custom keycode used only as a combo output. It
does not appear directly in `keymaps[][]`.

The current authored path is:

- `MS_BTN1` + `MS_BTN2` combo -> `CLICK_SPAM`
- `CLICK_SPAM` hold -> `REPEAT_WHILE_HELD(MS_BTN1, 100)`

So pressing both primary mouse buttons together on `LAYER_NAV` or
`LAYER_POINTER` turns into a held repeat action that taps left click at `100 Hz`
until release.

That `100 Hz` setting is also the current authored maximum: `100` repeats per
second.

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
- a lower-left shortcut cluster built around `VIA_MACRO_5`, `VIA_MACRO_4`,
  `VIA_MACRO_3`, `VIA_MACRO_8`, and `VIA_MACRO_9`

This makes it feel like a symbols layer first, but with a small system-control
cluster attached.

### `LAYER_NAV`

This is the densest current layer. It combines:

- media playback
- volume and brightness
- an arrow cluster
- direct macOS shortcuts
- `MS_BTN1` and `MS_BTN2`
- `DRAGSCROLL`
- four VIA macros

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

For the current macOS setup, `PINCH_MODE` is meant to be used with
[BetterMouse](https://better-mouse.com/), which turns the command-scroll path
into pinch-style zoom. `ZOOM_MODE` is the fallback explicit zoom path that does
not depend on BetterMouse.

One important current detail:

- there is no plain `ARROW_MODE` key physically placed in `keymaps[][]` right
  now
- there is also no dedicated `key_behaviors[]` row for `ARROW_MODE` in the
  current profile
- arrow mode is exposed through the `KC_RIGHT_ALT` tap lock action instead
- because arrow mode emits real arrow taps, it can still be combined with held
  modifiers for bigger jumps
- the current base layout keeps those modifiers nearby: `KC_RIGHT_ALT` sits on
  the far-right pinky key, and `KC_LEFT_GUI` stays in the thumb cluster for
  `Cmd+Arrow` line jumps

So the current pointing setup is not "every mode gets a visible physical mode
key." It is more opinionated than that.

## Macro And Shortcut Surfaces

The current profile uses VIA defaults more than hardcoded firmware macros:

- all `MACRO_0` through `MACRO_15` hardcoded slots are currently empty
- `VIA_MACRO_0` through `VIA_MACRO_9` currently have defaults

But a lot of the current shortcut surface is not implemented through macro
slots at all. Many standard macOS commands are bound directly as modded
keycodes on the layers, especially on `LAYER_NAV`.

In practice, the profile is set up so those shortcuts are quick to reach from
the base layer. `LT(LAYER_NAV, KC_F)` is one of the main access points, and
`LT(LAYER_NAV, KC_SLSH)` provides a second nav entry on the right side.

Current direct macOS-standard shortcuts bound in layers include:

| Binding | Standard macOS meaning | Current layer |
| --- | --- | --- |
| `G(KC_Q)` | quit the current app | `LAYER_NAV` |
| `G(KC_W)` | close the front window | `LAYER_NAV` |
| `G(KC_A)` | select all | `LAYER_NAV` |
| `G(KC_X)` | cut | `LAYER_NAV` |
| `G(KC_C)` | copy | `LAYER_NAV` |
| `G(KC_V)` | paste | `LAYER_NAV` |
| `G(KC_Z)` | undo | `LAYER_NAV` |
| `LSG(KC_Z)` | redo | `LAYER_NAV` |

The VIA defaults are a smaller mixed set. Some are standard macOS shortcuts,
while others are just my current app-launcher or utility bindings:

| Slot | Current payload | Current meaning |
| --- | --- | --- |
| `VIA_MACRO_0` | `{KC_LGUI,KC_SPC}` | standard macOS Spotlight shortcut |
| `VIA_MACRO_1` | `{KC_LALT,KC_SPC}` | current launcher shortcut for Claude or ChatGPT |
| `VIA_MACRO_2` | `{KC_LALT,KC_LGUI,KC_SPC}` | current launcher shortcut for [Warp Terminal](https://www.warp.dev/) |
| `VIA_MACRO_3` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_C}` | current OCR text-copy shortcut for [TextGrabber](https://apps.apple.com/us/app/textgrabber/id6451423640?mt=12) |
| `VIA_MACRO_4` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_X}` | current screenshot shortcut |
| `VIA_MACRO_5` | `{KC_LCTL,KC_LGUI,KC_SPC}` | standard macOS Character Viewer shortcut |
| `VIA_MACRO_6` | `{KC_LALT,KC_LGUI,KC_8}` | current shortcut for my [macOS Accessibility Zoom](https://support.apple.com/guide/mac-help/zoom-in-on-whats-onscreen-mh40579/mac) setup, used as a picture-in-picture looking glass |
| `VIA_MACRO_7` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_V}` | current shortcut for [Maccy](https://maccy.app/), my clipboard manager |
| `VIA_MACRO_8` | `{KC_LSFT,KC_LGUI,KC_V}` | current shortcut for VS Code preview |
| `VIA_MACRO_9` | `{KC_LSFT,KC_LGUI,KC_P}` | current shortcut for the VS Code command palette |

So the current profile uses both:

- direct modded keycodes for many standard macOS editing and app shortcuts
- VIA macros for a smaller set of macOS launcher/system shortcuts and
  personal utility bindings

Some other shortcuts in the current profile, especially several VIA slots, are
simply personal shortcuts rather than standard macOS conventions.

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
- [VIA_TO_QMK.md](./VIA_TO_QMK.md): VIA export round-trip workflow for the
  current keymap source

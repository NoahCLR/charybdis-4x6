# Current Keymap

This document describes Noah's current authored profile on top of the shared
`noah` userspace.

These are current choices, not guarantees of the shared runtime. If you want
the general interaction model, see [INTERACTION_MODEL.md](./INTERACTION_MODEL.md).

## Current Timing Defaults

The current keymap sets these defaults in
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- `TAPPING_TERM = 200`
- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`

There are also current per-key exceptions:

- `RIGHT_THUMB` uses `.tap_hold_term = 100`
- `LT(LAYER_NAV, KC_SLSH)` uses `.tap_hold_term = 100`

## Representative Keys

### Nav Arrows

`KC_LEFT` and `KC_RIGHT` on `LAYER_NAV` currently do this:

`Left Arrow`:

- tap -> `Left`
- hold past `150 ms`, release before `400 ms` -> `Option+Left`
- hold past `400 ms` -> `Cmd+Left`

`Right Arrow`:

- tap -> `Right`
- hold past `150 ms`, release before `400 ms` -> `Option+Right`
- hold past `400 ms` -> `Cmd+Right`

This gives the current profile character, word, and line movement on the same
two keys.

### Number Row

The current number row is not only numbers.

Examples:

- tap `1` -> `1`
- hold `1` -> `!`
- tap `6` twice -> play/pause
- tap `7` twice -> next track
- hold the second tap of `7` -> keep next-track held
- tap `8` twice -> previous track
- hold the second tap of `8` -> keep previous-track held

### Thumb Layer Keys

`LEFT_THUMB` and `RIGHT_THUMB` are keymap-local custom keycodes whose behavior
is currently defined entirely by their `key_behaviors[]` rows.

`LEFT_THUMB` currently does this:

- hold -> `MO(LAYER_SYM)`
- single tap -> `LOCK_LAYER(LAYER_SYM)`
- double tap -> play/pause
- double-tap longer hold -> `LOCK_LAYER(LAYER_NUM)`
- triple tap -> next track
- triple-tap longer hold -> keep next-track held
- quadruple tap -> previous track
- quadruple-tap longer hold -> keep previous-track held

`RIGHT_THUMB` currently does the same media pattern, but its base layer action
is:

- hold -> `MO(LAYER_NAV)`
- single tap -> `LOCK_LAYER(LAYER_NAV)`

The current profile also gives `RIGHT_THUMB` a shorter tap-hold term of
`100 ms`.

### Slash / Nav Key

`LT(LAYER_NAV, KC_SLSH)` currently does three useful things:

- tap -> `/`
- hold -> `LAYER_NAV`
- double-tap hold -> `LOCK_LAYER(LAYER_NAV)`

In the current profile, `LAYER_NAV` also enables Charybdis auto-sniping through
the keymap [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

This key also keeps a shorter `100 ms` tap-hold term so slash still types like
a normal slash key.

### Escape

`KC_ESC` is currently configured like this:

- tap -> `Esc`
- double tap -> `~`
- long hold -> `Alt+Cmd+Esc`

That gives the current profile normal escape, tilde, and force quit on the
same key.

### Pointer-Mode Keys

The current profile uses authored `key_behaviors[]` rows on several pd-mode
keys to add taps or higher-tap branches on top of the default momentary hold.

Current patterns:

- `ARROW_MODE`: hold for momentary arrow mode, double-tap hold to lock
- `DRAGSCROLL`: single tap `.`, hold for momentary scrolling, double-tap hold
  to lock
- `VOLUME_MODE`: single tap `N`, hold for volume control, quick double tap to
  mute
- `BRIGHTNESS_MODE`: single tap `H`, hold for brightness control
- `PINCH_MODE`: single tap `J`, second quick tap sends `VIA_MACRO_6`, second
  hold enters `ZOOM_MODE`

`PINCH_MODE` is the most specialized current example because it mixes a plain
tap, a media-style second tap, and a second-press branch into another pd mode.

## Related Files

- [`keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):
  current authored layout, combos, macros, and `key_behaviors[]`
- [`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):
  current timing values, layer enum, auto-mouse, and sniping settings
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md): shared interaction semantics
- [POINTER_MODES.md](./POINTER_MODES.md): raw pd-mode behavior after activation

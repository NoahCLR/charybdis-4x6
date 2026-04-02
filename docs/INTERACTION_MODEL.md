# Interaction Model

This userspace has a custom key-behavior engine.

The point of that engine is simple: one authored keycode can do more than one
thing, and the rules are consistent enough that the board stays usable.

This document is a description of what the keys actually do and why those behaviors are useful.

## What The Engine Adds

For keys with authored `key_behaviors[]` rows, the firmware can distinguish
between:

- tap
- hold
- longer hold
- single tap through quintuple tap
- hold styles that fire at different times

That is what makes these behaviors possible:

- arrow keys that cover character, word, and line movement
- thumb layer keys that also lock layers and control media
- number and punctuation keys that expose shifted symbols on hold
- pointer-mode keys that can lock, mute, or branch into another mode

## Default Timing

The default timing values come from the active keymap `config.h`:

- `CUSTOM_TAP_HOLD_TERM = 150`
- `CUSTOM_LONGER_HOLD_TERM = 400`
- `CUSTOM_MULTI_TAP_TERM = 150`
- `TAPPING_TERM = 200` for built-in QMK dual-role keys such as `LT()` and `MT()`

Those are only the global defaults. Individual `key_behaviors[]` rows can
override them with:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

If one of those fields is omitted, C zero-initializes it. A value of `0` means
"use the default timing for this row."

In plain terms:

- quick release before `150 ms` is treated as a tap
- crossing `150 ms` can trigger the hold behavior
- crossing `400 ms` can trigger the longer-hold behavior
- repeated taps must stay within `150 ms` of each other to remain part of the same sequence

One practical consequence: a single tap on a multi-tap key is delayed by one multi-tap window so the firmware can tell whether you meant one tap or more.

One important nuance: inside `key_behaviors[]`, omitted `.tap_hold_term`
inherits `TAPPING_TERM` for `LT()` rows, but `CUSTOM_TAP_HOLD_TERM` for other
custom rows.

## The Three Hold Modes

The important detail is that not every hold behaves the same way.

### `PRESS_AND_HOLD_UNTIL_RELEASE`

The alternate action becomes active at the hold threshold and stays active until you let go.

Actual examples:

- hold `1` -> `!`
- hold `2` -> `@`
- hold `-` -> `_`
- hold `Enter` -> `Shift+Enter`
- hold the second tap of `7` -> keep `KC_MNXT` held
- hold the second tap of `8` -> keep `KC_MPRV` held

Why it is useful:

- shifted symbols feel immediate
- held symbols can repeat naturally
- media next/previous can be held on the final tap instead of only tapped once

For media keys, the exact result of holding `KC_MNXT` or `KC_MPRV` depends on the app:

- some players treat it as repeated next/previous
- some treat it more like scan / fast-forward / rewind

The important point is that the userspace does not only give you "next song". It also gives you a held version of that media action on keys like `7`, `8`, and the higher-tap thumb gestures.

### `TAP_AT_HOLD_THRESHOLD`

The alternate action fires once as soon as the threshold is crossed.

Actual examples:

- double-tap-hold `MO(LAYER_SYM)` -> lock `LAYER_NUM`
- double-tap-hold `MO(LAYER_NAV)` -> lock `LAYER_NUM`
- double-tap-hold `/` -> lock `LAYER_NAV`
- long hold `Esc` -> `Alt+Cmd+Esc`
- long hold `Left Arrow` on `LAYER_NAV` -> `Cmd+Left`
- long hold `Right Arrow` on `LAYER_NAV` -> `Cmd+Right`

Why it is useful:

- layer locks happen as soon as you commit to them
- one-shot system shortcuts do not wait for release
- a key can escalate cleanly from "hold" to "long hold"

### `TAP_ON_RELEASE_AFTER_HOLD`

Nothing fires at the hold threshold. The action is sent when you release the key.
If a longer-hold tier takes over before release, this release-based hold does
not fire.

Actual examples:

- medium hold `Left Arrow` on `LAYER_NAV` -> `Option+Left` on release
- medium hold `Right Arrow` on `LAYER_NAV` -> `Option+Right` on release

This is one of the most useful details in the repo.

On macOS, those arrows give you:

- tap -> move by one character
- medium hold -> move by one word
- longer hold -> move to the beginning or end of the line

That means the same left/right keys cover character, word, and line movement.

The release-based middle tier matters because it avoids an early jump while you are still deciding whether you want a word move or a full line move.

## Multi-Tap Behavior

Multi-tap is built into the same engine. It is not a side feature.

The board can currently resolve:

- single tap / hold / longer hold
- double tap / double-tap hold / double-tap longer hold
- triple tap / triple-tap hold / triple-tap longer hold
- quadruple tap / quadruple-tap hold / quadruple-tap longer hold
- quintuple tap / quintuple-tap hold / quintuple-tap longer hold

That is why the thumb layer keys can do all of this without extra physical keys:

- hold for the layer
- single tap to lock the layer
- double tap for play/pause
- double-tap hold to lock `LAYER_NUM`
- triple tap for next track
- triple-tap hold to keep `KC_MNXT` held
- quadruple tap for previous track
- quadruple-tap hold to keep `KC_MPRV` held

Again, the held media behavior may act like repeated skip or scan depending on the player.

## Concrete Examples

### Nav Arrows

`KC_LEFT` and `KC_RIGHT` on `LAYER_NAV` are a good example of the engine being worth the complexity.

`Left Arrow` on `LAYER_NAV`:

- tap -> `Left`
- hold past `150 ms`, release before `400 ms` -> `Option+Left`
- hold past `400 ms` -> `Cmd+Left`

`Right Arrow` on `LAYER_NAV`:

- tap -> `Right`
- hold past `150 ms`, release before `400 ms` -> `Option+Right`
- hold past `400 ms` -> `Cmd+Right`

User-facing value:

- one key handles character movement
- the same key handles word movement
- the same key handles line movement

This is better than putting separate shortcuts on separate keys, because the motion hierarchy stays under the same finger.

### Number Row

The number row is not only numbers.

Examples:

- tap `1` -> `1`
- hold `1` -> `!`
- tap `6` twice -> play/pause
- tap `7` twice -> next track
- hold the second tap of `7` -> keep next-track held
- tap `8` twice -> previous track
- hold the second tap of `8` -> keep previous-track held

User-facing value:

- symbols stay on the number row
- basic media stays on the number row
- next/previous has both a tap version and a held version

### Thumb Layer Keys

`MO(LAYER_SYM)` and `MO(LAYER_NAV)` are not simple momentary layer keys.

They currently do this:

- hold -> momentary layer
- single tap -> lock that layer
- double tap -> play/pause
- double-tap hold -> lock `LAYER_NUM`
- triple tap -> next track
- triple-tap hold -> keep next-track held
- quadruple tap -> previous track
- quadruple-tap hold -> keep previous-track held

User-facing value:

- the most important layers live on strong thumb keys
- layer lock does not need a dedicated key
- `LAYER_NUM` lock is reachable from either thumb
- media control is available without moving away from the thumb cluster

### Slash / Nav Key

`LT(LAYER_NAV, KC_SLSH)` does three useful things:

- tap -> `/`
- hold -> `LAYER_NAV`
- double-tap hold -> lock `LAYER_NAV`

In the current keymap, `LAYER_NAV` also enables Charybdis sniping, so this key
is part of the precise-cursor workflow as well as the navigation layer
workflow.

One important implementation detail: this key keeps the normal `LT()` tap
timing instead of using the global custom threshold, so the slash key still
types like a normal slash key.

### Escape

`Esc` is another compact example:

- tap -> `Esc`
- double tap -> `~`
- long hold -> `Alt+Cmd+Esc`

That gives you normal escape, tilde, and force quit on the same key without making plain `Esc` awkward.

### Pointer-Mode Keys

The pointer keys use the same timing model instead of inventing their own rules.

Unless a `[0]` tap override is authored, a quick single tap on a pd-mode key
still sends the base-layer key at that physical position. The examples below
focus on the extra hold and second-press behavior.

Examples:

- `ARROW_MODE`: hold for momentary arrow mode, quick double tap to lock
- `DRAGSCROLL`: hold for momentary scrolling, quick double tap to lock
- `VOLUME_MODE`: hold for volume control, quick double tap to mute
- `PINCH_MODE`: first hold for
  [BetterMouse](https://better-mouse.com/)-backed command-scroll pinch on
  macOS, second quick tap for Accessibility Zoom, second hold for `ZOOM_MODE`

`PINCH_MODE` matters because it shows that the system can also branch into a completely different second-press path when needed.

## Next Doc

- Read [POINTER_MODES.md](./POINTER_MODES.md) for the trackball-specific side of the same interaction model.

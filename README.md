# Noah's Charybdis 4x6 firmware

This is the firmware I type on every day, for the
[BastardKB Charybdis 4x6](https://bastardkb.com/) with an RP2040 controller.
Pair it with **[Charybdis Ark](https://github.com/NoahCLR/charybdis-ark)**,
its editor app, and you can change keys, layers, tap/hold behaviours, combos,
macros, lighting and pointing modes on the connected keyboard. You don't write
any C and you don't reflash.

## What it does

- **The keyboard keeps its own profile.** Keys, layers, behaviours, combos,
  macros, custom keys, pointing modes, lighting and the keyboard's name all
  live on the keyboard, and Ark reads and edits them there. Edits run on both
  halves straight away and wait for your Save, or the keyboard saves them
  itself a few seconds after you stop. A save writes both halves so that a
  power cut leaves the old profile or the new one, never half of each. The
  VIA app isn't supported: I dropped it so the whole profile lives in one
  place.
- **Keys that do more than one thing.** Each key can do something different on
  tap, hold and long hold, and again on a double, triple or up to quintuple
  tap. Combos and custom keys use the same behaviours, and a custom key can
  hold a shortcut of up to six keys at once, media keys among them, with
  modifiers from both sides (Left Ctrl + Right Alt + K) and any number of
  layers with it, held, locked, moved to, tap-toggled or one-shot together.
- **A trackball that changes roles.** Thirty-two pointing slots turn the
  ball into scrolling, volume, zoom, arrow keys, mouse-button clicks, macros
  or your own shortcuts, held or locked, or on whenever a layer is. A mode
  can bring its own layer of keys with it. A pointer layer comes up by itself
  when you move the ball, and sniping slows it down for precise work on the
  layers you pick.
- **Lights that tell you what is going on.** They show which layer and mode
  are on, which keys made a combo, which tap count a key is on and whether a
  hold has started, and they fade as the pointer layer is about to drop.
- **Macros in your layout.** Macros type through your computer's keyboard
  layout (US, Dutch, German, French, British and more, on macOS, Windows and
  Linux), accents included, with Unicode entry for emoji.
- **Two halves that act like one keyboard.** Layers, modes, combos, lighting
  and saved edits stay in step across the cable.

> **Heads-up:** this is an opinionated firmware, not a copy-paste QMK keymap.
> I've reshaped some QMK ideas into a Charybdis-specific runtime with its own
> behaviour tables, lighting language and split sync. If you want small
> snippets for a normal keymap, it's probably not the easiest place to start.

## Get started

1. Download `bastardkb_charybdis_4x6_noah_right.uf2` and
   `bastardkb_charybdis_4x6_noah_left.uf2` from the
   [latest release](https://github.com/NoahCLR/charybdis-4x6/releases/latest).
2. Flash each half: double-tap its reset button and copy its `.uf2` onto the
   drive that appears. The right half is the one with the USB cable. Always
   flash both halves from the same release.
3. Open [Charybdis Ark](https://github.com/NoahCLR/charybdis-ark#install) with
   the keyboard connected.

An update can start the keyboard from its defaults, so export a backup in Ark
first. [Installing and recovering](docs/guide/installing.md) has the details.

## Configure it in C (optional)

With Ark you don't have to. But the defaults are plain data in the
[keymap folder](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/), and the
shared runtime in [`users/noah/`](./users/noah/) handles the rest. This is my
right thumb key, slightly trimmed:

```c
{
    .keycode = CUSTOM_KEY_0, // "Right Thumb"
    .tap_counts = {
        // tap: lock Navigation; hold: Navigation while held
        [0] = { .tap  = TAP_SENDS(LOCK_LAYER(LAYER_NAV)),
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV)) },
        // double tap: play/pause; hold: Esc; hold longer: lock Number
        [1] = { .tap       = TAP_SENDS(KC_MPLY),
                .hold      = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE),
                .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM)) },
        // triple tap: next track
        [2] = { .tap = TAP_SENDS(KC_MNXT) },
    },
},
```

[Authoring in C](docs/guide/authoring.md) covers the files and building the
pair yourself.

## Docs

- [Firmware guide](docs/guide/README.md): what the keyboard does, installing
  and recovering, and authoring in C
- [Ark's guide](https://github.com/NoahCLR/charybdis-ark/tree/dev/docs/guide),
  for editing the keyboard
- [Development](docs/development.md) and the
  [architecture](docs/architecture/README.md), for working on the firmware
  itself

## Thanks

The Charybdis is Quentin's open-source design from
[BastardKB](https://bastardkb.com/), and its hardware files are in the
[Charybdis project](https://github.com/Bastardkb/Charybdis). His design and the
community's mods made this build possible, and it has given me hundreds of
hours of good firmware and hardware tinkering. If you want to support him, buy
the hardware from [BastardKB](https://bastardkb.com/), not from a knockoff seller.

I use AI in the workflow around this repo. The firmware is still my own daily
driver, and I've spent many hours tuning the hardware, layout, behaviour and
docs by hand.

## A little show-off of my build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

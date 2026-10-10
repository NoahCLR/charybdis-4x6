# Noah's Charybdis 4x6 firmware

This is the firmware I type on every day, for the
[BastardKB Charybdis 4x6](https://bastardkb.com/) with an RP2040 controller.
Pair it with **[Charybdis Ark](https://github.com/NoahCLR/charybdis-ark)**,
its editor app, and you can change keys, layers, tap/hold behaviours, combos,
macros, lighting and trackball modes on the connected keyboard. You don't write
any C and you don't reflash.

## What it does

- **Live editing with Ark.** Ark reads everything straight from the keyboard.
  Your changes go into a draft that you review before anything is saved, and
  Apply writes both halves and reads them back. You can export a whole profile
  to one file and import it again. Small edits can reuse unchanged profile data
  while the keyboard still validates and saves the complete result.
- **Text macros in your layout.** Macros type through your computer's keyboard
  layout (US, Dutch, German, French, UK and more on macOS, Windows and Linux),
  accents included, with Unicode entry for emoji; see the
  [guide](docs/GUIDE.md#text-macros-and-your-keyboard-layout).
- **Keys that do more than one thing.** Each key can do something different on
  tap, hold and longer hold, and again on double, triple or up to quintuple
  tap, each with its own timing. Combos can use the same behaviours.
- **A trackball that changes roles.** Thirty-two pointing-mode slots turn the ball
  into scrolling, volume, brightness, zoom, arrow keys or your own shortcuts,
  held or locked. A pointer layer comes up by itself when you move the ball,
  and you can snipe for precise movement.
- **Lights that tell you what is going on.** They show which layer and mode are
  active, which keys made a combo, which tap is waiting and whether a hold has
  committed. They also fade to show when the auto-mouse layer is about to drop.
- **Two halves that act like one keyboard.** Layers, modes, combos, lighting
  and saved edits stay in sync across the cable.
- **Room to grow.** It has 16 layers, 128 named macros and 128 custom keys you
  can name and give behaviours to.

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
3. [Install Charybdis Ark](https://github.com/NoahCLR/charybdis-ark#install)
   and open it with the keyboard connected.

Only this side-specific pair works with Ark. A plain `qmk compile` image
doesn't. Firmware accepts current-format profiles; keep a backup before an
upgrade, and use Ark's import translation for older backups.

## Configure it in C (optional)

With Ark you don't have to. But the compiled defaults are plain data in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
and [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h),
and the shared runtime in [`users/noah/`](./users/noah/) handles the rest. This
is my right thumb key, slightly trimmed:

```c
{
    .keycode = CUSTOM_KEY_0, // "Right Thumb"
    .tap_counts = {
        // tap: lock Nav; hold: Nav while held
        [0] = { .tap  = TAP_SENDS(LOCK_LAYER(LAYER_NAV)),
                .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV)) },
        // double tap: play/pause; hold: Esc; hold longer: lock Numbers
        [1] = { .tap       = TAP_SENDS(KC_MPLY),
                .hold      = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE),
                .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM)) },
        // triple tap: next track
        [2] = { .tap = TAP_SENDS(KC_MNXT) },
    },
},
```

To build it yourself, use my
[BastardKB QMK fork](https://github.com/NoahCLR/bastardkb-qmk), set
`QMK_USERSPACE` to this repo and run `sh tools/build-firmware-pair.sh`. The
[firmware guide](docs/GUIDE.md) has the full behaviour vocabulary, the
lighting model, split sync and the build details.

## Docs

- [Firmware guide](docs/GUIDE.md): the long version of this page
- [My keymap](docs/KEYMAP.md) and its
  [generated overview](docs/KEYMAP-OVERVIEW.md)
- [Tap, hold and multi-tap](docs/INTERACTION_MODEL.md),
  [pointing modes](docs/POINTER_MODES.md) and
  [lighting](docs/RGB_CONFIG.md), in depth
- [Third-party apps in my setup](docs/MY_SETUP.md)
- [Hook overrides](docs/HOOK_OVERRIDES.md) and
  [profile introspection](docs/tooling/PROFILE_INTROSPECT.md), for authoring in C
- [Development](docs/DEVELOPMENT.md) and the
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

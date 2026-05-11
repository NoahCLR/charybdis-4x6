# Noah's Charybdis Userspace

This repo is the shared userspace for my Charybdis 4x6.

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this split trackball
board rather than stock QMK conventions.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the interesting behavior centralized and
editable, so the board can grow through authored profile data instead of
scattered runtime rewrites.

> **Opinionated userspace warning:** This repo depends on QMK, but it is not a
> standard copy-paste QMK keymap. I have interpreted some QMK surfaces
> differently and shaped them into a Charybdis-specific userspace with its own
> runtime, data tables, RGB language, split sync, and Profile Studio workflow.
> If you are looking for small snippets to drop into a normal keymap, this is
> probably not the easiest place to start. It is more useful as an example of a
> very opinionated firmware model.
>
> **Firmware note:** This userspace is updated for QMK `0.32.5` and builds
> against my [`qmk-latest` firmware branch](https://github.com/NoahCLR/bastardkb-qmk/tree/qmk-latest)
> rather than the older `bkb-master`-based setup. That branch also carries my
> auto-mouse timer getter changes, which this userspace uses for the
> auto-mouse RGB timeout fade and split-synced progress.
>
> **Build note:** Use that firmware fork, point `QMK_USERSPACE` at this repo,
> and build with:
>
> ```sh
> qmk compile -kb bastardkb/charybdis/4x6 -km noah
> ```

This repo is built around the open-source Charybdis from
[BastardKB](https://bastardkb.com/), designed by Quentin. The hardware files
are available in the
[BastardKB Charybdis project](https://github.com/Bastardkb/Charybdis).
Quentin's design, and the many mods the community has built around this board,
are what made this build possible. This keyboard has given me hundreds of
hours of useful firmware and hardware tinkering. If you want to support the
creator, buy the hardware from [BastardKB](https://bastardkb.com/) rather than
from a knockoff seller.

## What This Userspace Is For

This repo is my Charybdis 4x6 userspace and profile. The interesting part is
how the profile is authored: key behavior lives in data tables, RGB feedback has
its own authored language, and the shared runtime turns those choices into
firmware behavior.

The two main authoring files are:

- [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):
  what keys, layers, combos, macros, and per-key behaviors exist
- [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c):
  how layers, pointer modes, combos, key states, and auto-mouse timing are
  shown on the LEDs

The goal is not to copy one exact layout. The useful part is the model: you can
describe what a key should do, describe what the lights should show, and let
the shared userspace handle the timing, split sync, trackball modes, VIA
bridges, and RGB rendering behind that.

## What You Can Build

You can keep a layout readable while still giving the board a lot of
behavior:

- layers for typing, numbers, symbols, navigation, pointer controls, or any
  other surface you want
- combos for simultaneous chords, including chords that emit a keycode handled
  by `key_behaviors[]`
- VIA-editable macro slots alongside hardcoded source-owned macros
- keys that do one thing on tap, another on hold, another on longer hold, and
  different things again on double-tap or higher tap counts
- pointing-mode keys that can be simple momentary holds, locks, or richer
  tap/hold keys using the same behavior table as the rest of the board

You can also make the trackball change roles instead of only moving the cursor.
The current profile includes:

- `DRAGSCROLL`: ball motion becomes scrolling, as either a momentary hold or a
  lock
- `PINCH_MODE`: command-modified scrolling for pinch-style zoom on macOS; in my
  setup this expects third-party software such as
  [BetterMouse](https://better-mouse.com/) to translate that gesture
- `ZOOM_MODE`: explicit keyboard zoom using `Cmd+=` and `Cmd+-`
- `ARROW_MODE`: dominant ball motion sends arrow-key taps instead of cursor
  movement
- `VOLUME_MODE` and `BRIGHTNESS_MODE`: vertical ball motion changes system
  volume or display brightness
- `CLICK_SPAM`: not a pointing mode, but a mouse-button combo output that uses
  the behavior table to repeat left-click while held
- auto-mouse and auto-sniping layers that keep pointer work available
  automatically while you move between typing and trackball use

Because mode keycodes and their generated `*_LOCK` keycodes are normal actions,
`key_behaviors[]` can make one key hold a mode momentarily and toggle its lock on
a double-tap or double-tap hold. Locks are also easy to leave: pressing or
holding the same runtime-handled mode key while that mode is locked clears the
lock, then behaves as a normal momentary mode until release. Activating or
locking a different pointing mode clears the previous mode lock too.

Those are capabilities, not a fixed layout prescription. `keymap.c` decides
where these ideas live.

## Profile Studio

If you are new to this userspace, start with Charybdis Profile Studio. It is
the repo-local VS Code extension for editing the profile visually, so you can
click through the layout, macros, RGB, and defaults before digging into the C
model.

It works directly on the selected profile's authored source files. For my
current profile, those are:

- [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

There is no sidecar profile database. The C files stay the source of truth.
The Studio parses those files, shows a VS Code webview, stages edits, and
patches the same authored blocks when you apply changes.

The profile picker can also create, clone, rename, and delete Charybdis 4x6
keymaps under `keyboards/bastardkb/charybdis/4x6/keymaps/<name>/` while keeping
`qmk.json` build targets in sync. A generated profile gets a blank `keymap.c`
authoring surface, compile-ready `config.h` and `rgb_config.c`, and a small
`rules.mk` that points QMK at the shared `users/noah` runtime. It is registered
in `qmk.json`, so you can build it with:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km <name>
```

For a read-only overview of my current config, start with the generated
[`KEYMAP-OVERVIEW.md`](./docs/KEYMAP-OVERVIEW.md). The companion
[`KEYMAP.md`](./docs/KEYMAP.md) explains the current profile choices in prose.
Other profiles can generate their own overview with
`python3 tools/profile_introspect.py --keymap <name> --write`; those reports
default to `docs/profiles/<name>/KEYMAP-OVERVIEW.md`.
Profile Studio also has a Profile overview row with a `Create overview doc`
action for the active profile. It warns when Studio has unapplied edits because
the overview is generated from source files on disk.
The Firmware row's `Compile left + right` action opens the Charybdis Profile
Studio output pane and streams QMK while it builds left and right UF2 files for
the active profile.

Use Profile Studio when you want to:

- click keys on a visual Charybdis layout and edit layer slots
- add or remove layers through the supported source patches
- edit selected-key behavior rows and combo-output behavior rows
- append simple combos from physical key selections, or load an existing combo
  to edit its output and inputs
- edit VIA macro slots with a macro builder and key-event recorder
- choose layer and pointing-mode colors
- build reusable LED groups by selecting LEDs on the board
- edit auto-mouse fade settings, combo feedback, and key-behavior feedback
- compile left and right firmware outputs for the selected profile
- configure behavior-specific `config.h` defaults for key timing, normal
  pointer speed, pointing modes, sniping, auto-mouse, base lighting, and
  lighting feedback

Screenshots:
[`Layout`](./docs/media/profile-studio/studio-layout-tab.png),
[`Macros`](./docs/media/profile-studio/studio-macros-tab.png),
[`RGB`](./docs/media/profile-studio/studio-rgb-tab.png), and
[`Defaults`](./docs/media/profile-studio/studio-defaults-tab.png).

From VS Code with this repo folder open:

1. Run the VS Code task `Install Profile Studio Extension`.
2. Reload VS Code.
3. Open it from the `$(keyboard) Profile Studio` status bar item, or run
   `Charybdis: Open Profile Studio` from the command palette.

You can also install it from a shell:

```sh
cd tools/charybdis-profile-studio
npm run install:local
```

The full Studio guide is
[`docs/tooling/PROFILE_STUDIO.md`](./docs/tooling/PROFILE_STUDIO.md).

The next sections explain the keymap and RGB models that the Studio edits.

## The Keymap Model

[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) is the
main profile file. It is where you make the board yours.

Use it for:

- the physical layer layout
- combos
- VIA macro defaults
- hardcoded macros
- custom keycodes
- `key_behaviors[]`
- pointing-mode key placement and richer mode gestures

The important table is `key_behaviors[]`. Stock QMK already has useful pieces
of this idea: `LT()` and `MT()` cover common tap/hold keys, and Tap Dance can
make tap counts choose different outputs. This userspace is a different model:
one authored behavior row can combine tap counts, hold tiers, timing, RGB
feedback, combo outputs, pointer-mode ownership, layer ownership, and split
state in one place.

That means a key can branch more deliberately:

- tap can send one action
- hold can keep a modifier, layer, mouse button, or pointing mode active
- longer hold can do a stronger or different action
- double-tap and higher tap counts can expose locks, media, macros, layer
  changes, or alternate actions
- timing can be left at profile defaults or tuned per key

Combos can enter that same table too. If a combo emits a keycode that has a
`key_behaviors[]` row, the chord can reuse the same tap, hold, longer-hold, and
multi-tap behavior as a physical key.

Here is the shape of one authored row, based on the `RIGHT_THUMB` row in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c#L435).
The timing lines are optional row-local overrides, and branch confirm is the
short RGB-visible pause after a double-tap or higher branch wins, before the
action fires. The snippet shows one useful helper mix, not the full helper
vocabulary; the list below shows the other helpers you can use.

```c
{
    .keycode = RIGHT_THUMB,
    .tap_hold_term = 150,
    .longer_hold_term = 400,
    .multi_tap_term = 150,
    .rgb_branch_confirm_term = 150,
    .skip_rgb_branch_confirm = false,
    .tap_counts = {
        [0] = {
            .tap = TAP_SENDS(LOCK_LAYER(LAYER_NAV)),
            .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV)),
        },
        [1] = {
            .tap = TAP_SENDS(KC_MPLY),
            .hold = TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE),
            .long_hold = TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM)),
        },
        [2] = {
            .tap = TAP_SENDS(KC_MNXT),
            .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT),
        },
        [3] = {
            .tap = TAP_SENDS(KC_MPRV),
            .long_hold = PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV),
        },
    },
},
```

In that example, `RIGHT_THUMB` can be placed directly on a layer or emitted by a
combo. Either way, the behavior row is the same.

The double-tap `.hold` uses the release-based helper because that branch also
has a later `.long_hold`. The single-tap layer hold and the media long-holds use
the press-and-hold helper because those branches can stay active until key
release.

The vocabulary is:

- `tap_counts[0]`, `[1]`, `[2]`, and onward are tap-count branches: single
  press, double press, triple press, and so on
- `.tap` is the quick-release tier for that branch
- `.hold` is the first hold tier for that branch
- `.long_hold` is the later hold tier for that branch
- `.tap_hold_term`, `.longer_hold_term`, and `.multi_tap_term` override timing
  for one row
- `.rgb_branch_confirm_term = ms` sets the RGB-visible committed-branch window
  for one row
- `.skip_rgb_branch_confirm = true` skips that RGB branch-confirm window for one
  row

The helper vocabulary is:

- `TAP_SENDS(action)`: quick release sends `action`
- `PRESS_AND_HOLD_UNTIL_RELEASE(action)`: cross the hold threshold, press or
  register `action`, release it when the key is released
- `REPEAT_WHILE_HELD(action, hz)`: cross the hold threshold, tap `action`
  repeatedly at `hz` until release
- `TAP_AT_HOLD_THRESHOLD(action)`: send `action` once as soon as the hold tier
  commits
- `TAP_ON_RELEASE_AFTER_HOLD(action)`: qualify the hold at the threshold, then
  send `action` on release unless a longer hold replaces it

`action` can be a normal keycode, a modified keycode such as `S(KC_1)`, a VIA
macro, a hardcoded macro, a supported QMK behavior keycode such as `OSM()` or
`MT()`, a generated pointing-mode lock such as `DRAGSCROLL_LOCK`, or a layer
lock through `LOCK_LAYER(layer)`. For custom momentary layer holds, use
`PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))` so the userspace owns the layer
state. Inside any helper, `KC_TRNS` means "use the lower active layer's
matching tap, hold, or long-hold behavior here."

The matching key-behavior RGB config in
[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L319)
follows that same model. You do not need every field in every profile; this
example shows the vocabulary.

```c
const key_behavior_feedback_color_config_t key_behavior_feedback_colors = {
    .tap_pending_color = HSV(0, 0, 150),
    RGB_TAP_BRANCH_COLORS(
        HSV(200, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // double tap
        HSV(180, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // triple tap
        HSV(143, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), // quadruple tap
        HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS)   // quintuple tap
    ),
    .branch_confirm_mode = KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS,
    .tap_committed_color = HSV(85, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .tap_commit_mode = KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS,
    .hold_active_color = HSV(18, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .long_hold_active_color = HSV(148, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS),
    .locality = RGB_KEY_HALF,
};

static const key_behavior_feedback_led_group_t
    key_behavior_feedback_led_groups_data[] = RGB_LED_GROUP_TABLE(
        {
            .semantic = KEY_FEEDBACK_GROUP_ALL,
            .color = HSV(0, 0, 0),
            .led_group = RGB_LED_GROUP_THUMBS,
        },
    );
```

- `tap_pending_color`: the firmware is waiting to see whether a second tap or
  higher branch will arrive
- `RGB_TAP_BRANCH_COLORS(...)`: the colors shown while the branch is committed
  and the RGB branch-confirm window is active
- `tap_committed_color`: a tap action just fired and does not already have a
  layer or pointing-mode state to show; inherited normal-tap repeats from a
  branch that omits `.tap` stay quiet
- `hold_active_color`: the `.hold` tier is pending, active, or committing
- `long_hold_active_color`: the `.long_hold` tier is active or committing
- `branch_confirm_mode`: chooses whether committed branches get the
  branch-color window; `KEY_FEEDBACK_BRANCH_CONFIRM_OFF` disables it and
  `KEY_FEEDBACK_BRANCH_CONFIRM_NON_BASE_TAPS` enables it for double-tap and
  higher authored branches
- `tap_commit_mode`: chooses whether tap commits pulse;
  `KEY_FEEDBACK_TAP_COMMIT_OFF` disables pulses and
  `KEY_FEEDBACK_TAP_COMMIT_NON_BASE_TAPS` enables them for double-tap and
  higher authored tap branches
- `locality`: chooses where the feedback paints with `RGB_BOTH_HALVES`,
  `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, or `RGB_KEYS_ONLY`
- key-behavior LED group semantics let named LED groups follow
  `KEY_FEEDBACK_GROUP_UNRESOLVED_TAP_BRANCH`,
  `KEY_FEEDBACK_GROUP_TAP_BRANCH_COMMITTED`,
  `KEY_FEEDBACK_GROUP_TAP_COMMITTED`, `KEY_FEEDBACK_GROUP_HOLD_ACTIVE`,
  `KEY_FEEDBACK_GROUP_LONG_HOLD_ACTIVE`, or `KEY_FEEDBACK_GROUP_ALL`

That behavior-specific feedback is the key-level part of the broader RGB
language. It gives you room to design compact keys without turning the source
into a pile of one-off feature code. The next section covers the layer,
pointing-mode, combo, LED-group, preview, and auto-mouse surfaces that use the
same authored RGB model.

## The RGB Model

RGB is used as feedback, not just decoration.

[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
lets you author the visible language of the board:

- layer colors, either across the board or only on keys used by that layer
- reusable LED groups for thumbs, rows, halves, clusters, or any physical
  group that makes sense on the board
- pointing-mode colors that can paint both halves, one fixed half, the half
  that triggered the mode, or only the exact triggering keys
- combo feedback so chords can light near the keys that made them
- key-behavior feedback for waiting, preview, tap-count, hold, and repeat
  states
- preview overlays that show a pending momentary-layer hold before the layer
  becomes active
- auto-mouse timeout feedback that fades as the temporary pointer layer is
  about to clear

`locality` is the RGB word for where a feedback surface paints. Depending on
the table, it can mean both halves, one fixed half, the half that owns the
triggering key or combo, or only the exact triggering keys with options such as
`RGB_BOTH_HALVES`, `RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, and
`RGB_KEYS_ONLY`.

My build also has one extra trackball LED at LED `56`, documented in the
[`rgb_config.c` LED map](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c#L53).
It is optional: boards without that physical LED are still compatible with this
userspace; they just will not show trackball-specific LED group accents.

In practice, the lights can answer a few simple questions while you use the
board: which layer is active, which trackball mode is live, which physical keys
created a combo, whether a key is waiting for another tap, which tap-count
branch won, whether a hold or repeat action has committed, and how close the
auto-mouse layer is to timing out.

The auto-mouse RGB timer is a good example of the design style. When the
trackball wakes the pointer layer, the LEDs can start from the authored pointer
layer look and then fade toward the board state that will remain after the
auto-mouse layer drops. In plain terms: the lights can show how much time is
left before the board returns to normal.

## Split Sync

A Charybdis has one controller per half, so runtime state cannot just live on
whichever half saw the key first. QMK's normal split settings cover the active
layer set and activity timer; this userspace adds custom split RPCs in
[`users/noah/config.h`](./users/noah/config.h#L42) and
[`runtime_sync.h`](./users/noah/lib/split/runtime_sync.h):

- `PUT_SPLIT_RUNTIME_BASE_SYNC`: auto-mouse RGB progress, active or locked
  pointing-mode IDs, key-local pointing-mode ownership, and preview-layer state
- `PUT_SPLIT_COMBO_FEEDBACK_SYNC`: combo underlay and overlay footprints, so
  `RGB_KEY_HALF` and `RGB_KEYS_ONLY` know which half or exact keys caused the
  combo
- `PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC` and
  `PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC`: key-feedback flash visibility, semantic
  state, broad owner groups, and tap-branch colors
- `PUT_VIA_KEYMAP_SYNC`: mirrored VIA dynamic-keymap writes

You can forget those packet names immediately. The point is that both halves
know the same layers, keys, combos, pointing modes, and feedback state, so the
board behaves and lights up like one device instead of two disconnected halves.

## Main Files

If you want to adapt the profile, start here:

| File | Use It For |
| --- | --- |
| [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | layers, combos, macros, custom keycodes, and `key_behaviors[]` |
| [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, LED groups, pointing-mode colors, combo feedback, key feedback, and auto-mouse fade |
| [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing defaults, auto-mouse settings, RGB feedback toggles, and pointer policy |
| [`users/noah/config.h`](./users/noah/config.h) | split transport, LED geometry, pointing-device hardware settings, and shared board-level QMK overrides |

Most profile work should stay in the first three files. The shared runtime
under [`users/noah/`](./users/noah/) exists so those authored files can stay
small and data-driven.

## Docs Map

Use the docs based on what you want to change:

- [`docs/KEYMAP-OVERVIEW.md`](./docs/KEYMAP-OVERVIEW.md): generated visual
  report of the authored profile
- [`docs/KEYMAP.md`](./docs/KEYMAP.md): prose notes for the current authored
  profile choices
- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): tap, hold,
  longer-hold, and multi-tap semantics
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): what each trackball mode
  does once active
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): RGB authoring model, render
  order, LED groups, and auto-mouse fade
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): maintainer guide for
  adding another pointing-device mode
- [`docs/KEY_RUNTIME.md`](./docs/KEY_RUNTIME.md): maintainer map of the
  handled-key runtime and ownership model
- [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md): how to override QMK
  hooks without dropping shared userspace behavior
- [`docs/tooling/PROFILE_STUDIO.md`](./docs/tooling/PROFILE_STUDIO.md):
  Profile Studio workflow
- [`docs/tooling/PROFILE_INTROSPECT.md`](./docs/tooling/PROFILE_INTROSPECT.md):
  generated profile docs workflow
- [`docs/tooling/VIA_TO_QMK.md`](./docs/tooling/VIA_TO_QMK.md): round-trip VIA
  exports back into source
- [`docs/architecture/README.md`](./docs/architecture/README.md): maintainer
  entry point for runtime ownership and source boundaries

## AI Workflow Note

I do use AI as part of the workflow around this repo.

The config is still hand-owned daily-driver firmware. Many hours have gone
into tuning the hardware, the layout, the runtime behavior, and the
documentation.

## A Little Show-Off Of My Build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

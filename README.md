# Noah's Charybdis Userspace

This repo is the shared userspace for my Charybdis 4x6.

It is intentionally Charybdis-specific. The trackball behavior, split sync,
auto-mouse layer, and RGB assumptions are built around this split trackball
board rather than stock QMK conventions.

This is still a personal configuration, but it is not meant to be a pile of
one-off hacks. The point is to keep the shared runtime centralized and
editable, so someone changing
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) or the
keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
can adjust the board without having to rework the runtime.

> **Firmware note:** This userspace is updated for QMK `0.32.5` and builds
> against my [`qmk-latest` firmware branch](https://github.com/NoahCLR/bastardkb-qmk/tree/qmk-latest)
> rather than the older `bkb-master`-based setup.
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
are what made this build possible. If you want to support the creator, buy the
hardware from
[BastardKB](https://bastardkb.com/) rather than from a knockoff seller.

## What This Repo Offers

- a readable, source-controlled Charybdis 4x6 userspace that still works well
  with VIA
- richer per-key behavior than a stock keymap: tap, hold, longer-hold, and
  multi-tap branches on one authored key
- trackball mode switching for dragscroll, pinch, zoom, arrows, volume, and
  brightness
- auto-mouse support that can surface a pointer layer when the ball moves
- RGB used as feedback for layers, pointing modes, and key-behavior state
- generated visual profile docs so you can understand the current board
  without reading raw source first

## Start Here

If you want to understand the current board layout quickly, start with
[`docs/KEYMAP-OVERVIEW.md`](./docs/KEYMAP-OVERVIEW.md). It is the fastest
overview of the current profile: layer images, combo badges, key-behavior
markers, reachable pointing modes, macro inventory, and the current authored
colors.

After that, the first source files worth reading are:

- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

That is the intended entry path for most readers: first see what the board does
in the overview, then inspect the three authored files that define how the
current profile is shaped.

## Current Profile Shape

The current profile keeps the board compact and layered rather than trying to
dedicate a physical key to every role:

- `LAYER_BASE` stays close to normal typing and carries the most-used custom
  thumb and home-row behavior
- `LAYER_NUM`, `LAYER_SYM`, `LAYER_NAV`, and `LAYER_POINTER` split number
  entry, symbols, navigation/system control, and pointer utilities into
  distinct surfaces
- a small combo set adds chorded `Tab`, pointer-side `CLICK_SPAM`, and
  right-side Cmd paths without turning the layout into a combo-heavy board
- the current profile leans macOS-first, especially in launcher, editing, and
  system shortcuts

For the current concrete profile choices in prose, see
[`docs/KEYMAP.md`](./docs/KEYMAP.md).

## Key Behavior

The richer custom tap, hold, longer-hold, and multi-tap behavior is authored
in `key_behaviors[]`. Plain keys without a row keep their normal QMK behavior.

This is the main feature that lets one physical key stay compact without being
limited to a single role. A key can stay simple, or it can branch across tap
count and hold depth without needing a separate bespoke feature for each case.

### Actions

An action in a `key_behaviors[]` row can be:

- a plain keycode such as `KC_MPLY` or `S(KC_1)`
- a hardcoded or VIA macro such as `MACRO_0` or `VIA_MACRO_6`
- a layer lock such as `LOCK_LAYER(layer)`
- a generated pointing-mode lock keycode such as `ARROW_MODE_LOCK`
- a supported QMK behavior keycode such as `OSM()` or `MT()`
- an owned momentary layer hold such as
  `PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))`
- a keymap-local custom keycode declared in
  [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)

In practice, that is what makes patterns like these possible:

- number-row symbols on hold
- keys that combine momentary layer access, layer locks, and higher-tap media
- navigation keys that cover character, word, and line movement on one surface
- pointing-mode keys that can lock, mute, or branch into another mode
- custom authored keycodes whose whole behavior comes from one row

For the shared interaction rules, see
[`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md).

### Timing

Default timing lives in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- built-in QMK dual-role timing through `TAPPING_TERM`
- custom key-behavior defaults through `CUSTOM_TAP_HOLD_TERM`,
  `CUSTOM_LONGER_HOLD_TERM`, `CUSTOM_MULTI_TAP_TERM`, and
  `CUSTOM_TAP_BRANCH_CONFIRM_TERM`

Timing can also be customized per key with `.tap_hold_term`,
`.longer_hold_term`, `.multi_tap_term`, and `.branch_confirm_term =
KEY_BEHAVIOR_TERM(ms)`.

### Hold Tiers

The custom key system supports four hold styles:

- `PRESS_AND_HOLD_UNTIL_RELEASE(...)`: activate at threshold and keep held
- `REPEAT_WHILE_HELD(action, hz)`: start at threshold, then repeat while held
- `TAP_AT_HOLD_THRESHOLD(...)`: fire once immediately at threshold
- `TAP_ON_RELEASE_AFTER_HOLD(...)`: qualify the hold, then fire once on release

That is what lets one key behave differently in different contexts without
forcing every special case into its own separate subsystem.

### Multi-Tap

Multi-tap is part of the same model, not a separate feature. A key can define
different behavior for the first tap, second tap, third tap, and so on.

That is why a key can keep its normal hold role while still exposing locks,
media, alternate taps, or branch actions on higher tap counts.

## Combos

Combos are authored directly in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c). They
are separate from the custom key-behavior engine: a combo is just a
simultaneous chord that emits one keycode or action.

At runtime, a combo still keeps one representative owner key for ownership and
release matching, but the userspace now also tracks the full physical combo
footprint for locality-sensitive features. That means combo-backed key
feedback can stay on the relevant key or half, and `RGB_KEY_HALF`
can broaden to both halves when a combo spans both sides of the board.
`RGB_KEYS_ONLY` can narrow the same PD ownership truth to the
exact triggering key footprint.

The runtime also remembers a complete physical combo while QMK's combo buffer
is waiting out `COMBO_TERM`, including the quick-tap path where a member has
already released before QMK emits the combo output. Combo-backed multi-tap
behaviors that complete inside their authored tap window therefore do not lose
that tap chain to QMK's output delay.

Combo outputs that match an authored `key_behaviors[]` keycode use the same
tap, hold, and multi-tap rules as the physical key. That includes release-time
tap settlement: a terminal `TAP_SENDS(...)` branch reached by tapping a combo
does not fire on the combo press itself; it commits when the combo output
release settles the tap branch.

That footprint tracking follows the live resolved keycodes QMK sees, so dynamic
keymap changes remain authoritative. It is not guessed from static comments or
hardcoded layout assumptions.

The current visual report marks combo input keys with badges in the layer
images and lists their outputs in the combo tables, so you do not have to scan
the raw combo arrays to understand where they live.

## Macros

This userspace intentionally keeps two macro surfaces:

- `VIA_MACRO_n` for 64 VIA/QMK dynamic macro slots backed by the enlarged
  RP2040 VIA macro buffer
- `MACRO_n` for hardcoded repo-owned custom macros

That split keeps editable VIA defaults and source-owned firmware behavior
separate while still letting both kinds of macros appear in layers, combos, and
`key_behaviors[]`.

## Pointing-Device Modes

Pointing-device modes are a core part of what makes this userspace feel like a
Charybdis userspace rather than a generic keyboard config.

A plain pointing-mode keycode works as a default momentary mode key. That same
key can also participate in `key_behaviors[]`, which means it can gain richer
tap, hold, longer-hold, and multi-tap behavior like the rest of the board.
If that authored tap path can branch into a different pointing mode, the
runtime defers the first mode until the hold threshold so two mode lifecycles
do not overlap while the tap count is still unresolved.
Committed double-tap and higher branches can also hold in a
branch-confirmation window before their tap, hold, long-hold, or PD-mode action
fires, so RGB feedback and the actual model use the same timing. The base
single-tap branch skips that confirmation window after its multi-tap wait.

The current runtime supports these mode families:

- `DRAGSCROLL`
- `PINCH_MODE`
- `ZOOM_MODE`
- `ARROW_MODE`
- `VOLUME_MODE`
- `BRIGHTNESS_MODE`

Those are runtime capabilities. The keymap decides where they live, which ones
stay as simple holds, which ones gain alternate taps, and which ones can lock.
When a mode is already locked, pressing that same runtime-handled mode key
clears the lock immediately; if you keep holding the key, the mode remains
active only as a momentary hold until release. Dedicated `*_LOCK` actions still
toggle locks explicitly.

For the raw mode behavior and pointer-layer policy, see
[`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md).

## Auto-Mouse

Auto-mouse can bring up a configured layer when the trackball moves and clear
it again after a configured timeout. The keymap chooses the target layer,
timeout, and related pointer behavior through the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

That keeps pointer access fast without forcing the pointer layer to be manually
held every time the ball is used.

## RGB

RGB is used as feedback, not decoration. The authored color configuration lives
in [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).

The runtime supports:

- per-layer colors
- per-layer LED group highlights, including one inherited-color group that can
  apply to every active layer
- optional per-mode RGB locality on both halves, a fixed half, the key half,
  or the exact triggering key footprint
- optional per-mode LED group highlights, including one inherited-color group
  that can apply to every pointing mode
- optional steady combo feedback that can sit above or below preview / PD
  depending on live ownership
- optional combo feedback LED group highlights
- optional auto-mouse countdown gradient feedback
- optional truthful key-behavior state feedback on both halves, a fixed half,
  only the key half, or only the key itself, with flashing held/repeat feedback
  starting its visible window from the key that activated it; broad modes follow
  the newest active feedback owner instead of merging offset flash phases
- optional tap-count branch-confirmation colors so committed double-tap and higher
  branches can show which branch won before action feedback takes over, while
  base single-tap candidates stay quiet and higher hold tiers replace older
  feedback once their behavior commits
- optional key-behavior feedback LED group highlights

With the default feedback stages enabled, RGB is not just there to look nice.
It tells you what layer is active, which pointing mode is live, which combos
are currently held, and when the key-behavior engine is waiting, committing a
tap, previewing, or actively holding.

For the full RGB authoring model, see
[`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md).

## Main Files To Change

If you want to change the current profile, start here:

| File | Use It For |
| --- | --- |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | physical layout, combos, keymap-local custom keycodes, `VIA_MACROS(MACRO)`, `HARDCODED_MACROS(MACRO)`, and the authored `key_behaviors[]` table |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing, auto-mouse settings, sniping, dragscroll DPI, and keymap-facing feature config |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pointing-mode colors, combo feedback, LED groups, and key-behavior feedback colors |

## Tooling

This repo ships with small maintenance tools:

- [`tools/via_to_qmk_layout.py`](./tools/via_to_qmk_layout.py): round-trip the
  VIA-owned parts of the layout back into source. Full workflow:
  [`docs/tooling/VIA_TO_QMK.md`](./docs/tooling/VIA_TO_QMK.md)
- [`tools/profile_introspect.py`](./tools/profile_introspect.py): regenerate
  the visual profile report and SVG previews. Full workflow:
  [`docs/tooling/PROFILE_INTROSPECT.md`](./docs/tooling/PROFILE_INTROSPECT.md)
- [`tools/charybdis-profile-studio/`](./tools/charybdis-profile-studio/):
  standalone VS Code extension for editing `keymap.c`, keymap `config.h`, and
  `rgb_config.c`
  directly. Full workflow:
  [`docs/tooling/PROFILE_STUDIO.md`](./docs/tooling/PROFILE_STUDIO.md)

Useful commands:

- `python3 tools/via_to_qmk_layout.py --print`
- `python3 tools/via_to_qmk_layout.py --write`
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_profile_introspection_checks.sh`

## Verification

For current-profile changes, treat the generated overview as part of the source
change. Regenerate it after editing `keymap.c`, the keymap `config.h`,
`users/noah/config.h`, `rgb_config.c`, or the shared pd-mode manifest:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
```

For firmware behavior changes, use the focused host runner for the subsystem
while iterating, then run the full host suite before compiling:

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

Docs-only changes can skip host tests and firmware compile when they do not
touch runtime source, authored profile inputs, build wiring, or generated
firmware inputs. Still run `git diff --check` before handing them back.

## Docs Map

Use the docs based on what you need:

- [`docs/KEYMAP-OVERVIEW.md`](./docs/KEYMAP-OVERVIEW.md): visual layer report
  with layer images, combo badges, key behaviors, reachable pointing modes,
  and macro inventory
- [`docs/tooling/PROFILE_INTROSPECT.md`](./docs/tooling/PROFILE_INTROSPECT.md):
  how the visual profile report is generated and verified
- [`docs/tooling/PROFILE_STUDIO.md`](./docs/tooling/PROFILE_STUDIO.md): local
  VS Code GUI for editing `keymap.c`, keymap `config.h`, and `rgb_config.c`
- [`docs/KEYMAP.md`](./docs/KEYMAP.md): the current concrete profile choices
  and how the layers are currently used
- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): the interaction
  semantics for tap, hold, longer-hold, and multi-tap behavior
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): what each pointing mode
  does once it is active
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): authored RGB colors, overlays,
  and LED group configuration
- [`docs/tooling/VIA_TO_QMK.md`](./docs/tooling/VIA_TO_QMK.md): VIA export
  round-trip workflow
- [`docs/KEY_RUNTIME.md`](./docs/KEY_RUNTIME.md): maintainer-facing handled-key
  runtime map
- [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md): how to override weak
  hooks without dropping shared behavior
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): how to add a new
  pointing-device mode safely
- [`docs/architecture/README.md`](./docs/architecture/README.md): maintainer
  entry point for runtime ownership, source boundaries, and change routing
- [`docs/architecture/change-guide.md`](./docs/architecture/change-guide.md):
  where to edit common runtime surfaces and which checks cover them
- [`docs/architecture/source-map.md`](./docs/architecture/source-map.md): source
  package inventory, generated-doc/tooling surfaces, and host runner map
- [`docs/architecture/runtime-flow.md`](./docs/architecture/runtime-flow.md):
  hook, key-runtime, pointing, RGB, split, macro, and VIA flow diagrams

## AI Workflow Note

I do use AI as part of the workflow around this repo.

That does not make this a throwaway generated config. This board is my daily
driver, and many hours have gone into tuning the hardware, the layout, the
runtime behavior.

## A Little Show-Off Of My Build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

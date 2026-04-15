# Noah's Charybdis Userspace

This is the shared userspace for my `noah` Charybdis 4x6 keymaps.

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
I do use AI as part of the workflow around this repo, but this is not a
throwaway generated config: this board is my daily driver, and many
hours have gone into tuning both the hardware and the firmware. Quentin's
design, and the many mods the community has built around this board, have made
that possible, and I am very thankful for that. If you want to support the
creator, buy the hardware from
[BastardKB](https://bastardkb.com/) rather than from a knockoff seller.

## What This Userspace Is For

This userspace is built around a small set of systems that make the board easy
to understand and easy to change:

- [`key_behaviors[]`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  in [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  is the main customization table: one authored row can give a key different
  tap, hold, and longer-hold actions at each tap count, plus per-key timing
  overrides. Actions can be plain keycodes, macros, layer locks, pointer-mode
  locks, supported QMK behavior keycodes like `OSM()` or `MT()`, owned
  momentary layer holds such as `PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))`,
  repeat holds such as `REPEAT_WHILE_HELD(KC_LEFT, 100)`, or keymap-local
  custom keycodes
- pointer modes are a core part of what makes this userspace different: the
  trackball can become dragscroll, pinch, zoom, arrows, volume, or brightness,
  with plain mode keycodes working as default momentary holds and
  `key_behaviors[]` able to layer richer tap, hold, multi-tap, or lock
  behavior on top
- `AUTO_MOUSE` brings up the pointer layer when the trackball moves and clears
  it again after the configured timeout
- RGB is functional feedback, not decoration.
  [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
  defines layer colors, pointer-mode colors, and LED group highlights. On top
  of that, two optional overlays, an auto-mouse countdown gradient and
  key-behavior engine feedback, can be independently toggled
- the userspace hooks into QMK through weak defaults in
  [`hooks.c`](./users/noah/hooks.c). If you add repo-specific hook overrides,
  call the matching `noah_*` helper to keep the shared behavior unless you are
  intentionally replacing it (see
  [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md))
- `split_runtime_sync` syncs active and locked pointing-device mode ids,
  auto-mouse progress, key-feedback flags, and preview-layer state from master
  to slave so both halves render consistently

The README is intentionally capability-focused. It explains what the shared
runtime supports and how the pieces fit together. If you want one concrete
authored profile built on top of those systems, start with
[`docs/KEYMAP.md`](./docs/KEYMAP.md) and
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c). Those
show the current concrete configuration of layers, combos, `key_behaviors[]`,
VIA macro defaults, hardcoded macros, and pointing-mode entry gestures.
If you want the maintainer-facing runtime map for the handled-key engine,
start with [`docs/KEY_RUNTIME.md`](./docs/KEY_RUNTIME.md).

There is also a small VIA bridge in
[`via layouts/via_to_qmk_layout.py`](<./via layouts/via_to_qmk_layout.py>).
If you want the browser config UI, start with [VIA](https://usevia.app/).
That script is useful when you want to experiment quickly in VIA without
giving up a readable, source-controlled
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c): it
converts VIA exports back into the authored tables this repo uses.
For the full round-trip workflow, see
[`docs/VIA_TO_QMK.md`](./docs/VIA_TO_QMK.md).

There is also an authored-profile introspector in
[`tools/profile_introspect.py`](./tools/profile_introspect.py). It reads the
authored keymap surfaces directly from source, derives the physical `LAYOUT()`
order from the live Charybdis `keyboard.json`, uses the repo's HSV hue wheel
for preview swatches, and generates:

- per-layer visual SVG previews in
  [`docs/generated/profile-introspection-assets/`](./docs/generated/profile-introspection-assets/)
  using the authored `layer_colors[]` config from
  [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- a rendered layer-map and inventory report in
  [`docs/generated/profile-introspection.md`](./docs/generated/profile-introspection.md)
  including the authored key-behavior feedback LED colors from
  [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c),
  visible color swatches, and only filled macro slots

Practical usage:

- `python3 'via layouts/via_to_qmk_layout.py' --print` previews rewritten
  `VIA_MACROS(MACRO)` and `keymaps[][]`
- `python3 'via layouts/via_to_qmk_layout.py' --write` rewrites those VIA-owned
  sections in
  [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- `python3 'via layouts/via_to_qmk_layout.py' --via-json path/to/export.json`
  uses a specific VIA export instead of choosing one from `via layouts/`
- `python3 tools/profile_introspect.py --write` regenerates the authored
  profile report under [`docs/generated/`](./docs/generated/) and the SVG
  assets under
  [`docs/generated/profile-introspection-assets/`](./docs/generated/profile-introspection-assets/)
- `python3 tools/profile_introspect.py --check` verifies those generated
  artifacts are current
- `python3 tools/profile_introspect.py --print-markdown` previews the rendered
  report without rewriting files

## Where To Change Things

If you want to adapt this userspace, these are the main files to touch first:

| File | What You Change There |
| --- | --- |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | physical layout, combos, keymap-local custom keycodes, `VIA_MACROS(MACRO)`, `HARDCODED_MACROS(MACRO)`, and the authored `key_behaviors[]` table |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pointer-mode colors, LED groups, auto-mouse gradient endpoints, and key-behavior feedback colors |
| [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | tap/hold timing, multi-tap timing, RGB overlay toggles (`RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`, `RGB_AUTOMOUSE_GRADIENT_ENABLE`), auto-mouse target layer and timeout, auto-sniping, dragscroll DPI, and other keymap-facing behavior |
| [`users/noah/noah_keymap_ids.h`](./users/noah/noah_keymap_ids.h) | shared layer ids, hardcoded macro keycodes, generated pd-mode and layer-lock keycode ranges, and the `NOAH_KEYMAP_SAFE_RANGE` boundary for keymap-local keycodes |
| [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](./users/noah/lib/pointing/defs/pd_mode_manifest.h) | shared pd-mode definitions: mode keycodes, generated lock keycodes, handlers, DPI metadata, and manifest traits |
| [`users/noah/lib/pointing/runtime/`](./users/noah/lib/pointing/runtime/) | shared pd-mode runtime machinery: registry materialization, lifecycle transitions, active/locked state, and runtime dispatch |
| [`users/noah/lib/pointing/policy/`](./users/noah/lib/pointing/policy/) | pointer-policy glue that keeps layer ownership aligned with pd-mode state |
| [`users/noah/lib/pointing/modes/`](./users/noah/lib/pointing/modes/) | mode-owned pointing behavior: motion transforms, key interception, shared mode helpers, and mode-specific lifecycle glue such as `PINCH_MODE` |
| [`users/noah/source_manifest.mk`](./users/noah/source_manifest.mk) | canonical userspace source inventory for firmware builds and host compile gates; add new shared translation units here |
| [`users/noah/config.h`](./users/noah/config.h) | split transport settings, RGB geometry, pointing-device polling, idle-noise suppression, sensor/report settings, local dragscroll tuning, and low-level QMK overrides |

In other words:

- if you want to change what a key does, start in [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- if you want to change how the board looks, start in [`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- if you want to change how the keyboard feels, start in the keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- if you want to add a layer, update the layer enum in the keymap [`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h); `LAYER_COUNT` is the sentinel last value and should stay last
- if you want to add a shared custom keycode surface, start in [`noah_keymap_ids.h`](./users/noah/noah_keymap_ids.h)
- if you want to add a shared pointing-device mode, start in [`pd_mode_manifest.h`](./users/noah/lib/pointing/defs/pd_mode_manifest.h) and the matching mode-owned files under [`users/noah/lib/pointing/modes/`](./users/noah/lib/pointing/modes/)
- if you add a new shared userspace `.c` file, wire it into [`users/noah/source_manifest.mk`](./users/noah/source_manifest.mk) in the same pass
- if you want to change board plumbing, start in [`users/noah/config.h`](./users/noah/config.h)

## Build Wiring And Boundaries

The shared userspace build surface is centered on
[`users/noah/rules.mk`](./users/noah/rules.mk) and
[`users/noah/source_manifest.mk`](./users/noah/source_manifest.mk).

- [`rules.mk`](./users/noah/rules.mk) pulls in the canonical source manifest
  and enables the current shared feature set (`VIA_ENABLE`, `COMBO_ENABLE`,
  `LTO_ENABLE`)
- [`source_manifest.mk`](./users/noah/source_manifest.mk) splits the userspace
  into `NOAH_COMMON_SOURCES`, `NOAH_POINTING_SOURCES`,
  `NOAH_AUTOMOUSE_SOURCES`, and `NOAH_RGB_KEYMAP_SOURCES`, so firmware builds
  and host compile gates share one source inventory
- [`keymap_materialize.h`](./users/noah/keymap_materialize.h) turns the
  declarative blocks in
  [`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
  into the runtime symbols consumed by the macro system, combo output checks,
  and key-behavior lookup

One important compile-gated boundary in this repo:

- runtime modules under [`users/noah/`](./users/noah/) must not include
  [`noah_keymap.h`](./users/noah/noah_keymap.h) directly
- keymap-owned translation units under
  [`keyboards/.../keymaps/noah/`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/)
  must not include [`noah_runtime.h`](./users/noah/noah_runtime.h) directly

That boundary, plus several internal-runtime sealing rules, is enforced by
[`sh tests/host/run_feature_gate_compile_tests.sh`](./tests/host/run_feature_gate_compile_tests.sh).

## Layer Model And VIA

This userspace does not require one fixed layer stack, but it does expect the
keymap to own a normal layer enum in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h), with
`LAYER_COUNT` as the last sentinel.

That enum is the source of truth for several runtime-owned systems:

- layer locks
- authored momentary-layer holds through `key_behaviors[]`
- RGB layer colors and layer LED groups
- the configured auto-mouse target layer
- VIA dynamic layer count

In other words, the runtime is layer-aware, but the keymap decides which layers
exist and what they are for.

If you change the layout in VIA and want to bring it back into source, use
[`via_to_qmk_layout.py`](<./via layouts/via_to_qmk_layout.py>) in
[`via layouts`](<./via layouts>). It round-trips VIA layer data, macro slots,
and supported custom keycodes back into the authored tables in
[`keymap.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

## Macros

This userspace uses two macro surfaces on purpose:

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

The hardcoded macro payload surface is richer than plain text. It can express:

- literal text
- tap chords such as `{KC_LGUI,KC_SPC}`
- explicit key down / key up events
- delays between steps

The same payload language is used for source-authored defaults, and the repo
can seed VIA's macro EEPROM defaults from those authored values on first init
and supported reset paths.

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

Keys with authored behavior rows are resolved per tap index. In other words, a
single press, double press, triple press, and so on can each have their own
independent behavior branch.

Each tap-count branch can define:

- its own tap action
- its own hold tier
- its own longer-hold tier
- its own hold style

So the engine can distinguish between:

- single tap through quintuple tap
- tap, hold, and longer-hold outcomes within each tap index
- hold styles that fire at different times

This is what makes patterns like these possible:

- number-row symbols on hold
- keys that combine momentary layer access, layer locks, and higher-tap media
- navigation keys that cover character, word, and line movement on one surface
- pointer-mode keys that can lock, mute, or branch into another mode
- custom authored keycodes whose entire behavior comes from one row

### Actions

An action in a `key_behaviors[]` row can be:

- a plain keycode (`KC_MPLY`, `S(KC_1)`)
- a hardcoded or VIA macro (`MACRO_0`, `VIA_MACRO_6`)
- a layer lock (`LOCK_LAYER(layer)`)
- a pointer-mode lock (`LOCK_PD_MODE(mode_keycode)`)
- a supported QMK behavior keycode such as `OSM()` or `MT()`
- an owned momentary layer hold such as
  `PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))`
- a keymap-local custom keycode declared in `keymap.c`

Raw QMK layer actions such as `TG()`, `TO()`, `TT()`, `OSL()`, and `LM()` are
intentionally not supported inside `key_behaviors[]`. This keeps layer changes
inside the userspace layer-ownership model instead of letting raw QMK actions
bypass it. Use `LOCK_LAYER(...)` for persistent layer changes. Use `LT()` when
the physical key itself should be a layer-tap key. If a custom behavior row
needs a momentary layer hold, use `PRESS_AND_HOLD_UNTIL_RELEASE(MO(layer))`.

### Timing

Default timing lives in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- built-in QMK dual-role timing through `TAPPING_TERM`
- custom key-behavior defaults through `CUSTOM_TAP_HOLD_TERM`,
  `CUSTOM_LONGER_HOLD_TERM`, and `CUSTOM_MULTI_TAP_TERM`

There is one important nuance: built-in QMK dual-role keys like `LT()` and
`MT()` still use `TAPPING_TERM`. Inside `key_behaviors[]`, an omitted
`.tap_hold_term` also falls back to `TAPPING_TERM` for `LT()` rows, but to
`CUSTOM_TAP_HOLD_TERM` for other custom rows.

Timing can also be customized per key. A `key_behaviors[]` row may set:

- `.tap_hold_term`
- `.longer_hold_term`
- `.multi_tap_term`

If one of those fields is omitted, C zero-initializes it. A value of `0` means
"use the default timing for this row."

### Hold Tiers

The custom keys support four different hold styles:

- `PRESS_AND_HOLD_UNTIL_RELEASE(...)`: activate at threshold and keep held
- `REPEAT_WHILE_HELD(action, hz)`: fire once at threshold, then keep tapping at
  the authored cadence while held
- `TAP_AT_HOLD_THRESHOLD(...)`: fire once immediately at threshold
- `TAP_ON_RELEASE_AFTER_HOLD(...)`: qualify the hold, then fire once on release

`REPEAT_WHILE_HELD(...)` currently supports authored frequencies in the range
`1..100 Hz`. The current maximum is `100` repeats per second.

That is what lets one key behave differently in different contexts without
inventing a separate timing system for each feature.

### Multi-Tap

Multi-tap is part of the same model, not a separate feature. A key can define
different behavior for the first tap, second tap, third tap, and so on.

That is why a key can keep its normal hold role while still exposing locks,
media, alternate taps, or branch actions on higher tap counts.

For the shared interaction model, see
[`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md). For current profile
examples, see [`docs/KEYMAP.md`](./docs/KEYMAP.md).

## Pointing-Device Modes

Pointing-device modes have a simple default surface and can also plug into the
same authored key-behavior engine as the rest of the board.

A plain pd-mode keycode in the keymap works as a default momentary mode key. If
that same keycode is given a `key_behaviors[]` row, it can also gain authored
tap, hold, longer-hold, and multi-tap behavior.

The current runtime supports these mode families:

- `DRAGSCROLL`: converts ball movement into scrolling
- `PINCH_MODE`: dragscroll with an owned real `Cmd` hold for
  [BetterMouse](https://better-mouse.com/)-backed pinch emulation on macOS
- `ZOOM_MODE`: converts vertical movement into `Cmd+-` and `Cmd+=`
- `ARROW_MODE`: converts ball movement into arrow keys and repurposes mouse
  buttons for editing
- `VOLUME_MODE`: converts vertical movement into volume control
- `BRIGHTNESS_MODE`: converts vertical movement into display brightness control

Those modes are runtime capabilities. The keymap chooses where they live, which
ones stay as simple default momentary keys, which ones get richer authored
behavior, and which gestures or taps branch into other actions.

Shared pd-mode rules in the current runtime:

- unlocked modes are exclusive while held; the newest active mode wins
- locked modes are exclusive; locking another mode clears the previous lock
- arrow mode repurposes mouse buttons for selection, copy, and paste
- per-mode pointer DPI overrides are optional and keymap-configured
- keymaps can combine momentary use, locked use, alternate taps, and
  higher-tap branches on the same mode key

For the raw mode behavior and pointer-layer policy, see
[`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md).

## Auto-Mouse

Auto-mouse can bring up a configured layer when the trackball moves and clear
it again after a configured timeout. The keymap chooses the target layer,
timeout, and any related pointer or navigation layers through the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h).

In this runtime, auto-mouse also cooperates with pointing-device modes and
layer-hold keys:

- non-arrow modes can keep the configured pointer layer anchored while active
- arrow mode prefers staying on the current typing or nav surface rather than
  forcing the pointer layer back underneath it
- split runtime sync mirrors the feedback state both halves need to render that
  behavior consistently

## RGB

RGB is used as feedback, not decoration. All visual configuration lives in
[`rgb_config.c`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
That file defines layer colors, pointer-mode colors, per-layer and per-mode LED
group highlights, auto-mouse gradient endpoints, and key-behavior feedback
colors. You can edit colors and LED groups there without touching any runtime
code.

The runtime supports:

- each layer can have a solid color
- each layer can highlight specific LEDs through LED groups
- each pointing-device mode can paint the right half a mode color
- each pointing-device mode can highlight specific LEDs through mode LED groups

On top of that, two optional overlays add dynamic feedback. Both can be
independently toggled in the keymap
[`config.h`](./keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):

- **Auto-mouse gradient** (`RGB_AUTOMOUSE_GRADIENT_ENABLE`): the configured
  auto-mouse layer uses its authored layer-rendered start state and fades
  toward the real post-timeout layer render by default, so you can see how
  much timeout remains before the pointer layer clears
- **Key-behavior feedback** (`RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`): the
  key-behavior engine projects its state into the RGB overlay on both halves:
  multi-tap pending, pending momentary-layer previews, hold pending, trigger
  pulses, and active held non-layer actions. Momentary layer holds use preview
  and active layer color instead of a trigger pulse or persistent hold overlay

Both overlays are purely additive. With both disabled, `rgb_config.c` still
provides full layer and pointer-mode color feedback. With both enabled, the
render order is: composed layer colors plus layer LED groups, auto-mouse fade
on that layer stage when active, preview-layer overlay, pointer-mode color,
mode LED groups, key-behavior overlay.

The master half computes the feedback state and syncs what the slave needs
through `split_runtime_sync`, so both halves render consistently.

For the full RGB authoring model, render order, and how to change feedback
colors, see [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md).

## If You Want To Go Deeper

The main implementation lives under [`users/noah/`](./users/noah/), but most
customization does not need low-level changes.

These docs are the next place to look:

- [`docs/KEY_RUNTIME.md`](./docs/KEY_RUNTIME.md): maintainer-facing handled-key
  runtime map, test surfaces, and tracing/debug notes
- [`docs/INTERACTION_MODEL.md`](./docs/INTERACTION_MODEL.md): tap, hold, and
  multi-tap engine semantics
- [`docs/KEYMAP.md`](./docs/KEYMAP.md): current concrete profile choices,
  representative keys, and timing values
- [`docs/POINTER_MODES.md`](./docs/POINTER_MODES.md): pointer-layer policy and
  raw trackball mode behavior
- [`docs/RGB_CONFIG.md`](./docs/RGB_CONFIG.md): RGB colors, key-behavior
  feedback, LED groups, and auto-mouse gradient configuration
- [`docs/VIA_TO_QMK.md`](./docs/VIA_TO_QMK.md): how the VIA export bridge
  rewrites `VIA_MACROS(MACRO)` and `keymaps[][]` back into `keymap.c`
- [`docs/HOOK_OVERRIDES.md`](./docs/HOOK_OVERRIDES.md): how the weak-hook
  model works and how to override QMK hooks without silently dropping shared
  behavior
- [`docs/ADDING_PD_MODE.md`](./docs/ADDING_PD_MODE.md): how to add a new
  pointing-device mode safely

## A Little Show-Off Of My Build

<div align="center">
<video src="https://github.com/user-attachments/assets/fb5749e2-6f30-44de-99d7-9bd47f94659a" controls></video>
</div>

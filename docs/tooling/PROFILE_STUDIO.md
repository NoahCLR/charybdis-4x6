# Charybdis Profile Studio

Charybdis Profile Studio is the repo-local VS Code extension under
[`tools/charybdis-profile-studio/`](../../tools/charybdis-profile-studio/).
It edits the authored profile visually while keeping the C source files as the
only source of truth.

For the shorter user-facing guide and screenshot links, see
[`tools/charybdis-profile-studio/README.md`](../../tools/charybdis-profile-studio/README.md).

## Source Ownership

There is no sidecar profile format. The Studio parses existing C authoring
blocks, renders a VS Code webview, and applies narrow patches back to those
same blocks.

The extension writes the selected profile under
`keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`. For the current `noah`
profile, the editable files are:

| File | Studio-owned edit surfaces |
| --- | --- |
| [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum entries and profile defaults |
| [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | layer slots, combos, VIA macros, and simple `key_behaviors[]` rows |
| [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pointing-mode colors, LED groups, auto-mouse fade, combo feedback, and key-behavior feedback |

For complex behavior rows, direct source editing is still expected after using
the Studio as a starter.

## Profiles And New Keymaps

The header profile picker chooses the active keymap folder. Write actions carry
that profile id back to the extension host, so a stale write is rejected if the
profile changed before the patch applies.

`New profile` creates a Charybdis 4x6 keymap from the starter templates under
[`tools/charybdis-profile-studio/templates/charybdis-4x6/`](../../tools/charybdis-profile-studio/templates/charybdis-4x6/).
The generated folder contains:

| File | Starter role |
| --- | --- |
| `rules.mk` | sets `USER_NAME := noah` so QMK loads the shared runtime |
| `config.h` | seeds compile-ready layer, timing, pointer, and RGB defaults |
| `keymap.c` | starts fresh with empty macro payloads, no active combos, no active key behaviors, a plain base layer, and transparent support layers |
| `rgb_config.c` | seeds compile-ready layer, pointing-mode, combo, key-feedback, and LED-group tables |

The create flow also appends the profile to `qmk.json`. Build a generated
profile with:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km <name>
```

Generic profile validation can target any generated profile directory:

```sh
sh tests/host/run_real_profile_validation_tests.sh keyboards/bastardkb/charybdis/4x6/keymaps/<name>
```

The profile controls also support:

- `Clone`: copies the active profile folder to a new keymap and registers it in
  `qmk.json`
- `Rename`: moves the active non-`noah` profile folder and updates its
  `qmk.json` build target
- `Delete`: removes the active non-`noah` profile folder and removes its
  `qmk.json` build target

The default generated overview still describes the current `noah` profile:
[`docs/KEYMAP-OVERVIEW.md`](../KEYMAP-OVERVIEW.md), with prose context in
[`docs/KEYMAP.md`](../KEYMAP.md). Other profiles can generate their own overview
with:

```sh
python3 tools/profile_introspect.py --keymap <name> --write
python3 tools/profile_introspect.py --keymap <name> --check
```

Those profile-specific outputs default to
`docs/profiles/<name>/KEYMAP-OVERVIEW.md` and
`docs/media/profiles/<name>/profile-introspection/`.

The header Profile overview row has a `Create overview doc` button that runs
the same `profile_introspect.py --write` action for the active profile. It
warns before generation when Studio has staged layout/layer edits or dirty local
forms, because the overview is generated from the source files currently on
disk.
The Firmware row's `Compile left + right` button builds
`bastardkb_charybdis_4x6_<name>_left.uf2` with `FORCE_MASTER=yes` and
`bastardkb_charybdis_4x6_<name>_right.uf2` with `FORCE_SLAVE=yes`. It opens the
Charybdis Profile Studio output pane and streams the QMK output while each side
builds.

## Editing Model

The UI is split into Layout, Macros, RGB, and Defaults work areas. Most edits
are local drafts until an apply action writes them to source.

- Layout-slot edits are staged and written to `keymap.c` only when a layout
  apply action is pressed.
- The Layout board's apply button appears on every layer whenever any layer has
  staged layout edits, and writes all staged layout edits in one pass.
- Layer add/delete is a separate staged operation written only by
  `Apply layer changes`.
- The header `Apply all` button appears when staged layout or layer-structure
  changes exist, then writes those staged changes together.
- New-layer key edits are included in the staged layer payload, so they are
  written with the new layer.
- Compile and profile-overview generation warn before running when Studio has
  staged or dirty edits. The warning lists staged layout/layer changes that
  `Apply all` can write and local form edits that still need their own
  card-level Apply action.
- Switching between Layout, Macros, RGB, and Defaults preserves dirty form
  drafts in the previous view. View tabs show an orange dot while that view
  still has unapplied local edits.
- Local Studio edits support normal undo/redo before they are written:
  `Cmd+Z` / `Ctrl+Z` undo, and `Cmd+Shift+Z`, `Ctrl+Shift+Z`, or `Ctrl+Y` redo.
- Reload reparses `keymap.c`, `config.h`, and `rgb_config.c` from disk and
  discards uncommitted Studio edits.

Reload discards dirty fields, staged layout edits, staged layer changes, combo
input picking, RGB group selections, reusable LED group drafts, picker state,
and undo/redo history.

## Current Edit Surfaces

| Area | What it edits |
| --- | --- |
| Layout | `keymaps[][]` layer keycode slots through a physical SVG board based on profile introspection geometry |
| Layers | staged layer add/delete changes, including `config.h` enum entries, transparent `keymaps[][]` blocks, and default `layer_colors[]` rows |
| Combos | appended `COMBOS(COMBO)` rows, including rows built from selected physical keys |
| Behaviors | appended simple `key_behaviors[]` rows and feedback settings for authored behavior rows |
| Macros | `VIA_MACROS(MACRO)` through a 64-slot macro builder |
| Defaults | `config.h` defaults not already owned by layer structure or RGB color authoring flows |
| RGB colors | `layer_colors[]`, `pd_mode_colors[]`, auto-mouse fade, combo feedback, and key-behavior feedback |
| RGB groups | reusable `RGB_LED_GROUP_*` definitions and stage-specific LED group tables |

Defaults are grouped by the behavior they affect: key timing, normal pointer
speed, pointing-mode speeds, sniping, auto-mouse, base lighting, and lighting
feedback.

## Layout Area

Use the Layout area for physical key and layer work.

Supported board interactions:

- click a key to select it
- double-click a key to open the keycode picker and stage a replacement
- drag one key onto another to stage a slot swap
- copy/paste the selected keycode onto another selected slot
- use the selected-key sidecar to stage keycode edits into the same pending
  layout set

Single layout slots accept one keycode expression. Comma-separated clipboard
text such as `KC_L, KC_K, KC_J` is rejected, while nested QMK expressions such
as `LT(LAYER_NAV, KC_F)` remain valid. Rejection messages appear at the bottom
of the Layout board beside the apply area.

Layer add/delete rules:

- the `+` button opens an inline new-layer form
- the staged layer starts fully transparent
- the `-` button stages deletion of the active non-base layer
- deletion fails if references to the layer remain after the owned enum,
  keymap block, layer color, and layer LED group rows are removed

The selected-key editor shows the authored `key_behaviors[]` row for the active
key when one exists. It also follows keys that are later changed to an existing
behavior-owned keycode.

## Combos And Layer Overview

The Layout area includes combo authoring below the selected-key editor.

- `Pick input keys on layout` lets the active layer board choose multiple
  physical keys as new combo inputs.
- The combo input field also exposes the multi-key picker before appending a
  `COMBOS(COMBO)` row.
- `Edit combo` loads an existing combo's output and inputs into the combo
  builder.
- When the exact existing combo inputs are selected on the layout, the combo
  builder also fills in the current output automatically.
- Layer combo rows show behavior rows triggered by the combo output keycode.
- `Edit behavior` or `Create behavior` loads the combo output into the same
  behavior editor used for selected physical keys.

The layer overview lists macros directly placed on the layer or reachable
through visible behavior and combo output paths.

## Macros Area

The Macros area edits `VIA_MACROS(MACRO)` in `keymap.c`.

It supports:

- selecting any `VIA_MACRO_0` through `VIA_MACRO_63`
- editing the raw payload
- inserting text, tap, chord, key-down, key-up, and delay steps
- recording live keydown/keyup events with optional elapsed-time delays
- inspecting the parsed command preview before writing the slot

Live recording edits only the selected macro draft until `Apply macro` is
pressed. Compact recording folds matching down/up pairs into text, taps, and
chords where possible; exact recording keeps explicit `{+KC_*}` and `{-KC_*}`
events.

## Key Picker

Editable key fields use a VIA-style picker.

Normal keys can be entered as user-facing labels such as `A`, `Enter`,
`Space`, the classic transparent token `_______`, or modifier chords such as
`Shift+\`` and `Alt+Cmd+Esc`. Advanced QMK expressions still pass through when
needed.

The picker includes:

- an SVG-based full-size keyboard tab
- a shared search box
- Keyboard and All QMK search across the full catalog
- category tabs for narrowed results
- symbol, navigation, numpad, and more-keys menus
- layer, pointing-mode, macro, custom, and QMK-sourced keycode sections
- modifier buttons for chorded keys
- multi-key selection where the field expects a comma-separated key list
- `LT(layer, key)` building by selecting the layer target first and then the
  tap key

Search accepts both raw QMK tokens like `KC_X` and user-facing labels like
`X`. When available, QMK sections are read from sibling `bastardkb-qmk`
keycode metadata, including US extra aliases.

Selections remain pending until OK. Read-only controls are styled separately
from editable controls.

## Defaults Area

The Defaults area writes corresponding `config.h` default macros directly.
Behavior timing override fields show resolved default milliseconds as
placeholder text when the authored row leaves the override empty.

Defaults sections are behavior-oriented instead of file-section mirrors:

- normal pointer speed is separate from pointing-mode speed overrides
- sniping keeps its DPI ladder and auto-trigger together
- base lighting includes the inactivity timeout
- lighting feedback includes feedback refresh cadence

Per-mode DPI override fields label `0` as the normal pointer DPI fallback
instead of presenting it as a literal zero-DPI mode.

Defaults controls have field-specific tooltips that describe the firmware
behavior they influence and the practical effect of changing the value.

Numeric-only fields such as HSV hue/saturation channels, timing overrides, and
repeat-Hz values are validated inline before the Studio sends a write request.
The extension validates the request again before patching the backing C file.

## RGB Area

The RGB area edits the visible language in `rgb_config.c`.

It covers:

- layer colors
- pointing-mode colors
- reusable `RGB_LED_GROUP_*` definitions near the LED map
- layer, pointing-mode, combo, and key-behavior LED group tables
- auto-mouse fade destination color and fade mode
- combo feedback color and locality
- key-behavior feedback colors, tap-count branch-confirm colors,
  branch-confirm mode, tap commit mode, and locality

All major panels are collapsible. The RGB page keeps a panel for every
authored section in `rgb_config.c`, even when a table currently has no active
rows.

Nested RGB sections such as layer colors, pointing-mode colors, auto-mouse
fade, combo feedback, and key-behavior tap-count branch colors are collapsible.
Named tap-count branch sections inside the behavior editor are collapsible too.
Repeat Hz is shown only when the selected hold-tier helper is
`REPEAT_WHILE_HELD`.

LED group tables are nested under their owning RGB section:

- layer groups under Layer Colors
- pointing-mode groups under Pointing-mode Colors
- combo groups under Combo Feedback
- key-behavior groups under Key Behavior Feedback

The reusable LED group panel is global to the RGB page because
`RGB_LED_GROUP_*` definitions can be referenced by any stage-specific LED group
table.

The LED group builder previews already-defined groups for the currently
selected table while keeping the pending new-row selection separate. It can
append rows from either a reusable `RGB_LED_GROUP_*` definition or a one-off
inline `RGB_LED_GROUP(...)` selection. Layer and pointing-mode groups include
all-target owner choices.

## RGB Color Rules

The RGB page includes compact color-picker controls for editable colors and
dropdowns for fields with known option sets. The picker updates HSV fields and
the dedicated swatch column in collapsed section summaries; applying a card
still writes the same `HSV(...)` expressions back to `rgb_config.c`.

Layer color summaries follow the firmware pass-through rule for
`HSV(0, 0, 0)`: the base layer preview uses the configured default RGB Matrix
color, and higher layers are marked as pass-through instead of previewing
literal black.

LED group summaries follow the group inheritance rule for `HSV(0, 0, 0)`.
Layer, pointing-mode, combo, and key-behavior groups inherit their active stage
color unless the row uses a nonzero HSV override. This includes
`KEY_FEEDBACK_GROUP_ALL`.

Most visible controls, tables, collapsed summaries, color previews, and SVG
keys expose hover tooltips that describe what the field edits or displays. In
the Layout area, the board-level help icon carries the general edit gestures;
hovering a physical key focuses on that key's `key_behaviors[]` row.

## Native Workspace Use

Open this repo folder in VS Code, then run the VS Code task
`Install Profile Studio Extension` and reload VS Code. The repo includes that
task in `.vscode/tasks.json`; a separate multi-root workspace file is not
required.

The task symlinks the extension into VS Code's normal user extension directory,
so it runs in the current workspace instead of an Extension Development Host.

After reload, open the Studio with one of these entry points:

- the `$(keyboard) Profile Studio` status bar item
- `Charybdis: Open Profile Studio` from the command palette
- the editor title action when `keymap.c`, `config.h`, or `rgb_config.c` is
  open

You can also install it from a shell, from the repo root:

```sh
cd tools/charybdis-profile-studio
npm run install:local
```

## Development Run

If you open the extension folder by itself, its local launch config supports
the F5 extension-development flow.

Or launch the development host directly:

```sh
cd /path/to/charybdis-4x6
code --extensionDevelopmentPath="$PWD/tools/charybdis-profile-studio" "$PWD"
```

Then run `Charybdis: Open Profile Studio` from the command palette.

## Screenshot Capture

To regenerate the Studio screenshots used by the repo docs:

```sh
cd tools/charybdis-profile-studio
npm run screenshots
```

The script renders the real Studio webview model in headless Chrome and writes
the Layout, Macros, RGB, and Defaults PNGs to
`docs/media/profile-studio/`. The default capture width is a wide desktop
viewport so the Charybdis layout is not cropped.

Override the width with:

```sh
npm run screenshots -- --width 1600
```

Set `CHROME_BIN=/path/to/chrome` if Chrome or Chromium is not in a standard
location.

## Verification After Edits

To check the extension after Studio documentation or extension changes:

```sh
cd tools/charybdis-profile-studio
npm run check
```

That runs a JavaScript syntax check and verifies that the Studio parser can
associate every current `key_behaviors[]` row with the active layout or combo
output through canonical key-expression matching.

Profile Studio edits authored firmware inputs. After using it to change source,
run the same checks as direct edits to the touched surfaces:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
sh tests/host/run_profile_introspection_checks.sh
sh tests/host/run_all_profile_validation_tests.sh
sh tests/host/run_all_host_tests.sh
sh tests/host/run_all_profile_compile_tests.sh
```

For RGB-only edits, the focused RGB validation and render tests are the useful
inner loop before the full suite.

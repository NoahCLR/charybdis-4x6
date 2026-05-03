# Charybdis Profile Studio

Charybdis Profile Studio is the repo-local VS Code extension under
[`tools/charybdis-profile-studio/`](../../tools/charybdis-profile-studio/).

It is intentionally standalone: the authored source files remain the source of
truth, and the extension writes only these files:

- [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

There is no sidecar profile format. The extension parses the existing C
authoring blocks, renders a VS Code webview, and applies narrow source patches
back to the same blocks.

## Current Edit Surfaces

- `keymaps[][]` layer keycode slots through a physical SVG board based on the
  profile introspection geometry
- staged layer add/delete changes; applying them updates the layer enum in
  `config.h`, transparent `keymaps[][]` blocks in `keymap.c`, and random
  default `layer_colors[]` rows in `rgb_config.c`
- `layer_colors[]`
- `pd_mode_colors[]`
- RGB LED group tables, including appending new inline `RGB_LED_GROUP(...)`
  rows by selecting physical LEDs on the RGB layout
- the LED group builder previews already-defined groups for the currently
  selected table while keeping the pending new-row selection separate
- a key-behavior LED group all-feedback mode that writes
  `KEY_FEEDBACK_GROUP_ALL` and lets firmware resolve the active semantic's
  configured feedback color at render time; specific feedback group rows
  override that all-feedback base, and selected LEDs preview this mode as equal
  vertical bands including every configured tap-count branch-confirm color
- auto-mouse fade destination color and fade mode
- combo feedback color/locality
- key-behavior feedback colors, tap-count branch-confirm colors, tap commit mode, and
  locality
- `VIA_MACROS(MACRO)` through a dedicated macro builder with a 64-slot browser,
  raw payload editor, step insertion controls, live key-event recording, and a
  parsed payload preview
- appended `COMBOS(COMBO)` rows, including from selected physical keys on the
  active layout
- appended simple `key_behaviors[]` rows

The UI is split into Layout, Macros, and RGB work areas. The Layout
panel embeds selected-key behavior editing: if a key on the active layer has an
authored `key_behaviors[]` row, the behavior appears there, including when a key
is later changed to an existing behavior-owned keycode.
On the Layout board, clicking a key selects it, double-clicking opens the
keycode picker and stages the picked keycode for that key, dragging one key onto
another stages a slot swap, and copy/paste stages the selected keycode on
another selected slot. The selected-key sidecar stages keycode edits into that
same pending layout set.
Local Studio edits support normal undo/redo shortcuts before they are written:
`Cmd+Z` / `Ctrl+Z` undo, and `Cmd+Shift+Z`, `Ctrl+Shift+Z`, or `Ctrl+Y` redo.
Single layout slots only accept one keycode expression: comma-separated
clipboard text such as `KC_L, KC_K, KC_J` is rejected, while nested QMK
expressions such as `LT(LAYER_NAV, KC_F)` remain valid. Layout-slot rejection
messages appear at the bottom of the Layout board beside the layout apply area.
Staged layout edits are written to `keymap.c` only when an apply action is
pressed. The Layout board's apply button appears on every layer whenever any
layer has staged layout edits, and writes all staged layout edits in one pass.
Layer add/delete is a separate staged operation: the `+` button opens an inline
new-layer form, the staged layer starts fully transparent, the `-` button
stages deletion of the active non-base layer, and `Apply layer changes` is the
only action that writes those structural changes to source. Deletes fail if
references to the layer remain in the authored files after the owned enum,
keymap block, layer color, and layer LED group rows are removed.
The header `Apply all` button appears when staged layout or layer-structure
changes exist and writes those staged changes together. New-layer key edits are
included in the staged layer payload, so they are written with the new layer
instead of requiring a separate layout apply.
The Layout page also has a combo sidecar below the selected-key editor. Its
`Pick input keys on layout` action lets the active layer board choose multiple
physical keys as the new combo inputs, and the input field also exposes the
multi-key picker before appending a `COMBOS(COMBO)` row.
Layer combo rows also show behavior rows triggered by the combo output keycode,
so combo-driven key behaviors are visible from the active layer view.
The layer overview also lists macros directly placed on the layer or reachable
through visible behavior and combo output paths. The Macros page is dedicated to
VIA macro authoring: select any `VIA_MACRO_0` through `VIA_MACRO_63`, edit its
raw payload, insert text/tap/chord/key-down/key-up/delay steps, record live
keydown/keyup events with optional elapsed-time delays, and inspect the parsed
command preview before writing the slot back to `keymap.c`. Live recording edits
only the selected macro draft until `Apply macro` is pressed. Compact recording
folds matching down/up pairs into text, taps, and chords where possible; exact
recording keeps explicit `{+KC_*}` and `{-KC_*}` events.

Normal keys can be entered as user-facing labels such as `A`, `Enter`, `Space`,
the classic transparent token `_______`, or modifier chords such as
`Shift+\`` and `Alt+Cmd+Esc`; advanced QMK expressions still pass through when
needed. Key fields also have a VIA-style picker with an SVG-based full-size keyboard tab,
a shared search box where Keyboard and All QMK search the full catalog while
category tabs narrow the results, symbol/navigation/numpad/more-keys menus,
layer, pointing-mode, macro, custom, and QMK-sourced keycode sections. Search
accepts both raw QMK tokens like `KC_X` and user-facing labels like `X`. The QMK
sections are read from the sibling `bastardkb-qmk` keycode metadata, including
US extra aliases, when available.
The picker keeps selections pending until OK, supports modifier buttons
for chorded keys, supports multi-key selection where the field expects a
comma-separated key list, and builds `LT(layer, key)` values by selecting the
layer target first and then selecting the tap key. Read-only controls are styled
separately from editable controls.
Behavior timing override fields show the resolved default milliseconds in their
placeholder text when the authored row leaves the override empty.
Numeric-only fields such as HSV hue/saturation channels, timing overrides, and
repeat-Hz values are validated inline before the studio sends a write request;
the extension validates the request again before patching the backing `.c`
file.
The header Reload button reloads `keymap.c`, `config.h`, and `rgb_config.c` from disk and
discards uncommitted Studio edits, including dirty fields, staged layout edits,
combo input picking, RGB group selections, picker state, and undo/redo history.

All major panels are collapsible. The RGB page keeps a panel for every authored
section in `rgb_config.c`, even when a table currently has no active rows.
Nested RGB submenus such as layer colors, pointing-mode colors, auto-mouse
fade, combo feedback, and key-behavior tap-count branch colors are collapsible too.
LED group tables are nested under their owning RGB section: layer groups under
Layer Colors, pointing-mode groups under Pointing-mode Colors, combo groups
under Combo Feedback, and key-behavior groups under Key Behavior Feedback.
Named tap-count branch sections inside the behavior editor are collapsible; repeat Hz
is shown only when the selected hold-tier helper is `REPEAT_WHILE_HELD`.

The RGB page includes compact color-picker controls for editable colors and
dropdowns for fields with known option sets. The picker updates the HSV fields
and the dedicated swatch column in collapsed section summaries; applying a card
still writes the same `HSV(...)` expressions back to `rgb_config.c`. Layer
color summaries follow the firmware pass-through rule for `HSV(0, 0, 0)`: the
base layer preview uses the configured default RGB Matrix color, and higher
layers are marked as pass-through instead of previewing literal black.

Most visible controls, tables, collapsed summaries, color previews, and SVG
keys expose hover tooltips that describe what the field edits or displays.

For complex behavior rows, direct source editing is still expected after using
the studio as a starter.

## Native Workspace Use

From the existing Charybdis workspace, run the VS Code task
`Install Profile Studio Extension`, then reload VS Code. The extension is
symlinked into VS Code's normal user extension directory, so it runs in the
current workspace instead of an Extension Development Host.

After reload, use the `$(keyboard) Profile Studio` status bar item or run
`Charybdis: Open Profile Studio` from the command palette.
The command is also contributed to the editor title when `keymap.c` or
`rgb_config.c` is open.

You can also install it from a shell:

```sh
cd /Users/noah/dev/charybdis/charybdis-4x6/tools/charybdis-profile-studio
npm run install:local
```

## Development Run

If you open the extension folder by itself, its local launch config supports
the F5 extension-development flow.

Or launch the development host directly:

```sh
code --extensionDevelopmentPath=/Users/noah/dev/charybdis/charybdis-4x6/tools/charybdis-profile-studio /Users/noah/dev/charybdis/charybdis.code-workspace
```

Then run `Charybdis: Open Profile Studio` from the command palette.

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
sh tests/host/run_real_profile_validation_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

For RGB-only edits, the focused RGB validation and render tests are the useful
inner loop before the full suite.

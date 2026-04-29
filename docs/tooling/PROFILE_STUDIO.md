# Charybdis Profile Studio

Charybdis Profile Studio is the repo-local VS Code extension under
[`tools/charybdis-profile-studio/`](../../tools/charybdis-profile-studio/).

It is intentionally standalone: the authored `.c` files remain the source of
truth, and the extension writes only these files:

- [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

There is no sidecar profile format. The extension parses the existing C
authoring blocks, renders a VS Code webview, and applies narrow source patches
back to the same blocks.

## Current Edit Surfaces

- `keymaps[][]` layer keycode slots through a physical SVG board based on the
  profile introspection geometry
- `layer_colors[]`
- `pd_mode_colors[]`
- RGB LED group tables, including appending new inline `RGB_LED_GROUP(...)`
  rows by selecting physical LEDs on the RGB layout
- auto-mouse fade destination color and fade mode
- combo feedback color/locality
- key-behavior feedback colors, tap-branch colors, tap commit mode, and
  locality
- `VIA_MACROS(MACRO)`
- appended `COMBOS(COMBO)` rows
- appended simple `key_behaviors[]` rows

The UI is split into Layout, Macros & combos, and RGB work areas. The Layout
page owns layer-filtered key behavior editing: if a key on the active layer has
an authored `key_behaviors[]` row, the behavior appears there, including when a
key is later changed to an existing behavior-owned keycode.
Layer combo rows also show behavior rows triggered by the combo output keycode,
so combo-driven key behaviors are visible from the active layer view.

Normal keys can be entered as user-facing labels such as `A`, `Enter`, `Space`,
the classic transparent token `_______`, or modifier chords such as
`Shift+\`` and `Alt+Cmd+Esc`; advanced QMK expressions still pass through when
needed. Read-only controls are styled separately from editable controls.
Behavior timing override fields show the resolved default milliseconds in their
placeholder text when the authored row leaves the override empty.

All major panels are collapsible. The RGB page keeps a panel for every authored
section in `rgb_config.c`, even when a table currently has no active rows.
Nested RGB submenus such as layer colors, pointing-mode colors, auto-mouse
fade, combo feedback, and key-behavior tap branches are collapsible too.
Named tap branch sections inside the behavior editor are collapsible; repeat Hz
is shown only when the selected hold-tier helper is `REPEAT_WHILE_HELD`.

The RGB page includes compact color-picker controls for editable colors and
dropdowns for fields with known option sets. The picker updates the HSV fields
and the dedicated swatch column in collapsed section summaries; applying a card
still writes the same `HSV(...)` expressions back to `rgb_config.c`.

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

Profile Studio edits authored firmware inputs. After using it to change source,
run the same checks as direct edits to the touched surfaces:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
sh tests/host/run_real_profile_validation_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

For RGB-only edits, the focused RGB validation and render tests are the useful
inner loop before the full suite.

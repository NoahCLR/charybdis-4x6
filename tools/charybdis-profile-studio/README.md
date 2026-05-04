# Charybdis Profile Studio

Charybdis Profile Studio is a standalone VS Code extension for editing this
repo's authored configuration surfaces directly:

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`

Those authored source files remain the source of truth. The extension does not
use a sidecar profile database and does not write generated docs or firmware
inputs outside those profile files.

## Current Features

- separate Layout, Macros, and RGB work areas
- visual Charybdis 4x6 SVG board using the same physical geometry as the
  profile introspection previews
- layer previews use `layer_colors[]`, including the base-layer default RGB
  fallback used by the introspection report
- click a key, edit it as a normal key label such as `A`, `Enter`, `Space`, or
  `_______`, and stage the matching `LAYOUT()` slot edit
- double-click a layout key to stage a picker-selected keycode, stage drag/drop
  slot swaps, and stage copy/paste or sidecar keycode edits before applying
  them to `keymap.c`
- undo or redo local Studio edits before writing them, using normal
  `Cmd+Z` / `Ctrl+Z`, `Cmd+Shift+Z`, `Ctrl+Shift+Z`, or `Ctrl+Y` shortcuts
- reject comma-separated clipboard lists and random text when editing one layout
  slot, while still allowing nested QMK expressions such as `LT(layer, key)`;
  layout-slot rejection messages appear at the bottom of the Layout board
- use a VIA-style picker for key fields, with an SVG-based full-size keyboard tab,
  a shared search box where Keyboard and All QMK search the full catalog while
  category tabs narrow the results, symbol/navigation/numpad/more-keys menus,
  layer/pointing-mode/macro/custom sections, and QMK-sourced keycode menus from
  the sibling QMK keycode metadata, including US extra aliases; search accepts
  both raw QMK tokens like `KC_X` and user-facing labels like `X`
- choose modifier chords from picker buttons and confirm the pending keycode
  with OK before it changes the field
- build `LT(layer, key)` values from the Layers picker by selecting the layer
  target first and then selecting the tap key
- edit modifier chords as user-facing labels such as `Shift+\`` or
  `Alt+Cmd+Esc`
- disabled/read-only fields are visually distinct from editable controls
- view and save the selected key behavior directly inside the active layer
  layout panel; the layer filters visible behavior rows by keys currently
  present on that layer
- stage layer add/delete changes; applying them updates the layer enum in
  `config.h`, transparent `keymaps[][]` blocks in `keymap.c`, and random
  default `layer_colors[]` rows in `rgb_config.c`
- the layer overview lists layer-reachable behaviors, macros, combos, and
  pointing modes
- layer combo rows can load an existing combo into the combo builder and can
  load the combo output into the behavior editor
- selecting the exact inputs for an existing combo fills the combo output
  automatically
- append combo rows from the active layer by selecting multiple physical keys
  on the layout sidecar or by using the multi-key picker
- behavior timing fields show the resolved default milliseconds in their
  placeholder text when a row leaves the override empty
- numeric-only fields such as HSV hue/saturation channels, behavior timing
  overrides, and repeat-Hz values are validated inline before writes are sent;
  the extension validates again before patching the backing `.c` file
- named tap-count branch sections in the behavior editor are collapsible, and
  repeat Hz is only shown when the selected hold helper is
  `REPEAT_WHILE_HELD`
- RGB submenus such as layer colors, pointing-mode colors, auto-mouse fade,
  combo feedback, and key-behavior feedback colors are collapsible
- LED group tables are nested under their owning RGB section instead of shown
  as separate top-level panels
- edit `layer_colors[]` HSV values and render mode
- use compact color pickers for editable RGB colors, including a dedicated
  preview column in collapsed section summaries
- layer color previews treat `HSV(0, 0, 0)` as the firmware pass-through
  sentinel, so the base layer swatch shows the configured default RGB Matrix
  color instead of literal black
- use dropdowns for RGB fields with fixed option sets, including auto-mouse
  fade mode, combo locality, and key-behavior feedback policy
- append RGB LED group rows by selecting keys on the physical RGB layout,
  including layer, pointing-mode, combo, and key-behavior feedback tables
- the LED group builder previews already-defined groups for the currently
  selected table, separate from the pending new-row selection
- layer and pointing-mode LED groups include all-target owners for one group row
  that works across every layer or every pointing mode
- LED group rows show `HSV(0, 0, 0)` as inherited stage color rather than a
  black override, including `KEY_FEEDBACK_GROUP_ALL`
- inspect every active `rgb_config.c` surface, including LED group tables,
  automouse fade, combo feedback, and key-behavior feedback
- edit `pd_mode_colors[]` HSV values and locality
- edit `VIA_MACROS(MACRO)` payload strings from a dedicated macro builder with
  a 64-slot browser, raw payload editor, step insertion for text, key taps,
  chords, key down/up events, delays, a live keydown/keyup recorder with
  compact or exact output, and a parsed payload preview
- record macro timing gaps as optional delay commands while keeping recorded
  payloads draft-only until the selected slot's Apply button writes `keymap.c`
- append `COMBOS(COMBO)` rows from selected physical keys on the active layout
- append simple `key_behaviors[]` rows for tap, hold, and long-hold actions
- open the backing source file from the studio
- refresh from disk and discard uncommitted Studio edits
- hover panels, tabs, controls, tables, color previews, and SVG keys for
  tooltips that explain what each part edits or displays

## Native Workspace Use

From the existing Charybdis workspace, run the VS Code task
`Install Profile Studio Extension`, then reload VS Code. The extension is
symlinked into VS Code's normal user extension directory, so it runs in the
current workspace instead of an Extension Development Host.

After reload, use the `$(keyboard) Profile Studio` status bar item or run
`Charybdis: Open Profile Studio` from the command palette.
The command is also available from the editor title when `keymap.c` or
`rgb_config.c` is open.

You can also install it from a shell:

```sh
cd /Users/noah/dev/charybdis/charybdis-4x6/tools/charybdis-profile-studio
npm run install:local
```

## Development Host

If you open this extension folder by itself, its local launch config supports
the F5 extension-development flow.

You can also launch the development host from a shell:

```sh
code --extensionDevelopmentPath=/Users/noah/dev/charybdis/charybdis-4x6/tools/charybdis-profile-studio /Users/noah/dev/charybdis/charybdis.code-workspace
```

Then run `Charybdis: Open Profile Studio` from the command palette.

## Checks

This extension has no runtime npm dependencies. To check it:

```sh
npm run check
```

That command runs a JavaScript syntax check and verifies that the parser can
associate current `key_behaviors[]` rows with layout keys and combo outputs
using canonical key-expression matching.

After using the studio to change authored firmware inputs, run the same repo
checks you would run for direct source edits. The full checklist lives in
[`docs/tooling/PROFILE_STUDIO.md`](../../docs/tooling/PROFILE_STUDIO.md).

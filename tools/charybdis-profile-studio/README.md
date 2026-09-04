# Charybdis Profile Studio

Charybdis Profile Studio is a repo-local VS Code extension for editing this
userspace visually. It is meant for layout keys, layers, combos, behavior rows,
VIA macros, RGB colors, LED feedback tables, and profile-level defaults.

The current editor has no sidecar database: it parses the three C authoring
files and patches their authored blocks. That is the present implementation.
The target live workflow is device-first: a connected keyboard supplies the
complete supported logical profile, Studio edits a generation-bound draft, and
the result is committed and read back from both halves. C remains the compiled
default and explicit import/export representation rather than the mandatory
live-session source. See
[`docs/architecture/device-resident-profile.md`](../../docs/architecture/device-resident-profile.md).
The finished product is intended to be first-grade keyboard control software,
including complete readback, conflict-safe apply, backup/restore, recovery,
compatibility guidance, and a normal workflow that does not require an open
firmware repository. See
[`PROFILE_STUDIO_PRODUCT_GOAL.md`](../../docs/tooling/PROFILE_STUDIO_PRODUCT_GOAL.md).

The header also has a `Live keyboard` row. `Find keyboards` scans for the
Charybdis Raw HID interface, `Connect` reads Profile Wire capabilities and
status, and `Refresh` repeats those reads. With ordinary firmware this remains
read-only. With the separately labeled live-edit test firmware, `Apply live`
builds the complete RGB and `key_behaviors[]` domains from the source files on
disk, persists them on both halves, and activates them without reflashing.
Existing card-level Apply buttons still mean source-only edits; use them before
`Apply live` so the desired values are present on disk.

This milestone cannot yet download the committed custom-profile payload. Its
read surface returns capabilities, status, generations, and digests; layout
keycodes are independently readable through VIA. Complete profile readback and
opening Studio from keyboard state are planned, not current features.

## What It Edits

Profile Studio reads and writes the selected Charybdis 4x6 profile under
`keyboards/bastardkb/charybdis/4x6/keymaps/<name>/`. For the current `noah`
profile, those files are:

- [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h):
  layer names, timing defaults, and profile-level settings
- [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):
  layout slots, combos, macros, and `key_behaviors[]`
- [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c):
  layer colors, pointing-mode colors, LED groups, auto-mouse fade, combo
  feedback, and key-behavior feedback

There is no sidecar database to keep in sync. If you edit the C files by hand,
reload the Studio and it will parse the current source again.

The header profile picker can switch profiles, create a new one, clone the
active one, rename non-default profiles, or delete non-default profiles. `New
profile` creates `rules.mk`, `config.h`, `keymap.c`, and `rgb_config.c` from
the bundled starter templates, registers the keymap in `qmk.json`, and keeps
the generated `keymap.c` intentionally blank: no active combos, no active key
behavior rows, and empty macro payloads. Clone, rename, and delete keep the
matching `qmk.json` build target synchronized.

Generated profiles reuse the shared runtime through `USER_NAME := noah` in
their `rules.mk`, so they build with:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km <name>
```

For a read-only overview of the current `noah` profile, see
[`docs/KEYMAP-OVERVIEW.md`](../../docs/KEYMAP-OVERVIEW.md). The companion
[`docs/KEYMAP.md`](../../docs/KEYMAP.md) explains the same current config in
prose.
Use the Profile overview row's `Create overview doc` action to run the
generated overview workflow for the active profile. It warns when Studio has
unapplied edits because the overview is generated from source files on disk.
Use the Firmware row's `Compile left + right` action to build
`bastardkb_charybdis_4x6_<name>_left.uf2` with `FORCE_SLAVE=yes` and
`NOAH_PHYSICAL_HALF=left`, and the matching right artifact with
`FORCE_MASTER=yes` and `NOAH_PHYSICAL_HALF=right`. The first setting selects
the intended transport role for this `MASTER_RIGHT` keyboard; the second
embeds a stable physical identity in flash for handedness and future profile
origin. The action opens the Charybdis Profile Studio output pane and streams
QMK output while the build runs.
Use `Compile live-edit test` for the engineering pair. It produces
`bastardkb_charybdis_4x6_<name>_live_edit_left.uf2` and
`bastardkb_charybdis_4x6_<name>_live_edit_right.uf2` with the live-profile owner
and mutation route enabled together. Flash each file to its matching physical
half before expecting `Apply live` to become available. The Live panel must
report both `Second half detected: Yes` and `Halves converged: Yes`; working
keys alone prove only QMK's core split transport, not the separate live-profile
endpoint. If an older test pair leaves a candidate in `PREPARING_PEER`,
power-cycle both halves together and flash the freshly built pair before
retrying.

## Screenshots

- [Layout tab](../../docs/media/profile-studio/studio-layout-tab.png)
- [Macros tab](../../docs/media/profile-studio/studio-macros-tab.png)
- [RGB tab](../../docs/media/profile-studio/studio-rgb-tab.png)
- [Defaults tab](../../docs/media/profile-studio/studio-defaults-tab.png)

## How To Open It

Open this repo folder in VS Code, then run the VS Code task:

```text
Install Profile Studio Extension
```

Reload VS Code after the task finishes. The extension is then available from:

- the `$(keyboard) Profile Studio` status bar item
- `Charybdis: Open Profile Studio` in the command palette
- the editor title when `keymap.c`, `config.h`, or `rgb_config.c` is open

You can also install it from a shell:

```sh
cd /path/to/charybdis-4x6/tools/charybdis-profile-studio
npm run install:local
```

The install task symlinks this folder into VS Code's normal user extension
directory. That lets the Studio run in the current repo window instead of only
inside an Extension Development Host.

## Mental Model

The current source-driven Studio has two kinds of edits:

- staged local edits, such as changing layout slots or adding a layer
- direct apply actions, such as saving a macro, combo, behavior row, or RGB
  section

Staged edits are visible in the UI until you apply them. Reload discards
uncommitted Studio edits and reparses the source files from disk.

For complex behavior rows, direct source editing is still expected in the
current implementation. That is an interim limitation, not the product goal.
Normal use of the finished configurator opens from the keyboard and does not
require understanding or editing the underlying C model.

## Layout Tab

Use the Layout tab for physical-key work.

You can select keys on the Charybdis board, edit layer slots, stage key swaps,
copy and paste keycodes, add or remove layers, and build combos from selected
physical keys. The selected-key panel also shows the matching
`key_behaviors[]` row when the selected keycode has one.

The layer overview explains what is reachable from the active layer: authored
behavior rows, combo outputs, macros, and pointing modes. Combo outputs can
enter the same behavior editor as physical keys, so a chord can reuse the same
branch behavior model.

Dirty form drafts survive switching between the Layout, Macros, RGB, and
Defaults tabs; tabs show an orange dot while they still have unapplied edits.

## Macros Tab

Use the Macros tab for `VIA_MACROS(MACRO)` payloads.

The tab has a 64-slot browser, a raw payload editor, a step builder, a live
key-event recorder, and a parsed preview. Recorded input stays as a draft until
you apply the selected macro slot.

## RGB Tab

Use the RGB tab for the visible language in `rgb_config.c`.

It covers layer colors, pointing-mode colors, reusable LED groups, stage-specific
LED group rows, auto-mouse fade, combo feedback, and key-behavior feedback.
The RGB page keeps the firmware vocabulary visible: HSV values, render modes,
locality, inherited LED group color, tap-branch colors, and tap/hold/long
hold feedback.

The LED group builder lets you select physical LED indices on the board and
write either reusable groups or one-off inline group rows.

## Defaults Tab

Use the Defaults tab for `config.h` defaults that are not already owned by the
Layout or RGB authoring flows.

It edits key timing, normal pointer speed, pointing-mode speeds, sniping,
auto-mouse, base lighting, and lighting feedback defaults directly in
`config.h`.
Per-mode DPI overrides show that `0` keeps the normal pointer DPI.

## Key Picker

Editable key fields use a VIA-style picker. It accepts friendly labels such as
`A`, `Enter`, `Space`, and `Shift+Esc`, while still allowing raw QMK
expressions when needed.

The picker also knows about layers, pointing modes, macros, custom keycodes,
modifier chords, multi-key combo input fields, and `LT(layer, key)` values.
When sibling QMK keycode metadata is available, the Studio uses it to populate
the QMK catalog and aliases.

## Regenerate Screenshots

After visual Studio changes, regenerate the screenshots used by the repo docs:

```sh
cd /path/to/charybdis-4x6/tools/charybdis-profile-studio
npm run screenshots
```

The script renders the current Studio webview model in headless Chrome and
writes the PNGs to `docs/media/profile-studio/`. The default width is a wide
desktop capture so the Charybdis board is not cropped.

Useful options:

```sh
npm run screenshots -- --width 1920
CHROME_BIN=/path/to/chrome npm run screenshots
```

## Development Run

If you want to run the extension without installing it into VS Code:

```sh
cd /path/to/charybdis-4x6
code --extensionDevelopmentPath="$PWD/tools/charybdis-profile-studio" "$PWD"
```

Then run `Charybdis: Open Profile Studio` from the command palette in the
Extension Development Host window.

## Live Link Transport Development

The hardware-independent transport boundary for live profile editing
lives under [`live-link/`](live-link/). It defines an injected device adapter,
a serialized request coordinator, a fake adapter, a lazy-loaded `node-hid`
3.x desktop adapter, the Profile Wire V1 read codec, and the host-side service
used by the Studio's live connection controls. It also contains the canonical
whole-profile, RGB, key-behavior, and source-to-profile compiler path that turns
the parsed model into deterministic Profile Wire bytes.

The RGB codec validates compiled-stage inclusion, complete layer/PD/tap-color
surfaces, brightness and geometry ceilings, and every selector/group
reference. The candidate coordinator uploads, validates, commits, waits through
split preparation/convergence and safe activation, and the device service
verifies the final active/committed digest. See
[`live-link/rgb-domain-v1.md`](live-link/rgb-domain-v1.md) for the exact
payload contract and intentional deferrals.

The desktop adapter enumerates only the Charybdis QMK Raw HID interface (VID
`0xA8F8`, PID `0x1833`, usage page `0xFF60`, usage `0x61`). It adds the native
report-id byte on writes and removes it when present on reads, while keeping the
transport contract at exact 32-byte protocol frames.

To check whether a matching interface is visible without opening or writing to
the keyboard, run:

```sh
npm run probe:live-link
npm run probe:live-link -- --json
```

Run its Node built-in test suite with:

```sh
npm run test:live-link
```

The coordinator accepts only exact 32-byte reports. It permits one outstanding
request per connected device and invalidates a session after an in-flight
timeout or cancellation so a late response cannot be mistaken for a later
request. See [`live-link/README.md`](live-link/README.md) for the adapter and
error-code and native-adapter contracts.

## Checks

For extension changes:

```sh
cd /path/to/charybdis-4x6/tools/charybdis-profile-studio
npm run check
```

That command runs JavaScript syntax checks and verifies that the Studio parser
can associate current `key_behaviors[]` rows with layout keys and combo outputs.

After using the Studio to change authored firmware inputs, run the same repo
checks you would run after direct source edits. The full checklist lives in
[`docs/tooling/PROFILE_STUDIO.md`](../../docs/tooling/PROFILE_STUDIO.md).

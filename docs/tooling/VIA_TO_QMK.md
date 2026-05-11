# VIA To QMK Workflow

This doc explains the VIA bridge script in
[`tools/via_to_qmk_layout.py`](../../tools/via_to_qmk_layout.py).

Use it when you want to experiment in VIA, export the result, and then sync the
VIA-owned parts of the layout back into a source-controlled profile `keymap.c`.
By default it targets the `noah` profile.

## What The Script Does

The script reads a VIA export JSON and renders:

- `VIA_MACROS(MACRO)`
- `keymaps[][]`

The source profile stores 64 VIA macro defaults. VIA backups made from older
firmware with fewer `macros[]` entries are accepted; the script pads the missing
slots as empty macro defaults. Exports with more than 64 macro entries are
rejected so extra payloads are not silently dropped.

The checked-in VIA export currently covers the populated layer blocks. The
script reads the selected profile's layer enum from `config.h` before rendering
layer wrappers such as `MO(3)` or `LT(3, KC_F)`, so generated layer names follow
the target profile.

In `--write` mode, it can rewrite either or both of those sections in the
selected profile's `keymap.c`.

It does not rewrite:

- `HARDCODED_MACROS(MACRO)`
- `COMBOS(COMBO)`
- `key_behaviors[]`
- `enum keymap_custom_keycodes`
- comments outside the rewritten `VIA_MACROS(MACRO)` and `keymaps[][]` blocks

So this is a bridge for VIA-owned layout and VIA macro defaults. It is not a
general formatter for the rest of the keymap.

## Normal Workflow

Recommended path:

1. Make layout or VIA macro changes in VIA.
2. Export the VIA JSON.
3. Preview the generated QMK output.
4. Rewrite the parts you want back into `keymap.c`.
5. Compile and flash.
6. If you changed VIA default macros and want those defaults reseeded into
   EEPROM, reset the relevant EEPROM/VIA macro state after flashing.

## Commands

Run from the repo root with Python 3.

Preview generated output:

```sh
python3 tools/via_to_qmk_layout.py --print
```

Preview a specific export:

```sh
python3 tools/via_to_qmk_layout.py --print --via-json /path/to/export.json
```

Run the default interactive write flow:

```sh
python3 tools/via_to_qmk_layout.py
```

Write back into `keymap.c`:

```sh
python3 tools/via_to_qmk_layout.py --write
```

Write from a specific export:

```sh
python3 tools/via_to_qmk_layout.py --write --via-json /path/to/export.json
```

Target another profile by name:

```sh
python3 tools/via_to_qmk_layout.py --keymap <name> --print --via-json /path/to/export.json
python3 tools/via_to_qmk_layout.py --keymap <name> --write --via-json /path/to/export.json
```

Target a profile directory or `keymap.c` directly:

```sh
python3 tools/via_to_qmk_layout.py \
  --keymap-path keyboards/bastardkb/charybdis/4x6/keymaps/<name> \
  --write \
  --via-json /path/to/export.json
```

When you use `--write`, the script asks two separate questions:

- update `VIA_MACROS(MACRO)` from the export `macros[]`
- update `keymaps[][]` from the export `layers`

So you can sync macros only, layers only, or both.

## If You Omit `--via-json`

If you do not pass `--via-json`, the script uses the only JSON export in
`tools/` automatically when there is just one. If there are multiple
exports, it interactively asks you to choose one.

## Token Mapping

The script translates VIA tokens into the symbols used by this repo.

Important cases:

- `MACRO(n)` in VIA export becomes `VIA_MACRO_n` for slots `0` through `63`
- upstream Charybdis keyboard keycodes `CUSTOM(0)` through `CUSTOM(7)` map to
  the keyboard-defined symbols such as `DPI_MOD`, `DPI_RMOD`, `S_D_MOD`, and
  `S_D_RMOD`
- `CUSTOM(64 + n)` maps into this userspace custom-keycode range
- shared pd-mode keycodes are loaded from
  [`pd_mode_manifest.h`](../../users/noah/lib/pointing/defs/pd_mode_manifest.h)
- keymap-local custom keycodes are loaded from the selected profile's
  `enum keymap_custom_keycodes`
- layer wrappers such as `MO(3)` or `LT(3,KC_F)` are rewritten back to the
  selected profile's named layer enum symbols

That means most keymap-local additions do not require manual script edits. If
you add a new keymap-local custom keycode in `keymap.c`, the script can usually
pick it up automatically.

If a VIA export has more layer arrays than the selected profile enum defines,
extra layers are named `LAYER_0`, `LAYER_1`, and onward in the preview. Add the
layers to the target profile before writing if those layers should round-trip as
named enum entries.

## What It Treats As Source Of Truth

If you run the script in `--write` mode and confirm a rewrite, the selected VIA
export becomes authoritative for the rewritten section.

If you do not run the script, the firmware builds exactly from what is already
authored in the selected profile's `keymap.c`.

## What To Check After A Rewrite

After syncing from VIA, check these things:

- the right layers were rewritten
- exported layer names still match the keymap enum, especially after adding or
  deleting layers
- `VIA_MACROS(MACRO)` matches the export you intended
- custom keycodes still resolved to the expected symbolic names
- no profile-specific authored behavior in `key_behaviors[]` now conflicts with
  the new physical placement

Then build:

```sh
python3 tools/profile_introspect.py --write
python3 tools/profile_introspect.py --check
sh tests/host/run_real_profile_validation_tests.sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

For another profile, pass the same profile target to the profile checks and
firmware compile:

```sh
python3 tools/profile_introspect.py --keymap <name> --write
python3 tools/profile_introspect.py --keymap <name> --check
sh tests/host/run_real_profile_validation_tests.sh keyboards/bastardkb/charybdis/4x6/keymaps/<name>
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km <name>
```

For this repo's normal maintenance workflow, run those host checks before the
firmware compile whenever the rewrite changes authored layers or VIA macro
defaults in source.

## What This Script Is Good For

- quick VIA experimentation without giving up a readable `keymap.c`
- round-tripping VIA macro defaults back into source
- restoring a VIA-edited layer layout into the repo's authored layout blocks

## What This Script Is Not For

- editing `key_behaviors[]`
- documenting your current profile
- changing the shared custom runtime
- changing hardcoded firmware macros

Those still belong in the normal source files and docs.

## Related Docs

- [PROFILE_INTROSPECT.md](./PROFILE_INTROSPECT.md): generated overview and SVG
  asset workflow
- [KEYMAP.md](../KEYMAP.md): current authored profile choices and layer intent

# VIA To QMK Workflow

This doc explains the VIA bridge script in
[`tools/via_to_qmk_layout.py`](../../tools/via_to_qmk_layout.py).

Use it when you want to experiment in VIA, export the result, and then sync the
VIA-owned parts of the layout back into the source-controlled
[`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

## What The Script Does

The script reads a VIA export JSON and renders:

- `VIA_MACROS(MACRO)`
- `keymaps[][]`

This profile expects VIA exports with 64 macro entries. VIA backups made from
older 16-slot firmware need their `macros[]` array padded before this script
will accept them.

The checked-in VIA export currently covers the populated layer blocks. If a
new layer is added in source or through Profile Studio, update the script's
`LAYER_NAMES` mapping and inspect the generated layer names before using
`--write`.

In `--write` mode, it can rewrite either or both of those sections in
[`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

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
- keymap-local custom keycodes are loaded from
  [`enum keymap_custom_keycodes`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- layer wrappers such as `MO(3)` or `LT(3,KC_F)` are rewritten back to the
  named layer enum symbols from this keymap

That means most keymap-local additions do not require manual script edits. If
you add a new keymap-local custom keycode in `keymap.c`, the script can usually
pick it up automatically.

Layer enum changes are the exception: keep the script's layer-name mapping in
sync with any layer that should be round-tripped from VIA.

## What It Treats As Source Of Truth

If you run the script in `--write` mode and confirm a rewrite, the selected VIA
export becomes authoritative for the rewritten section.

If you do not run the script, the firmware builds exactly from what is already
authored in [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

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

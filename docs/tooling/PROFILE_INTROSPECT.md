# Profile Introspection Workflow

This doc explains the authored-profile introspector in
[`tools/profile_introspect.py`](../../tools/profile_introspect.py).

Use it when you want to regenerate or verify the visual profile report that
this repo derives from the authored keymap and RGB sources.

## What The Script Reads

The introspector reads these authored inputs directly:

- [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- [`users/noah/config.h`](../../users/noah/config.h)
- [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- [`pd_mode_manifest.h`](../../users/noah/lib/pointing/defs/pd_mode_manifest.h)

It also uses the current `LAYOUT()` slot order from
[`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
so the generated previews stay aligned with the physical key positions defined
in source.

## What The Script Writes

In `--write` mode, the script regenerates:

- [`docs/KEYMAP-OVERVIEW.md`](../KEYMAP-OVERVIEW.md)
- SVG layer previews under
  [`docs/media/profile-introspection/`](../media/profile-introspection/)
- SVG color swatches under
  [`docs/media/profile-introspection/`](../media/profile-introspection/)

Those outputs are generated artifacts. If you changed one of the authored
inputs above, regenerate them in the same pass instead of editing the generated
Markdown or SVG files by hand.

Current generated profile assets live under `docs/media/profile-introspection/`.
Older scratch outputs under `docs/media/generated/` or top-level
`docs/generated/` are treated as stale and should be removed rather than
recreated.

## What The Generated Report Includes

The report is meant to be a visual snapshot of the current authored profile. It
includes:

- per-layer visual previews with authored layer colors
- layer LED-group overlays in the per-layer previews
- transparent-key visibility for partial layers
- key-behavior markers and timing summaries
- combos, macro inventory, and VIA macro defaults
- pointer-mode reachability, mode colors, LED groups, and layer-local mode
  entry paths
- auto-mouse, combo-feedback, and key-behavior feedback colors and policy with
  generated swatches

Layer enum entries without an authored `keymaps[][]` block remain visible in
the report's reference section, but they do not get a layer preview until they
have real authored layer data.

The parser accepts normal C comments in authored inputs. In RGB authoring, this
includes comments next to `HSV(...)` arguments inside helper macros such as
`RGB_TAP_BRANCH_COLORS(...)`.

## Commands

Run from the repo root with Python 3.

Preview the rendered Markdown without rewriting files:

```sh
python3 tools/profile_introspect.py --print-markdown
```

Regenerate the report and SVG assets:

```sh
python3 tools/profile_introspect.py --write
```

Verify the generated outputs are current:

```sh
python3 tools/profile_introspect.py --check
```

Run the same verification through the host-suite wrapper:

```sh
sh tests/host/run_profile_introspection_checks.sh
```

## When To Run It

Run the introspector whenever you change authored inputs that affect the
documented profile view, especially:

- layer contents or macros in
  [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- layer enum or keymap-facing timing/config surfaces in
  [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- shared userspace config surfaces that appear in the generated config table in
  [`users/noah/config.h`](../../users/noah/config.h)
- layer colors, LED groups, mode colors, or feedback colors in
  [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- shared pd-mode definitions in
  [`pd_mode_manifest.h`](../../users/noah/lib/pointing/defs/pd_mode_manifest.h)

That is a repo guardrail as well: if any authored input to the introspector
changes, regenerate the outputs with `--write` and verify them with `--check`
in the same pass.

## What To Review After A Rewrite

After regenerating the outputs, check that:

- layer order and names still match the keymap's `LAYER_*` enum
- moved keys appear in the expected physical positions
- transparent `TRNS` keys still show through where you expect
- layer colors, LED-group overlays, pointer-mode colors, and feedback swatches
  match authored data
- key-behavior dots and timing legends still describe the intended behavior

If the authored profile changed, this repo's normal maintenance workflow also
expects the matching profile validation and host-test passes before firmware
compile.

## What This Script Is Not For

- rewriting `keymaps[][]` or `VIA_MACROS(MACRO)` from a VIA export
- editing generated docs by hand
- changing runtime behavior on its own

For the VIA round-trip workflow, see [VIA_TO_QMK.md](./VIA_TO_QMK.md).

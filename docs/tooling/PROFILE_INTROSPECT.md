# Profile Introspection Workflow

This doc explains the authored-profile introspector in
[`tools/profile_introspect.py`](../../tools/profile_introspect.py).

Use it when you want to regenerate or verify the visual profile report that
this repo derives from the authored keymap and RGB sources. It does not read
the keyboard, reflect saved live overrides, or parse the engine records in
`pd_config.c`; PD reachability and colors are not a complete slot export.

## What The Script Reads

The introspector reads these authored inputs directly. By default it targets the
`noah` profile:

- [`keymap.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`config.h`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- [`users/noah/config.h`](../../users/noah/config.h)
- [`rgb_config.c`](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)
- [`pd_mode_manifest.h`](../../users/noah/lib/pointing/defs/pd_mode_manifest.h)

For another profile, pass `--keymap <name>` or `--keymap-path <path>`. The
script then reads that profile's `keymap.c`, `config.h`, and `rgb_config.c`
while still reading shared userspace config and pointing-mode definitions from
`users/noah/`.

It also uses the selected profile's current `LAYOUT()` slot order from
`keymap.c`, so the generated previews stay aligned with the physical key
positions defined in source.

## What The Script Writes

In default `noah` `--write` mode, the script regenerates:

- [`docs/KEYMAP-OVERVIEW.md`](../KEYMAP-OVERVIEW.md)
- `authored_macro_bank.h` in the selected keymap directory: source macro
  defaults encoded with the firmware C encoder, used by the compiled profile
- SVG layer previews under
  [`docs/media/profile-introspection/`](../media/profile-introspection/)
- SVG color swatches under
  [`docs/media/profile-introspection/`](../media/profile-introspection/)

For non-`noah` profiles, the default outputs are profile-specific:

- `docs/profiles/<name>/KEYMAP-OVERVIEW.md`
- SVG layer previews and color swatches under
  `docs/media/profiles/<name>/profile-introspection/`

Use `--output-dir <dir>` and `--asset-dir <dir>` to override those locations.

The macro header always belongs to the selected keymap, even with custom
documentation output directories. Its generation needs a host C compiler
(`cc` on PATH), the selected QMK headers (`QMK_ROOT` in a worktree), and
macro compiler inputs under `users/noah/`. It invokes the real-profile
validation harness to check or generate that header.

Those outputs are generated artifacts. If you changed one of the selected
profile's authored inputs above, regenerate them in the same pass instead of
editing the generated Markdown or SVG files by hand.

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
- combos, macro inventory, and profile macro defaults (`VIA_MACROS` in C)
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

Run from the repo root with Python 3 and a host C compiler.

Preview the rendered Markdown without rewriting files:

```sh
python3 tools/profile_introspect.py --print-markdown
```

Regenerate the report, SVG assets and compiled macro bank:

```sh
python3 tools/profile_introspect.py --write
```

Verify the generated outputs are current:

```sh
python3 tools/profile_introspect.py --check
```

Preview, write, or check another registered profile:

```sh
python3 tools/profile_introspect.py --keymap <name> --print-markdown
python3 tools/profile_introspect.py --keymap <name> --write
python3 tools/profile_introspect.py --keymap <name> --check
```

Target a profile directory directly, with explicit output paths:

```sh
python3 tools/profile_introspect.py \
  --keymap-path keyboards/bastardkb/charybdis/4x6/keymaps/<name> \
  --output-dir docs/profiles/<name> \
  --asset-dir docs/media/profiles/<name>/profile-introspection \
  --write
```

Run the same verification through the host-suite wrapper:

```sh
sh tests/host/run_profile_introspection_checks.sh
```

## When To Run It

Run the introspector whenever you change authored inputs that affect the
documented profile view, especially:

- layer contents or macros in the selected profile's `keymap.c`
- layer enum or keymap-facing timing/config surfaces in the selected profile's
  `config.h`
- shared userspace config surfaces that appear in the generated config table in
  [`users/noah/config.h`](../../users/noah/config.h)
- layer colors, LED groups, mode colors, or feedback colors in the selected
  profile's `rgb_config.c`
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

The historical VIA-to-C converter is retired; see
[VIA_TO_QMK.md](./VIA_TO_QMK.md).

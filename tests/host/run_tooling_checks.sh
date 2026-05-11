#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$ROOT/../.."
PYTHON="${PYTHON:-python3}"
TMP_ROOT="${TMPDIR:-/tmp}"
PYTHON_CACHE="$TMP_ROOT/charybdis-tooling-pycache"
VIA_EXPORT="$REPO_ROOT/tools/charybdis.layout.json"
VIA_PREVIEW="$TMP_ROOT/charybdis-via-to-qmk-preview.txt"
PROFILE_PREVIEW="$TMP_ROOT/charybdis-profile-introspect.md"
ALT_PROFILE_ROOT="$(mktemp -d "$TMP_ROOT/charybdis-alt-profile.XXXXXX")"
ALT_PROFILE_PATH="$ALT_PROFILE_ROOT/alt_profile"
ALT_PROFILE_DOCS="$ALT_PROFILE_ROOT/docs"
ALT_PROFILE_ASSETS="$ALT_PROFILE_ROOT/assets"

cleanup() {
    rm -rf "$ALT_PROFILE_ROOT"
}
trap cleanup EXIT

export PYTHONDONTWRITEBYTECODE=1
export PYTHONPYCACHEPREFIX="$PYTHON_CACHE"

npm --prefix "$REPO_ROOT/tools/charybdis-profile-studio" run check

"$PYTHON" -m py_compile \
    "$REPO_ROOT/tools/profile_introspect.py" \
    "$REPO_ROOT/tools/via_to_qmk_layout.py"

"$PYTHON" "$REPO_ROOT/tools/profile_introspect.py" --check
"$PYTHON" "$REPO_ROOT/tools/profile_introspect.py" --keymap noah --check
"$PYTHON" "$REPO_ROOT/tools/profile_introspect.py" --print-markdown > "$PROFILE_PREVIEW"

mkdir -p "$ALT_PROFILE_PATH"
cp "$REPO_ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c" "$ALT_PROFILE_PATH/keymap.c"
cp "$REPO_ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h" "$ALT_PROFILE_PATH/config.h"
cp "$REPO_ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c" "$ALT_PROFILE_PATH/rgb_config.c"

"$PYTHON" "$REPO_ROOT/tools/profile_introspect.py" \
    --keymap-path "$ALT_PROFILE_PATH" \
    --output-dir "$ALT_PROFILE_DOCS" \
    --asset-dir "$ALT_PROFILE_ASSETS" \
    --write
"$PYTHON" "$REPO_ROOT/tools/profile_introspect.py" \
    --keymap-path "$ALT_PROFILE_PATH" \
    --output-dir "$ALT_PROFILE_DOCS" \
    --asset-dir "$ALT_PROFILE_ASSETS" \
    --check

"$PYTHON" "$REPO_ROOT/tools/via_to_qmk_layout.py" --print --via-json "$VIA_EXPORT" > "$VIA_PREVIEW"
"$PYTHON" "$REPO_ROOT/tools/via_to_qmk_layout.py" --keymap-path "$ALT_PROFILE_PATH" --print --via-json "$VIA_EXPORT" > "$VIA_PREVIEW"

"$PYTHON" - "$REPO_ROOT" <<'PY'
import importlib.util
import shutil
import sys
import tempfile
from pathlib import Path

repo_root = Path(sys.argv[1]).resolve()
script_path = repo_root / "tools" / "via_to_qmk_layout.py"
export_path = repo_root / "tools" / "charybdis.layout.json"

spec = importlib.util.spec_from_file_location("via_to_qmk_layout_check", script_path)
via = importlib.util.module_from_spec(spec)
spec.loader.exec_module(via)

keymap_text = via.KEYMAP_FILE.read_text()
via_data = via.load_via_data(export_path)

expected_macros = via.render_via_macros_block(via.load_all_exported_via_macros(via_data, export_path))
macro_start, macro_end = via.find_define_block_range(keymap_text, via._VIA_MACROS_DEFINE_PATTERN, "VIA_MACROS(MACRO)")
current_macros = keymap_text[macro_start:macro_end].rstrip("\n")

short_macro_export = {**via_data, "macros": via_data["macros"][:16]}
padded_macro_map = via.load_all_exported_via_macros(short_macro_export, export_path)
if len(padded_macro_map) != via.VIA_MACRO_COUNT:
    raise SystemExit("short VIA macros[] export was not padded to the full macro count")
if padded_macro_map[0]["via"]["value"] != via_data["macros"][0]:
    raise SystemExit("short VIA macros[] export did not preserve existing macro entries")
if padded_macro_map[16]["via"]["value"] != "":
    raise SystemExit("short VIA macros[] export did not pad missing entries as empty macros")

keymap_start = keymap_text.find(via._KEYMAPS_DECL)
if keymap_start == -1:
    raise SystemExit("could not find keymaps declaration")
keymap_end = keymap_text.find("};\n", keymap_start + len(via._KEYMAPS_DECL))
if keymap_end == -1:
    raise SystemExit("could not find keymaps terminator")
keymap_end += len("};")
current_keymaps = keymap_text[keymap_start:keymap_end]
expected_keymaps = via.render_keymaps_block(export_path)

if current_macros != expected_macros:
    raise SystemExit("VIA_MACROS(MACRO) differs from tools/charybdis.layout.json")
if current_keymaps != expected_keymaps:
    raise SystemExit("keymaps[][] differs from tools/charybdis.layout.json")

with tempfile.TemporaryDirectory() as tmpdir:
    temp_keymap = Path(tmpdir) / "keymap.c"
    temp_config = Path(tmpdir) / "config.h"
    shutil.copyfile(via.KEYMAP_FILE, temp_keymap)
    shutil.copyfile(via.KEYMAP_FILE.with_name("config.h"), temp_config)
    original_keymap = via.KEYMAP_FILE
    original_layers = list(via.LAYER_NAMES)
    original_replacements = dict(via.REPLACEMENTS)
    via.configure_keymap_target(None, temp_keymap)
    try:
        via.write_generated_outputs(export_path, rewrite_macros=True, rewrite_keymaps=True)
        rewritten_text = temp_keymap.read_text()
    finally:
        via.KEYMAP_FILE = original_keymap
        via.LAYER_NAMES = original_layers
        via.REPLACEMENTS.clear()
        via.REPLACEMENTS.update(original_replacements)

if rewritten_text != keymap_text:
    raise SystemExit("VIA write simulation changed keymap.c content for the checked-in export")
PY

echo "tooling checks passed"

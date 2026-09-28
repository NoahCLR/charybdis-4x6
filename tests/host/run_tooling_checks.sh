#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$ROOT/../.."
PYTHON="${PYTHON:-python3}"
TMP_ROOT="${TMPDIR:-/tmp}"
PYTHON_CACHE="$TMP_ROOT/charybdis-tooling-pycache"
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

"$PYTHON" -m py_compile "$REPO_ROOT/tools/profile_introspect.py"

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

echo "tooling checks passed"

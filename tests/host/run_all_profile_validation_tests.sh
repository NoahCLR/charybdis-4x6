#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$ROOT/../.."

. "$ROOT/noah_source_manifest.sh"

profiles="$(charybdis_qmk_build_target_keymaps "$REPO_ROOT")"

if [ -z "$profiles" ]; then
    echo "no Charybdis 4x6 qmk.json build targets found" >&2
    exit 1
fi

for keymap in $profiles; do
    keymap_path="keyboards/bastardkb/charybdis/4x6/keymaps/$keymap"
    echo "validating profile: $keymap"
    sh "$ROOT/run_real_profile_validation_tests.sh" "$keymap_path"
done

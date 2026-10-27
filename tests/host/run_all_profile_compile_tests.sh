#!/bin/sh

set -eu
# It writes into QMK (a qmk compile), so it works on the checkouts themselves.
noah_writes_checkout=1
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$ROOT/../.."

. "$ROOT/noah_source_manifest.sh"

profiles="$(charybdis_qmk_build_target_keymaps "$REPO_ROOT")"

if [ -z "$profiles" ]; then
    echo "no Charybdis 4x6 qmk.json build targets found" >&2
    exit 1
fi

for keymap in $profiles; do
    echo "compiling profile: $keymap"
    qmk compile -kb bastardkb/charybdis/4x6 -km "$keymap"
done

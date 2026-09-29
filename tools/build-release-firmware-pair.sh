#!/bin/sh
set -eu

# Build the same flashable pair as tools/build-firmware-pair.sh and export
# stable asset names for CI releases. Keep its numbered local output separate.
REPO_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$REPO_ROOT}"
PAIR_BUILD_ROOT="$(mktemp -d)"
trap 'rm -rf "$PAIR_BUILD_ROOT"' EXIT HUP INT TERM

BUILD_ROOT="$PAIR_BUILD_ROOT" sh "$REPO_ROOT/tools/build-firmware-pair.sh"

branch="$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD)"
pair_dir="$PAIR_BUILD_ROOT/$branch"
mkdir -p "$OUTPUT_DIR"
for half in right left; do
    source="$pair_dir/1_charybdis_${half}.uf2"
    if [ ! -s "$source" ]; then
        echo "Expected $half firmware not found: $source" >&2
        exit 1
    fi
    cp "$source" "$OUTPUT_DIR/bastardkb_charybdis_4x6_noah_${half}.uf2"
done

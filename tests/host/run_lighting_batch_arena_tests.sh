#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCE=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_batch_arena.c) SOURCE="$ROOT/$src" ;; esac
done
[ -n "$SOURCE" ]
for variant in normal sanitized; do
    flags="-O2"
    [ "$variant" != sanitized ] || flags="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -I"$ROOT" \
      "$ROOT/tests/host/lighting_batch_arena_test.c" "$SOURCE" -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant"
done

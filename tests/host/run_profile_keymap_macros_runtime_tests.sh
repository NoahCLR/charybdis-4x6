#!/bin/sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags \
        -DLAYER_COUNT=16 -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/profile_keymap_macros_runtime_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/active_profile_keymap_macros_runtime.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_keymap.c" \
        "$ROOT/users/noah/lib/macro/profile_macro_provider.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
        -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant"
done

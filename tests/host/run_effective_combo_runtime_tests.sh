#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DVIA_ENABLE -DCOMBO_ENABLE -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/effective_combo_runtime_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_combo_runtime.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_profile_provider.c" \
        "$ROOT/users/noah/lib/compat/qmk_effective_combos.c" \
        "$ROOT/users/noah/lib/compat/qmk_combo_readback.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/test_$name"
    "$BUILD_DIR/test_$name" "$ROOT/tests/fixtures/combo_domain_v1.fixture"
}
build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

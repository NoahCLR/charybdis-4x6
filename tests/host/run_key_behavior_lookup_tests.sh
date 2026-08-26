#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DKEY_BEHAVIOR_LOOKUP_TEST_INSTRUMENTATION \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/key_behavior_lookup_test.c" \
        "$ROOT/users/noah/lib/action/action_kind.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_key_behavior_runtime.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c" \
        "$ROOT/users/noah/lib/key/behavior/handled_key_defaults.c" \
        "$ROOT/users/noah/lib/key/behavior/handled_key_resolution_accessors.c" \
        "$ROOT/users/noah/lib/key/behavior/handled_key_lookup.c" \
        "$ROOT/users/noah/lib/key/behavior/handled_key_transparency.c" \
        "$ROOT/users/noah/lib/key/behavior/handled_key_materialize.c" \
        -o "$BUILD_DIR/key_behavior_lookup_test_$name"
    "$BUILD_DIR/key_behavior_lookup_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
for source in \
    "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/effective_key_behavior_runtime.c" \
    "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c"
do
    object="$BUILD_DIR/$(basename "$source" .c)_cortex_m0plus.o"
    "$ARM_CC" -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -mcpu=cortex-m0plus -mthumb \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -c "$source" \
        -o "$object"
done

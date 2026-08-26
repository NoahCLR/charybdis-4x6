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
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/profile_activation_policy_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_activation_policy.c" \
        -o "$BUILD_DIR/profile_activation_policy_test_$name"
    "$BUILD_DIR/profile_activation_policy_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

# The policy must still compile when QMK's one-shot action surface is removed;
# those fields then remain zero and cannot create a false blocker.
build_and_run no_oneshot -DNO_ACTION_ONESHOT

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -mcpu=cortex-m0plus -mthumb \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/profile/runtime/profile_activation_policy.c" \
    -o "$BUILD_DIR/profile_activation_policy_cortex_m0plus.o"

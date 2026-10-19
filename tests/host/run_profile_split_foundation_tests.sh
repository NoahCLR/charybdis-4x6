#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_split_foundation_test.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
        -o "$BUILD_DIR/profile_split_foundation_test_$name"
    "$BUILD_DIR/profile_split_foundation_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -mcpu=cortex-m0plus -mthumb \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
    -o "$BUILD_DIR/profile_split_authority_cortex_m0plus.o"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -mcpu=cortex-m0plus -mthumb \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
    -o "$BUILD_DIR/profile_split_protocol_v1_cortex_m0plus.o"

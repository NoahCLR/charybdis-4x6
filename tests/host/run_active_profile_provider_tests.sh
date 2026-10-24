#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

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
        "$ROOT/tests/host/active_profile_provider_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/active_profile_provider.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_keymap_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_macros_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/active_profile_provider_test_$name"
    "$BUILD_DIR/active_profile_provider_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -mcpu=cortex-m0plus -mthumb \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/runtime/active_profile_provider.c" \
    -o "$BUILD_DIR/active_profile_provider_cortex_m0plus.o"

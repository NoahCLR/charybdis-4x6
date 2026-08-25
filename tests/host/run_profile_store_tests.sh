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
    cc -std=c11 -Wall -Wextra -Werror -pedantic \
        -DVIA_ENABLE \
        -DDYNAMIC_KEYMAP_EEPROM_MAX_ADDR=0x1FFFu \
        -DNOAH_PROFILE_STORAGE_SLOT_A_START_ADDR=0x2000u \
        -DNOAH_PROFILE_STORAGE_SLOT_A_END_ADDR=0x2FFFu \
        -DNOAH_PROFILE_STORAGE_SLOT_B_START_ADDR=0x3000u \
        -DNOAH_PROFILE_STORAGE_SLOT_B_END_ADDR=0x3FFFu \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_store_test.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$@" \
        -o "$BUILD_DIR/profile_store_test_$name"
    "$BUILD_DIR/profile_store_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

SLOTS="-DNOAH_PROFILE_STORAGE_SLOT_A_START_ADDR=0x9000u -DNOAH_PROFILE_STORAGE_SLOT_A_END_ADDR=0x15FFFu -DNOAH_PROFILE_STORAGE_SLOT_B_START_ADDR=0x16000u -DNOAH_PROFILE_STORAGE_SLOT_B_END_ADDR=0x22FFFu"
# 128 KiB test slots (NOAH_PROFILE_TEST_WIDE_SLOTS): payloads past 65,535 bytes.
WIDE_SLOTS="-DNOAH_PROFILE_TEST_WIDE_SLOTS -DNOAH_PROFILE_STORAGE_SLOT_A_START_ADDR=0x9000u -DNOAH_PROFILE_STORAGE_SLOT_A_END_ADDR=0x28FFFu -DNOAH_PROFILE_STORAGE_SLOT_B_START_ADDR=0x29000u -DNOAH_PROFILE_STORAGE_SLOT_B_END_ADDR=0x48FFFu"

build_and_run() {
    name="$1"
    slots="$2"
    shift 2
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic \
        -DVIA_ENABLE \
        -DDYNAMIC_KEYMAP_EEPROM_MAX_ADDR=0x8FFFu \
        $slots \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_store_test.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$@" \
        -o "$BUILD_DIR/profile_store_test_$name"
    "$BUILD_DIR/profile_store_test_$name"
}

build_and_run normal "$SLOTS"
build_and_run sanitized "$SLOTS" -fsanitize=address,undefined -fno-omit-frame-pointer
build_and_run wide "$WIDE_SLOTS" -fsanitize=address,undefined -fno-omit-frame-pointer

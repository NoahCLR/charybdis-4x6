#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

for variant in normal sanitized; do
    flags=""
    if [ "$variant" = sanitized ]; then flags="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags \
        -DUSB_PRODUCT_STRING_USER \
        -DNOAH_LIVE_PROFILE_OWNER_ENABLE \
        -DNOAH_PORTABLE_PROFILE_ENABLE \
        -DVIA_ENABLE \
        -DSPLIT_KEYBOARD \
        -DSPLIT_TRANSACTION_IDS_USER \
        -DDYNAMIC_KEYMAP_EEPROM_MAX_ADDR=0x8FFFu \
        -DNOAH_PROFILE_STORAGE_SLOT_A_START_ADDR=0x9000u \
        -DNOAH_PROFILE_STORAGE_SLOT_A_END_ADDR=0x15FFFu \
        -DNOAH_PROFILE_STORAGE_SLOT_B_START_ADDR=0x16000u \
        -DNOAH_PROFILE_STORAGE_SLOT_B_END_ADDR=0x22FFFu \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/qmk_usb_product_name_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_usb_product_name.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/test_$variant"
    "$BUILD_DIR/test_$variant"
done

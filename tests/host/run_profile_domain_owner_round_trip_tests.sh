#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
KEYMAP_PATH="$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah"
BUILD_DIR="$(mktemp -d)"
CONFIG="$BUILD_DIR/compile_config.h"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

{
    printf '#pragma once\n'
    printf '#include "%s/users/noah/config.h"\n' "$ROOT"
    printf '#include "%s/config.h"\n' "$KEYMAP_PATH"
} >"$CONFIG"
# The populated combo/settings fixture is translated to current wire domains;
# RGB, key behaviors and pointing content come from the real authored writer.
python3 "$ROOT/tests/host/translate_eight_slot_profile.py" "$ROOT/tests/fixtures/client-regression/portable.bin.pd5" "$BUILD_DIR/portable32.bin.pd5"

build_and_run() {
    name="$1"
    shift
    bin="$BUILD_DIR/profile_domain_owner_round_trip_test_$name"
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DCOMBO_ENABLE \
        -DPOINTING_DEVICE_ENABLE \
        -DRGB_MATRIX_ENABLE \
        -DRGB_MATRIX_WS2812 \
        -DVIA_ENABLE \
        -DMCU_RP \
        -DTOTAL_EEPROM_BYTE_COUNT=0x4800u \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$CONFIG" \
        "$ROOT/tests/host/profile_domain_owner_round_trip_test.c" \
        "$KEYMAP_PATH/keymap.c" \
        "$KEYMAP_PATH/pd_config.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_pd_runtime.c" \
        "$KEYMAP_PATH/rgb_config.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_compiled_defaults_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_action_runtime_v1.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_action_placement_v1.c" \
        "$ROOT/users/noah/lib/action/action_kind.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_owner.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_profile_provider.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_key_behavior_runtime.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_rgb_runtime.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_combo_runtime.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_settings_runtime.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_candidate_v1.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_reconciler.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_transaction.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_store_backend.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_peer_store_backend.c" \
        -o "$bin"
    "$bin" "$BUILD_DIR/portable32.bin.pd5"
}
build_and_run normal -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE
build_and_run sanitized -DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -fsanitize=address,undefined -fno-omit-frame-pointer

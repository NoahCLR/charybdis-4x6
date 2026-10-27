#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# The owner build of the store runtime: SPLIT_KEYBOARD brings in
# users/noah/config.h's split RPC ids (SPLIT_TRANSACTION_IDS_USER) and its
# 128-byte RPC buffers.
build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DSPLIT_KEYBOARD \
        -DNOAH_PROFILE_SPLIT_TRANSPORT_TEST \
        -DNOAH_LIVE_PROFILE_OWNER_ENABLE \
        -DNOAH_LIVE_PROFILE_MUTATION_ENABLE \
        -DTOTAL_EEPROM_BYTE_COUNT=0x9000u \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/profile_store_runtime_owner_test.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store_runtime.c" \
        "$ROOT/users/noah/lib/compat/qmk_durable_io.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_flash.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_split_transport.c" \
        "$ROOT/users/noah/lib/compat/qmk_profile_working.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_owner.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_working_copy.c" \
        "$ROOT/users/noah/lib/profile/split/profile_working_sync.c" \
        "$ROOT/users/noah/lib/profile/runtime/active_profile_keymap_macros_runtime.c" \
        "$ROOT/users/noah/lib/profile/runtime/active_profile_provider.c" \
        "$ROOT/users/noah/lib/profile/runtime/active_profile_pd_runtime.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_candidate_v1.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_keymap_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_macros_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_reconciler.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_flash_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_transaction.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_store_backend.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_other_half_store_backend.c" \
        -o "$BUILD_DIR/profile_store_runtime_owner_test_$name"
    "$BUILD_DIR/profile_store_runtime_owner_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

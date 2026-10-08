#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# The real owner, host staging handler and VIA split sync in one binary, so
# neither side's fake can hide a contract the other side does not keep.
build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DVIA_ENABLE \
        -DSPLIT_TRANSACTION_IDS_USER \
        -DTOTAL_EEPROM_BYTE_COUNT=0x4800u \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$ROOT/tests/host/profile_owner_via_integration_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_logical_profile.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_split_sync.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_sync_protocol.c" \
        "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_owner.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_profile_provider.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_candidate_v1.c" \
        "$ROOT/users/noah/lib/profile/protocol/profile_wire_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" \
        "$ROOT/users/noah/lib/profile/split/profile_split_reconciler.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_store.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_transaction.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_candidate_store_backend.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_peer_store_backend.c" \
        -o "$BUILD_DIR/profile_owner_via_integration_test_$name"
    "$BUILD_DIR/profile_owner_via_integration_test_$name"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

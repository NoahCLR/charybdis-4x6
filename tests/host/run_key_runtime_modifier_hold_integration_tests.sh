#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/key_runtime_modifier_hold_integration_test"

. "$ROOT/tests/host/noah_source_manifest.sh"
MODIFIER_HOLD_SUPPORT_SOURCES="$(noah_host_key_runtime_modifier_hold_support_paths "$ROOT")"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Intentional word splitting for derived support source list.
# shellcheck disable=SC2086
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/key_runtime_integration_harness.c" \
    "$ROOT/tests/host/key_runtime_modifier_hold_integration_test.c" \
    $MODIFIER_HOLD_SUPPORT_SOURCES \
    "$ROOT/users/noah/lib/profile/runtime/profile_activation_policy.c" \
    "$ROOT/users/noah/lib/profile/runtime/profile_working_copy.c" \
    "$ROOT/users/noah/lib/profile/runtime/effective_profile_provider.c" \
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
    -o "$BIN"

"$BIN"

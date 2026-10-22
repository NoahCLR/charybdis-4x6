#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

python3 - "$ROOT/tests/fixtures/populated_profile_v4.fixture" "$BUILD_DIR/populated.bin" <<'PYTHON'
import pathlib, sys
fields = dict(line.split("=", 1) for line in pathlib.Path(sys.argv[1]).read_text().splitlines() if "=" in line)
pathlib.Path(sys.argv[2]).write_bytes(bytes.fromhex(fields["profile.hex"]))
PYTHON

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_working_copy_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/profile_working_copy.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_profile_provider.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_validator_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_keymap_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_macros_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_combo_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_settings_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/key_behavior_domain_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_domain_registry.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_blob_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        "$ROOT/users/noah/lib/profile/storage/profile_checksum.c" \
        -o "$BUILD_DIR/profile_working_copy_test_$name"
    "$BUILD_DIR/profile_working_copy_test_$name" "$BUILD_DIR/populated.bin"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

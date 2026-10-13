#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

# Standard lighting: the shared vectors' conversion of the compiled tables.
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/standard.bin" <<'PYTHON'
import json, pathlib, sys
vectors = json.loads(pathlib.Path(sys.argv[1]).read_text())
standard = next(case for case in vectors["conversion"] if case["name"] == "standard")
pathlib.Path(sys.argv[2]).write_bytes(bytes.fromhex(standard["hex"]))
PYTHON

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" \
        -DNOAH_LIGHTING_SCHEMA_ALL_KINDS \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/effective_rgb_runtime_test.c" \
        "$ROOT/users/noah/lib/profile/runtime/effective_rgb_runtime.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/effective_rgb_runtime_test_$name"
    "$BUILD_DIR/effective_rgb_runtime_test_$name" "$BUILD_DIR/standard.bin"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic \
    -mcpu=cortex-m0plus -mthumb \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/runtime/effective_rgb_runtime.c" \
    -o "$BUILD_DIR/effective_rgb_runtime_cortex_m0plus.o"

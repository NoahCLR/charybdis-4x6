#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

python3 - "$ROOT/tests/fixtures/rgb_domain_v3.json" "$BUILD_DIR/vectors.txt" <<'PY'
import json, pathlib, sys
fixture = json.loads(pathlib.Path(sys.argv[1]).read_text())
limits = fixture["limits"]
lines = [f"{limits['compiledStageMask']} {limits['logicalLayerCount']} {limits['maximumBrightness']} {limits['tapBranchColorCount']} {limits['supportedPdModeMask']}"]
lines += [f"OK 0 {case['name']} {case['hex']}" for case in fixture["valid"]]
lines += [f"{case['error']['code']} {case['error']['offset']} {case['name']} {case['hex']}" for case in fixture["invalid"]]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PY

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic -DNOAH_PD_PROFILE_ENABLE "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_rgb_v3_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/profile_rgb_v3_test_$name"
    "$BUILD_DIR/profile_rgb_v3_test_$name" "$BUILD_DIR/vectors.txt"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic -DNOAH_PD_PROFILE_ENABLE \
    -mcpu=cortex-m0plus -mthumb \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -c "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    -o "$BUILD_DIR/profile_rgb_v3_cortex_m0plus.o"

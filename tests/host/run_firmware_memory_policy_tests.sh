#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_firmware_memory_policy.sh"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
check() {
    expected="$1"
    printf '%s\n' "$2" > "$BUILD_DIR/cflags.txt"
    actual="$(noah_firmware_static_ram_policy "$BUILD_DIR/cflags.txt" "${3:-yes}")"
    if [ "$actual" != "$expected" ]; then
        echo "memory policy: expected $expected, got $actual for $2" >&2
        exit 1
    fi
}
test "$(noah_firmware_static_ram_policy "$BUILD_DIR/missing")" = 57344
check 57344 '-DRGB_MATRIX_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE'
check 226304 '-DNOAH_PD_PROFILE_ENABLE'
check 226304 '-DNOAH_PD_PROFILE_ENABLE -DRGB_MATRIX_ENABLE'
check 226304 '-DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE'
profile='-DNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -DRGB_MATRIX_ENABLE'
check 226304 "$profile" no
check 229376 "$profile"
check 229376 "$profile -DNOAH_LIGHTING_TARGET_KEYS_THAT=0"
check 229376 "$profile -DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=0"
check 226304 "$profile -DNOAH_LIGHTING_TARGET_KEYS_THAT=0 -DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=0"
check 57344 '-DXNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -DRGB_MATRIX_ENABLE'
echo 'firmware memory artifact policies: query increment scoped; unrelated and disabled-cache limits preserved'

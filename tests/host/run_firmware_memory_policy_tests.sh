#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_firmware_memory_policy.sh"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
check() {
    expected="$1"
    printf '%s\n' "$2" > "$BUILD_DIR/cflags.txt"
    actual="$(noah_firmware_static_ram_policy "$BUILD_DIR/cflags.txt" "${3:-yes}" "${4:-no}")"
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
check 229376 "$profile -DNOAH_PD_PROFILE_ENABLE=-1"
check 57344 '-DXNOAH_PD_PROFILE_ENABLE -DNOAH_PORTABLE_PROFILE_ENABLE -DRGB_MATRIX_ENABLE'
check 247808 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1" yes yes
check 229376 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1" yes no
check 229376 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=0" yes yes
check 229376 "$profile" yes yes
check 226304 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1" no yes
check 226304 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_TARGET_KEYS_THAT=0 -DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=0" yes yes
# Only the final preprocessor definition selects the configuration. Unknown
# expressions must fail closed rather than grant the larger artifact allowance.
for zero in 0 0U 0L 0x0 '(0)' 00; do
    check 57344 "$profile -DNOAH_PD_PROFILE_ENABLE=$zero" yes yes
    check 226304 "$profile -DNOAH_PORTABLE_PROFILE_ENABLE=$zero" yes yes
    check 226304 "$profile -DRGB_MATRIX_ENABLE=$zero" yes yes
done
check 57344 "$profile -UNOAH_PD_PROFILE_ENABLE"
check 226304 "$profile -UNOAH_PORTABLE_PROFILE_ENABLE"
check 226304 "$profile -URGB_MATRIX_ENABLE"
check 57344 "$profile -DNOAH_PD_PROFILE_ENABLE=1 -DNOAH_PD_PROFILE_ENABLE=0"
check 229376 "$profile -DNOAH_PD_PROFILE_ENABLE=0 -DNOAH_PD_PROFILE_ENABLE=2U"
check 229376 "$profile -UNOAH_PD_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE"
check 57344 "$profile -DNOAH_PD_PROFILE_ENABLE=unknown"
check 229376 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -UNOAH_LIGHTING_EVENT_CAPTURE_ENABLE" yes yes
check 247808 "$profile -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=0 -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=2" yes yes
check 226304 "$profile -UNOAH_LIGHTING_TARGET_KEYS_THAT -UNOAH_LIGHTING_PAINT_WHAT_IT_DOES"
check 57344 "$profile -D NOAH_PD_PROFILE_ENABLE=0"
check 229376 "$profile -U NOAH_PD_PROFILE_ENABLE -D NOAH_PD_PROFILE_ENABLE"
echo 'firmware memory artifact policies: query increment scoped; unrelated and disabled-cache limits preserved'

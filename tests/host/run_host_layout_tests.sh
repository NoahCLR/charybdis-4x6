#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

python3 "$ROOT/tools/host_layouts/build.py" --check

# Layout 0 must equal QMK's own US send_string tables, read from the pinned QMK.
{
    printf '#include <stdint.h>\n#include "keycodes.h"\n#define PROGMEM\n'
    printf '#ifndef XXXXXXX\n#define XXXXXXX KC_NO\n#endif\n'
    sed -n '/^#define KCLUT_ENTRY/,/<< 7 )/p' "$QMK_ROOT/quantum/send_string/send_string.h"
    awk '/^__attribute__\(\(weak\)\) const uint8_t ascii_to_(shift|altgr|dead|keycode)_lut/,/^};/' \
        "$QMK_ROOT/quantum/send_string/send_string.c" |
        sed -e 's/^__attribute__((weak)) //' -e 's/ascii_to_\([a-z]*\)_lut/qmk_us_\1_lut/'
} > "$BUILD_DIR/qmk_us_luts.c"

# The Windows layouts must type ASCII as QMK's own send_string tables for them do.
for lang in german french uk us_international; do
    {
        printf '#include <stdint.h>\n#include "keycodes.h"\n#include "keymap_%s.h"\n#define PROGMEM\n' "$lang"
        printf '#ifndef XXXXXXX\n#define XXXXXXX KC_NO\n#endif\n'
        sed -n '/^#define KCLUT_ENTRY/,/<< 7 )/p' "$QMK_ROOT/quantum/send_string/send_string.h"
        header="$QMK_ROOT/quantum/keymap_extras/sendstring_$lang.h"
        for table in shift altgr dead keycode; do
            # A table the language omits is QMK's weak US default.
            if grep -q "ascii_to_${table}_lut" "$header"; then source="$header"; else source="$QMK_ROOT/quantum/send_string/send_string.c"; fi
            awk "/^(__attribute__\\(\\(weak\\)\\) )?const uint8_t ascii_to_${table}_lut/,/^};/" "$source" |
                sed -e 's/^__attribute__((weak)) //' -e "s/ascii_to_${table}_lut/qmk_${lang}_${table}_lut/"
        done
    } > "$BUILD_DIR/qmk_${lang}_luts.c"
done

for flags in "" "-fsanitize=address,undefined -fno-omit-frame-pointer"; do
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags \
        -I"$QMK_ROOT/quantum" \
        -I"$QMK_ROOT/quantum/keymap_extras" \
        -I"$ROOT/users/noah/lib/macro" \
        -I"$ROOT" \
        "$ROOT/tests/host/host_layout_test.c" \
        "$ROOT/users/noah/lib/macro/host_layout.c" \
        "$ROOT/users/noah/lib/macro/host_layout_tables.c" \
        "$BUILD_DIR/qmk_us_luts.c" \
        "$BUILD_DIR/qmk_german_luts.c" "$BUILD_DIR/qmk_french_luts.c" \
        "$BUILD_DIR/qmk_uk_luts.c" "$BUILD_DIR/qmk_us_international_luts.c" \
        -o "$BUILD_DIR/host_layout_test"
    "$BUILD_DIR/host_layout_test"
done

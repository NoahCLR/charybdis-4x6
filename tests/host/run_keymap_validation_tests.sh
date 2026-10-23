#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/keymap_validation_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DCOMBO_ENABLE \
    -DCONSOLE_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    -include "$ROOT/tests/host/include/noah_compile_config.h" \
    "$ROOT/tests/host/keymap_validation_test.c" \
    "$ROOT/users/noah/lib/action/action_kind.c" \
    "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c" \
    "$ROOT/users/noah/lib/key/behavior/keymap_validation.c" \
    -o "$BIN"

"$BIN"

# CUSTOM_KEYS shortcut rows (D-F26): good rows compile into the table, and
# each bad row is a compile error.
shortcut_rows() {
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$ROOT/tests/host/include/noah_compile_config.h" \
        "$@" \
        "$ROOT/tests/host/keymap_custom_key_shortcut_rows.c" \
        -o "$BUILD_DIR/shortcut_rows"
}
shortcut_rows
"$BUILD_DIR/shortcut_rows"
for bad in \
    'SHORTCUT(CUSTOM_KEY_9, "", MOD_RALT, KC_A)' \
    'SHORTCUT(CUSTOM_KEY_9, "", MOD_BIT(KC_LCTL), KC_A)' \
    'SHORTCUT(CUSTOM_KEY_9, "", 0, KC_A)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_MODS(KC_A), KC_B)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_MODS(KC_LCTL, KC_A), KC_B)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_NO_MODS, KC_LCTL)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_NO_MODS, KC_AUDIO_MUTE)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_NO_MODS, MS_UP)' \
    'SHORTCUT(CUSTOM_KEY_9, "", SHORTCUT_NO_MODS, KC_NO)' \
    'SHORTCUT(MACRO_0, "", SHORTCUT_NO_MODS, KC_A)'; do
    if shortcut_rows "-DSHORTCUT_ROW_BAD(SHORTCUT)=$bad" 2>/dev/null; then
        echo "a bad CUSTOM_KEYS shortcut row compiled: $bad" >&2
        exit 1
    fi
done
echo "keymap custom key shortcut rows: bad rows refused"

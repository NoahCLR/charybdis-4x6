#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
KEYMAP_PATH_ARG="${1:-keyboards/bastardkb/charybdis/4x6/keymaps/noah}"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

case "$KEYMAP_PATH_ARG" in
    /*) KEYMAP_PATH_ABS="$KEYMAP_PATH_ARG" ;;
    *) KEYMAP_PATH_ABS="$ROOT/$KEYMAP_PATH_ARG" ;;
esac

if [ ! -f "$KEYMAP_PATH_ABS/keymap.c" ]; then
    echo "keymap.c not found: $KEYMAP_PATH_ABS/keymap.c" >&2
    exit 1
fi
if [ ! -f "$KEYMAP_PATH_ABS/rgb_config.c" ]; then
    echo "rgb_config.c not found: $KEYMAP_PATH_ABS/rgb_config.c" >&2
    exit 1
fi
if [ ! -f "$KEYMAP_PATH_ABS/config.h" ]; then
    echo "config.h not found: $KEYMAP_PATH_ABS/config.h" >&2
    exit 1
fi

write_config_variant() {
    name="$1"
    config_header="$BUILD_DIR/${name}_compile_config.h"

    {
        printf '#pragma once\n\n'
        printf '#include "%s/users/noah/config.h"\n' "$ROOT"
        printf '#include "%s/config.h"\n' "$KEYMAP_PATH_ABS"

        if [ "$name" = "no_rgb_feedback" ]; then
            cat <<'EOF'

#ifdef RGB_PD_MODE_FEEDBACK_ENABLE
#    undef RGB_PD_MODE_FEEDBACK_ENABLE
#endif

#ifdef RGB_COMBO_FEEDBACK_ENABLE
#    undef RGB_COMBO_FEEDBACK_ENABLE
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#    undef RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE
#endif

#ifdef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#    undef RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS
#endif

#ifdef RGB_AUTOMOUSE_GRADIENT_ENABLE
#    undef RGB_AUTOMOUSE_GRADIENT_ENABLE
#endif

#ifdef AUTOMOUSE_RGB_DEAD_TIME
#    undef AUTOMOUSE_RGB_DEAD_TIME
#endif
EOF
        fi
    } >"$config_header"

    printf '%s\n' "$config_header"
}

run_validation_variant() {
    name="$1"
    config_header="$(write_config_variant "$name")"
    bin="$BUILD_DIR/real_profile_validation_${name}_test"

    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DCONSOLE_ENABLE \
        -DCOMBO_ENABLE \
        -DSPLIT_KEYBOARD \
        -DPOINTING_DEVICE_ENABLE \
        -DRGB_MATRIX_ENABLE \
        -DRGB_MATRIX_WS2812 \
        -DQMK_STUB_SUPPRESS_LAYER_COUNT \
        -DQMK_KEYBOARD_H='"noah_real_profile_keyboard.h"' \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        -include "$config_header" \
        "$ROOT/tests/host/real_profile_validation_test.c" \
        "$KEYMAP_PATH_ABS/keymap.c" \
        "$KEYMAP_PATH_ABS/rgb_config.c" \
        "$ROOT/users/noah/lib/action/action_kind.c" \
        "$ROOT/users/noah/lib/key/behavior/key_behavior_lookup.c" \
        "$ROOT/users/noah/lib/key/behavior/keymap_validation.c" \
        "$ROOT/users/noah/lib/macro/macro_dispatch.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
        "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
        -o "$bin"

    "$bin"
}

run_validation_variant default
run_validation_variant no_rgb_feedback

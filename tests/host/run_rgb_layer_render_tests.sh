#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/rgb_layer_render_test"
BIN_END_FILL_UNPAINTED="$BUILD_DIR/rgb_layer_render_test_end_fill_unpainted"
BIN_END_OVERRIDE="$BUILD_DIR/rgb_layer_render_test_end_override"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPD_MODE_VOLUME_DPI=0 \
    -DPD_MODE_BRIGHTNESS_DPI=0 \
    -DPD_MODE_ZOOM_DPI=400 \
    -DPD_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/rgb_runtime.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPD_MODE_VOLUME_DPI=0 \
    -DPD_MODE_BRIGHTNESS_DPI=0 \
    -DPD_MODE_ZOOM_DPI=400 \
    -DPD_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/rgb_runtime.c" \
    -o "$BIN_END_FILL_UNPAINTED"

"$BIN_END_FILL_UNPAINTED"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DPOINTING_DEVICE_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPD_MODE_VOLUME_DPI=0 \
    -DPD_MODE_BRIGHTNESS_DPI=0 \
    -DPD_MODE_ZOOM_DPI=400 \
    -DPD_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/rgb_runtime.c" \
    -o "$BIN_END_OVERRIDE"

"$BIN_END_OVERRIDE"

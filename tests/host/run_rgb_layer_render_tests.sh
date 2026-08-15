#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/rgb_layer_render_test"
BIN_WORKLOAD="$BUILD_DIR/rgb_render_workload_test"
BIN_BASE_UNDERLAY="$BUILD_DIR/rgb_base_underlay_test"
BIN_END_FILL_UNPAINTED="$BUILD_DIR/rgb_layer_render_test_end_fill_unpainted"
BIN_END_OVERRIDE="$BUILD_DIR/rgb_layer_render_test_end_override"
BIN_KEY_HALF="$BUILD_DIR/rgb_layer_render_test_key_half"
BIN_KEY="$BUILD_DIR/rgb_layer_render_test_key"
BIN_KEY_LEFT="$BUILD_DIR/rgb_layer_render_test_key_left"
BIN_KEY_RIGHT="$BUILD_DIR/rgb_layer_render_test_key_right"
BIN_FEEDBACK_GROUPS="$BUILD_DIR/rgb_layer_render_test_feedback_groups"
BIN_LAYER_GROUPS="$BUILD_DIR/rgb_layer_render_test_layer_groups"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_LED_COUNT=4 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_base_underlay_test.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    -o "$BIN_BASE_UNDERLAY"

"$BIN_BASE_UNDERLAY"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_RUNTIME_RENDER_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DRGB_MATRIX_SPLIT=1 \
    -DRGB_MATRIX_LED_PROCESS_LIMIT=12 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=29 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPD_MODE_VOLUME_DPI=0 \
    -DPD_MODE_BRIGHTNESS_DPI=0 \
    -DPD_MODE_ZOOM_DPI=400 \
    -DPD_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=58 \
    -DRGB_LAYER_RENDER_TEST_WORKLOAD=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_WORKLOAD"

"$BIN_WORKLOAD"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_FEEDBACK_GROUPS"

"$BIN_FEEDBACK_GROUPS"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_LAYER_GROUPS=1 \
    -DRGB_LAYER_STAGE_TEST_BACKEND \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_LAYER_GROUPS"

LAYER_GROUP_STATUS=0
for scenario in inherit universal no-base ordering chunks; do
    if ! "$BIN_LAYER_GROUPS" "$scenario"; then
        LAYER_GROUP_STATUS=1
    fi
done
if [ "$LAYER_GROUP_STATUS" -ne 0 ]; then
    exit "$LAYER_GROUP_STATUS"
fi

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_END_FILL_UNPAINTED"

"$BIN_END_FILL_UNPAINTED"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_END_OVERRIDE"

"$BIN_END_OVERRIDE"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_KEY_HALF"

"$BIN_KEY_HALF"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_KEY"

"$BIN_KEY"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_KEY_LEFT"

"$BIN_KEY_LEFT"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_PD_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
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
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/users/noah/lib/compat/qmk_contract.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pd_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_KEY_RIGHT"

"$BIN_KEY_RIGHT"

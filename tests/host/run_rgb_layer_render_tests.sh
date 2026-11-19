#!/bin/sh

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
noah_host_require_tool rg
BUILD_DIR="$(mktemp -d)"
BIN="$BUILD_DIR/rgb_layer_render_test"
BIN_WORKLOAD="$BUILD_DIR/rgb_render_workload_test"
BIN_IDLE_TIMEOUT="$BUILD_DIR/rgb_layer_render_test_idle_timeout"
BIN_BASE_UNDERLAY="$BUILD_DIR/rgb_base_underlay_test"
BIN_PD_ACTIVE_PROFILE="$BUILD_DIR/rgb_pd_active_profile_test"
BIN_COMBO_ACTIVE_PROFILE="$BUILD_DIR/rgb_combo_active_profile_test"
BIN_KEY_ACTIVE_PROFILE="$BUILD_DIR/rgb_key_active_profile_test"
BIN_AUTOCORRECT_ACTIVE_PROFILE="$BUILD_DIR/rgb_autocorrect_active_profile_test"
BIN_END_FILL_UNPAINTED="$BUILD_DIR/rgb_layer_render_test_end_fill_unpainted"
BIN_END_OVERRIDE="$BUILD_DIR/rgb_layer_render_test_end_override"
BIN_KEY_HALF="$BUILD_DIR/rgb_layer_render_test_key_half"
BIN_KEY="$BUILD_DIR/rgb_layer_render_test_key"
BIN_KEY_LEFT="$BUILD_DIR/rgb_layer_render_test_key_left"
BIN_KEY_RIGHT="$BUILD_DIR/rgb_layer_render_test_key_right"
BIN_FEEDBACK_GROUPS="$BUILD_DIR/rgb_layer_render_test_feedback_groups"
BIN_LAYER_GROUPS="$BUILD_DIR/rgb_layer_render_test_layer_groups"
BIN_FIXTURE="$BUILD_DIR/rgb_render_fixture_test"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DAUTO_MOUSE_TIME=900 \
    -DRGB_MATRIX_LED_COUNT=4 \
    -DNOAH_ACTIVE_PROFILE_RGB_RUNTIME_TEST_BACKEND \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_base_underlay_test.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    -o "$BIN_BASE_UNDERLAY"

"$BIN_BASE_UNDERLAY" "$ROOT/tests/fixtures/rgb_domain_v1.fixture"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DMATRIX_ROWS=2 \
    -DMATRIX_COLS=2 \
    -DRGB_MATRIX_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LEFT_LED_COUNT=4 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_pd_active_profile_test.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    -o "$BIN_PD_ACTIVE_PROFILE"

"$BIN_PD_ACTIVE_PROFILE" "$ROOT/tests/fixtures/rgb_domain_v1.fixture"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DMATRIX_ROWS=2 \
    -DMATRIX_COLS=1 \
    -DRGB_MATRIX_ENABLE \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LEFT_LED_COUNT=4 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_combo_active_profile_test.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    -o "$BIN_COMBO_ACTIVE_PROFILE"

"$BIN_COMBO_ACTIVE_PROFILE" "$ROOT/tests/fixtures/rgb_domain_v1.fixture"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DMATRIX_ROWS=2 \
    -DMATRIX_COLS=4 \
    -DRGB_MATRIX_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LEFT_LED_COUNT=4 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_key_active_profile_test.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    -o "$BIN_KEY_ACTIVE_PROFILE"

"$BIN_KEY_ACTIVE_PROFILE" "$ROOT/tests/fixtures/rgb_domain_v1.fixture"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DQMK_STUB_SUPPRESS_LAYER_COUNT \
    -DLAYER_COUNT=3 \
    -DMATRIX_ROWS=2 \
    -DMATRIX_COLS=4 \
    -DRGB_MATRIX_ENABLE \
    -DRGB_AUTOCORRECT_FEEDBACK_ENABLE \
    -DRGB_AUTOCORRECT_FEEDBACK_FLASH_MS=200 \
    -DRGB_ACTIVE_PROFILE_CONFIG_TEST_BACKEND \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LEFT_LED_COUNT=4 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_autocorrect_active_profile_test.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_autocorrect_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    -o "$BIN_AUTOCORRECT_ACTIVE_PROFILE"

"$BIN_AUTOCORRECT_ACTIVE_PROFILE"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DRGB_ACTIVE_PROFILE_CONFIG_TEST_BACKEND \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DRGB_AUTOCORRECT_FEEDBACK_ENABLE \
    -DRGB_AUTOCORRECT_FEEDBACK_FLASH_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_autocorrect_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN"

"$BIN"

cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DNOAH_PORTABLE_PROFILE_ENABLE \
    -DRGB_LAYER_RENDER_TEST_IDLE_TIMEOUT=1 \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_IDLE_TIMEOUT"

"$BIN_IDLE_TIMEOUT"

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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=29 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=58 \
    -DRGB_LAYER_RENDER_TEST_WORKLOAD=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_LAYER_GROUPS=1 \
    -DRGB_LAYER_STAGE_TEST_BACKEND \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_LAYER_GROUPS"

LAYER_GROUP_STATUS=0
for scenario in inherit universal no-base ordering chunks passthrough-link; do
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_AUTOMOUSE_END_FILL_UNPAINTED=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_AUTOMOUSE_END_OVERRIDE=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_KEY=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_LEFT_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
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
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DRGB_LEFT_LED_COUNT=4 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=8 \
    -DRGB_LAYER_RENDER_TEST_KEY_FEEDBACK_RIGHT_HALF=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    -o "$BIN_KEY_RIGHT"

"$BIN_KEY_RIGHT"

# The shared drawing cases, on the real board's tables, through every stage
# with QMK's own hsv_to_rgb. The JSON's board must be the keyboard's at the
# BK commit qmk-pin.json names, whatever QMK_ROOT has checked out.
python3 - "$ROOT" "$QMK_ROOT" "$BUILD_DIR/rgb_render_v1.txt" <<'PY'
import json, pathlib, subprocess, sys
root, qmk, out = (pathlib.Path(arg) for arg in sys.argv[1:])
sys.path.insert(0, str(root / "tests/host"))
import profile_logical_form
fixture = json.loads((root / "tests/fixtures/rgb_render_v1.json").read_text())
pin = json.loads((root / "qmk-pin.json").read_text())["commit"]
shown = subprocess.run(["git", "-C", str(qmk), "show", f"{pin}:keyboards/bastardkb/charybdis/4x6/keyboard.json"], capture_output=True, text=True)
if shown.returncode != 0:
    sys.exit(f"{qmk} does not hold the pinned BK commit {pin}: {shown.stderr.strip()}")
keyboard = json.loads(shown.stdout)
board = fixture["board"]
keys = [key["matrix"] for key in keyboard["layouts"]["LAYOUT"]["layout"]]
leds = [led.get("matrix") for led in keyboard["rgb_matrix"]["layout"]]
# Every key carries one LED, and an LED filed under a matrix position is
# filed under a key's: a table that files a key's LED elsewhere leaves that
# key dark wherever lighting follows keys.
faults = [f"layout key {index} (matrix {matrix}) carries {leds.count(matrix)} LEDs" for index, matrix in enumerate(keys) if leds.count(matrix) != 1]
faults += [f"LED {index} is filed under matrix {matrix}, which no layout key uses" for index, matrix in enumerate(leds) if matrix is not None and matrix not in keys]
if faults:
    sys.exit(f"keyboard.json at BK {pin}: " + "; ".join(faults))
assert [key["matrix"] for key in board["keys"]] == keys, "the fixture's key order is not the keyboard's LAYOUT"
assert [led["matrix"] for led in board["leds"]] == leds, "the fixture's LEDs are not the keyboard's rgb_matrix.layout"
assert keyboard["rgb_matrix"]["split_count"] == [board["leftLeds"], len(leds) - board["leftLeds"]]
for key in board["keys"]:
    assert key["leds"] == [index for index, matrix in enumerate(leds) if matrix == key["matrix"]], key
    assert key["half"] == ("left" if key["matrix"][0] < 5 else "right"), key
for led in board["leds"]:
    assert led["half"] == ("left" if led["led"] < board["leftLeds"] else "right"), led
for name, profile in fixture["profiles"].items():
    assert profile_logical_form.rgb(bytes.fromhex(profile["hex"])) == profile["lighting"], f"profile {name}: its lighting is not its bytes"
limits = fixture["limits"]
lines = [f"limits {limits['compiledStageMask']} {limits['logicalLayerCount']} {limits['maximumBrightness']} {limits['tapBranchColorCount']} {limits['supportedPdModeMask']}"]
lines += [f"led {index} {matrix[0]} {matrix[1]}" if matrix else f"led {index} 255 255" for index, matrix in enumerate(leds)]
for layer, mapped in fixture["keymap"]["mapped"].items():
    lines += [f"map {layer} {keys[key][0]} {keys[key][1]}" for key in mapped]
lines.append("background " + " ".join(map(str, fixture["background"]["hsv"])))
def positions(layout):
    return [str(len(layout))] + [f"{keys[key][0]} {keys[key][1]}" for key in layout]
for case in fixture["cases"]:
    state = case["state"]
    lines += [f"case {case['name']}", "rgb " + fixture["profiles"][case["profile"]]["hex"], f"lighting {int(state['lightingOn'])} {int(state['idle'])}"]
    lines.append(f"layers {sum(1 << layer for layer in state['layersOn'])}")
    if state["pointing"]:
        pointing = state["pointing"]
        owner = keys[pointing["key"]] if pointing["key"] is not None else [255, 255]
        link = sum(1 << layer for layer in pointing["link"]["linked"] + pointing["link"]["turnsOn"])
        lines.append(f"pointing {pointing['slot']} {owner[0]} {owner[1]} {link}")
    if state["combo"]:
        lines.append(" ".join(["combo", str(int(state["combo"]["runsPointingMode"]))] + positions(state["combo"]["keys"])))
    if state["behaviour"]:
        behaviour = state["behaviour"]
        lines.append(f"behaviour {behaviour['case']} {behaviour.get('tapCount', 0)} {keys[behaviour['key']][0]} {keys[behaviour['key']][1]}")
    if state["replacement"]:
        lines.append(" ".join(["replacement", state["replacement"]["case"]] + positions(state["replacement"]["keys"])))
    assert len(case["leds"]) == len(leds), case["name"]
    lines += [f"expect {index} {led['rgb'][0]} {led['rgb'][1]} {led['rgb'][2]}" for index, led in enumerate(case["leds"])]
    lines.append("end")
out.write_text("\n".join(lines) + "\n")
PY

cc -std=c11 -c -w "$QMK_ROOT/quantum/color.c" -o "$BUILD_DIR/qmk_color.o"
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -DNOAH_RUNTIME_DIAG_TEST_BACKEND \
    -DNOAH_PORTABLE_PROFILE_ENABLE \
    -DRGB_MATRIX_ENABLE \
    -DRGB_MATRIX_WS2812 \
    -DRGB_MATRIX_SPLIT=1 \
    -DRGB_MATRIX_LED_PROCESS_LIMIT=12 \
    -DCOMBO_ENABLE \
    -DRGB_COMBO_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_ENABLE \
    -DRGB_POINTING_MODE_FEEDBACK_ENABLE \
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE \
    -DRGB_AUTOMOUSE_GRADIENT_ENABLE \
    -DRGB_POINTING_MODE_ACTIVE_HALF_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE \
    -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 \
    -DRGB_AUTOCORRECT_FEEDBACK_ENABLE \
    -DRGB_AUTOCORRECT_FEEDBACK_FLASH_MS=200 \
    -DSPLIT_TRANSACTION_IDS_USER \
    -DMATRIX_ROWS=10 \
    -DMATRIX_COLS=6 \
    -DRGB_LEFT_LED_COUNT=29 \
    -DAUTO_MOUSE_TIME=120u \
    -DAUTOMOUSE_RGB_DEAD_TIME=20u \
    -DPOINTING_MODE_VOLUME_DPI=0 \
    -DPOINTING_MODE_BRIGHTNESS_DPI=0 \
    -DPOINTING_MODE_ZOOM_DPI=400 \
    -DPOINTING_MODE_ARROW_DPI=400 \
    -DRGB_MATRIX_LED_COUNT=58 \
    -DRGB_LAYER_RENDER_TEST_FIXTURE=1 \
    -I"$ROOT" \
    -I"$ROOT/users/noah" \
    -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/rgb_layer_render_test.c" \
    "$ROOT/tests/host/profile_macro_provider_stub.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_autocorrect_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_active_profile_config.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c" \
    "$ROOT/users/noah/lib/profile/runtime/active_profile_rgb_runtime.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
    "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
    "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
    "$BUILD_DIR/qmk_color.o" \
    -o "$BIN_FIXTURE"

"$BIN_FIXTURE" "$BUILD_DIR/rgb_render_v1.txt"

if rg -n '\b(layer_colors|layer_led_groups|layer_led_group_count|automouse_fade_end_config|pointing_mode_colors|pointing_mode_color_count|pointing_mode_led_groups|pointing_mode_led_group_count|combo_feedback_colors|combo_feedback_led_groups|combo_feedback_led_group_count|key_behavior_feedback_colors|key_behavior_feedback_led_groups|key_behavior_feedback_led_group_count)\b' \
    "$ROOT/users/noah/lib/rgb/stages/rgb_layer_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_pointing_mode_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_key_feedback_stage.c" \
    "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse_stage.c" \
    "$ROOT/users/noah/lib/rgb/stages/rgb_preview_stage.c" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c"
then
    echo "direct authored RGB reads escaped the active-profile config adapter" >&2
    exit 1
fi

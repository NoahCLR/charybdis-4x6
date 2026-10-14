#!/bin/sh
# The lighting runtime's render contract, scenario by scenario: authored
# version-4 tables become Standard lighting and the rule engine must paint
# what those tables always meant, on the USB half and on the other half from
# the synced lighting facts.

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
noah_host_require_tool rg
. "$ROOT/tests/host/noah_source_manifest.sh"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

LIGHTING_SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in
        users/noah/lib/rgb/lighting/*) LIGHTING_SOURCES="$LIGHTING_SOURCES $ROOT/$src" ;;
    esac
done
PROFILE_SOURCES="$ROOT/users/noah/lib/profile/schema/profile_rgb_compiled_v1.c
    $ROOT/users/noah/lib/profile/runtime/effective_rgb_runtime.c
    $ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c
    $ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c
    $ROOT/users/noah/lib/profile/schema/profile_lighting_v1_convert.c
    $ROOT/users/noah/lib/profile/schema/profile_reader.c"
SCENE_FLAGS="-DNOAH_LIGHTING_FACT_COMBO_HELD=0 -DNOAH_LIGHTING_FACT_BEHAVIOUR=0 -DNOAH_LIGHTING_PAINT_OUTCOME=0 -DNOAH_RUNTIME_DIAG_TEST_BACKEND -DRGB_RUNTIME_RENDER_TEST_BACKEND -DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812
    -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE -DPOINTING_DEVICE_ENABLE -DRGB_PD_MODE_FEEDBACK_ENABLE
    -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE -DRGB_PD_MODE_ACTIVE_HALF_ENABLE
    -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 -DSPLIT_TRANSACTION_IDS_USER
    -DAUTO_MOUSE_TIME=120u -DAUTOMOUSE_RGB_DEAD_TIME=20u
    -DPD_MODE_VOLUME_DPI=0 -DPD_MODE_BRIGHTNESS_DPI=0 -DPD_MODE_ZOOM_DPI=400 -DPD_MODE_ARROW_DPI=400"

build_scene() {
    name="$1"
    shift
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' $SCENE_FLAGS "$@" \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/rgb_layer_render_test.c" \
        "$ROOT/users/noah/lib/compat/qmk_contract.c" \
        "$ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_config_defaults.c" \
        $PROFILE_SOURCES $LIGHTING_SOURCES \
        "$ROOT/users/noah/lib/rgb/core/rgb_validation.c" \
        "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" \
        "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" \
        -o "$BUILD_DIR/$name"
}

SMALL="-DRGB_LEFT_LED_COUNT=4 -DRGB_MATRIX_LED_COUNT=8"

build_scene scene $SMALL
"$BUILD_DIR/scene"

build_scene workload -DRGB_MATRIX_SPLIT=1 -DRGB_MATRIX_LED_PROCESS_LIMIT=12 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_LED_COUNT=58 -DRGB_LAYER_RENDER_TEST_WORKLOAD=1
"$BUILD_DIR/workload"

build_scene feedback_groups $SMALL -DRGB_LAYER_RENDER_TEST_FEEDBACK_GROUPS=1
"$BUILD_DIR/feedback_groups"

build_scene layer_groups $SMALL -DRGB_LAYER_RENDER_TEST_LAYER_GROUPS=1
LAYER_GROUP_STATUS=0
for scenario in inherit universal no-base ordering chunks; do
    if ! "$BUILD_DIR/layer_groups" "$scenario"; then
        LAYER_GROUP_STATUS=1
    fi
done
if [ "$LAYER_GROUP_STATUS" -ne 0 ]; then
    exit "$LAYER_GROUP_STATUS"
fi

for variant in AUTOMOUSE_END_FILL_UNPAINTED AUTOMOUSE_END_OVERRIDE KEY_FEEDBACK_KEY_HALF KEY_FEEDBACK_KEY KEY_FEEDBACK_LEFT_HALF KEY_FEEDBACK_RIGHT_HALF; do
    build_scene "scene_$variant" $SMALL "-DRGB_LAYER_RENDER_TEST_$variant=1"
    "$BUILD_DIR/scene_$variant"
done

# The engine reads the published program only, never the authored tables.
if rg -n '\b(layer_colors|layer_led_groups|layer_led_group_count|automouse_fade_end_config|pd_mode_colors|pd_mode_color_count|pd_mode_led_groups|pd_mode_led_group_count|combo_feedback_colors|combo_feedback_led_groups|combo_feedback_led_group_count|key_behavior_feedback_colors|key_behavior_feedback_led_groups|key_behavior_feedback_led_group_count)\b' \
    "$ROOT/users/noah/lib/rgb/lighting" \
    "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c"
then
    echo "direct authored RGB reads escaped the lighting program" >&2
    exit 1
fi
echo "rgb render scenarios passed"

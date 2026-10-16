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
SCENE_FLAGS="-DRGB_MATRIX_SLEEP -DRGB_RUNTIME_CLOCK_TEST_BACKEND -I$ROOT/tests/host/rgb_clock_include -DNOAH_LIGHTING_FACT_COMBO_HELD=0 -DNOAH_LIGHTING_FACT_BEHAVIOUR=0 -DNOAH_LIGHTING_PAINT_OUTCOME=0 -DNOAH_RUNTIME_DIAG_TEST_BACKEND -DRGB_RUNTIME_RENDER_TEST_BACKEND -DRGB_MATRIX_ENABLE -DRGB_MATRIX_WS2812
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
        "$ROOT/tests/host/${RENDER_TEST_SOURCE:-rgb_layer_render_test.c}" \
        "$ROOT/users/noah/lib/compat/qmk_contract.c" \
        "$ROOT/users/noah/lib/compat/qmk_lighting_snapshot_transport.c" \
        "$ROOT/users/noah/lib/compat/qmk_lighting_live.c" \
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

build_scene scene_enabled $SMALL -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_TIMING_LINGER=1 -DRPC_M2S_BUFFER_SIZE=128
"$BUILD_DIR/scene_enabled"

build_scene scene_sanitized $SMALL -fsanitize=address,undefined -fno-omit-frame-pointer
"$BUILD_DIR/scene_sanitized"

build_scene cached_scene $SMALL -DNOAH_LIGHTING_TARGET_KEYS_THAT=1 -DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=1
"$BUILD_DIR/cached_scene"
build_scene cached_scene_sanitized $SMALL -DNOAH_LIGHTING_TARGET_KEYS_THAT=1 -DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=1 -fsanitize=address,undefined -fno-omit-frame-pointer
"$BUILD_DIR/cached_scene_sanitized"

for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    RENDER_TEST_SOURCE=rgb_chunk_composition_test.c build_scene "chunks_$variant" -DRGB_MATRIX_SPLIT=1 -DRGB_MATRIX_LED_PROCESS_LIMIT=12 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_LED_COUNT=58 -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_TIMING_LINGER=1 -DNOAH_LIGHTING_TARGET_KEYS_THAT=1 -DRPC_M2S_BUFFER_SIZE=128 $flags
    "$BUILD_DIR/chunks_$variant"
done
RENDER_TEST_SOURCE=rgb_layer_render_test.c

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

# Compile the pinned native effect bodies and RGB task/control state machine,
# not a handwritten approximation of completion/flush. The outer fixture uses
# the target's proved once-per-loop keyboard_task -> housekeeping order.
python3 - "$QMK_ROOT" "$BUILD_DIR" <<'NATIVE'
from pathlib import Path
import sys,re
qmk=Path(sys.argv[1]);out=Path(sys.argv[2]);src=(qmk/'quantum/rgb_matrix/rgb_matrix.c').read_text()
def function(text,name):
    m=re.search(r'^.*?\b'+name+r'\([^;]*?\)\s*\{',text,re.M)
    assert m,name
    begin=m.start();at=text.index('{',m.start());depth=1;end=at+1
    while depth:
        depth += (text[end]=='{')-(text[end]=='}');end+=1
    return text[begin:end]+'\n'
# Exact selected primary-source definitions; the fixture supplies hardware,
# scheduling and seeded randomness, not altered effect/task implementations.
names=['rgb_matrix_get_limits','rgb_matrix_none','rgb_task_timers','rgb_task_sync','rgb_task_start','rgb_task_render','rgb_task_flush','rgb_matrix_task','rgb_matrix_indicators','rgb_matrix_indicators_advanced','rgb_matrix_set_suspend_state','rgb_matrix_get_suspend_state','rgb_matrix_is_enabled','rgb_matrix_get_mode','rgb_matrix_toggle_eeprom_helper','rgb_matrix_enable_noeeprom','rgb_matrix_disable_noeeprom','rgb_matrix_mode_eeprom_helper']
(out/'native_rgb_tasks.inc').write_text('\n'.join(function(src,n) for n in names))
(out/'native_rgb_limits.inc').write_text(function((qmk/'quantum/rgb_matrix/rgb_matrix.h').read_text(),'rgb_matrix_check_finished_leds'))
(out/'rgb_matrix_effects.inc').write_text('RGB_MATRIX_EFFECT(RAINDROPS, 0)\nRGB_MATRIX_EFFECT(JELLYBEAN_RAINDROPS, 0)\nRGB_MATRIX_EFFECT(DIGITAL_RAIN, 0)\n')
for name in ['raindrops','jellybean_raindrops','digital_rain']:
    (out/(name+'_anim.h')).write_text((qmk/('quantum/rgb_matrix/animations/'+name+'_anim.h')).read_text())
# Fail source-backed ownership tests if the target loop/flush assumptions drift.
main=(qmk/'quantum/main.c').read_text();keyboard=(qmk/'quantum/keyboard.c').read_text()
loop=main[main.index('while (true)'):];assert loop.index('protocol_keyboard_task();')<loop.index('housekeeping_task();')
k=function(keyboard,'keyboard_task');assert k.count('rgb_matrix_task();')==1
ws=(qmk/'platforms/chibios/drivers/vendor/RP/RP2040/ws2812_vendor.c').read_text()
w=function(ws,'ws2812_flush');assert w.index('WS2812_BUFFER[i] =')<w.index('dmaChannelEnableX(')
assert 'rgb_matrix_set_color' not in function(src,'rgb_task_start')
suspend=function((qmk/'quantum/quantum.c').read_text(),'suspend_power_down_quantum')
assert suspend.index('suspend_power_down_kb();')<suspend.index('rgb_matrix_set_suspend_state(true);')
print('pinned native effect/task/loop/flush-copy/suspend source boundaries verified')
NATIVE
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    for geometry in chunk full asymmetric; do
        case "$geometry" in
            chunk) bounds="-DRGB_MATRIX_LED_PROCESS_LIMIT=12 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_LED_COUNT=58" ;;
            full) bounds="-DRGB_MATRIX_LED_PROCESS_LIMIT=0 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_LED_COUNT=58" ;;
            asymmetric) bounds="-DRGB_MATRIX_LED_PROCESS_LIMIT=7 -DRGB_LEFT_LED_COUNT=17 -DRGB_MATRIX_LED_COUNT=58" ;;
        esac
        RENDER_TEST_SOURCE=rgb_effect_owner_test.c build_scene "native_${geometry}_$variant" -I"$BUILD_DIR" -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_LIGHTING_FACT_LAYER_STATUS=0 -DNOAH_LIGHTING_FACT_MACRO_PLAYING=0 -DNOAH_LIGHTING_FACT_KEY_PRESSED=0 -DNOAH_LIGHTING_FACT_HOST_LOCK=0 -DNOAH_LIGHTING_FACT_POINTER=0 -DRGB_NATIVE_EFFECT_FIXTURE '-DRGB_MATRIX_SPLIT={RGB_LEFT_LED_COUNT,RGB_MATRIX_LED_COUNT-RGB_LEFT_LED_COUNT}' $bounds -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_TIMING_LINGER=1 -DNOAH_LIGHTING_TARGET_KEYS_THAT=1 -DRPC_M2S_BUFFER_SIZE=128 $flags
        "$BUILD_DIR/native_${geometry}_$variant"
        RENDER_TEST_SOURCE=rgb_effect_owner_test.c build_scene "live_native_${geometry}_$variant" -I"$BUILD_DIR" -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_LIGHTING_FACT_LAYER_STATUS=0 -DNOAH_LIGHTING_FACT_MACRO_PLAYING=0 -DNOAH_LIGHTING_FACT_KEY_PRESSED=0 -DNOAH_LIGHTING_FACT_HOST_LOCK=0 -DNOAH_LIGHTING_FACT_POINTER=0 -DRGB_NATIVE_EFFECT_FIXTURE '-DRGB_MATRIX_SPLIT={RGB_LEFT_LED_COUNT,RGB_MATRIX_LED_COUNT-RGB_LEFT_LED_COUNT}' $bounds -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_TIMING_LINGER=1 -DNOAH_LIGHTING_TARGET_KEYS_THAT=1 -DRPC_M2S_BUFFER_SIZE=128 -DNOAH_LIGHTING_LIVE_ENABLE=1 -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DVIA_ENABLE -DSPLIT_KEYBOARD $flags
        "$BUILD_DIR/live_native_${geometry}_$variant"

    done
done

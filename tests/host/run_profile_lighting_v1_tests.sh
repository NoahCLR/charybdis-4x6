#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/vectors.txt" <<'PY'
import json, pathlib, sys
fixture = json.loads(pathlib.Path(sys.argv[1]).read_text())

def mask(ids):
    return sum(1 << i for i in ids)

def limits(data):
    return " ".join(str(v) for v in (data["tapDepth"], data["logicalLayers"], data["maximumBrightness"], data["supportedModeMask"], mask(data["facts"]), mask(data["targets"]), mask(data["paints"]), mask(data["timings"]), mask(data["ruleExtensions"]), mask(data["domainExtensions"]), mask(data["things"])))

lines = [limits(fixture["limits"])]
lines += [f"CODEC OK 0 {case['name']} {case['hex']}" for case in fixture["codec"]["valid"]]
lines += [f"CODEC {case['error']['code']} {case['error']['offset']} {case['name']} {case['hex']}" for case in fixture["codec"]["invalid"]]
lines += [f"CONVERT {case['name']} {limits(case['limits'])} {case['v4']} {case['hex']}" for case in fixture["conversion"]]
lines += [f"CODEC OK 0 render-{case['name']} {case['hex']}" for case in fixture["render"]]
lines += [f"CAPABILITY {page} {hex}" for page, hex in enumerate(fixture["capability"])]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PY

# The schema compiles every registry row, as a full RGB build does.
FLAGS="-DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DNOAH_PORTABLE_PROFILE_ENABLE -DRGB_MATRIX_ENABLE -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DPOINTING_DEVICE_ENABLE -DRGB_PD_MODE_FEEDBACK_ENABLE -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE -DNOAH_PD_PROFILE_ENABLE"

build_and_run() {
    name="$1"
    shift
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $FLAGS "$@" \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        "$ROOT/tests/host/profile_lighting_v1_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1_convert.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/profile_lighting_v1_test_$name"
    "$BUILD_DIR/profile_lighting_v1_test_$name" "$BUILD_DIR/vectors.txt"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
for source in profile_lighting_v1.c profile_lighting_v1_convert.c; do
    # shellcheck disable=SC2086
    "$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic $FLAGS \
        -mcpu=cortex-m0plus -mthumb \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -c "$ROOT/users/noah/lib/profile/schema/$source" \
        -o "$BUILD_DIR/${source%.c}_cortex_m0plus.o"
done

# Compile out each new kind independently; existing rows remain available.
for disabled in none COMBO_HELD BEHAVIOUR OUTCOME KEYS_THAT WHAT_IT_DOES LAYER_LOCKED LAYER_UNLOCKED ONESHOT_USED MODE_STARTED MODE_ENDED COMBO_FIRED TAP_FIRED HOLD_FIRED LONG_HOLD_FIRED MACRO_PLAYED LINGER FLASH; do
    case "$disabled" in
        none) gate_flags="" ;;
        KEYS_THAT) gate_flags="-DNOAH_LIGHTING_TARGET_KEYS_THAT=0" ;;
        WHAT_IT_DOES) gate_flags="-DNOAH_LIGHTING_PAINT_WHAT_IT_DOES=0" ;;
        LINGER|FLASH) gate_flags="-DNOAH_LIGHTING_TIMING_$disabled=0" ;;
        OUTCOME) gate_flags="-DNOAH_LIGHTING_PAINT_OUTCOME=0" ;;
        *) gate_flags="-DNOAH_LIGHTING_FACT_$disabled=0" ;;
    esac
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $FLAGS $gate_flags \
        -I"$ROOT" -I"$ROOT/users/noah" \
        "$ROOT/tests/host/lighting_module_gates_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/module_gate_$disabled"
    "$BUILD_DIR/module_gate_$disabled"
done

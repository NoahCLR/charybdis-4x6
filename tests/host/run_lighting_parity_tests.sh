#!/bin/sh
# Old stage renderer vs lighting rule engine over a corpus of situations and
# version-4 configurations: the frames must be identical (D-F26).

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
SITUATIONS="${LIGHTING_PARITY_SITUATIONS:-4000}"

cleanup() {
    rm -rf "$BUILD_DIR"
}

trap cleanup EXIT INT TERM

. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"

# The compiled profile's version-4 domain and synthetic ones covering every
# locality, layer mode, fade mode, group selector and disabled stage.
PYTHONDONTWRITEBYTECODE=1 python3 - "$ROOT" "$BUILD_DIR/configurations.txt" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
sys.path.insert(0, str(root / "tools"))
import lighting_reference as ref
configs = []
v4 = json.loads((root / "tests/fixtures/rgb_domain_v4.json").read_text())
configs.append(next(case["hex"] for case in v4["valid"] if case["name"] == "compiled"))
vectors = json.loads((root / "tests/fixtures/lighting_vectors_v1.json").read_text())
configs += [case["v4"] for case in vectors["conversion"] if case["name"] != "standard" and case["limits"] == vectors["limits"]]
thumbs = [24, 25, 26, 27, 28, 53, 54, 55]
groups = sorted([thumbs, [0, 7, 30], [10], [40, 41, 42, 43]], key=lambda leds: bytes(sum(1 << (l % 8) for l in leds if l // 8 == b) for b in range(8)))
def layers(mode_of):
    rows = []
    for n in range(16):
        colour = [(n * 37) % 256, 255, 120 + (n * 5) % 80] if n % 3 else [0, 0, 0]
        if n == 2:
            colour = [17, 0, 0]
        rows.append([n] + colour + [mode_of(n)])
    return rows
for variant in range(10):
    stages = [0x1F, 0x1F, 0x1E, 0x1B, 0x17, 0x0F, 0x1F, 0x1D, 0x1F, 0x13][variant]
    pd = [[n, (n * 23) % 256, 200 - n, 30 + n * 5, (n + variant) % 5] for n in range(32)]
    pd[3][1:4] = [0, 0, 0]
    configs.append(ref.encode_v4(
        stages=stages, groups=groups, layers=layers(lambda n: (n + variant) % 2),
        layer_rows=[[0xFF, 0, 0, 0, 0], [3, 90, 255, 180, 1], [5, 0, 0, 0, 3], [0, 10, 20, 30, 2]][: variant % 5],
        automouse=(variant % 3, 167, 255, 199), pd=pd,
        pd_rows=[[0xFF, 0, 0, 0, 0], [4, 99, 255, 150, 2], [7, 0, 0, 0, 1]][: variant % 4],
        combo=(191, 255, 200, variant % 5), combo_rows=[[0, 0, 0, 3], [21, 255, 200, 1]][: variant % 3],
        branches=[[169, 255, 200], [222, 255, 200], [85, 255, 200], [25, 255, 200]],
        key=(0, 0, 150, 18, 255, 200, 148, 255, 200, variant % 2, (variant + 3) % 5),
        key_rows=[[0xFF, 0, 0, 0, 0], [2, 21, 255, 200, 1], [0, 0, 0, 0, 2], [3, 0, 0, 0, 3], [1, 5, 5, 5, 0]][: variant % 6],
    ).hex())
pathlib.Path(sys.argv[2]).write_text("\n".join(configs) + "\n")
PY

FLAGS="-DNOAH_LIGHTING_FACT_COMBO_HELD=0 -DNOAH_LIGHTING_FACT_BEHAVIOUR=0 -DNOAH_LIGHTING_PAINT_OUTCOME=0 -DQMK_KEYBOARD_H=\"qmk_stub.h\" -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DRGB_MATRIX_ENABLE -DRGB_MATRIX_LED_COUNT=58 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_SPLIT={29,29} -DRGB_MATRIX_WS2812 -DRGB_MATRIX_LED_PROCESS_LIMIT=8 -DRGB_MATRIX_MAXIMUM_BRIGHTNESS=200 -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE -DPOINTING_DEVICE_ENABLE -DRGB_PD_MODE_FEEDBACK_ENABLE -DRGB_PD_MODE_ACTIVE_HALF_ENABLE -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE -DAUTO_MOUSE_TIME=900 -DRGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS=200 -DNOAH_PD_PROFILE_ENABLE"
COMMON="$ROOT/tests/host/lighting_parity_test.c $ROOT/users/noah/lib/rgb/automouse/rgb_automouse.c $ROOT/users/noah/lib/profile/schema/profile_rgb_v1.c $ROOT/users/noah/lib/profile/schema/profile_reader.c"

. "$ROOT/tests/host/noah_source_manifest.sh"
LIGHTING_SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in
        users/noah/lib/rgb/lighting/*) LIGHTING_SOURCES="$LIGHTING_SOURCES $ROOT/$src" ;;
    esac
done
ORACLE="$ROOT/tests/host/legacy_rgb"

build() {
    name="$1"
    shift
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-function -pedantic $FLAGS \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" "$@" -o "$BUILD_DIR/$name"
}

# shellcheck disable=SC2086
build oracle -DLIGHTING_PARITY_ORACLE -I"$ORACLE" -I"$ORACLE/core" $COMMON \
    "$ORACLE/core/rgb_runtime.c" "$ORACLE/core/rgb_effective_config.c" "$ORACLE/automouse/rgb_automouse_stage.c" \
    "$ORACLE/stages/rgb_layer_stage.c" "$ORACLE/stages/rgb_preview_stage.c" "$ORACLE/stages/rgb_pd_mode_stage.c" \
    "$ORACLE/stages/rgb_combo_feedback_stage.c" "$ORACLE/stages/rgb_key_feedback_stage.c"
# shellcheck disable=SC2086
build engine $COMMON "$ROOT/users/noah/lib/rgb/core/rgb_runtime.c" $LIGHTING_SOURCES \
    "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1_convert.c"

"$BUILD_DIR/oracle" "$BUILD_DIR/configurations.txt" "$SITUATIONS" > "$BUILD_DIR/oracle.txt" 2>/dev/null
"$BUILD_DIR/engine" "$BUILD_DIR/configurations.txt" "$SITUATIONS" > "$BUILD_DIR/engine.txt" 2> "$BUILD_DIR/coverage.txt"

if ! cmp -s "$BUILD_DIR/oracle.txt" "$BUILD_DIR/engine.txt"; then
    echo "lighting parity: the engine's frames differ from the stage renderer's" >&2
    diff "$BUILD_DIR/oracle.txt" "$BUILD_DIR/engine.txt" | head -20 >&2
    exit 1
fi
frames=$(grep -c '^[0-9]' "$BUILD_DIR/engine.txt")
painted=$(grep '^[0-9]' "$BUILD_DIR/engine.txt" | grep -cv ' - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -$' || true)
configurations=$(wc -l < "$BUILD_DIR/configurations.txt" | tr -d ' ')
echo "lighting parity: $frames half-frames over $configurations configurations identical ($painted with lighting)"
sed 's/^/lighting parity /' "$BUILD_DIR/coverage.txt"

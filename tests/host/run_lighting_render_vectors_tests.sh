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

python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/vectors.txt" <<'PY'
import json, pathlib, sys
fixture = json.loads(pathlib.Path(sys.argv[1]).read_text())
geometry = fixture["geometry"]
columns = geometry["matrixColumns"]

def bitmap(keys):
    out = bytearray(8)
    for key in keys:
        out[key // 8] |= 1 << (key % 8)
    return out.hex()

STATE = {"branch": 0, "tap": 1, "hold": 2, "long": 3}
OWNERS = ["board", "left", "right", "branch", "tap", "hold", "long"]
lines = ["GEOMETRY " + " ".join(str(-1 if led is None else led) for led in geometry["keyLeds"])]
for case in fixture["render"]:
    lines.append(f"CASE {case['name']} {case['hex']}")
    for step in case["steps"]:
        s = step["situation"]
        layers = 1
        for layer in s.get("layers", []):
            layers |= 1 << layer
        mode = s.get("mode")
        sides = 0
        if mode:
            for half in mode.get("halves", []):
                sides |= 1 if half == "left" else 2
        fields = [str(s.get("time", 0)), str(layers), str(255 if s.get("preview") is None else s["preview"]),
                  str(mode["id"] if mode else 255), str(sides), str(1 if mode and mode.get("keys") else 0), bitmap(mode.get("keys", []) if mode else []),
                  bitmap(s.get("combos", {}).get("owning", [])), bitmap(s.get("combos", {}).get("others", []))]
        mapped = s.get("mapped", {})
        fields += [bitmap(mapped.get(str(layer), [])) for layer in range(16)]
        behaviour = s.get("behaviour", {"keys": {}, "owners": {}})
        keys = behaviour.get("keys", {})
        fields.append(str(len(keys)))
        for key, entry in sorted(keys.items(), key=lambda item: int(item[0])):
            fields += [key, str(STATE[entry["state"]]), str(entry.get("branch", 0)), str(int(bool(entry.get("flashing")))), str(int(bool(entry.get("lit"))))]
        owners = behaviour.get("owners", {})
        fields += [str(255 if owners.get(slot) is None else owners[slot]) for slot in OWNERS]
        effect = s.get("effect", {})
        fields.append(str(len(effect)))
        for led, colour in effect.items():
            fields += [led, colour]
        fade = s.get("fade")
        fields += [str(fade["progress"]), str(fade["timeout"]), str(fade["deadTime"]), str(fade["autoMouseLayer"])] if fade else ["0", "900", "300", "255"]
        fields += step["expect"]
        lines.append("STEP " + " ".join(fields))
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PY

. "$ROOT/tests/host/noah_source_manifest.sh"
LIGHTING_SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in
        users/noah/lib/rgb/lighting/*) LIGHTING_SOURCES="$LIGHTING_SOURCES $ROOT/$src" ;;
    esac
done

build_and_run() {
    name="$1"
    shift
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic "$@" \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -DMATRIX_ROWS=10 -DMATRIX_COLS=6 \
        -DRGB_MATRIX_ENABLE -DRGB_MATRIX_LED_COUNT=58 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_MAXIMUM_BRIGHTNESS=200 \
        -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE \
        -DPOINTING_DEVICE_ENABLE -DRGB_PD_MODE_FEEDBACK_ENABLE -DRGB_PD_MODE_ACTIVE_HALF_ENABLE \
        -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE -DAUTO_MOUSE_TIME=900 \
        -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE \
        -I"$ROOT" \
        -I"$ROOT/users/noah" \
        -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/lighting_render_vectors_test.c" \
        $LIGHTING_SOURCES \
        "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/lighting_render_vectors_test_$name"
    "$BUILD_DIR/lighting_render_vectors_test_$name" "$BUILD_DIR/vectors.txt"
}

build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

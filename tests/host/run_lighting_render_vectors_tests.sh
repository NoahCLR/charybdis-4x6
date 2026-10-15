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
        instances = s.get("combos", {}).get("instances", [])
        fields.append(str(len(instances)))
        for item in instances:
            fields += [str(item["id"]), str(item["role"]), bitmap(item["keys"])]
        mapped = s.get("mapped", {})
        fields += [bitmap(mapped.get(str(layer), [])) for layer in range(16)]
        entries = [(int(layer),int(key),item) for layer,keys in s.get("keymap", {}).items() for key,item in keys.items()]
        fields.append(str(len(entries)))
        for layer,key,item in entries:
            fields += [str(layer),str(key),str(item["code"]),str(item.get("behaviour",255))]
        combos = s.get("queryCombos", {})
        fields.append(str(len(combos)))
        for row,item in combos.items():
            fields += [str(row),str(int(item.get("enabled",True))),str(sum(1 << layer for layer in item.get("layers",range(16)))),str(len(item["inputs"]))] + list(map(str,item["inputs"]))
        behaviour = s.get("behaviour", {"keys": {}, "owners": {}})
        keys = behaviour.get("keys", {})
        fields.append(str(len(keys)))
        for key, entry in sorted(keys.items(), key=lambda item: int(item[0])):
            fields += [key, str(STATE[entry["state"]]), str(entry.get("branch", 0)), str(int(bool(entry.get("flashing")))), str(int(bool(entry.get("lit")))), str(entry.get("id", 255)), str(entry.get("outcome", 0))]
        owners = behaviour.get("owners", {})
        fields += [str(255 if owners.get(slot) is None else owners[slot]) for slot in OWNERS]
        effect = s.get("effect", {})
        fields.append(str(len(effect)))
        for led, colour in effect.items():
            fields += [led, colour]
        fade = s.get("fade")
        fields += [str(fade["progress"]), str(fade["timeout"]), str(fade["deadTime"]), str(fade["autoMouseLayer"])] if fade else ["0", "900", "300", "255"]
        state = bytearray(309)
        owners = s.get("layerOwners", {})
        for key, layer in owners.items(): state[int(key)] = layer + 1
        locked = sum(1 << layer for layer in s.get("lockedLayers", []))
        state[60:62] = locked.to_bytes(2, "little")
        state[62] = s.get("oneshotLayer", 255)
        state[63] = max(s.get("layers", []) + [0])
        mode = s.get("modeStatus")
        state[64:75] = bytes([255,255,0]) + bytes(8)
        if mode:
            state[64] = mode["id"]
            state[65] = mode["id"] if mode.get("locked") else 255
            state[66] = sum(1 if half == "left" else 2 for half in mode.get("halves", []))
            state[67:75] = bytes.fromhex(bitmap(mode.get("keys", [])))
        state[75:115] = bytes([255]) * 40
        for index, item in enumerate(s.get("comboLayers", [])):
            at = 75 + 10 * index
            state[at:at+10] = bytes([item["id"],item["layer"]]) + bytes.fromhex(bitmap(item["keys"]))
        for key, item in s.get("behaviourPhases", {}).items():
            at = 115 + 3 * int(key)
            state[at:at+3] = bytes([item["id"]+1, sum(1 << phase for phase in item["phases"]),item["branch"]])
        state[295] = s.get("macroPlaying", 255)
        state[296:304] = bytes.fromhex(bitmap(s.get("pressedKeys", [])))
        state[304:306] = bytes(s.get("modifiers", [0,0]))
        state[306] = s.get("hostLocks", 0)
        state[307] = s.get("pointer", 0)
        state[308] = int(s.get("idle", False))
        fields.append(state.hex())
        fields += step["expect"]
        lines.append("STEP " + " ".join(fields))
for case in fixture.get("occurrences", []):
    lines.append(f"OCCURRENCE {case['name']} {case['hex']}")
    for step in case["steps"]:
        event, situation = step["event"], step["situation"]
        layers = 1 | sum(1 << layer for layer in situation.get("layers", []))
        modifiers = situation.get("modifiers", [0,0])
        fields = [step["order"], event["kind"], event["thing"], event.get("branch",0), event.get("slot",0), event.get("outcome",0),
                  event.get("key",255), int(event.get("owners") is not None), layers, situation.get("hostLocks",0), situation.get("macroPlaying",255), *modifiers]
        lines.append("QUALIFY " + " ".join(map(str,fields)) + " " + bitmap(event.get("owners") or []) + " " + bitmap(situation.get("pressedKeys",[])) + " " + (step["expect"] or "-"))
for case in fixture['historyObservations']:
    lines.append(f"HISTORY {case['name']} {case['hex']}")
    for step in case['steps']:
        data=step['situation']; behaviours=data.get('behaviour',{}).get('keys',{})
        fields=[step['elapsed'],step['order'],sum(1<<layer for layer in set(data.get('layers',[]))|{0}),len(behaviours)]
        for key,item in sorted(behaviours.items(),key=lambda p:int(p[0])):
            fields += [key,item['id'],{'hold':0,'long':1,'tap':2}[item['state']],item.get('outcome',0)]
        fields += [bitmap(data.get('pressedKeys',[])),*step['pixels']]
        lines.append('OBSERVE '+' '.join(map(str,fields)))
for case in fixture['historyEvents']:
    lines.append(f"EVENT_HISTORY {case['name']} {case['hex']}")
    for step in case['steps']:
        data=step['situation'];event=step['event']
        fields=[step['elapsed'],step['order'],sum(1<<layer for layer in set(data.get('layers',[]))|{0}),0,bitmap([])]
        fields += [event['kind'] if event else 255,event['thing'] if event else 0,event.get('branch',0) if event else 0,
                   event.get('slot',0) if event else 0,event.get('outcome',0) if event else 0,
                   event.get('key',255) if event else 255,int(event is not None and event.get('owners') is not None),bitmap(event.get('owners',[]) if event else [])]
        fields += step['pixels'];lines.append('OBSERVE '+' '.join(map(str,fields)))
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PY

. "$ROOT/tests/host/noah_source_manifest.sh"
LIGHTING_SOURCES=""
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
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
        -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DNOAH_LIGHTING_TIMING_TEST_BACKEND -DNOAH_LIGHTING_REFERENCE_COMPONENT_TESTS \
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

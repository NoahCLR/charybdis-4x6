#!/bin/sh
# The autocorrect engine: every case of tests/fixtures/autocorrect_v1.json
# typed through the report path (the ledger, QMK's report hooks, the macro
# engine and the typing stage) into a model of the computer, then the queue,
# the hold timer and the stage's idle cost on their own.

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

# Each case as lines the test reads: the list encoded as the domain, and the
# events as key strokes on the case's layout. Then the lists the test's own
# cases name, encoded the same way.
python3 - "$ROOT" "$BUILD_DIR/cases.txt" "$BUILD_DIR/lists.txt" <<'PYCODE'
import importlib.util, json, pathlib, sys
root = pathlib.Path(sys.argv[1])

def tool(name):
    spec = importlib.util.spec_from_file_location(f"{name}_build", root / "tools" / name / "build.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module

autocorrect, layouts = tool("autocorrect"), tool("host_layouts")

fixture = json.loads((root / "tests/fixtures/autocorrect_v1.json").read_text(encoding="utf-8"))
host = {layout["slug"]: layout for layout in json.loads((root / "tests/fixtures/host_layouts_v1.json").read_text(encoding="utf-8"))["layouts"]}
words = autocorrect.WORDS
capitals = {fold: char for char, fold in words.items() if fold != char}
MODS = {"Ctrl": 0x01, "Shift": 0x02, "Alt": 0x04, "Gui": 0x08, "AltGr": 0x40}
KEYS = {"Enter": 0x28, "Escape": 0x29, "Backspace": 0x2A, "Tab": 0x2B, "Delete": 0x4C, "Right": 0x4F, "Left": 0x50, "Home": 0x4A, "End": 0x4D}
hexed = lambda text: text.encode("utf-8").hex() or "-"

def stroke(text):
    value = layouts.encode_stroke(text)
    return value & 0xFF, (MODS["Shift"] if value & layouts.SHIFT else 0) | (MODS["AltGr"] if value & layouts.ALTGR else 0)

def strokes(layout, caps, char):
    """The key strokes that show this character on this layout under this Caps Lock."""
    if char == "\n":
        return [(KEYS["Enter"], 0)]
    if char == "\t":
        return [(KEYS["Tab"], 0)]
    macos = layout["os"] == "macos"
    if caps and char in capitals:
        assert not macos, f"{char!r} cannot show under Caps Lock on macOS"
        char = capitals[char]
    elif caps and words.get(char, char) != char:
        char = words[char]
    return [stroke(text) for text in layout["chars"][char]]

names = set()
lines = []
rules = set()
for case in fixture["cases"]:
    assert case["name"] not in names, case["name"]
    names.add(case["name"])
    rules.add(case["rule"])
    assert ("screen" in case) != ("keyboardOnly" in case), case["name"]
    assert ("result" in case) != ("resultEndsWith" in case), case["name"]
    layout, caps = host[case["layout"]], case["capsLock"]
    lines.append(f"CASE {case['name']}")
    lines.append(f"LAYOUT {layout['id']} {1 if layout['os'] == 'macos' else 0}")
    lines.append(f"ON {0 if case.get('on') is False else 1}")
    lines.append(f"CAPS {1 if caps else 0}")
    lines.append(f"LIST {autocorrect.encode(case['list']).hex()}")
    for slot, text in case.get("macros", {}).items():
        lines.append(f"SLOT {int(slot)} {hexed(text)}")
    for event in case["events"]:
        if "type" in event or ("fast" in event and "macro" not in event):
            fast = "fast" in event
            for char in event.get("type", event.get("fast")):
                for key, mods in strokes(layout, caps, char):
                    lines.append(f"{'FAST' if fast else 'TAP'} {key} {mods}")
        elif "key" in event:
            key = KEYS.get(event["key"]) or layouts.KEYCODES[f"KC_{event['key']}"]
            lines.append(f"TAP {key} {sum(MODS[name] for name in event.get('mods', []))}")
        elif "stroke" in event:
            lines.append("TAP %d %d" % stroke(event["stroke"]))
        elif "tap" in event:
            lines.append(f"TAP {layouts.KEYCODES[event['tap']]} 0")
        elif "press" in event or "release" in event:
            lines.append(f"{'PRESS' if 'press' in event else 'RELEASE'} {MODS[event.get('press', event.get('release'))]}")
        elif "click" in event:
            lines.append("CLICK")
        elif "hold" in event:
            (key, mods), = strokes(layout, caps, event["hold"])
            lines.append(f"HOLD {key} {mods} {event['ms']} {event['repeats']}")
        elif "macro" in event:
            lines.append(f"MACRO {1 if event.get('fast') else 0} {hexed(event['macro'])}")
        elif "mash" in event:
            (key, mods), = strokes(layout, caps, event["mash"])
            lines += [f"FAST {key} {mods}"] * event["count"]
        elif "leader" in event:
            lines.append("LEADER")
        elif "wait" in event:
            lines.append(f"WAIT {event['wait']}")
        elif "override" in event:
            lines.append(f"OVERRIDE {1 if event['override'] else 0}")
        else:
            raise SystemExit(f"{case['name']}: unknown event {event}")
    if "strokes" in case:
        lines.append(f"STROKES {case['strokes']['erased']} {case['strokes']['typed']}")
    lines.append(f"{'RESULT' if 'result' in case else 'ENDS'} {hexed(case.get('result', case.get('resultEndsWith')))}")
# D-F69's ten rules, and the leader's (D-F71).
assert rules >= set(range(1, 12)), "every rule needs a case"
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n", encoding="utf-8")

def entry(correction, typo, match="whole word", trigger="as a word ends"):
    return {"correction": correction, "trigger": trigger, "typos": [{"typo": typo, "match": match}]}

lists = {
    "phone": [entry("+31 6 1234 5678", "pnumber")],
    # More part-of-word typos than one scan compares, and a list whose first
    # correction is another text.
    "forty": [entry(f"ab{n:02}", f"aq{n:02}", "anywhere") for n in range(1, 41)],
    "other": [entry("WRONG", "zq", "anywhere")],
    # Replacements that fire after the leader (D-F71) beside one that fires as
    # a word ends, and a macro; and a list with nothing else.
    "leader": [entry("+31 6 1234 5678", "pn", trigger="after the leader"), entry("alpha", "abc", trigger="after the leader"), entry("delta", "abcd", trigger="after the leader"), entry("the", "teh"), {"macro": 5, "trigger": "after the leader", "typos": [{"typo": "sig", "match": "whole word"}]}],
    "leader-only": [entry("+31 6 1234 5678", "pn", trigger="after the leader")],
}
pathlib.Path(sys.argv[3]).write_text("".join(f"{name} {autocorrect.encode(entries).hex()}\n" for name, entries in lists.items()), encoding="utf-8")
PYCODE

# The replacement flash is built shorter than the behaviour-feedback flash
# half-period (200 ms here), so the test proves it keeps its own time (D-F70).
for flags in "" "-fsanitize=address,undefined -fno-omit-frame-pointer"; do
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags -DNKRO_ENABLE \
        -DRGB_AUTOCORRECT_FEEDBACK_FLASH_MS=120 \
        -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/send_string" \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/autocorrect_test.c" \
        "$ROOT/users/noah/lib/autocorrect/autocorrect.c" \
        "$ROOT/users/noah/lib/autocorrect/autocorrect_feedback.c" \
        "$ROOT/users/noah/lib/autocorrect/autocorrect_switch.c" \
        "$ROOT/users/noah/lib/key/runtime/slot/origin_registry.c" \
        "$ROOT/users/noah/lib/macro/macro_payload_run.c" \
        "$ROOT/users/noah/lib/macro/macro_input_guard.c" \
        "$ROOT/users/noah/lib/macro/macro_slot_provider.c" \
        "$ROOT/users/noah/lib/macro/host_layout.c" \
        "$ROOT/users/noah/lib/macro/host_layout_tables.c" \
        "$ROOT/users/noah/lib/action/owned_keycode.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_autocorrect_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_autocorrect_words_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/test"
    "$BUILD_DIR/test" "$BUILD_DIR/cases.txt" "$BUILD_DIR/lists.txt"
done

# The firmware's compiler takes the module and holds its state to its policy.
ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
"$ARM_CC" -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -mcpu=cortex-m0plus -mthumb -DNKRO_ENABLE \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/autocorrect/autocorrect.c" -o "$BUILD_DIR/autocorrect.o"

echo "autocorrect host tests passed"

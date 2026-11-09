#!/bin/sh
# The autocorrect list domain (0x80): the generated fixture and word table are
# current, the validator and lookups hold on the fixture's vectors and on a
# list at every ceiling, and the authored-list check refuses a stale header.

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM

python3 "$ROOT/tools/autocorrect/build.py" --check

# Each vector as a line: code, table, row (-1 for none), name, hex. The list
# at every ceiling is built here, not kept in the fixture.
python3 - "$ROOT" "$BUILD_DIR/vectors.txt" <<'PYCODE'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
sys.path.insert(0, str(root / "tools" / "autocorrect"))
import build
fixture = json.loads((root / "tests/fixtures/autocorrect_domain_v1.json").read_text())
none = lambda value: -1 if value is None else value
lines = []
for case in fixture["valid"]:
    assert build.encode(case["entries"]).hex() == case["hex"], case["name"]
    lines.append(f"OK -1 -1 {case['name']} {case['hex']}")
lines.append(f"OK -1 -1 maximum {build.encode(build.maximum_entries()).hex()}")
for case in fixture["invalid"]:
    error = case["error"]
    lines.append(f"{error['code']} {none(error['table'])} {none(error['row'])} {case['name']} {case['hex'] or '-'}")
# The encoder refuses what the domain refuses.
for entries in (
    [{"correction": "the", "typos": []}],
    [{"correction": "the", "typos": [{"typo": "the", "match": "whole word"}]}],
    [{"correction": "the", "typos": [{"typo": "Teh", "match": "whole word"}]}],
    [{"correction": "the", "typos": [{"typo": "t-eh", "match": "whole word"}]}],
    [{"correction": "the", "typos": [{"typo": "teh", "match": "whole word"}]}, {"correction": "then", "typos": [{"typo": "teh", "match": "anywhere"}]}],
    [{"correction": "x" * 33, "typos": [{"typo": "teh", "match": "whole word"}]}],
    build.maximum_entries() + [{"correction": "one more", "typos": [{"typo": "onemore", "match": "whole word"}]}],
):
    try:
        build.encode(entries)
    except build.ListError:
        continue
    raise SystemExit(f"the encoder accepted {entries[-1]}")
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PYCODE

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" -I"$ROOT" \
        "$ROOT/tests/host/profile_autocorrect_v1_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_autocorrect_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_autocorrect_words_v1.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_reader.c" \
        -o "$BUILD_DIR/$name"
    "$BUILD_DIR/$name" "$BUILD_DIR/vectors.txt"
}
build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

# The authored list: its header is written from the list, the check passes on
# what was written and fails once the list and the header disagree.
KEYMAP="$BUILD_DIR/keymap"
mkdir "$KEYMAP"
printf '# A test list.\n:teh: -> the\n:Hte: -> the\ntihng -> thing\n:nu -> un\nmnet: -> ment\n' >"$KEYMAP/autocorrect.txt"
python3 "$ROOT/tools/autocorrect/build.py" --write-authored "$KEYMAP"
python3 "$ROOT/tools/autocorrect/build.py" --check-authored "$KEYMAP"
python3 - "$ROOT" "$KEYMAP/authored_autocorrect_list.h" <<'PYCODE'
import pathlib, re, sys
sys.path.insert(0, str(pathlib.Path(sys.argv[1]) / "tools" / "autocorrect"))
import build
data = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", pathlib.Path(sys.argv[2]).read_text().split("#define", 1)[1]))
whole, start, end, anywhere = build.MATCHES
assert data == build.encode([
    {"correction": "the", "typos": [{"typo": "teh", "match": whole}, {"typo": "hte", "match": whole}]},
    {"correction": "thing", "typos": [{"typo": "tihng", "match": anywhere}]},
    {"correction": "un", "typos": [{"typo": "nu", "match": start}]},
    {"correction": "ment", "typos": [{"typo": "mnet", "match": end}]},
])
PYCODE
printf ':becuase: -> because\n' >>"$KEYMAP/autocorrect.txt"
if python3 "$ROOT/tools/autocorrect/build.py" --check-authored "$KEYMAP" 2>"$BUILD_DIR/stale.txt"; then
    echo "a stale authored_autocorrect_list.h passed its check" >&2
    exit 1
fi
grep -q "stale generated files" "$BUILD_DIR/stale.txt"
# A list the domain cannot hold fails with its line or its reason.
printf ':teh: -> the\n:teh: -> then\n' >"$KEYMAP/autocorrect.txt"
if python3 "$ROOT/tools/autocorrect/build.py" --check-authored "$KEYMAP" 2>"$BUILD_DIR/refused.txt"; then
    echo "an authored list with one typo for two corrections passed" >&2
    exit 1
fi
grep -q "listed twice" "$BUILD_DIR/refused.txt"

# The firmware's compiler takes the module.
ARM_CC="${ARM_CC:-arm-none-eabi-gcc}"
for source in profile_autocorrect_v1 profile_autocorrect_words_v1; do
    "$ARM_CC" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb -I"$ROOT" -I"$ROOT/users/noah" \
        -c "$ROOT/users/noah/lib/profile/schema/$source.c" -o "$BUILD_DIR/$source.o"
done

echo "profile autocorrect v1 tests passed"

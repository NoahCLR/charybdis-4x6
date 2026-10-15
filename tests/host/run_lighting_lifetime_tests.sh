#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/vectors.txt" <<'PY'
import json,pathlib,sys
fixture=json.loads(pathlib.Path(sys.argv[1]).read_text())
operations={'idle':0,'match':1,'depart':2,'event':3,'nonqualifying':4}
lines=[]
for case in fixture['lifetimes']:
    p=case['parameters']
    lines.append(f"CASE {case['name']} {p.get('show_after',0)} {p.get('linger',0)} {p.get('flash',0)}")
    for step in case['steps']:
        e=step['expect']
        lines.append('STEP '+' '.join(map(str,[step['elapsed'],operations[step['observation']],step['batch'],int(e['visible']),e['wake'],e['batch'] or 0])))
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
PY
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCE=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_lifetime.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_retained.c) SOURCE="$SOURCE $ROOT/$src" ;; esac
done
[ -n "$SOURCE" ]
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -I"$ROOT" "$ROOT/tests/host/lighting_lifetime_test.c" $SOURCE -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant" "$BUILD_DIR/vectors.txt"
done
for src in $SOURCE; do
    "${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb -c "$src" -o "$BUILD_DIR/$(basename "$src" .c)_arm.o"
done

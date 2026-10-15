#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/vectors.txt" <<'PY'
import json,pathlib,sys
lines=[]
for case in json.loads(pathlib.Path(sys.argv[1]).read_text())['retainedOrdering']:
    lines.append(f"CASE {case['name']} {len(case['cells'])}")
    for c in case['cells']:
        p=c['parameters']
        lines.append(' '.join(map(str,[c['kind'],c['thing'],{'matrix':0,'layer-desc':1,'constant':2}[c['natural']],p.get('show_after',0),p.get('linger',0),c['colour']])))
    for step in case['steps']:
        values=['STEP',step['elapsed'],step['order']]
        for batch in step['matching']: values += [len(batch),*batch]
        e=step['expect'];values += [e['pixel'],len(e['members'])]
        for member in e['members']: values+=member
        values+=e['entries'];lines.append(' '.join(map(str,values)))
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
PY
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_lifetime.c|users/noah/lib/rgb/lighting/lighting_priority.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_retained_ordering_test.c" $SOURCES -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant" "$BUILD_DIR/vectors.txt"
done
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_priority.c" -o "$BUILD_DIR/priority_arm.o"

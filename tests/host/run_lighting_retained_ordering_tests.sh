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
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_retained_render.c|users/noah/lib/rgb/lighting/lighting_history_store.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_lifetime.c|users/noah/lib/rgb/lighting/lighting_priority.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
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

# Production-backed counterparts use real registry walkers and admission. The
# original seven reference traces/assertions above remain unchanged, including
# the explicitly synthetic cross-kind comparator fixture.
python3 - "$ROOT" "$BUILD_DIR" <<'PY'
import copy,json,pathlib,sys
root=pathlib.Path(sys.argv[1]);out=pathlib.Path(sys.argv[2]);sys.path.insert(0,str(root/'tools'))
import lighting_reference as ref
source=json.loads((root/'tests/fixtures/lighting_vectors_v1.json').read_text())['retainedOrdering']
cases=[copy.deepcopy(c) for c in source if 'simultaneous-fallback' not in c['name'] and 'entry-recency-starts' not in c['name']]
# A real fact has one timing per rule and one behaviour identity per key.
# Preserve the show-before-maturity property with uniform100ms, distinct live
# keys, then an eligible newer tail at the older live batch's returning key0.
uniform=copy.deepcopy(source[-1]);uniform['name']+='.uniform100-real-keys'
for cell in uniform['cells']:cell['parameters']['show_after']=100
uniform['steps']=[]
for elapsed,matching,pixel,members,entries in [
    (0,[[0],[]],'-',[],[1,0]),
    (50,[[1],[0]],'-',[],[1,2]),
    (50,[[1],[0]],'c80000',[[0,0]],[1,2]),
    (50,[[1],[0]],'0000c8',[[1,0],[0,0]],[1,2]),
    (1,[[0,1],[]],'0000c8',[[1,0],[0,0],[0,1]],[1,2]),
]:uniform['steps'].append(dict(elapsed=elapsed,matching=matching,expect=dict(pixel=pixel,members=members,entries=entries)))
cases.append(uniform)
# Kind2/4 same-ID and duplicate identical-key members cannot be one real
# registry fact. These are reachable same-kind simultaneous controls instead:
# entry equality survives partial departure/refresh, then thing-ID breaks the
# exact natural tie. The original synthetic comparator remains reference-only.
for ids in [(0,1),(127,126)]:
    case=dict(name='simultaneous-real-behaviour-'+str(ids[0])+'-'+str(ids[1]),cells=copy.deepcopy(source[0]['cells']),steps=[])
    for c,i in zip(case['cells'],ids):c['thing']=i
    b_first=ids[1]<ids[0]
    for elapsed,matching,pixel,members in [
        (0,[[0],[1]],'c80000',[[0,0],[1,0]]),
        (10,[[],[0,1]],'0000c8' if b_first else 'c80000',[[1,0],[0,0],[1,1]] if b_first else [[0,0],[1,0],[1,1]]),
        (999,[[],[0,1]],'0000c8' if b_first else 'c80000',[[1,0],[0,0],[1,1]] if b_first else [[0,0],[1,0],[1,1]]),
        (1,[[],[0,1]],'0000c8',[[1,0],[1,1]]),
    ]:case['steps'].append(dict(elapsed=elapsed,matching=matching,expect=dict(pixel=pixel,members=members,entries=[1,1])))
    cases.append(case)
for keys in (2,60):
    lines=[]
    for case in cases:
        cells=case['cells'];kind=cells[0]['kind'];slots=ref.Limits().colour_slots(kind)
        default=[0]+[ref.NONE]*slots;records=[default.copy() for _ in range(max(c['thing'] for c in cells)+1)]
        for index,c in enumerate(cells):records[c['thing']][1]=index
        params=cells[0]['parameters'];assert all(c['parameters']==params for c in cells)
        # Layer-held exposes real owners and existing layer natural priority.
        condition=ref.part(0,9,255,1) if kind==1 else ref.part(0,8,255,4)
        parts=[condition,ref.part(2,0),ref.part(3,3)]
        for timing,name in [(1,'show_after'),(2,'linger')]:
            n=params[name]
            if n:parts.append(ref.part(5,timing,n&255,n>>8))
        rule=ref.rule(*parts)
        rules=([dict(rule,flags=1) for _ in range(254)] if 'wrapping-timer' in case['name'] else [])+[rule]
        wire=ref.encode(dict(palette=[[0,255,200],[170,255,200]],things=[dict(kind=kind,default=default,records=records)],rules=rules))
        program=ref.validate(wire,ref.Limits());cost=ref.working_cost(program,matrix_keys=keys)
        start=0xffffffff-65534 if 'wrapping-timer' in case['name'] else 0
        lines.append(f"CASE {case['name']} {wire.hex()} 2 {cost['required']} {int(cost['admitted'])} {start} {len(rules)-1}")
        lines += [f"{c['kind']} {c['thing']}" for c in cells]
        for step in case['steps']:
            e=step['expect'];values=['STEP',step['elapsed']]
            for batch in step['matching']:values += [len(batch),*batch]
            values += [e['pixel'],len(e['members'])]
            for member in e['members']:values+=member
            values += [int(bool(step['matching'][i]) or any(m[0]==i for m in e['members'])) for i in range(2)]
            a,b=map(int,e['entries']);values += [(a>b)-(a<b)]
            lines.append(' '.join(map(str,values)))
        lines.append('END')
    (out/f'production-{keys}.txt').write_text('\n'.join(lines)+'\n')
PY
PRODUCTION_SOURCES=""
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
    case "$src" in users/noah/lib/rgb/lighting/*) PRODUCTION_SOURCES="$PRODUCTION_SOURCES $ROOT/$src" ;; esac
done
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    for geometry in small default; do
        case "$geometry" in small) bounds="-DMATRIX_ROWS=2 -DMATRIX_COLS=1"; keys=2 ;; default) bounds="-DMATRIX_ROWS=10 -DMATRIX_COLS=6";keys=60 ;; esac
        # Same full module bindings/QMK stand-ins as production render vectors;
        # no substitute fact or priority function, no larger working pool.
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags $bounds \
          -DQMK_KEYBOARD_H='"qmk_stub.h"' -DRGB_MATRIX_ENABLE -DRGB_MATRIX_LED_COUNT=58 -DRGB_LEFT_LED_COUNT=29 -DRGB_MATRIX_MAXIMUM_BRIGHTNESS=200 \
          -DRGB_KEY_BEHAVIOR_FEEDBACK_ENABLE -DCOMBO_ENABLE -DRGB_COMBO_FEEDBACK_ENABLE -DPOINTING_DEVICE_ENABLE \
          -DRGB_PD_MODE_FEEDBACK_ENABLE -DRGB_PD_MODE_ACTIVE_HALF_ENABLE -DPOINTING_DEVICE_AUTO_MOUSE_ENABLE -DRGB_AUTOMOUSE_GRADIENT_ENABLE -DAUTO_MOUSE_TIME=900 \
          -DNOAH_PORTABLE_PROFILE_ENABLE -DNOAH_PD_PROFILE_ENABLE -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DNOAH_LIGHTING_TIMING_TEST_BACKEND -DNOAH_LIGHTING_REFERENCE_COMPONENT_TESTS \
          -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
          "$ROOT/tests/host/lighting_working_ordering_test.c" $PRODUCTION_SOURCES \
          "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/production-$geometry-$variant"
        "$BUILD_DIR/production-$geometry-$variant" "$BUILD_DIR/production-$keys.txt"
    done
done

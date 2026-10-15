#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_history_owner.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_history_bank.c|users/noah/lib/rgb/lighting/lighting_history_observe.c|users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/kinds/fact_behaviour.c|users/noah/lib/rgb/lighting/kinds/fact_key_pressed.c|users/noah/lib/rgb/lighting/lighting_history_store.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_retained_group.c|users/noah/lib/rgb/lighting/lighting_lifetime.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/observations.txt" <<'VECTORS'
import json,pathlib,sys
vectors=json.loads(pathlib.Path(sys.argv[1]).read_text())
lines=[]
def bitmap(keys):
    out=bytearray(8)
    for key in keys:out[key//8]|=1<<(key%8)
    return out.hex()
slots={'hold':0,'long':1,'tap':2}
for case in vectors['historyObservations']:
    lines.append(f"CASE {case['name']} {len(bytes.fromhex(case['hex']))} {case['hex']}")
    for step in case['steps']:
        data=step['situation']; behaviours=data.get('behaviour',{}).get('keys',{})
        fields=['FRAME',step['elapsed'],step['order'],sum(1<<layer for layer in set(data.get('layers',[]))|{0}),len(behaviours)]
        for key,item in sorted(behaviours.items(),key=lambda p:int(p[0])):fields += [key,item['id'],slots[item['state']],item.get('outcome',0)]
        fields += [bitmap(data.get('pressedKeys',[])),len(step['expect'])]
        for cell in step['expect']:
            fields += [cell['rule'],cell['thing'],int(cell['visible']),cell['wake'],cell['entry'],len(cell['batch'])]
            for member in cell['batch']:
                sides=sum(1 if h=='left' else 2 for h in member['halves'])
                fields += [member['kind'],member['thing'],member['slot'],member['outcome'],sides,int(bool(member['keys'])),bitmap(member['keys'])]
        lines.append(' '.join(map(str,fields)))
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
VECTORS
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # The independent shared calendars also pass through real validated rule
    # bytes, optional timing bindings and the state/event owner, without counters.
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_history_observe_test.c" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/lifetimes_$variant"
    "$BUILD_DIR/lifetimes_$variant" "$BUILD_DIR/observations.txt"
done
# Compile the actual owner implementation for the target ISA too. Linked paths
# and resource policies still require the later enabled, exact-head pair gate.
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
    -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_history_observe.c" -o "$BUILD_DIR/observe_arm.o"

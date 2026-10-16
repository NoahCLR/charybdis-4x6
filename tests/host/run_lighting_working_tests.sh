#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_history_owner.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_history_bank.c|users/noah/lib/rgb/lighting/lighting_working.c|users/noah/lib/rgb/lighting/lighting_freshness.c|users/noah/lib/rgb/lighting/lighting_snapshot.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/kinds/fact_behaviour.c|users/noah/lib/rgb/lighting/kinds/fact_key_pressed.c|users/noah/lib/rgb/lighting/lighting_history_store.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_retained_group.c|users/noah/lib/rgb/lighting/lighting_lifetime.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/freshness.txt" "$BUILD_DIR/costs.txt" "$BUILD_DIR/snapshots.txt" "$BUILD_DIR/calendars.txt" <<'VECTORS'
import json,pathlib,sys
lines=[]
for c in json.loads(pathlib.Path(sys.argv[1]).read_text())['freshness']:
    fields=[c[k] for k in ('capture','before','remote','after','now')]+[int(c['bounds'] is not None)]+(c['bounds'] or [0,0])+[c['show'],c['flash'],int(c['visible'])]
    lines.append(' '.join(map(str,fields)))
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
lines=[f"{c['name']} {c['hex']} {c['bank']} {c['maximum']} {c['required']} {int(c['admitted'])}" for c in json.loads(pathlib.Path(sys.argv[1]).read_text())['workingMemory']]
pathlib.Path(sys.argv[3]).write_text('\n'.join(lines)+'\n')
lines=[f"{c['epoch']} {c['sequence']} {c['payload']} {c['request']}" for c in json.loads(pathlib.Path(sys.argv[1]).read_text())['snapshotTransport']]
pathlib.Path(sys.argv[4]).write_text('\n'.join(lines)+'\n')
lines=[];operations={'idle':0,'match':1,'depart':2,'event':3,'nonqualifying':4}
for case in json.loads(pathlib.Path(sys.argv[1]).read_text())['lifetimes']:
    p=case['parameters'];lines.append(f"CASE {case['name']} {p.get('show_after',0)} {p.get('linger',0)} {p.get('flash',0)}")
    for step in case['steps']:
        e=step['expect'];lines.append('STEP '+' '.join(map(str,[step['elapsed'],operations[step['observation']],step['batch'],int(e['visible']),e['wake'],e['batch'] or 0])))
pathlib.Path(sys.argv[5]).write_text('\n'.join(lines)+'\n')

VECTORS
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # The independent shared calendars also pass through real validated rule
    # bytes, optional timing bindings and the state/event owner, without counters.
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_LIGHTING_HISTORY_WORK_TEST_BACKEND -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_working_test.c" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/lifetimes_$variant"
    "$BUILD_DIR/lifetimes_$variant" "$BUILD_DIR/freshness.txt" "$BUILD_DIR/costs.txt" "$BUILD_DIR/snapshots.txt"
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_working_lifetime_test.c" "$ROOT/users/noah/lib/rgb/lighting/lighting_working.c" "$ROOT/users/noah/lib/rgb/lighting/lighting_history_index.c" "$ROOT/users/noah/lib/rgb/lighting/kinds/fact_layer_status.c" \
      "$ROOT/users/noah/lib/rgb/lighting/kinds/timing_show_after.c" "$ROOT/users/noah/lib/rgb/lighting/kinds/timing_linger.c" "$ROOT/users/noah/lib/rgb/lighting/kinds/timing_flash.c" "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/calendars_$variant"
    "$BUILD_DIR/calendars_$variant" "$BUILD_DIR/calendars.txt"

done
# Compile the actual owner implementation for the target ISA too. Linked paths
# and resource policies still require the later enabled, exact-head pair gate.
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
    -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_working.c" -o "$BUILD_DIR/observe_arm.o"

SOURCES=""
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
    case "$src" in users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/lighting_working.c|users/noah/lib/rgb/lighting/lighting_snapshot.c|users/noah/lib/rgb/lighting/lighting_freshness.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
ADAPTER="$ROOT/users/noah/lib/compat/qmk_lighting_snapshot_transport.c"
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    baseflags="-std=c11 -Wall -Wextra -Werror -pedantic -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRGB_PD_MODE_ACTIVE_HALF_ENABLE -DSPLIT_TRANSACTION_IDS_USER -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32"
    # shellcheck disable=SC2086
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      -Dnoah_qmk_lighting_snapshot_readiness=peer_readiness -Dnoah_qmk_lighting_snapshot_peer_readiness=peer_peer_readiness -Dnoah_qmk_lighting_snapshot_init=peer_init -Dnoah_qmk_lighting_snapshot_bind_facts=peer_bind -Dnoah_qmk_lighting_snapshot_step=peer_step -Dnoah_qmk_lighting_snapshot_close=peer_close -Dnoah_qmk_lighting_snapshot_changed=peer_changed -Dnoah_qmk_lighting_snapshot_fact_changes=peer_fact_changes -c "$ADAPTER" -o "$BUILD_DIR/peer_$variant.o"
    # shellcheck disable=SC2086
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/qmk_lighting_snapshot_test.c" "$ROOT/tests/host/qmk_lighting_snapshot_base_peer.c" "$ROOT/users/noah/lib/split/runtime_sync.c" "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" "$ADAPTER" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" "$BUILD_DIR/peer_$variant.o" -o "$BUILD_DIR/qmk_$variant"
    "$BUILD_DIR/qmk_$variant"
done

for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    baseflags="-std=c11 -Wall -Wextra -Werror -pedantic -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRGB_PD_MODE_ACTIVE_HALF_ENABLE -DSPLIT_TRANSACTION_IDS_USER -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32 -DNOAH_LIGHTING_LIVE_ENABLE=1 -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DVIA_ENABLE -DSPLIT_KEYBOARD"
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" -Dnoah_lighting_live_handle=peer_live_handle -Dnoah_lighting_live_task=peer_live_task -Dnoah_lighting_live_close=peer_live_close -Dnoah_lighting_live_colour=peer_live_colour -c "$ROOT/users/noah/lib/compat/qmk_lighting_live.c" -o "$BUILD_DIR/live_peer_$variant.o"
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" "$ROOT/tests/host/qmk_lighting_snapshot_test.c" "$ROOT/tests/host/qmk_lighting_snapshot_base_peer.c" "$ROOT/users/noah/lib/split/runtime_sync.c" "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" "$ADAPTER" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" "$ROOT/users/noah/lib/compat/qmk_lighting_live.c" "$BUILD_DIR/peer_$variant.o" "$BUILD_DIR/live_peer_$variant.o" -o "$BUILD_DIR/combined_$variant"
    "$BUILD_DIR/combined_$variant"
done

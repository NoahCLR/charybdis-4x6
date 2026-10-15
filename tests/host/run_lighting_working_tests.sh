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
    case "$src" in users/noah/lib/rgb/lighting/lighting_history_owner.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_history_bank.c|users/noah/lib/rgb/lighting/lighting_working.c|users/noah/lib/rgb/lighting/lighting_freshness.c|users/noah/lib/rgb/lighting/lighting_snapshot.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/kinds/fact_behaviour.c|users/noah/lib/rgb/lighting/kinds/fact_key_pressed.c|users/noah/lib/rgb/lighting/lighting_history_store.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_retained_group.c|users/noah/lib/rgb/lighting/lighting_lifetime.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
python3 - "$ROOT/tests/fixtures/lighting_vectors_v1.json" "$BUILD_DIR/freshness.txt" "$BUILD_DIR/costs.txt" "$BUILD_DIR/snapshots.txt" <<'VECTORS'
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
done
# Compile the actual owner implementation for the target ISA too. Linked paths
# and resource policies still require the later enabled, exact-head pair gate.
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
    -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_working.c" -o "$BUILD_DIR/observe_arm.o"

SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_working.c|users/noah/lib/rgb/lighting/lighting_snapshot.c|users/noah/lib/rgb/lighting/lighting_freshness.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
ADAPTER="$ROOT/users/noah/lib/compat/qmk_lighting_snapshot_transport.c"
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    baseflags="-std=c11 -Wall -Wextra -Werror -pedantic -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DSPLIT_TRANSACTION_IDS_USER -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32"
    # shellcheck disable=SC2086
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      -Dnoah_qmk_lighting_snapshot_init=peer_init -Dnoah_qmk_lighting_snapshot_bind_facts=peer_bind -Dnoah_qmk_lighting_snapshot_step=peer_step -Dnoah_qmk_lighting_snapshot_close=peer_close -c "$ADAPTER" -o "$BUILD_DIR/peer_$variant.o"
    # shellcheck disable=SC2086
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/qmk_lighting_snapshot_test.c" "$ADAPTER" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" "$BUILD_DIR/peer_$variant.o" -o "$BUILD_DIR/qmk_$variant"
    "$BUILD_DIR/qmk_$variant"
done

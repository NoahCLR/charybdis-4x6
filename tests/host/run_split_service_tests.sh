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
    case "$src" in users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/lighting_working.c|users/noah/lib/rgb/lighting/lighting_snapshot.c|users/noah/lib/rgb/lighting/lighting_freshness.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
ADAPTER="$ROOT/users/noah/lib/compat/qmk_lighting_snapshot_transport.c"
SERVICE_SOURCE="$ROOT/users/noah/lib/compat/qmk_split_service.c"
if [ "${1:-}" = --unpaced ]; then SERVICE_SOURCE="$ROOT/tests/host/split_service_unpaced.c"; shift; fi
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    baseflags="-include $ROOT/tests/host/include/noah_compile_config.h -DNOAH_SPLIT_SERVICE_COMBINED_TEST -DNOAH_PROFILE_SPLIT_TRANSPORT_TEST -std=c11 -Wall -Wextra -Werror -pedantic -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRGB_PD_MODE_ACTIVE_HALF_ENABLE -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32 -DNOAH_LIGHTING_LIVE_ENABLE=1 -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DVIA_ENABLE -DSPLIT_KEYBOARD"
cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
-Dnoah_qmk_lighting_snapshot_readiness=peer_readiness -Dnoah_qmk_lighting_snapshot_peer_readiness=peer_peer_readiness -Dnoah_qmk_lighting_snapshot_init=peer_init -Dnoah_qmk_lighting_snapshot_bind_facts=peer_bind -Dnoah_qmk_lighting_snapshot_step=peer_step -Dnoah_qmk_lighting_snapshot_close=peer_close -Dnoah_qmk_lighting_snapshot_changed=peer_changed -Dnoah_qmk_lighting_snapshot_fact_changes=peer_fact_changes -c "$ADAPTER" -o "$BUILD_DIR/peer_$variant.o"
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" -Dnoah_lighting_live_handle=peer_live_handle -Dnoah_lighting_live_task=peer_live_task -Dnoah_lighting_live_close=peer_live_close -Dnoah_lighting_live_colour=peer_live_colour -c "$ROOT/users/noah/lib/compat/qmk_lighting_live.c" -o "$BUILD_DIR/live_peer_$variant.o"
    cc $baseflags $flags -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" "$ROOT/tests/host/split_service_test.c" "$ROOT/tests/host/split_service_profile.c" "$ROOT/tests/host/split_service_via.c" "$ROOT/tests/host/split_service_mirror_peer.c" "$ROOT/users/noah/lib/compat/qmk_via_split_mirror.c" "$ROOT/users/noah/lib/compat/qmk_via_contract.c" "$ROOT/users/noah/lib/compat/qmk_durable_io.c" "$ROOT/users/noah/lib/profile/split/profile_split_reconciler.c" "$ROOT/users/noah/lib/profile/split/profile_split_authority.c" "$ROOT/users/noah/lib/profile/split/profile_split_protocol_v1.c" "$ROOT/users/noah/lib/compat/qmk_profile_split_transport.c" "$ROOT/users/noah/lib/compat/qmk_via_split_sync.c" "$ROOT/users/noah/lib/compat/qmk_via_sync_metadata.c" "$ROOT/users/noah/lib/compat/qmk_via_sync_protocol.c" "$ROOT/users/noah/lib/compat/qmk_via_sync_state.c" "$ROOT/tests/host/qmk_lighting_snapshot_base_peer.c" "$ROOT/users/noah/lib/split/runtime_sync.c" "$ROOT/users/noah/lib/split/runtime_sync_dirty.c" "$ADAPTER" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" "$ROOT/users/noah/lib/compat/qmk_lighting_live.c" "$SERVICE_SOURCE" "$BUILD_DIR/peer_$variant.o" "$BUILD_DIR/live_peer_$variant.o" -o "$BUILD_DIR/combined_$variant"
    if [ "$#" -gt 0 ]; then "$BUILD_DIR/combined_$variant" "$1"; else
        for mode in 0 1 2 3 7 8 9 10 11; do "$BUILD_DIR/combined_$variant" "$mode"; done
    fi
done

if [ "$#" = 0 ]; then
    if sh "$0" --unpaced 0 > "$BUILD_DIR/unpaced.log" 2>&1; then
        echo 'unpaced negative control unexpectedly passed' >&2; exit 1
    fi
    cat "$BUILD_DIR/unpaced.log"
    grep -q 'blocked=734/1000 calls=266' "$BUILD_DIR/unpaced.log"
    echo 'original unpaced service-pressure negative control retained'
fi

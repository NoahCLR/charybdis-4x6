#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
ADAPTER=""
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in
        users/noah/lib/compat/qmk_lighting_event_transport.c) ADAPTER="$ROOT/$src" ;;
        users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_event_receiver.c|users/noah/lib/rgb/lighting/lighting_event_sender.c|users/noah/lib/rgb/lighting/lighting_lifetime.c) SOURCES="$SOURCES $ROOT/$src" ;;
    esac
done
[ -n "$ADAPTER" ]
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DSPLIT_TRANSACTION_IDS_USER -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32 -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        -Dnoah_qmk_lighting_event_transport_init=peer_init -Dnoah_qmk_lighting_event_transport_step=peer_step -c "$ADAPTER" -o "$BUILD_DIR/peer_$variant.o"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DSPLIT_TRANSACTION_IDS_USER -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32 -DQMK_KEYBOARD_H='"qmk_stub.h"' \
        -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
        "$ROOT/tests/host/qmk_lighting_event_transport_test.c" "$ADAPTER" $SOURCES "$BUILD_DIR/peer_$variant.o" -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant"
done

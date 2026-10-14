#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_source_manifest.sh"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
SOURCES="$(noah_source_manifest_absolute_userspace_paths_selected "$ROOT" NOAH_COMMON_SOURCES 'lib/rgb/lighting/lighting_event_protocol.c lib/rgb/lighting/lighting_event_receiver.c lib/rgb/lighting/lighting_event_sender.c')"
python3 "$ROOT/tests/host/lighting_event_transport_test.py" "$ROOT" "$BUILD_DIR/vectors.txt"
for variant in normal sanitized; do
    SANITIZERS=""
    if [ "$variant" = sanitized ]; then SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
    for component in codec receiver sender; do
        # shellcheck disable=SC2086
        cc -std=c11 -Wall -Wextra -Werror -pedantic $SANITIZERS \
          -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRPC_M2S_BUFFER_SIZE=128 -DRPC_S2M_BUFFER_SIZE=32 -I"$ROOT" \
          "$ROOT/tests/host/lighting_event_${component}_test.c" $SOURCES -o "$BUILD_DIR/probe"
        if [ "$component" = codec ]; then "$BUILD_DIR/probe" "$BUILD_DIR/vectors.txt"; else "$BUILD_DIR/probe"; fi
    done
done
# A consumer must not fit by hiding a request larger than its QMK allocation.
if cc -std=c11 -Werror -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DRPC_M2S_BUFFER_SIZE=32 -DRPC_S2M_BUFFER_SIZE=32 \
    -I"$ROOT" -x c -c -o "$BUILD_DIR/rejected.o" - >"$BUILD_DIR/rejected.log" 2>&1 <<'SOURCE'
#include "users/noah/lib/rgb/lighting/lighting_event_protocol.h"
SOURCE
then
    echo 'event transport unexpectedly accepted an undersized QMK request allocation' >&2; exit 1
fi
if ! rg -q 'event request fits existing QMK allocation' "$BUILD_DIR/rejected.log"; then
    cat "$BUILD_DIR/rejected.log" >&2; exit 1
fi
# Disabled consumer creates no transport-allocation requirement.
cc -std=c11 -Werror -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=0 -DRPC_M2S_BUFFER_SIZE=32 -DRPC_S2M_BUFFER_SIZE=32 \
    -I"$ROOT" -x c -c -o "$BUILD_DIR/disabled.o" - <<'SOURCE'
#include "users/noah/lib/rgb/lighting/lighting_event_protocol.h"
SOURCE
printf '%s\n' 'lighting grouped event codec/session/retry tests passed (normal and sanitized)'

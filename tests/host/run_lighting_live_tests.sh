#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
python3 "$ROOT/tools/lighting_live_reference.py" --check --vectors "$BUILD_DIR/vectors"
SOURCE="$ROOT/users/noah/lib/compat/qmk_lighting_live.c"
for variant in normal sanitized; do
    san=""
    [ "$variant" != sanitized ] || san='-fsanitize=address,undefined -fno-omit-frame-pointer'
    flags="-std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic -DNOAH_LIGHTING_LIVE_TEST -DNOAH_LIGHTING_LIVE_ENABLE=1 -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DVIA_ENABLE -DSPLIT_KEYBOARD -DRPC_M2S_BUFFER_SIZE=128 -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE"
    # Same production module, two separately owned halves; no alternate receiver.
    # shellcheck disable=SC2086
    cc $flags $san -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      -Dnoah_lighting_live_test_exhaust=peer_test_exhaust -Dnoah_lighting_live_handle=peer_handle -Dnoah_lighting_live_task=peer_task -Dnoah_lighting_live_close=peer_close -Dnoah_lighting_live_colour=peer_colour \
      -c "$SOURCE" -o "$BUILD_DIR/peer.o"
    # shellcheck disable=SC2086
    cc $flags $san -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_live_test.c" "$SOURCE" "$ROOT/users/noah/lib/compat/qmk_split_service.c" "$BUILD_DIR/peer.o" -o "$BUILD_DIR/probe"
    "$BUILD_DIR/probe" "$BUILD_DIR/vectors"
    for namespace in 1 2 3 4; do "$BUILD_DIR/probe" "$BUILD_DIR/vectors" "$namespace"; done
done
# No unavailable module creates storage, transport or unresolved link requirements.
cc -std=c11 -Wall -Wextra -Werror -pedantic -I"$ROOT" -c "$SOURCE" -o "$BUILD_DIR/off.o"
if nm -u "$BUILD_DIR/off.o" | rg .; then echo 'disabled live module has dependencies' >&2;exit 1;fi

for excluded in RGB_MATRIX_ENABLE VIA_ENABLE NOAH_LIVE_PROFILE_OWNER_ENABLE SPLIT_KEYBOARD; do
    flags="-DNOAH_LIGHTING_LIVE_ENABLE=1 -DRGB_MATRIX_ENABLE -DVIA_ENABLE -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DSPLIT_KEYBOARD"
    # The last -U overrides the otherwise enabled prerequisites.
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -U"$excluded" -I"$ROOT" -c "$SOURCE" -o "$BUILD_DIR/off.o"
    if nm -u "$BUILD_DIR/off.o" | rg .; then echo "missing $excluded still created live dependencies" >&2;exit 1;fi
done
# No storage/candidate entry exists anywhere in the live production object.
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic -DNOAH_LIGHTING_LIVE_ENABLE=1 -DNOAH_LIVE_PROFILE_OWNER_ENABLE -DVIA_ENABLE -DSPLIT_KEYBOARD -DRPC_M2S_BUFFER_SIZE=128 -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" -c "$SOURCE" -o "$BUILD_DIR/live.o"
if nm -u "$BUILD_DIR/live.o" | rg '(candidate|eeprom|profile_store|write|save)'; then echo 'volatile live object acquired a storage dependency' >&2;exit 1;fi

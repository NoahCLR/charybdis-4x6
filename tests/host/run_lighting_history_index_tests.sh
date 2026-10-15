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
for case in json.loads(pathlib.Path(sys.argv[1]).read_text())['historyIndex']:
    lines.append(f"{case['name']} {case['hex']} {case['total']}")
    lines += [' '.join(map(str,row)) for row in case['rules']]
pathlib.Path(sys.argv[2]).write_text('\n'.join(lines)+'\n')
PY
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_history_index_test.c" $SOURCES \
      "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/$variant"
    "$BUILD_DIR/$variant" "$BUILD_DIR/vectors.txt"
done
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb -DNOAH_LIGHTING_SCHEMA_ALL_KINDS \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_history_index.c" -o "$BUILD_DIR/index_arm.o"

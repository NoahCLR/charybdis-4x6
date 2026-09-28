#!/bin/sh
# Firmware macro sizing against a frozen regression corpus. Live owns the
# separate comparison against its current encoder and size estimator.
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$QMK_ROOT/platforms" -I"$QMK_ROOT/quantum/send_string" \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    "$ROOT/tests/host/macro_program_size_probe.c" \
    "$ROOT/users/noah/lib/macro/macro_payload.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_decode_qmk.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_keycodes.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_parse.c" \
    "$ROOT/users/noah/lib/macro/macro_payload_encode.c" \
    -o "$BUILD_DIR/probe"
python3 - "$ROOT/tests/fixtures/client-regression/macro-corpus.json.gz" "$BUILD_DIR/probe" <<'PYCODE'
import gzip, json, pathlib, subprocess, sys
cases = json.loads(gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes()))
result = subprocess.run([sys.argv[2]], input="".join(c["hex"] + "\n" for c in cases), text=True, capture_output=True, check=True)
actual = [int(value) for value in result.stdout.splitlines()]
assert actual == [c["expected"] for c in cases], "Firmware macro sizing differs from frozen regression corpus"
print(f"Firmware macro sizing: {len(cases)} regression cases passed")
PYCODE

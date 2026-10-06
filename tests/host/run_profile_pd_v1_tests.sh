#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
python3 - "$ROOT/tests/fixtures/client-regression/pd-corpus.bin.gz" "$BUILD_DIR/corpus.bin" <<'PYCODE'
import gzip, pathlib, sys
pathlib.Path(sys.argv[2]).write_bytes(gzip.decompress(pathlib.Path(sys.argv[1]).read_bytes()))
PYCODE
python3 - "$ROOT/tests/fixtures/pd_mode_domain_v2.json" "$BUILD_DIR/v2.txt" <<'PYCODE'
import json, pathlib, sys
fixture = json.loads(pathlib.Path(sys.argv[1]).read_text())
lines = [f"OK 0 {case['name']} {case['hex']}" for case in fixture['valid']]
lines += [f"{case['error']['code']} {case['error']['offset']} {case['name']} {case['hex']}" for case in fixture['invalid']]
pathlib.Path(sys.argv[2]).write_text("\n".join(lines) + "\n")
PYCODE

build_and_run() {
    name="$1"
    shift
    cc -std=c11 -Wall -Wextra -Werror -pedantic "$@" -I"$ROOT" \
        "$ROOT/tests/host/profile_pd_v1_test.c" \
        "$ROOT/users/noah/lib/profile/schema/profile_pd_v1.c" \
        -o "$BUILD_DIR/$name"
    "$BUILD_DIR/$name" "$BUILD_DIR/corpus.bin" "$BUILD_DIR/v2.txt"
}
build_and_run normal
build_and_run sanitized -fsanitize=address,undefined -fno-omit-frame-pointer

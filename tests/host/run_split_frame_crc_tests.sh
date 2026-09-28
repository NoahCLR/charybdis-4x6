#!/bin/sh
# The fork's split serial protocol with the frame CRC, driven over a
# fault-injectable link (split_frame_crc_test.py).
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
python3 "$ROOT/tests/host/split_frame_crc_test.py" "$QMK_ROOT" "$BUILD_DIR" "$ROOT"

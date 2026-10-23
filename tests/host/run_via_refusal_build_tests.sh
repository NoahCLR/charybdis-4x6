#!/bin/sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/host/via_refusal_build_test.py"

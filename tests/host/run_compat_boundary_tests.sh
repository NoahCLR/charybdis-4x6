#!/bin/sh
set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/host/compat_boundary_test.py"

#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
PYTHONDONTWRITEBYTECODE=1 python3 "$ROOT/tests/host/split_transport_build_test.py"

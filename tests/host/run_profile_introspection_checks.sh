#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
PYTHON="${PYTHON:-python3}"

"$PYTHON" "$ROOT/../../tools/profile_introspect.py" --check

#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

python3 "$ROOT/../../tools/profile_introspect.py" --check
python3 "$ROOT/check_profile_introspection_regressions.py"

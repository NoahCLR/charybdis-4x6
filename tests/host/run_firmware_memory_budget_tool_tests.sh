#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
PYTHON="${PYTHON:-python3}"
TMP_ROOT="${TMPDIR:-/tmp}"

export PYTHONDONTWRITEBYTECODE=1
export PYTHONPYCACHEPREFIX="$TMP_ROOT/charybdis-memory-budget-pycache"

"$PYTHON" "$ROOT/tests/host/firmware_memory_budget_tool_test.py"

echo "firmware memory-budget tool tests passed"


#!/bin/sh

set -eu

ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
PYTHON="${PYTHON:-python3}"
TMP_ROOT="${TMPDIR:-/tmp}"

export PYTHONDONTWRITEBYTECODE=1
export PYTHONPYCACHEPREFIX="$TMP_ROOT/charybdis-stack-budget-pycache"

"$PYTHON" "$ROOT/tests/host/firmware_stack_budget_tool_test.py"

echo "firmware stack-budget tool tests passed"

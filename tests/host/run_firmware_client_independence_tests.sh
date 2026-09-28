#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
python3 "$ROOT/tests/host/firmware_client_independence_test.py"

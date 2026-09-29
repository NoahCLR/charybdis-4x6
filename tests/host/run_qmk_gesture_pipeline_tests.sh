#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
NOAH_TEST_QMK_GESTURES=1 sh "$ROOT/run_pd_mode_key_runtime_integration_tests.sh"

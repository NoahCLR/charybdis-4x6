#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
USERSPACE="$ROOT/../../users/noah"

# Hold progress has one gate: key_runtime_core_press_token_hold_eligible(),
# for releases queued in QMK and in record admission alike.
callers="$(grep -rlE "noah_qmk_(gesture|admission)_release_pending\(" "$USERSPACE" | grep -v "/lib/compat/qmk_gesture_timing.h$" | grep -v "/lib/key/runtime/reducer/state_query.c$" || true)"
if [ -n "$callers" ]; then
    echo "queued-release checks must go through key_runtime_core_press_token_hold_eligible():" >&2
    echo "$callers" >&2
    exit 1
fi

NOAH_TEST_QMK_GESTURES=1 sh "$ROOT/run_pd_mode_key_runtime_integration_tests.sh"

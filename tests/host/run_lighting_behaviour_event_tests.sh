#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES="$(noah_host_key_runtime_scenario_support_paths "$ROOT")"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
for depth in 5 8 64 255; do
for sanitize in no yes; do
    SANITIZERS=""
    if [ "$sanitize" = yes ]; then SANITIZERS='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $SANITIZERS \
      -DQMK_KEYBOARD_H='"qmk_stub.h"' -DSPLIT_TRANSACTION_IDS_USER -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 \
      -DKEY_BEHAVIOR_MAX_TAP_COUNT="$depth" \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/key_runtime_scenario_harness.c" "$ROOT/tests/host/lighting_behaviour_event_test.c" \
      $SOURCES -o "$BUILD_DIR/probe"
    "$BUILD_DIR/probe"
done
done

#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_userspace_paths "$ROOT" NOAH_COMMON_SOURCES); do
 case "$src" in
 users/noah/lib/rgb/lighting/lighting_history_owner.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c|users/noah/lib/rgb/lighting/lighting_event_capture.c|users/noah/lib/rgb/lighting/lighting_occurrence.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_retained_group.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_lifetime.c|users/noah/lib/rgb/lighting/kinds/event_layer_locked.c|users/noah/lib/rgb/lighting/kinds/event_layer_unlocked.c|users/noah/lib/rgb/lighting/kinds/fact_layer_active.c|users/noah/lib/rgb/lighting/kinds/fact_layer_status.c) SOURCES="$SOURCES $ROOT/$src";;
 esac
done
for variant in normal sanitized; do
 flags="";[ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
 # shellcheck disable=SC2086
 cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter -pedantic $flags -DNOAH_HOST_TESTS -DRGB_MATRIX_ENABLE -DNOAH_LIGHTING_EVENT_CAPTURE_ENABLE=1 -DNOAH_LIGHTING_FACT_LAYER_STATUS=1 -DNOAH_LIGHTING_FACT_LAYER_LOCKED=1 -DNOAH_LIGHTING_FACT_LAYER_UNLOCKED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DQMK_KEYBOARD_H='"qmk_stub.h"' -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
 "$ROOT/tests/host/lighting_layer_capture_integration_test.c" $SOURCES \
 "$ROOT/users/noah/lib/state/ownership/layer_ownership.c" "$ROOT/users/noah/lib/state/diagnostics/runtime_diag.c" "$ROOT/users/noah/lib/state/shared/runtime_shared_state.c" "$ROOT/tests/host/key_runtime_core_state_unit_stub.c" "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/test-$variant"
 "$BUILD_DIR/test-$variant"
done

#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
. "$ROOT/tests/host/noah_source_manifest.sh"
SOURCES=""
for src in $(noah_source_manifest_lighting_test_paths "$ROOT"); do
    case "$src" in users/noah/lib/rgb/lighting/lighting_history_owner.c|users/noah/lib/rgb/lighting/lighting_history_index.c|users/noah/lib/rgb/lighting/kinds/timing_show_after.c|users/noah/lib/rgb/lighting/kinds/timing_linger.c|users/noah/lib/rgb/lighting/kinds/timing_flash.c|users/noah/lib/rgb/lighting/lighting_event_protocol.c|users/noah/lib/rgb/lighting/lighting_history_bank.c|users/noah/lib/rgb/lighting/kinds/fact_key_pressed.c|users/noah/lib/rgb/lighting/lighting_occurrence.c|users/noah/lib/rgb/lighting/kinds/event_macro_played.c|users/noah/lib/rgb/lighting/lighting_history_store.c|users/noah/lib/rgb/lighting/lighting_batch.c|users/noah/lib/rgb/lighting/lighting_batch_arena.c|users/noah/lib/rgb/lighting/lighting_owned_batch.c|users/noah/lib/rgb/lighting/lighting_retained.c|users/noah/lib/rgb/lighting/lighting_retained_group.c|users/noah/lib/rgb/lighting/lighting_lifetime.c) SOURCES="$SOURCES $ROOT/$src" ;; esac
done
for variant in normal sanitized; do
    flags=""
    [ "$variant" != sanitized ] || flags="-fsanitize=address,undefined -fno-omit-frame-pointer"
    # The independent shared calendars also pass through real validated rule
    # bytes, optional timing bindings and the state/event owner, without counters.
    # shellcheck disable=SC2086
    cc -std=c11 -Wall -Wextra -Werror -pedantic $flags -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_ARENA_USAGE_TEST_BACKEND -DNOAH_LIGHTING_FACT_KEY_PRESSED=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DNOAH_LIGHTING_FACT_LAYER_LOCKED=0 -DNOAH_LIGHTING_FACT_LAYER_UNLOCKED=0 -DNOAH_LIGHTING_FACT_ONESHOT_USED=0 -DNOAH_LIGHTING_FACT_MODE_STARTED=0 -DNOAH_LIGHTING_FACT_MODE_ENDED=0 -DNOAH_LIGHTING_FACT_COMBO_FIRED=0 -DNOAH_LIGHTING_FACT_TAP_FIRED=0 -DNOAH_LIGHTING_FACT_HOLD_FIRED=0 -DNOAH_LIGHTING_FACT_LONG_HOLD_FIRED=0 -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
      -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
      "$ROOT/tests/host/lighting_history_capacity_test.c" $SOURCES "$ROOT/users/noah/lib/profile/schema/profile_lighting_v1.c" "$ROOT/users/noah/lib/profile/schema/profile_reader.c" -o "$BUILD_DIR/lifetimes_$variant"
    "$BUILD_DIR/lifetimes_$variant"
done
# Compile the actual owner implementation for the target ISA too. Linked paths
# and resource policies still require the later enabled, exact-head pair gate.
"${ARM_CC:-arm-none-eabi-gcc}" -std=c11 -Wall -Wextra -Werror -pedantic -mcpu=cortex-m0plus -mthumb \
    -DMATRIX_ROWS=10 -DMATRIX_COLS=6 -DNOAH_LIGHTING_ARENA_USAGE_TEST_BACKEND -DNOAH_LIGHTING_FACT_KEY_PRESSED=1 -DNOAH_LIGHTING_FACT_MACRO_PLAYED=1 -DNOAH_LIGHTING_TIMING_FLASH=1 -DNOAH_LIGHTING_SCHEMA_ALL_KINDS -DNOAH_LIGHTING_FACT_LAYER_LOCKED=0 -DNOAH_LIGHTING_FACT_LAYER_UNLOCKED=0 -DNOAH_LIGHTING_FACT_ONESHOT_USED=0 -DNOAH_LIGHTING_FACT_MODE_STARTED=0 -DNOAH_LIGHTING_FACT_MODE_ENDED=0 -DNOAH_LIGHTING_FACT_COMBO_FIRED=0 -DNOAH_LIGHTING_FACT_TAP_FIRED=0 -DNOAH_LIGHTING_FACT_HOLD_FIRED=0 -DNOAH_LIGHTING_FACT_LONG_HOLD_FIRED=0 -DRGB_MATRIX_ENABLE -DQMK_KEYBOARD_H='"qmk_stub.h"' \
    -I"$ROOT" -I"$ROOT/users/noah" -I"$ROOT/tests/host/include" \
    -c "$ROOT/users/noah/lib/rgb/lighting/lighting_history_bank.c" -o "$BUILD_DIR/bank_arm.o"

#!/bin/sh
set -eu
ROOT="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
. "$ROOT/tests/host/noah_host_qmk_env.sh"
noah_host_export_qmk_cpath "$ROOT"
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT INT TERM
cc -std=c11 -Wall -Wextra -Werror -I"$ROOT" "$ROOT/tests/host/split_activity_policy_test.c" -o "$BUILD_DIR/policy"
"$BUILD_DIR/policy"
python3 "$ROOT/tests/host/split_activity_qmk_test.py" "$QMK_ROOT" "$BUILD_DIR" "$ROOT"

mkdir "$BUILD_DIR/include"
cat > "$BUILD_DIR/include/transactions.h" <<'HEADER'
#define NUM_TOTAL_TRANSACTIONS 8
#define PUT_ACTIVITY 1
#define QMK_SPLIT_ACTIVITY_POLICY_VERSION 1
HEADER
cat > "$BUILD_DIR/include/diagnostic_qmk.h" <<'HEADER'
#include <stdint.h>
#include <stdbool.h>
uint32_t test_clock(void);
bool is_keyboard_master(void);
#define chSysGetRealtimeCounterX test_clock
HEADER
for crc in "" "-DSPLIT_TRANSPORT_CRC"; do
    cc -std=c11 -Wall -Wextra -Werror -DSPLIT_TRANSACTION_DIAGNOSTICS $crc \
        -DQMK_KEYBOARD_H='"diagnostic_qmk.h"' -I"$BUILD_DIR/include" -I"$ROOT" \
        "$ROOT/tests/host/split_diagnostics_test.c" "$ROOT/users/noah/lib/compat/qmk_split_diagnostics.c" -o "$BUILD_DIR/diagnostics"
    "$BUILD_DIR/diagnostics"
done

node "$ROOT/tests/host/split_diagnostics_tool_test.cjs"

cat > "$BUILD_DIR/include/transactions.h" <<'HEADER'
#pragma once
#include <stdint.h>
#include <stdbool.h>
#define QMK_SPLIT_ACTIVITY_POLICY_VERSION 1
typedef struct {uint32_t matrix_timestamp,encoder_timestamp,pointing_device_timestamp;} split_slave_activity_sync_t;
bool split_activity_sync_should_send(const split_slave_activity_sync_t *,const split_slave_activity_sync_t *,uint32_t,bool,bool);
void split_activity_sync_sent(bool);
HEADER
cat > "$BUILD_DIR/include/adapter_qmk.h" <<'HEADER'
#pragma once
#include <stdint.h>
#include <stdbool.h>
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t);
HEADER
cat > "$BUILD_DIR/include/sync_timer.h" <<'HEADER'
#include <stdint.h>
uint32_t sync_timer_elapsed32(uint32_t);
HEADER
for portable in "" "-DNOAH_PORTABLE_PROFILE_ENABLE"; do
    cc -std=c11 -Wall -Wextra -Werror $portable -DSPLIT_ACTIVITY_ENABLE -DNOAH_SPLIT_ACTIVITY_COALESCE_ENABLE \
        -DRGB_MATRIX_TIMEOUT=900000 -DQMK_KEYBOARD_H='"adapter_qmk.h"' -I"$BUILD_DIR/include" -I"$ROOT" \
        "$ROOT/tests/host/split_activity_adapter_test.c" "$ROOT/users/noah/lib/compat/qmk_split_activity.c" -o "$BUILD_DIR/adapter"
    "$BUILD_DIR/adapter"
done

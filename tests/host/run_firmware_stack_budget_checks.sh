#!/bin/sh

# Target-only check: this compiles firmware and requires the ARM/QMK toolchain.
# Keep it separate from run_all_host_tests.sh, which must remain host-only.

set -eu

HOST_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$HOST_ROOT/../.." && pwd)"
PYTHON="${PYTHON:-python3}"
TARGET="${NOAH_STACK_BUDGET_TARGET:-bastardkb_charybdis_4x6_noah}"
KEYBOARD="${NOAH_STACK_BUDGET_KEYBOARD:-bastardkb/charybdis/4x6}"
KEYMAP="${NOAH_STACK_BUDGET_KEYMAP:-noah}"

. "$HOST_ROOT/noah_host_qmk_env.sh"
QMK_ROOT="$(noah_host_find_qmk_root "$ROOT")"
export QMK_ROOT

BUILD_MARKER="$(mktemp "${TMPDIR:-/tmp}/noah-stack-budget-build.XXXXXX")"
trap 'rm -f "$BUILD_MARKER"' 0 HUP INT TERM

NOAH_STACK_BUDGET_ENABLE=yes QMK_USERSPACE="$ROOT" \
    qmk compile -c -kb "$KEYBOARD" -km "$KEYMAP"

BUILD_ROOT="$QMK_ROOT/.build"
OBJECT_ROOT="$BUILD_ROOT/obj_$TARGET"
ELF="$BUILD_ROOT/$TARGET.elf"
MAP="$BUILD_ROOT/$TARGET.map"
CFLAGS_FILE="$OBJECT_ROOT/cflags.txt"
LDFLAGS_FILE="$OBJECT_ROOT/ldflags.txt"

if [ ! -f "$ELF" ]; then
    echo "stack-budget ELF not found after target build: $ELF" >&2
    exit 1
fi
if [ ! -f "$MAP" ]; then
    echo "stack-budget linker map not found after target build: $MAP" >&2
    exit 1
fi
for artifact in "$ELF" "$MAP" "$CFLAGS_FILE" "$LDFLAGS_FILE"; do
    if [ ! -f "$artifact" ] || [ ! "$artifact" -nt "$BUILD_MARKER" ]; then
        echo "stack-budget artifact was not freshly produced by this build: $artifact" >&2
        exit 1
    fi
done
if ! grep -F -- '-fno-shrink-wrap' "$CFLAGS_FILE" >/dev/null; then
    echo "target build did not record -fno-shrink-wrap in $CFLAGS_FILE" >&2
    exit 1
fi
if ! grep -F -- "$TARGET.map" "$LDFLAGS_FILE" >/dev/null; then
    echo "target build did not record the stack-budget linker map in $LDFLAGS_FILE" >&2
    exit 1
fi

"$PYTHON" "$ROOT/tools/check_firmware_stack_budget.py" \
    --manifest "$ROOT/tools/firmware_stack_budget.json" \
    --elf "$ELF" \
    --map "$MAP" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --objdump "${OBJDUMP:-arm-none-eabi-objdump}"

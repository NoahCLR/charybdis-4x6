#!/bin/sh

# Target-only engineering check: this compiles firmware and requires the
# ARM/QMK toolchain. It is deliberately separate from the host-only suite and
# from the ordinary production-firmware stack manifest.

set -eu

HOST_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$HOST_ROOT/../.." && pwd)"
PYTHON="${PYTHON:-python3}"
TARGET="bastardkb_charybdis_4x6_noah_live_mutation_stack_left"

. "$HOST_ROOT/noah_host_qmk_env.sh"
QMK_ROOT="$(noah_host_find_qmk_root "$ROOT")"
export QMK_ROOT

BUILD_MARKER="$(mktemp "${TMPDIR:-/tmp}/noah-live-owner-stack-build.XXXXXX")"
trap 'rm -f "$BUILD_MARKER"' 0 HUP INT TERM

QMK_USERSPACE="$ROOT" qmk compile -c \
    -kb bastardkb/charybdis/4x6 \
    -km noah \
    -e NOAH_STACK_BUDGET_ENABLE=yes \
    -e NOAH_LIVE_PROFILE_OWNER=yes \
    -e NOAH_LIVE_PROFILE_MUTATION=yes \
    -e NOAH_PHYSICAL_HALF=left \
    -e FORCE_SLAVE=yes \
    -e TARGET="$TARGET"

BUILD_ROOT="$QMK_ROOT/.build"
OBJECT_ROOT="$BUILD_ROOT/obj_$TARGET"
ELF="$BUILD_ROOT/$TARGET.elf"
MAP="$BUILD_ROOT/$TARGET.map"
CFLAGS_FILE="$OBJECT_ROOT/cflags.txt"
LDFLAGS_FILE="$OBJECT_ROOT/ldflags.txt"

for artifact in "$ELF" "$MAP" "$CFLAGS_FILE" "$LDFLAGS_FILE"; do
    if [ ! -f "$artifact" ] || [ ! "$artifact" -nt "$BUILD_MARKER" ]; then
        echo "live-owner stack artifact was not freshly produced: $artifact" >&2
        exit 1
    fi
done
if ! grep -F -- '-fno-shrink-wrap' "$CFLAGS_FILE" >/dev/null; then
    echo "engineering target did not record -fno-shrink-wrap in $CFLAGS_FILE" >&2
    exit 1
fi
if ! grep -F -- "$TARGET.map" "$LDFLAGS_FILE" >/dev/null; then
    echo "engineering target did not record its linker map in $LDFLAGS_FILE" >&2
    exit 1
fi

"$PYTHON" "$ROOT/tools/check_firmware_stack_budget.py" \
    --manifest "$ROOT/tools/firmware_stack_budget_live_profile_owner.json" \
    --elf "$ELF" \
    --map "$MAP" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --objdump "${OBJDUMP:-arm-none-eabi-objdump}"

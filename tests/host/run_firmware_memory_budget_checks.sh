#!/bin/sh

# Target-only check: run after a fresh ordinary firmware compile.

set -eu

HOST_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
ROOT="$(CDPATH= cd -- "$HOST_ROOT/../.." && pwd)"
PYTHON="${PYTHON:-python3}"
TARGET="${NOAH_MEMORY_BUDGET_TARGET:-bastardkb_charybdis_4x6_noah}"

. "$HOST_ROOT/noah_host_qmk_env.sh"
QMK_ROOT="$(noah_host_find_qmk_root "$ROOT")"
ELF="$QMK_ROOT/.build/$TARGET.elf"

if [ ! -f "$ELF" ]; then
    echo "memory-budget ELF not found: $ELF" >&2
    exit 1
fi
if find "$ROOT/users/noah" "$ROOT/keyboards/bastardkb/charybdis/4x6/keymaps/noah" \
    -type f \( -name '*.c' -o -name '*.h' -o -name '*.mk' \) -newer "$ELF" -print -quit | grep . >/dev/null; then
    echo "memory-budget ELF is older than firmware source; run a fresh target compile" >&2
    exit 1
fi

# The PD geometry adds 2 KiB of wear-level cache plus the bounded mode cache.
# Keep the old artifact policy intact; select the reviewed increments only from
# this artifact's recorded compiler flags (never its target name): 3 KiB for
# the eight-slot PD profile, then 4 KiB for 32 slots (the 32-slot mode cache and
# 32-bit mode masks). Both are policy, not capacity. See
# docs/architecture/memory-budgets.md for linked accounting and the
# outstanding physical high-water release gate.
STATIC_RAM_POLICY=57344
CFLAGS_FILE="$QMK_ROOT/.build/obj_$TARGET/cflags.txt"
if [ -f "$CFLAGS_FILE" ] && grep -Eq '(^|[[:space:]])-DNOAH_PD_PROFILE_ENABLE([[:space:]]|$)' "$CFLAGS_FILE"; then
    STATIC_RAM_POLICY=$((60416 + 4096))
fi

"$PYTHON" "$ROOT/tools/check_firmware_memory_budget.py" \
    --elf "$ELF" \
    --max-data-bss "$STATIC_RAM_POLICY" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --size "${SIZE:-arm-none-eabi-size}"

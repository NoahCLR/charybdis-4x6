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

"$PYTHON" "$ROOT/tools/check_firmware_memory_budget.py" \
    --elf "$ELF" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --size "${SIZE:-arm-none-eabi-size}"


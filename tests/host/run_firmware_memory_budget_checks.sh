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
# 32-bit mode masks). D-F14 then mirrors 140 KiB of logical EEPROM instead
# of 18 KiB, 124,928 more cache bytes, and its capacities (128 combos of
# sixteen inputs, the behaviour row index, 272 names, 128-byte pointing
# records) another 20 KiB over the 17,400 bytes they linked at. D-F26's
# lighting rules add one reviewed 16 KiB increment for the decoded program,
# the compiled Standard lighting and the engine's working set. Its portable RGB
# query configuration has a further reviewed 3 KiB for the geometry-bounded
# query basis. Compiling both consumers out retains the preceding threshold.
# All are
# policy, not capacity. See
# docs/architecture/memory-budgets.md for linked accounting and the
# outstanding physical high-water release gate.
CFLAGS_FILE="$QMK_ROOT/.build/obj_$TARGET/cflags.txt"
. "$HOST_ROOT/noah_firmware_memory_policy.sh"
QUERY_CACHE_LINKED=no
if "${NM:-arm-none-eabi-nm}" --defined-only "$ELF" | grep -Eq '[[:space:]]noah_lighting_keymap_refresh$'; then
    QUERY_CACHE_LINKED=yes
fi
STATIC_RAM_POLICY="$(noah_firmware_static_ram_policy "$CFLAGS_FILE" "$QUERY_CACHE_LINKED")"

"$PYTHON" "$ROOT/tools/check_firmware_memory_budget.py" \
    --elf "$ELF" \
    --max-data-bss "$STATIC_RAM_POLICY" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --size "${SIZE:-arm-none-eabi-size}"

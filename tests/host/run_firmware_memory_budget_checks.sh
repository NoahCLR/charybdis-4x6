#!/bin/sh

# Target-only check: run after a fresh ordinary firmware compile.

set -eu
. "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/in_build_image.sh"

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

# Every artifact carries the 32-slot PD profile engine. The policy is the
# 56 KiB pre-PD baseline plus each reviewed increment: 3 KiB for the
# eight-slot PD profile (2 KiB of wear-level cache plus the bounded mode
# cache), then 4 KiB for 32 slots (the 32-slot mode cache and 32-bit mode
# masks). D-F14 then mirrors 140 KiB of logical EEPROM instead
# of 18 KiB, 124,928 more cache bytes, and its capacities (128 combos of
# sixteen inputs, the behaviour row index, 272 names, 128-byte pointing
# records) another 20 KiB over the 17,400 bytes they linked at. D-F29 moves
# the profile slots out of logical EEPROM into their own flash region, so the
# cache mirrors the 36 KiB VIA region alone: 106,496 bytes fewer. D-F31 removes
# VIA, so it mirrors only QMK's 4 KiB eeconfig region: 32,768 bytes fewer.
# D-F32 adds the working copy, one slot payload of RAM (130,048 bytes), and
# its edit, sync and save state: one reviewed 130 KiB increment. D-F63 keeps
# each lighting group row's selector for a publication (656 bytes) and sizes
# the compiled lighting's cache to the authored tables (256 bytes fewer): 400
# bytes, as measured on both halves. D-F69 follows what is typed and holds
# keys back: its state with the queue of 240 changes (644 bytes), the scan's
# workspace for one 128-byte correction (704), the list's handle and
# publication count (28, D-F68) and the report filters' two working copies
# moved off the stack (64); with D-F66's switch keys that is 1,588 bytes, as
# measured on both halves (203,364 to 204,952 linked). All are policy, not
# capacity. See docs/spec/resource-policy.md for what each policy is and how
# to report it.
STATIC_RAM_POLICY=$((57344 + 3072 + 4096 + 124928 + 20480 - 106496 - 32768 + 133120 + 400 + 1588))

"$PYTHON" "$ROOT/tools/check_firmware_memory_budget.py" \
    --elf "$ELF" \
    --max-data-bss "$STATIC_RAM_POLICY" \
    --nm "${NM:-arm-none-eabi-nm}" \
    --size "${SIZE:-arm-none-eabi-size}"

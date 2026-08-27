# RP2040 Memory Model And Budget Policy

This document is the canonical memory terminology for this repository. Report
all firmware values per keyboard half: each half has its own RP2040 and runs its
own copy of the firmware.

## Physical Memory

The RP2040 contains 264 KiB, or 270,336 bytes, of SRAM.

| Linker region | Physical memory | Bytes | Current purpose |
| --- | --- | ---: | --- |
| `ram0` | word-striped SRAM0–3 | 262,144 | `.data`, `.bss`, small RAM-resident sections, and the remaining core-memory/newlib allocation span |
| `ram4` | SRAM4 | 4,096 | interrupt stack, process stack, and core-0 RTOS state |
| `ram5` | SRAM5 | 4,096 | reserved core-1 stacks; the overlapping 256-byte `ram7` boot region is not additional memory |

The target definition is
`../bastardkb-qmk/keyboards/bastardkb/charybdis/4x6/keyboard.json`; the active
linker map is
`../bastardkb-qmk/platforms/chibios/boards/common/ld/RP2040_FLASH_TIMECRIT.ld`.

## Current Linked Checkpoint

Freshly measured on 2026-08-27 after the effective combo-feedback RGB consumer:

| Measurement | Bytes | Meaning |
| --- | ---: | --- |
| SRAM0–3 `.data` | 22,984 | nonzero-initialized fixed data |
| SRAM0–3 `.bss` | 25,708 | zero-initialized fixed data |
| SRAM0–3 `.data + .bss` | 48,692 | regression metric, not total SRAM use |
| SRAM0–3 linker/core-memory span at boot | 213,448 | maximum allocator span before runtime allocations |
| Fixed linked section bytes across all SRAM banks | 56,152 | includes alignment, `.data`, `.bss`, RAM-resident sections, and reserved stacks; excludes runtime allocation |

The linker/core-memory span is `__heap_end__ - __heap_base__`. ChibiOS
initializes its core allocator from that range and the target's linked newlib
`_sbrk_r` obtains memory from it. It is not a second memory pool, guaranteed
unused RAM, or a runtime-free-memory measurement. Record allocator high-water
on hardware before making claims about runtime availability.

GNU `size` reports the linker-reserved `.heap` section and stack reservations
inside its aggregate BSS number. Do not add that aggregate BSS value to the
separate `.data`, `.bss`, core-memory-span, or stack figures.

## Regression Policies

The current automated thresholds are deliberately conservative policies:

| Policy | Limit | Current margin |
| --- | ---: | ---: |
| SRAM0–3 `.bss` maximum | 26,000 B | 292 B |
| SRAM0–3 `.data + .bss` maximum | 51,000 B | 2,308 B |
| SRAM0–3 linker/core-memory span minimum at boot | 204,800 B | 8,648 B |

These thresholds were introduced to detect regressions around earlier linked
images. They are not RP2040 capacity boundaries. A change may revise them only
with explicit rationale, bank-aware accounting, failure-path tests, a fresh
target build, and hardware allocator/stack evidence proportional to the risk.

For example, one nominal 4 KiB static profile buffer would raise the current
`.data + .bss` metric to about 52,796 bytes and leave about 209,344 bytes in the
boot core-memory span. Two would raise the metric to about 56,892 bytes and
leave about 205,248 bytes. Both designs violate current regression policies;
neither exhausts physical SRAM. The live-profile project keeps candidate bytes
in inactive EEPROM for power-loss-safe staging and deterministic memory use,
not because a 4 KiB buffer is physically impossible.

Repartitioning the existing 16 KiB logical EEPROM does not enlarge its linked
wear-level cache. Increasing logical EEPROM does increase that cache roughly
byte-for-byte and therefore requires a fresh linked measurement.

## Stack Accounting

SRAM4 is separate from the SRAM0–3 allocation span:

| SRAM4 reservation | Bytes |
| --- | ---: |
| Interrupt stack | 1,024 |
| Main process stack | 2,560 |
| Core-0 RTOS state | 288 |
| Unallocated outside those reservations | 224 |

The current worst reviewed process path is 1,880 bytes. It has 40 bytes to the
stricter 1,920-byte reviewed-path policy and 680 bytes to the physical
2,560-byte process-stack boundary. The stack tool analyzes named paths only;
it does not prove a global maximum or interrupt-stack high-water.

The split slave uses a separate 1,200-byte working area in SRAM0–3 with a
1,024-byte usable thread stack. It is not the RP2040 core-1 process stack.

## Required Reporting

Every memory report must name:

- the RP2040 half and physical/linker bank;
- fixed linked occupancy or the exact linker span being measured;
- the policy threshold separately from physical capacity;
- whether a number is a link-time value, reviewed-path estimate, or measured
  runtime high-water;
- the exact fresh build and command that produced it.

Use:

```sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
sh tests/host/run_firmware_memory_budget_checks.sh
sh tests/host/run_firmware_stack_budget_checks.sh
```

The stack runner performs its own clean instrumented build. Its PASS result is
explicitly limited to the paths in `tools/firmware_stack_budget.json`.

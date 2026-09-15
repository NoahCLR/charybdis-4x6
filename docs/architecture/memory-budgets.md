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

## Historical Ordinary Checkpoint — 2026-09-03

Freshly remeasured on 2026-09-03 at the first testable engineering live-edit
checkpoint.
The ordinary artifact does not link or allocate that engineering-only owner:

| Measurement | Bytes | Meaning |
| --- | ---: | --- |
| SRAM0–3 `.data` | 22,996 | nonzero-initialized fixed data |
| SRAM0–3 `.bss` | 25,876 | zero-initialized fixed data |
| SRAM0–3 `.data + .bss` | 48,872 | regression metric, not total SRAM use |
| SRAM0–3 linker/core-memory span at boot | 213,264 | maximum allocator span before runtime allocations |
| Fixed linked section bytes across all SRAM banks | 56,336 | includes alignment, `.data`, `.bss`, RAM-resident sections, and reserved stacks; excludes runtime allocation |

The linker/core-memory span is `__heap_end__ - __heap_base__`. ChibiOS
initializes its core allocator from that range and the target's linked newlib
`_sbrk_r` obtains memory from it. It is not a second memory pool, guaranteed
unused RAM, or a runtime-free-memory measurement. Record allocator high-water
on hardware before making claims about runtime availability.

## Historical Engineering Owner Checkpoint — 2026-09-03

The side-specific left engineering mutation artifact was freshly linked on
2026-09-03 with `NOAH_LIVE_PROFILE_OWNER=yes`,
`NOAH_LIVE_PROFILE_MUTATION=yes`, `NOAH_PHYSICAL_HALF=left`, and
`FORCE_SLAVE=yes`. It is deliberately not the ordinary firmware checkpoint:

| Measurement | Bytes | Policy result |
| --- | ---: | --- |
| Exact linked `runtime_owner` state | 3,172 | within its 4,096 B engineering state policy |
| SRAM0–3 `.data` | 23,000 | informational |
| SRAM0–3 `.bss` | 28,828 | **FAIL**, 2,828 B above the 26,000 B regression policy |
| SRAM0–3 `.data + .bss` | 51,828 | **FAIL**, 828 B above the 51,000 B regression policy |
| SRAM0–3 linker/core-memory span at boot | 210,312 | PASS, 5,512 B above the 204,800 B minimum |
| Fixed linked section bytes across all SRAM banks | 59,288 | informational |

These failures do not mean that the RP2040 is physically out of memory. Each
half still has 270,336 bytes of physical SRAM, and the linked SRAM0–3 core-
memory span passes its separate policy. They do mean that this artifact cannot
be promoted by silently treating old policy margin as hardware headroom.
Allocator and stack high-water measurements on the real two-half keyboard are
still required before revising the policies or enabling live mutation in the
ordinary build.

The dedicated engineering reviewed-path stack gate passes: the largest named
owner main-process path is split metadata exchange at 1,328/1,920 bytes, the
coherent VIA status-read path is 680/1,920 bytes, the Raw HID write dispatch is
392/1,920 bytes, and the largest named profile split callback is 264/768 bytes.
This is linked path evidence for
`tools/firmware_stack_budget_live_profile_owner.json`, not a global stack or
interrupt-stack maximum.

GNU `size` reports the linker-reserved `.heap` section and stack reservations
inside its aggregate BSS number. Do not add that aggregate BSS value to the
separate `.data`, `.bss`, core-memory-span, or stack figures.

## Current Portable-Profile Checkpoint — 2026-09-08

The eight-layer left artifact was built by `sh tools/build-firmware-pair.sh`
with `NOAH_PHYSICAL_HALF=left` and `FORCE_SLAVE=yes`. The owner and portable
settings are enabled. `sh tests/host/run_firmware_memory_budget_checks.sh`
measured these values per half:

| Measurement | Bytes | Meaning |
| --- | ---: | --- |
| SRAM0–3 `.data` | 4,152 | Fixed initialized data |
| SRAM0–3 `.bss` | 51,532 | Fixed zero-initialized data |
| `.data + .bss` | 55,684 | 1,660 B below the current 57,344 B regression tripwire |
| SRAM0–3 fixed linked prefix | 55,688 | Includes alignment |
| SRAM0–3 linker/core-memory span at boot | 206,456 | Allocator span before runtime allocation |
| Fixed linked occupancy across unique SRAM banks | 63,144 | Includes reserved stacks, excludes runtime allocation |

Portable settings add a 1,368-byte effective cache and a separate 1,368-byte
cold readback workspace. These avoid profile-reader calls during ordinary key,
macro, pointer and RGB execution. They do not enlarge logical EEPROM. The
incremental validator and provider state policies are 360 and 784 bytes.

Current automated policy is a 57,344-byte `.data + .bss` tripwire and a
4,096-byte minimum boot core-memory span. BSS alone is informational unless
`--max-static-bss` is explicitly provided. The older policy failures above are
historical, not the current gate thresholds. Neither current policy is the
RP2040's physical SRAM capacity. The 206,456-byte boot span is not a measured
runtime-free-memory value; allocator/stack high-water evidence remains pending.

One additional 4 KiB static buffer would raise `.data + .bss` to 59,780 bytes
and reduce the boot span to 202,360 bytes. That would cross the regression
tripwire without exhausting SRAM0–3. Any allocation still needs fresh linked
accounting and runtime evidence proportional to its use. Candidates remain in
inactive EEPROM for durable staging and deterministic memory use.

Repartitioning the existing 16 KiB logical EEPROM does not enlarge its linked
wear-level cache. Increasing logical EEPROM does increase that cache roughly
byte-for-byte and therefore requires a fresh linked measurement.

## Current Atomic-Apply Checkpoint — 2026-09-15

The normal eight-layer, owner-enabled left image from
`sh tools/build-firmware-pair.sh` links the `runtime_owner` at exactly 4,096
bytes, meeting its unchanged 4,096-byte engineering state policy. The owner
reuses boot-only store records for candidate staging after discovery and keeps
one pointer to immutable VIA transaction operations; these lifetime changes do
not alter the persistent record or wire layouts.

`sh tests/host/run_firmware_memory_budget_checks.sh` reports 4,152 bytes of
SRAM0–3 `.data`, 51,644 bytes of SRAM0–3 `.bss`, and 55,796 bytes combined.
That is 1,548 bytes below the 57,344-byte regression tripwire. The linked
SRAM0–3 core-memory span at boot is 206,344 bytes, and fixed occupancy across
the unique SRAM banks is 63,256 bytes. These are per-half linked measurements,
not runtime high-water values.

The fresh instrumented owner build passes the reviewed-path stack check. Its
largest named main-process path remains 1,824/1,920 bytes and its largest named
split-slave path remains 328/768 bytes. The manifest now follows VIA generation
commit through `noah_qmk_via_sync_state_complete_mutation`; the result covers
the named paths only.

## Stack Accounting

SRAM4 is separate from the SRAM0–3 allocation span:

| SRAM4 reservation | Bytes |
| --- | ---: |
| Interrupt stack | 1,024 |
| Main process stack | 2,560 |
| Core-0 RTOS state | 288 |
| Unallocated outside those reservations | 224 |

The portable image's reviewed process paths include complete settings/macro
readback and settings publication. The fresh linked measurements and exact
verification commands are recorded in the portable-profile acceptance notes
below. Keep reviewed-path margin separate from the physical 2,560-byte
process-stack boundary. The stack tool analyzes named paths only;
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

## Portable-Profile Stack Acceptance — 2026-09-08

The fresh instrumented left image was built with:

```sh
qmk compile -c -kb bastardkb/charybdis/4x6 -km noah \
  -e NOAH_PHYSICAL_HALF=left -e FORCE_SLAVE=yes -e NOAH_STACK_BUDGET_ENABLE=yes
```

Both `tools/firmware_stack_budget.json` and
`tools/firmware_stack_budget_live_profile_owner.json` pass against that ELF and
map using `python3 tools/check_firmware_stack_budget.py`. The new portable
readback path is 1,104 bytes; settings publication through the native EEPROM
write is 1,120 bytes. The largest reviewed main-process path is 1,824/1,920
bytes: 96 bytes of policy margin, separately 736 bytes to the physical
2,560-byte process-stack boundary. The largest reviewed split-slave path is
328/768 bytes, within the separate 1,024-byte usable thread stack.

The stack-only CRC boundary disables inlining, cloning and identical-function
merging so both manifests measure the named codec path consistently. It has no
effect in normal firmware. Instrumentation slightly changes static accounting:
`.data` 4,148 B, `.bss` 51,556 B, sum 55,704 B; boot SRAM0–3 core-memory span
206,432 B. The same memory policy passes. These checks cover named linked
paths, not a global/interrupt maximum or measured runtime high-water.

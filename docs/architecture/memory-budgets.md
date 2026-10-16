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

## Eight-PD-slot resource policy — 2026-09-20

Schema-2 owner builds deliberately expand logical EEPROM from 16 to 18 KiB.
The wear-level cache grows by 2,048 bytes per half; the mode cache adds 776
bytes plus readiness state, and button-release ownership adds bounded matrix
state. The old cold export workspace remains separate from effective settings:
sharing them would invalidate a multi-packet export when a profile publishes.
The owner drops a duplicate boot-only profile view, using the not-yet-initialized
provider's pending view before provider initialization copies the independent
compiled snapshot. Its linked state is now 4,016 bytes, below the unchanged
4,096-byte owner policy. Validator and provider policies stay 360/784 bytes.

The fresh right-half schema-2 ELF (`bastardkb_charybdis_4x6_noah_pd8_right`)
links 4,152 bytes of `.data` and 54,556 bytes of `.bss`: 58,708 bytes combined.
This fails the previous 57,344-byte policy by 1,364 bytes. We explicitly budget
3,072 additional bytes for this feature, making its regression tripwire 60,416
bytes. The measurement leaves 1,708 bytes of policy margin. The legacy geometry
keeps its original tripwire; the gate selects the increment only when the ELF's
recorded compiler flags enable `NOAH_PD_PROFILE_ENABLE`.

The SRAM0–3 fixed prefix is 58,712 bytes and its boot linker/core-memory span is
203,432 bytes. Fixed linked occupancy across the unique SRAM banks is 66,168
bytes per half. These are linked measurements, not runtime-free-memory or
high-water measurements. This budget change accounts for the agreed storage
and cache representation; it does not prove hardware cadence or runtime stack
safety. Measure allocator/stack high-water and the physical interruption matrix
on the pair before treating this as a release-qualified firmware upgrade.

Final pair verification (2026-09-21): normal right and left images both measure
58,708 bytes of `.data + .bss` and a 203,432-byte boot core-memory span per half.
The instrumented left image measures 58,696 and 203,440 bytes respectively; its
largest reviewed main-process path is 1,264/1,920 bytes and split-slave path is
280/768 bytes. PD cache initialization, EEPROM read and validation paths are
explicitly covered in the owner stack manifest. All corresponding memory and
reviewed-path gates pass under the documented feature-specific policy; runtime
high-water and hardware acceptance are still pending.

## Retired user macros and streamed settings readback — 2026-09-23

Removing the user-macro runtime frees its 16-byte slot metadata. The settings
readback (GET `0x07`) no longer keeps its own 1,368-byte cold copy; it streams
from the effective settings cache instead. That reverses the separation above.
The copy never kept a backup intact across a publication: the reader re-reads
page 0 after the chunks and requires the same length and digests, so a
publication or a live RGB/DPI change during the read fails it either way. The
effective cache itself stays 1,368 bytes: version-3 macro names fill the space
the user macros had, and the readback returns them from it.

Fresh right and left schema-2 pair ELFs each link 4,152 bytes of `.data` and
53,164 bytes of `.bss`: 57,316 bytes, 1,400 below the 58,716 measured before
this change and 3,100 bytes below the 60,416-byte tripwire. The SRAM0–3 fixed
prefix is 57,320 bytes and the boot linker/core-memory span 204,824 bytes per
half; fixed linked occupancy across the unique banks is 64,776 bytes. These are
linked measurements, not runtime high-water. The reviewed stack paths pass; the
largest main-process path is still 1,792 bytes and the settings readback path
416 bytes.

Settings version 4 then guarantees every macro name 20 ASCII characters, which
grows the effective settings cache from 1,368 to 1,656 bytes. Fresh right and
left pair ELFs each link 4,152 bytes of `.data` and 53,452 bytes of `.bss`:
57,604 bytes, exactly 288 more, and 2,812 below the 60,416-byte tripwire. The
boot core-memory span is 204,536 bytes per half and fixed linked occupancy
65,064 bytes. The reviewed stack paths still pass at the same figures.

## Custom-key names — 2026-09-28

Settings version 5 names the 64 custom keys, which grows the effective
settings cache from 1,656 to 3,000 bytes (312 + 128 × 21). A fresh left pair ELF
(`NOAH_PHYSICAL_HALF=left`, `FORCE_SLAVE=yes`) links 4,188 bytes of `.data` and
54,756 bytes of `.bss`: 58,944 bytes of `.data + .bss`, 1,472 below the
60,416-byte tripwire. The SRAM0–3 fixed prefix is 58,952 bytes and the boot
linker/core-memory span 203,192 bytes. These are linked measurements, not
runtime high-water. The custom-key and macro name tables are `const` and stay
in flash. The next static buffer of this size would cross the tripwire, not the
RP2040's SRAM: it needs fresh accounting and a policy decision, not a
workaround.

The reviewed stack paths pass. The pointing paths follow the one directional
engine, which LTO inlines into `pointing_device_task`: a fallback settlement
at 1,248 bytes, a literal tap at 456 and a QMK-function tap at 520; the retired
per-preset handlers are no longer listed. The largest main-process path is
still 1,800 bytes.

## Split frame CRC — 2026-09-28

The frame CRC (`SPLIT_TRANSPORT_CRC`, default on) adds one static staging
buffer per half in the fork's serial protocol, sized to the largest frame plus
the id and CRC bytes (34 bytes, `SPLIT_TRANSPORT_CRC_MAX_FRAME` = 32), and the
master's 22 per-id resend flags. A fresh left pair ELF
(`NOAH_PHYSICAL_HALF=left`, `FORCE_SLAVE=yes`) links 4,188 bytes of `.data` and
54,860 bytes of `.bss`: 59,048 bytes of `.data + .bss`, 1,368 below the
60,416-byte tripwire; the same pair built with `NOAH_SPLIT_CRC=no` links
58,984. These are linked
measurements, not runtime high-water. The frame code inlines into the split
slave thread's 40-byte root frame, and the reviewed stack paths of both gates
pass.

## Per-combo origin candidates

Combo-origin completion candidates are keyed by combo index up to the 32-combo
Profile Wire bound instead of sharing `COMBO_BUFFER_LENGTH` slots, so a chord
burst cannot fall back to delivery-time timing. The linked table is 768 B of
SRAM0–3 `.bss` per half (was 96 B, +672 B). The default `noah` build then
links 52,856 B of `.data + .bss`, 7,560 B below the 60,416 B tripwire. This is a
linked measurement, not runtime high-water evidence.

## Release compiler (GCC 14.2.1) — 2026-10-01

The flashable pair is now always built in the release build image
(`tools/build-image`, `arm-none-eabi-gcc` 14.2.1), not with the host's
compiler. Releases were already built there; local pairs used Homebrew GCC
8.5.0. Same source (userspace `556c1e08`, BK `0474783b`), `SKIP_VERSION=yes`,
`sh tests/host/run_firmware_memory_budget_checks.sh`, per half:

| Measurement | GCC 14.2.1 (image) | GCC 8.5.0 (host) |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,172 | — |
| SRAM0–3 `.bss` | 55,904 | — |
| `.data + .bss` | 60,076 (340 below the 60,416 tripwire) | 59,800 (616 below) |
| SRAM0–3 fixed linked prefix | 60,088 | 59,808 |
| SRAM0–3 linker/core-memory span at boot | 202,056 | 202,336 |
| Fixed linked occupancy across unique SRAM banks | 67,544 | 67,264 |

Both halves link the same SRAM figures under GCC 14 (left `FORCE_SLAVE`, right
`FORCE_MASTER`). GCC 14 links 276 more static bytes; the tripwire margin is now
340 bytes, so the next static growth must justify itself against it. These are
linked measurements, not runtime high-water.

## Thirty-two pointing slots — 2026-10-06

D-F09 grows the effective PD cache from eight to 32 materialized slots (3,072
bytes; the sparse domain is expanded once, at publication, so pointing never
reads storage) and widens every pd-mode mask from 16 to 32 bits: the
key-runtime press tokens, persistent intents and projections, the pd-mode
owner and state tables, and the owner's compatibility limits. Fresh pairs from
the release image (`arm-none-eabi-gcc` 14.2.1, BK `b8f5e32c`,
`SKIP_VERSION=yes`, `sh tests/host/run_firmware_memory_budget_checks.sh` on the
left image), per half:

| Measurement | Before (`dev` `4f4b92bd`) | 32 slots |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,172 | 4,172 |
| SRAM0–3 `.bss` | 55,912 | 59,560 |
| `.data + .bss` | 60,084 | 63,732 |
| SRAM0–3 fixed linked prefix | 60,096 | 63,744 |
| SRAM0–3 linker/core-memory span at boot | 202,048 | 198,400 |
| Fixed linked occupancy across unique SRAM banks | 67,552 | 71,200 |

By symbol, the PD cache grows by 2,296 bytes, the runtime singleton (key
runtime, pd-mode state and owner tables) by 1,272, the profile owner by 24, and
the compiled serializer's canonical behaviour order (64 rows, worked out once)
by 68. The 63,732 bytes fail the 60,416-byte tripwire by 3,316. We explicitly budget
4,096 additional bytes for this feature, making the schema-2 tripwire 64,512
bytes and leaving 780 bytes of policy margin; the gate still selects it only
from the ELF's recorded `NOAH_PD_PROFILE_ENABLE` flag. As with the eight-slot
increment, this is a regression policy, not a statement of capacity: the
boot span still leaves 198,400 bytes of SRAM0–3 to the core allocator, and an
ELF analysis on 2026-10-06 found that the only runtime allocation from it is
newlib `rand()`'s one-time state, so SRAM0–3 beyond the boot prefix is free at
runtime. These are linked measurements, not runtime high-water.

The live-owner stack gate's PD cache paths grow by 64–80 bytes (compiled cache
initialization 816 bytes, published EEPROM read 1,128, record validation 1,120,
all passing); the boot publication now reaches the cache through the provider
listener table, a declared indirect edge. In the release image both reviewed
stack gates report the same failures before and after this change (stale
adjacencies for paths GCC 14 inlines, such as `directional_step`), so they
are not new. Under host GCC 8.5.0, which no longer builds the flashed pair
(D-F05), the ordinary gate's worst main-process path reaches its 1,920-byte
budget (1,800 before), and two press-observation paths gain adjacency
failures where that compiler now inlines `handled_key_materialize_hold` and
`key_runtime_core_allocate_token_id`; both manifests need a GCC 14 review.

## Official QMK image (GCC 15.2.0) — 2026-10-07

The pair is now built in QMK's official `qmk_cli` image, copied unchanged into
`ghcr.io/noahclr/charybdis-build` (D-F05): QMK's `arm-none-eabi-gcc` 15.2.0
replaces Debian's 14.2.1. Same source (userspace `d13c0456`, BK `b8f5e32c`),
`SKIP_VERSION=yes`, `sh tests/host/run_firmware_memory_budget_checks.sh`, per
half; both halves link the same SRAM figures:

| Measurement | GCC 14.2.1 | GCC 15.2.0 |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,172 | 4,168 |
| SRAM0–3 `.bss` | 59,560 | 59,560 |
| `.data + .bss` | 63,732 (780 below the 64,512 tripwire) | 63,728 (784 below) |
| SRAM0–3 fixed linked prefix | 63,744 | 63,736 |
| SRAM0–3 linker/core-memory span at boot | 198,400 | 198,408 |
| SRAM4 fixed linked prefix | 3,872 | 3,872 |

Static RAM is effectively unchanged. Flash image sections (`.vectors`,
`.text`, `.rodata`, `.data`) shrink from 218,324 to 213,852 bytes on the right
and from 218,340 to 213,860 on the left, about 4.5 KB per half, across code
generation broadly. Separately, the official toolchain links byte-at-a-time
`memcpy` and `memset` (18 and 16 bytes) in place of newlib's word-wise ones (140
and 166 bytes), so larger aligned copies may be slower. These are linked
measurements, not runtime high-water or timing.
The reviewed stack manifests report the same unresolved findings under both
compilers, so this change neither adds nor resolves a stack-coverage gap.

Those figures came from the official image's `linux/arm64` variant. Pairs now
link the `linux/amd64` variant's newlib on every platform (D-F05), which
differs slightly. At userspace `bdd2b5a3` and BK `e8e2a57f` (QMK 0.34.6), the
left half links 4,164 B of `.data` and 59,560 B of `.bss` (63,724 B, 788 below
the tripwire), a 63,736 B fixed linked prefix, a 198,408 B core-memory span at
boot and 213,880 B of flash image sections.

## Complete compiled defaults representation

D-F12 retains a 40-byte compiled-profile handle: metadata and five domain
offset/length pairs. Factory RGB adds one 551-byte encoded cache, a 40-byte
reader view and readiness state in SRAM0–3. It shares the stored RGB decoder;
ordinary frames do no serialization. Compiled combos and settings warm the
existing effective caches instead of allocating another profile buffer.

The owner keeps a borrowed compiled snapshot for absent-domain fallback. The
validator copies its boot declaration at begin, so the owner does not retain a
second declaration. Its 4,096-byte state policy and the 64,512-byte schema-2
static-data policy remain unchanged. Neither is a physical SRAM limit.

The live-owner stack manifest covers initial serialization, cold factory RGB
writing and validation, compiled combo/settings cache publication, and the
injected native settings apply callback. These are reviewed linked paths only.
At that checkpoint the ordinary manifest's compiler-inlining gaps remained
open; D-F14's resource acceptance below re-reviews both manifests against
the pinned compiler. Neither establishes runtime stack high-water.

## Bigger-profile storage — D-F14

Logical EEPROM grows from 18 KiB to 140 KiB per half (a 12 KiB VIA region and
two 64 KiB custom-profile slots). Pinned QMK's wear-leveling driver mirrors the
whole logical EEPROM in SRAM0–3, so the cache grows by 124,928 bytes. The
schema-3 static-data tripwire therefore becomes 64,512 + 124,928 = 189,440
bytes; it is the old policy plus this one reviewed increment, not a statement
of capacity, and later D-F14 slices add their own measured increments.

A fresh left pair ELF with only the storage change linked 4,168 bytes of
`.data` and 185,144 bytes of `.bss`: 189,312 bytes of `.data + .bss`, 128
below the tripwire, and a 189,320-byte SRAM0–3 fixed prefix. The boot
linker/core-memory span is 72,824 bytes; the plan projected 189,288 and
72,848. The span is still the allocator's boot range, not runtime-free memory:
allocator and stack high-water on both halves remain hardware acceptance
evidence. Flash backing grows to 280 KiB at the top of flash, separate from
the image; a wear-leveling consolidation now rewrites 140 KiB, a pause the
hardware acceptance times.

The capacity slices (sixteen layers, 128 behaviour rows with a per-bank row
index, 128 combos of up to sixteen inputs, 128 VIA macros and custom keys, 272
counted names, 128-byte pointing records) then linked, in a fresh left ELF,
206,712 bytes of `.data + .bss` (4,168 `.data`): 17,400 bytes more. The largest
contributors are the owner's native combo table (the owner, `runtime_owner`,
links at 10,552 bytes against a new 11,264-byte engineering policy, up from
4,096), the settings cache at its 9,380-byte maximum (`settings`) and the
pointing cache's 4,096 bytes of 128-byte records (`records`). The static-data
tripwire gains one reviewed 20,480-byte D-F14 capacity increment to 209,920
bytes, 3,208 above this measurement. The SRAM0–3 boot linker/core-memory span
is 55,424 bytes and the true static headroom (SRAM0–3 minus the linked prefix
and the 4,096-byte arena floor) 51,328 bytes; neither is runtime-free memory.
That capacity-only left measurement predates participation. The current
D-F14 instrumented resource pair uses the pinned GCC 15.2.0 image, BK
`cb6719b55e143b76e6611418b90245106062bb23`, and records
each half before QMK's shared output is replaced. Right and left each link
4,164 bytes of `.data` and 202,640 bytes of `.bss` in SRAM0–3: 206,804 combined,
with 3,116 bytes of margin to the unchanged 209,920-byte regression policy.
The fixed SRAM0–3 prefix is 206,816 bytes, and the linker/core-memory span at
boot is 55,328 bytes. Fixed linked occupancy across unique SRAM banks is
214,272 bytes per half. These are instrumented linked figures, not runtime
free-memory or high-water measurements.

`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh` (with explicit
`QMK_ROOT` and `BUILD_ROOT` from this worktree) enables stack instrumentation
and checks static accounting and both reviewed-path manifests on each half,
preserving the ELF, map, compiler/linker flags and reports beside the pair.
The ordinary manifest retains every functional path while naming GCC 15's
linked callers for inlined helpers. It also covers participation/indexed row
reads, generated-output permission and origin reconstruction. The owner
manifest covers indexed publication and sixteen-member combo validation reads.
A failed gate stops the pair; build the ordinary pair separately for use.

Both halves pass the unchanged 1,920-byte main-process and 768-byte split-slave
reviewed-path policies. The ordinary manifest's largest named main-process
path is 1,912 bytes: 8 bytes of policy margin, separately 648 bytes to the
physical 2,560-byte process-stack boundary. Its largest split-slave path is
328 bytes. The owner manifest reaches 1,608 bytes in the main process and
344 bytes in the split-slave thread. Origin reconstruction remains a separate
compiled call so its workspace retires before recursive action projection;
inlining it had kept that workspace in the process frame and crossed the
main-process policy by 40 bytes. Neither a PASS nor these margins establish
a global or interrupt-stack maximum. Allocator/stack high-water and timing
on both physical halves remain hardware acceptance evidence.


### Stored macro expansion within the same EEPROM allocation

The macro expansion repartitions the existing 140 KiB logical EEPROM per half:
36 KiB belongs to VIA (34,903 macro bytes), followed by two 52 KiB custom-profile
slots. The SRAM0–3 EEPROM mirror remains 143,360 bytes, and flash backing
remains 286,720 bytes. Each 53,216-byte payload still holds the 37,700-byte
maximum profile with 15,516 bytes spare, retaining the 12 KiB future reserve.

Stored playback reuses the 516-byte linked IR window. Added provider state is
one 20-byte cursor, one 16-byte provider, a one-byte slot and a six-byte VIA
lookup cache; the engine adds two 32-bit callback/context pointers. The memory
gate includes the new provider symbols in named macro storage: 692 bytes on
each instrumented half. Authored C defaults continue to compile in a fixed
window; their stack workspace is unchanged rather than enlarged for long
stored macros.

The fresh instrumented pair uses pinned GCC 15.2.0 and BK
`173eae2b5e1b21c15a84ea90eb8fe84b434c63c5`. Right and left each link 4,168
bytes of `.data` and 202,920 bytes of `.bss` in SRAM0–3: 207,088 bytes combined,
2,832 below the unchanged 209,920-byte regression policy. The fixed SRAM0–3
prefix is 207,096 bytes and its boot linker/core-memory span 55,048 bytes.
Fixed occupancy across unique SRAM banks is 214,552 bytes per half. The
50,952-byte static headroom subtracts the linked prefix and 4,096-byte arena
floor; none of these figures measures runtime free memory or high-water.

The ordinary manifest retains its former functional paths with the compiler's
current linked callers and adds stored-window decoding, storage reads, refill
callbacks and persistent-hold output. Both halves pass: the largest reviewed
main-process path is 1,920 bytes, exactly the unchanged policy (640 bytes below
the physical 2,560-byte stack boundary); the largest split-slave path is 328
bytes. The owner manifest reaches 1,632 bytes in the main process and 344 bytes
in the split-slave thread. Macro startup decoding reaches 1,460 bytes and
scan-time refill 556 bytes. These are reviewed-path bounds only; interrupt and
global stack maxima, full-bank startup timing, cancellation, restore and
wear-level consolidation pauses remain acceptance on the physical keyboard.

## Lighting rules — D-F26

RGB domain version 5 (`lighting-rules-v1.md`) replaces the stage renderer with
one rule engine. Each half decodes the published lighting domain into RAM once
per publication and renders only from that program, so the format's 8 KiB
budget is resident. The instrumented pair at BK
`173eae2b5e1b21c15a84ea90eb8fe84b434c63c5` (pinned GCC 15.2.0) links, on each
half, 4,148 bytes of `.data` and 215,968 bytes of `.bss` in SRAM0–3: 220,116
bytes, 13,028 more than the 207,088 the same pin linked before. The
contributors:

| State | Bytes |
| --- | ---: |
| decoded program (`program`: 8,192 payload bytes, rule and map indexes) | 9,252 |
| compiled Standard lighting cache (`v5_bytes`) | 1,664 |
| engine working set (`subjects` 1,024, `gates` 384, `palette` 384, the frame 184, two fact frames 184) | 2,160 |
| version-4 conversion emitter (`emitter`) | 404 |
| split lighting-facts frame (built, sent, at-rest and staging copies) | 312 |

The removed stage snapshots, effective-frame tokens and per-stage split packets
account for the difference. The `.data + .bss` tripwire gains one reviewed
16,384-byte D-F26 increment, the plan's budget, to 226,304 bytes: 6,188 above
this measurement. The fixed SRAM0–3 prefix is 220,128 bytes, its boot
linker/core-memory span 42,016 bytes and fixed occupancy across unique SRAM
banks 227,584 bytes per half; the true static headroom (SRAM0–3 minus the
linked prefix and the 4,096-byte arena floor) is 37,920 bytes. None of these
measures runtime free memory or high-water.

The compiled lighting is built once, by `noah_profile_rgb_compiled_v1_prepare`
from the compiled profile's open and the program's load, both at boot depth.
The streamed compiled RGB domain only copies that cache, so a peer or
candidate transfer reading the compiled profile never runs the conversion
inside its reader (that nesting measured 2,208 bytes, over the main-process
policy). The ordinary manifest adds the lighting-facts build, fact collection
and the rule walk through facts, layer-key targets, paints over the effect
and palette colours: at most 984 bytes. The split slave's lighting-facts RPC
takes 80 bytes, replacing the three retired per-stage RPCs. The owner manifest
replaces the compiled-RGB materialization paths with the compiled lighting's
encoding, conversion, validation and program load from each entry point: the
largest is the published fallback's conversion at 1,544 bytes, and the
streamed peer-reuse read 1,696. Both halves pass with the unchanged 1,920-byte
main-process and 768-byte split-slave policies: the ordinary manifest's
largest main-process path is still 1,920 bytes and its largest split-slave
path 328; the owner manifest's largest are 1,912 and 624. The pointing path
through the synthetic QMK-function tap left the manifest: since #86 routes
pointing taps through the owned literal emitter, that edge no longer links.
These are reviewed-path bounds only; the physical high-water of both halves
remains hardware acceptance.

### Painting identities and outcomes

Painting adds identified combo and behaviour fact slices while preserving the
existing 78-byte split-facts prefix. The new slices are 40 bytes for four held
combo records and 180 bytes for sixty per-key behaviour records: 298 bytes in
all, sent through the existing atomic chunk protocol. Row and outcome metadata
reuse padding in interaction, effect and deferred-release records; the reducer's
active/queued pulse metadata adds eight bytes. Identity tables and maps reuse
the version-5 program and its existing 8 KiB payload budget.

A fresh `NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh` pair with
pinned GCC 15.2.0 and BK `d280a4471e414533f198f02e3faf33b7368ecea3` links the
following on **each half**:

| Measurement | Bytes |
| --- | ---: |
| SRAM0–3 `.data` | 4,368 |
| SRAM0–3 `.bss` | 217,752 |
| `.data + .bss` | 222,120 |
| SRAM0–3 fixed linked prefix | 222,128 |
| SRAM0–3 linker/core-memory span at boot | 40,016 |
| Fixed linked occupancy across unique SRAM banks | 229,584 |

The unchanged 226,304-byte static-data regression tripwire has 4,184 bytes
of margin; the boot allocator span passes its separate 4,096-byte floor.
Neither is measured runtime free memory. Each RP2040 still has 270,336 bytes
of physical SRAM across its banks.

The USB-name boot reader and rename safety snapshot keep their temporary
workspaces in separate compiled calls, so neither remains in `main` beneath
ordinary key processing. A separate tap-feedback projection helper similarly
retires its metadata workspace outside nested fallback settlement; the effect
projection boundary remains compiled to avoid retaining both workspaces in
its caller. Both stack manifests follow the actual linked boot-selection and
owner-status calls and explicitly cover these extracted paths. On each half,
the ordinary manifest's largest main-process path is 1,912/1,920 bytes and its
largest split-slave path is 328/768; the owner manifest reaches 1,912/1,920 and
624/768 respectively. Main-process policy margin is eight bytes, separately
648 bytes to the physical 2,560-byte process-stack boundary. These unchanged
policies cover reviewed linked paths only; global/interrupt maxima, allocator
and stack high-water, split cadence and outcome rendering remain acceptance
on both physical halves.

### Cached key queries and direct action paint

The per-layer native/behaviour basis for keys-that and what-it-does has an
additional **3,072-byte static regression-policy increment**, approved by the
orchestrator under Noah's delegated engineering decision authority. The relevant
D-F26 portable RGB profile artifact threshold is **229,376 bytes**, previously
226,304. The artifact selector reads recorded compiler flags: PD profile,
portable profile and RGB Matrix enabled, with at least one of the keys-that or
what-it-does modules enabled, and the cache-refresh implementation actually
linked in the ELF. Both consumers disabled retains226,304; unrelated
artifact policies retain their previous thresholds. This budgets this query
basis only, without promising space for event, linger or flash histories.

The representation is bounded by compiled geometry, independent of accepted
rule/query count: nativeu16 plus authored behaviour-rowu8 per key/layer, one
any-combo bitmap per layer, and one matrix-key index per LED. At16layers,
60matrix positions and58LEDs these linked arrays are1920+960+128+58 bytes,
plus generation4 and readiness1. They replace the old mapped128/generation4/
valid-layers4 cache. Existing resident combo rows are reused; there is no heap,
per-query mask bank, eviction, admission limit or format/budget redefinition.

Historical **right-only** GCC15.2.0 link at firmwaree352687b, BKd280a447, before
the amendment: data5280+bss223784=229064 B, a2928 B net linked increase over the
repaired pre-query checkpoint, including alignment/optimization effects. It
failed the old policy by2760 B; at the approved threshold it would have312 B
policy margin. SRAM0–3 fixed prefix229072 B, boot linker/core-memory span33072 B,
fixed occupancy across unique banks236528 B. The left was not built because
the tool stopped at the right gate. These are allocation evidence for the
amendment, **not a two-half verification pass or a pair to try**.

Physical270336 B per half, boot core-memory floor4096 B and reviewed-path stack
limits1920/768 B stay unchanged. Boot span is not runtime-free or high-water
memory. The cache refresh and query/paint paths must appear in both tracked
stack manifests and pass fresh linked checks on each half. Declared query
operation bounds and host tests do not prove physical cadence, global or
interrupt-stack maximum, allocator high-water or hardware acceptance. Those
remain explicit physical acceptance obligations before release.

## Linked RTOS context-save accounting

The reviewed timer-restoration paths include ChibiOS's ARMv6-M `__port_switch`.
At the pinned linked instruction sequence, it pushes r4–r7/lr (20 bytes), moves
r8–r11 into r4–r7 and pushes those (16 bytes), then stores that outgoing stack
pointer before loading another thread's saved stack. The outgoing reviewed
caller is therefore charged36 bytes. The other thread's restored continuation
belongs to that thread's stack; this is no global/thread/interrupt maximum proof.

The analyzer recognizes only the exact named16-instruction linked save/store/
load/restore sequence, including the context offsets and transfer registers.
A different sequence or arbitrary stack-pointer replacement remains unbounded
and fails the gate. This adds actual context-save coverage without a manifest
frame override, an excluded call edge or a changed stack policy.

## Admitted retained lighting candidate

The unreleased phase3 candidate reserves8220 bytes per half for new flash/linger
history. This is one fixed pool, not a raised static policy or a heap-backed
reservation. Registry bounds charge two complete profile-sized banks and one
replacement slot, Q=2*S+W. Current plus immutable outgoing/slave staging coexist;
replacement workspace is separately charged. No third frozen history bank is
used: render consumes the current scene synchronously and publishes owned LED
pixels. Source capture facts and replacement workspace coexist. The replacement
split protocol additionally owns one complete role-disjoint fact snapshot outside
the pool; its bytes must appear in linked accounting.

A separate decoded candidate program keeps reads/admission before active program
replacement. Provider publication preflights it at the existing safe boundary;
commit copies owned RAM without later source I/O. The working-memory capability
reports the reproducible conservative model and an over-budget candidate rejects
before activation. This deliberately amends payload-only acceptance for new
timing; unchanged Standard and state/show-after profiles retain their acceptance.
The lighting spec governs the exact descriptor/member layout and module bounds.

The scoped retained-lighting artifact has a247808-byte static tripwire,
18432 above its preceding229376 query-only policy. Selection requires the
recorded GCC15.2.0 configuration,60 matrix keys, four combo records, portable
RGB/PD ownership, the linked query cache and enabled retained consumer. The
memory gate proves compiler/geometry from its recorded flags and forced
headers; capture flags or the target filename alone do not select this policy.
All other artifact and geometry policies retain their previous thresholds.

The initial enabled links on BOTH halves measured data5292+bss242176=247468,
fixed SRAM0–3 prefix247480 and boot core-memory span14664. The18396-byte net
increase consists of8220 pool plus10176 fixed/control bytes,472 more than the
previous9704 fixed projection. Named fixed additions are candidate program9264,
coherent fact snapshot621, snapshot/session168, working control24, identities48,
flags4, revisions8, scene pointer4 and RPC resend entry1;34 bytes are alignment/
unnamed residual. The1024-byte capture/render union replaces the same-size
placement array, without a saving credit or another frame bank.

At that measured size the new policy margin is340 bytes and boot span exceeds
the4096 floor by10568. Neither is runtime-free memory or a high-water result.
This funds the implemented8220-pool/staged-program/coherent-fact representation
only; source corrections must be freshly linked against this fixed ceiling,
which does not float. Those initial links failed reviewed main-stack policy
and are not accepted pairs. Physical SRAM remains270336 bytes per half; core
floor4096 and reviewed main/slave1920/768 remain unchanged. Hardware cadence,
oscillator drift and allocator/global/interrupt-stack high-water acceptance
remain unproved. Host logical-clock and linked-path checks do not prove them.

The v4 snapshot correction retains the same8220 pool and staged program.
Additional control comprises a working-owner change latch/diagnostic revision,
four admission-derived slot/order dirty bitsets (76B for60keys/8220),
a registry-sized fact dirty bitset, one acknowledged baseline token and retry
backoff timestamps and a one-byte published readiness scalar inside the bounded
role-disjoint endpoint control (the base heartbeat response uses existing QMK
RPC allocation). It reuses the existing source fact snapshot and receiver
published/staging frames; no third621-byte fact buffer or history bank exists.
The compiler-flag policy selector follows final ordered `-D`/`-U` definitions:
valueless or proven nonzero integer enables a prerequisite; zero, undefined or
unknown expressions cannot grant a larger artifact allowance. The separate
compiler/geometry assertion remains required for247808. New exact-head links
must account for this control and actual stack paths under the fixed policies;
the historical initial sizes above are not a pass for later corrections.


The persistent-native-effect repair owns one local raw WS2812 pixel cache,
sized to the larger physical half:87B at29/29 RGB, plus one phase byte and one
basic/advanced suppression flag. It is fixed module state outside the8220
history pool, with no sharing assumption or heap allocation. Compiler alignment
and changed caller frames require fresh linked accounting; these source sizes
are not an approved static-policy increase or a runtime high-water result.
The scoped247808/core4096/main1920/slave768 policies remain unchanged. Exact
raw/physical ownership and supported target guards live in the lighting spec.
The pinned enabled candidate with this repair links5292B `.data` plus242360B
`.bss` per half (247652B static,156B policy margin), with14480B boot
core-memory span. The net92B static change includes compiler layout, not just
the89B declared cache/controls. These are linked figures for the recorded
configuration, not runtime availability or hardware high-water evidence.

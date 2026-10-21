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

The dated sections below record each change's linked measurements in order;
the newest is last.

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

## Portable-Profile Checkpoint — 2026-09-08

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

## Atomic-Apply Checkpoint — 2026-09-15

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

Measure the instrumented pair, built as the firmware anyone flashes. From a
task worktree, run the vault's `verify` with `NOAH_RESOURCE_CHECKS=yes`: it
sets `QMK_ROOT` and `BUILD_ROOT`, serializes builds, and runs

```sh
NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh
```

That build runs `tests/host/run_firmware_memory_budget_checks.sh` and both
reviewed-path manifests on each half and keeps the ELF, map, flags and reports
beside the pair. Every gate runs on both halves, whatever an earlier one
found; the build fails afterwards, naming each failed gate's report, if any
did. A PASS covers only the paths in `tools/firmware_stack_budget.json`
and `tools/firmware_stack_budget_live_profile_owner.json`. The standalone
`tests/host/run_firmware_stack_budget_checks.sh` compiles a plain `qmk compile`
image without the live-profile owner (D-L08), not the flashed pair.

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

D-F12 retains a compiled-profile handle of metadata and five domain
offset/length pairs: 40 bytes, 60 since D-F28 made the pairs 32-bit. Factory
RGB adds one 551-byte encoded cache, a 40-byte reader view and readiness state
in SRAM0–3. It shares the stored RGB decoder;
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
remains 286,720 bytes. Each 53,216-byte payload still holds the maximum
profile, 37,956 bytes since custom-key shortcuts (D-F26), with 15,260 bytes
spare, retaining the 12 KiB future reserve.

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

## Custom-key shortcuts — D-F26, 2026-10-17

Settings v8 adds 128 two-byte shortcut records, so the effective settings
cache (`settings`) grows by 256 bytes to its 9,669-byte maximum, on each half.
Fresh resource builds of the pinned image (`NOAH_RESOURCE_CHECKS=yes`
`tools/build-firmware-pair.sh`, GCC 15.2.0, BK `d280a4471e`), against `dev` at
`86ef5553` built the same way:

| Half (build) | SRAM0–3 `.data + .bss` | `dev` | Below the 209,920-byte tripwire |
| --- | --- | --- | --- |
| Right (`FORCE_MASTER`) | 208,336 bytes | 208,080 | 1,584 bytes |
| Left (`FORCE_SLAVE`) | 208,336 bytes | 208,080 | 1,584 bytes |

These are link-time values, not runtime high-water. The keymap's shortcut
table is `const` and stays in flash. Profile storage grows by the same 256
bytes; the maximum profile is 37,956 bytes with 15,260 spare in its slot.

The reviewed stack paths are link-time estimates from the same builds. Every
path in `tools/firmware_stack_budget.json` and
`tools/firmware_stack_budget_live_profile_owner.json` gives the same figure on
both halves as `dev`'s build: none grows. `process_record_user` is kept out of
line in every build (`users/noah/hooks.c`), since LTO began inlining it once
custom-key dispatch stopped replaying records, so the flashed firmware has the
call shape the paths measure. Three paths cover the shortcut itself, on both
halves: a held shortcut's press (1,288 bytes), a behaviour step tapping one
(1,408) and its release (1,360), all within the 1,920-byte main-process budget.

Both builds, `dev`'s included, failed the reviewed-path gate in the same way;
the next section gives the cause and the fix.

## Keyboard-name entry points out of line — 2026-10-17

The keyboard name (D-F25) added two entry points that only `main` reaches:
`noah_qmk_usb_product_name_pre_init`, from `keyboard_pre_init_user`, and
`noah_qmk_usb_product_name_task`, from housekeeping. LTO inlined both, with
the boot name read's store walk and the restart checks' owner and VIA
snapshots, so `main`'s own frame grew from 160 to 352 bytes. `main` is the
base of every main-process path, so six ordinary paths through fallback
settlement and seven owner peer-copy paths crossed the 1,920-byte policy
(worst 2,104 bytes, 456 from the physical 2,560-byte boundary).

Both entry points are now `noinline`
(`users/noah/lib/compat/qmk_usb_product_name.c`): their frames exist only
while they run, never beneath a key path. The owner manifest names three
paths from `main` through them (boot storage read 400 bytes, restart
readiness 680, USB restart 416), so inlining either again fails the gate as a
false adjacency. Anything `main` reaches only at boot or from housekeeping
with more than a few words of locals keeps a frame of its own.

The same builds refresh three adjacencies the linker now shapes differently.
Boot discovery reaches `io_read` through `noah_profile_store_boot_select_step`,
a function of its own since the boot name read shares it; the coherent VIA
status read reaches the authority status through `noah_profile_owner_status`;
and pointing-mode taps no longer replay a synthetic QMK record, so the stale
QMK-function tap path gives way to the pointing-mode macro start (D-F23):
812 bytes decoding a held sequence, 792 reading macro storage.

Fresh resource builds of the pinned image (`NOAH_RESOURCE_CHECKS=yes`
`tools/build-firmware-pair.sh`, GCC 15.2.0, BK `d280a4471e`) pass both
manifests on both halves. The worst main-process path is 1,912 bytes in each
manifest (ordinary: handled press projection with fallback settlement; owner:
peer large chunk candidate write), 8 bytes inside the policy and 648 bytes
from the physical boundary; the worst split-slave paths are 328 and 624 bytes
of 768. SRAM0–3 `.data + .bss` is 208,352 bytes per half, 1,568 below the
209,920-byte tripwire: 16 bytes of LTO partition padding, no new state. These
are link-time values for the named paths only, not runtime high-water or a
global or interrupt-stack maximum.

Later changes no longer pass the reviewed-path gate: the records below from
D-F29 on say which paths go over the policy (an
[open issue](../LIVE_EDIT_APP_DIRECTION.md#open-issues)). Until the gate
passes again, the figures above are the last ones it accepted.

## The new base: 32-bit profile lengths — D-F28, 2026-10-19

Every profile length and offset the firmware holds is now 32 bits, so the
structures that hold them grow. Pair build of the pinned image
(`tools/build-firmware-pair.sh`, GCC 15.2.0, BK `d280a4471e`), left half
(`FORCE_SLAVE`, the half built last), against the same build of
`chore/trial-review-fixes` at `fe4958a8` (`dev` with trial merges of #104 and
#105):

| | This change | `fe4958a8` |
| --- | ---: | ---: |
| SRAM0–3 `.data + .bss` | 208,668 bytes | 208,376 |
| Below the 209,920-byte tripwire | 1,252 bytes | 1,544 |
| Live-profile owner (`runtime_owner`) | 11,328 bytes | 11,068 |
| Compiled-profile handle | 60 bytes | 36 |

On Cortex-M0+ the owner's parts grow as follows: the candidate store backend
by 128 bytes (its sixteen remembered reuse ranges by 96, its validator by 20),
the split reconciler by 36 to its 896-byte allowance exactly, the store by
24, the effective-profile provider by 16, the peer store by 12 and the host
transaction by 8. The owner's engineering policy rises from 11,264 to 11,520
bytes and the validator's from 384 to 400; neither is a physical SRAM limit.
These are link-time values, not runtime high-water; the reviewed-path stack
gate was not run for this change (it already fails on `dev`, an
[open issue](../LIVE_EDIT_APP_DIRECTION.md#open-issues)).

Pulled chunks then moved into replies as large as pushed ones (D-F28), which
costs 184 bytes more per half in the same build: QMK's slave-to-master RPC
buffer rises from 32 to 128 bytes in the split shared memory (96), and the
reconciler's cached reply from 32 to 128 (96, so the owner links at 11,424
bytes, inside its 11,520 policy). The transport CRC's staging buffer already
held the 128-byte request and does not grow; LTO packing recovers 8 bytes.
SRAM0–3 `.data + .bss` is 208,852 bytes on the left half, 1,068 below the
unchanged 209,920-byte tripwire. The reconciler's own 32-bit policy rises from
896 to 1,024 bytes; it links at 992.

## Profile slots in their own flash region — D-F29, 2026-10-19

The two profile slots leave QMK's logical EEPROM for a flash region of their
own and are read in place, so the wear-leveling cache mirrors only the 36 KiB
VIA region: `WEAR_LEVELING_BACKING_SIZE` 73,728 bytes and `wear_leveling`
36,872 bytes per half, 106,496 fewer. Pair build of the pinned image
(`tools/build-firmware-pair.sh`, GCC 15.2.0, BK `d280a4471e`), against the
same build of the new base (`refactor/new-base-with-32-bit-profile-lengths`
at `86d7fd89`):

| | Right (`FORCE_MASTER`) | Left (`FORCE_SLAVE`) | New base, left |
| --- | ---: | ---: | ---: |
| SRAM0–3 `.data` | 4,360 bytes | 4,360 | — |
| SRAM0–3 `.bss` | 98,568 bytes | 98,568 | — |
| SRAM0–3 `.data + .bss` | 102,928 bytes | 102,928 | 208,852 |
| Static-data tripwire | 103,424 bytes | 103,424 | 209,920 |
| Below it | 496 bytes | 496 | 1,068 |
| Linker/core-memory span at boot | 159,208 bytes | 159,208 | — |
| Image end (`__flash_binary_end`) | 273,872 bytes | 273,840 | — |

The tripwire drops by exactly the 106,496 cache bytes, from 209,920 to
103,424 (`tests/host/run_firmware_memory_budget_checks.sh`); it stays a
regression policy, not a physical limit. The change itself costs 572 bytes:
the flash backend (`noah_qmk_profile_flash`, 336 bytes, its 256-byte page
buffer included), the other half's pulled pause figures, and the store's I/O
table, which gains reset and sync (the owner links at 11,440 bytes, inside its
11,520-byte policy). The boot span is the allocator's range, not runtime-free
memory; allocator and stack high-water on both halves remain keyboard
evidence.

After review the backend keeps an erase frontier, a write frontier, a failure
flag and a marker-step count: `noah_qmk_profile_flash` links at 352 bytes (16
more). The instrumented resource build of the right half
(`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh`, same compiler and
BK) links SRAM0–3 `.data` 4,356 and `.bss` 98,584 bytes, 102,940 together,
484 below the 103,424-byte tripwire.

Reviewed-path stack gates, on that right-half ELF, against the same build of
the new base at `86d7fd89`. Both manifests now name the flash backend: reads
(`flash_read`, `read_flash`, `rp2040_read`), writes and page programs
(`flash_write`, `write_flash`, `flush`, `rp2040_program_page`), syncs
(`io_sync`, `flash_sync`), the erase ahead of a save
(`noah_qmk_durable_io_matrix_scan` → `noah_qmk_durable_io_begin` →
`erase_next` → `rp2040_erase_sector`; the grant's start became its own
out-of-line step when the grant became one module) and
the boot-time forget (`main` → `rp2040_erase_sector`); the profile step is
reached through `noah_qmk_durable_io_profile_step`, its flash gate, into which
the compiler inlines the store runtime step. Paths stop at the driver
functions: the checker accepts no `bx ip` tail as evidence, so it cannot
follow the linker veneer into pico-sdk's RAM-resident `flash_range_erase` and
`flash_range_program`, whose frames add 40 bytes (plus the boot ROM's) with
interrupts off.

| Manifest | New base `86d7fd89` | This branch |
| --- | --- | --- |
| `firmware_stack_budget.json` | 6 key-runtime paths over the 1,920-byte budget (1,952–2,304) | the same 6, same figures; storage paths pass (boot discovery 672, behaviour lookups 1,072–1,336) |
| `firmware_stack_budget_live_profile_owner.json` | 2 over: peer large chunk write 1,984, repeated chunk read 1,952 | the same 2, now 2,024 (+40) and 2,016 (+64): the flash backend's write and read frames replace the EEPROM adapter's |

No path fails that passed before. The new flash paths pass: erase ahead in a
quiet grant 312 bytes, a write that erases first 1,016, the commit sync
1,080, the boot forget 168. The resource build stops at the right half's first
failing manifest, so the left half was not analysed.


## Profile keymap and macro domains — D-F30

The effective keymap/macro binding borrows one immutable reader and two domain
ranges instead of copying the matrix or bank. Key lookup reads two bytes from
the stored matrix, or directly reads authored const data for compiled fallback;
it never copies a provider snapshot or serializes compiled defaults on a key
path. Macro playback keeps the existing 516-byte IR window and decoder. Its
new slot metadata (128 bytes) and cached bank offset/length (6 bytes) coexist
with the VIA provider during this additive transition: the VIA provider is
still linked (its load, byte reader and invalidation symbols are in the right
ELF), so the named-macro subtotal includes both providers; the shared engine
window is counted once.

Writing the compiled defaults never compiles a macro: the authored macros are
generated already in the bank encoding (`authored_macro_bank.h`, about 330
bytes of flash), so the writer copies bytes. A save that reuses the macro
domain while the compiled defaults are active therefore no longer holds a
516-byte IR on the scan stack.

An instrumented right-half pair at firmware `ba24d84d`, based on the reviewed
D-F29 fixes at `1a8bdd5e`, uses GCC 15.2.0 in the pinned build image and BK
`d280a4471e`. SRAM0–3 `.data` is 4,352 bytes and `.bss` 98,792: 103,144
combined, 280 bytes below the unchanged 103,424-byte regression tripwire. The
named-macro subtotal is 826 bytes. These are linked figures, not runtime free
memory or high-water.

Ordinary manifest, macro paths: physical key lookup 432 bytes, compiled-default
macro parse 1,728, saved macro slot search 1,784, held-sequence decode 1,852 and
saved macro storage read 1,904. The decoder reads through the runtime's own
byte reader, which calls the published reader's callback for a range bounded
at publication, so the storage read is two frames shorter. Deferring the first
window to the scan refill was not taken: every window must pass preflight
before the first key is typed, on the key press. The slot search reads the bank
32 bytes at a time: host full-bank tests take 1,093 profile-bank reads to find
and play slot 127 (VIA's provider: 34,803) and 36,686 for a long macro after a
17,000-byte one (VIA: 69,624). Those are operation counts, not scan timings.

The resource build passes the static gate, then stops at the ordinary
manifest's six inherited key-runtime overruns (1,952–2,304 bytes), so it does
not build an instrumented left image. The owner manifest, run separately on
the preserved right ELF, fails only the two paths already over on D-F29: peer
large chunk write 1,968 and repeated chunk read 1,960 (D-F29 base 2,024 and
2,016). Everything else passes, including peer reuse mailbox admission and its
rejected reply (1,848 each, previously 1,936), large chunk mailbox decode
(1,840, previously 1,928), and the compiled keymap and macro reuse paths (peer
1,672, candidate 1,464; both writers are inlined into `write_domain`). The
review traced the earlier admission and decode overruns to two frames, not to
the domain metadata. `noah_qmk_durable_io_profile_step` had grown 24 bytes by
inlining `initialize_runtime_graph`, whose invalidator array had become the
provider's maximum of eight. That function is now boot-only and out of line,
with its array sized to the runtimes compiled in. `scan_running` copied the
whole split reconciler status to read two fields; it now reads only the
transfer offset and phase. No budgets or tests were weakened. The complete
stack gate and scan-cadence acceptance remain unresolved. Both halves' typing,
layers, combos and macros from flashed defaults, scan cadence and runtime
high-water remain keyboard acceptance.

## VIA removed: one store — D-F31, 2026-10-19

VIA leaves the build, and QMK's logical EEPROM holds only its eeconfig: the
wear-leveling backing returns to QMK's RP2040 default of 8,192 bytes, so its
RAM mirror (`wear_leveling`) is 4,104 bytes instead of 36,872. The VIA split
sync, mirror and staging state, its macro provider and its defaults seeder go
with it.

An instrumented right-half resource build (`NOAH_RESOURCE_CHECKS=yes`
`tools/build-firmware-pair.sh`, pinned GCC 15.2.0 image, BK `d280a4471e`) on
the keymap-and-macro domains' reviewed head `3d17afc0` plus this change links
4,332 bytes of `.data` and 65,440 of `.bss` in SRAM0–3: 69,772 combined,
33,372 fewer than D-F30's 103,144 (32,768 of them the mirror). The static-data
tripwire drops by exactly the mirror's 32,768 bytes, from 103,424 to 70,656
(`tests/host/run_firmware_memory_budget_checks.sh`), leaving 884 bytes. The
SRAM0–3 fixed prefix is 69,784 bytes and the boot linker/core-memory span
192,360; fixed linked occupancy across unique SRAM banks is 77,240 bytes.
Named macro storage is 692 bytes, with the profile provider's slot metadata
now the required symbol. These are linked figures, not runtime free memory
or high-water.

Stack (reviewed paths, same ELF): the ordinary manifest fails only on the six
handled-key paths D-F30 recorded (1,952–2,304 bytes against 1,920), so the
instrumented pair still stops before the left half. Its macro paths pass:
saved macro storage read 1,896, slot search 1,776, compiled-default macro
parse 1,720, physical key lookup 432. The owner manifest, run separately on
the preserved right ELF, now passes every path, including the two chunk paths
over budget since D-F29: peer large chunk write 1,896 and repeated chunk read
1,888 (D-F30: 1,968 and 1,960). Reuse mailbox admission and its rejected reply
are 1,776 (1,848), large chunk mailbox decode 1,752 (1,840), and compiled
keymap and macro reuse 1,632 on the peer and 1,432 on the candidate (1,672 and
1,464). The manifests drop every VIA path and name the Raw HID dispatcher
where they named VIA's custom-value hook. With a single stored caller left,
LTO inlined the macro window decoder, the macro compiler and the durable-I/O
scan, so all three are now `noinline` in every build
(`macro_payload_decode_qmk.c`, `macro_payload_parse.c`, `qmk_durable_io.c`),
keeping the measured call shape in the flashed firmware; inlined, the compiler
widened the provider's frame on the saved-macro storage read to 1,968 bytes.
The decoder's byte reads are a direct call. The complete stack gate remains
failing on the six inherited paths; no budget or test was weakened. Allocator
and stack high-water on both physical halves remain keyboard acceptance.

## Live edits — D-F32

Each half holds a working copy of its profile in RAM, one slot payload:
`runtime_working_bytes`, 130,048 bytes of `.bss` in SRAM0–3, the largest
static claim in the firmware. It is static so the boot span says exactly what
is left; the core allocator never holds it. The edit's state (its splice and
piece tables, its own validator, the saved snapshot Revert returns to, the
record of the last edit kept for the other half), the working-copy sync's
request, mailbox and reply, and Save's state live in the owner, which links at
13,860 bytes (11,368 before, inside the 16,384-byte policy that replaces
11,520). The owner's paused clock adds 12 bytes and the working RPC's
registration 8.

An instrumented right-half resource build (`NOAH_RESOURCE_CHECKS=yes`
`tools/build-firmware-pair.sh`, pinned GCC 15.2.0 image, BK `d280a4471e`),
against D-F31's measurement of the same base:

| | This change | D-F31 |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,340 bytes | 4,332 |
| SRAM0–3 `.bss` | 198,000 bytes | 65,440 |
| SRAM0–3 `.data + .bss` | 202,340 bytes | 69,772 |
| Static-data tripwire | 203,776 bytes | 70,656 |
| Below it | 1,436 bytes | 884 |
| Fixed linked prefix | 202,352 bytes | 69,784 |
| Linker/core-memory span at boot | 59,792 bytes | 192,360 |

The tripwire rises by one reviewed 130 KiB increment (133,120 bytes: the
buffer, the owner's growth and room for review fixes) to 203,776
(`tests/host/run_firmware_memory_budget_checks.sh`). It stays a regression
policy. The real margin is what SRAM0–3 has left: 55,696 bytes past the linked
prefix and the 4,096-byte arena floor, all of it the core allocator's boot
span (59,792 bytes with the floor), against 192,360 before. ChibiOS and the
split slave thread's working area are already in the linked prefix; what the
allocator actually hands out at run time is keyboard evidence still owed,
with stack high-water, on both halves. The left half links the same code and
was not separately analysed: the resource build stops at the right half's
first failing manifest (below).

Stack (reviewed paths, same ELF). The owner manifest passes every path. The
two chunk paths over budget on D-F29 and D-F30 stay inside it: peer large
chunk write 1,904 and repeated read 1,896 bytes (D-F31: 1,896 and 1,888);
reuse mailbox admission 1,784 (1,776), large chunk mailbox decode 1,760
(1,752). Those eight bytes are `scan_running`'s, which now hands the working
copy its scan grant; everything the live edits add there is out of line
(`scan_working`, `save_step`, `observe_working`, the boot-time compiled
projection, the transport registration, the paused clock), since each of
these, inlined, widened the durable-I/O step every storage path carries by up
to 48 bytes. New edit paths: Raw HID data admission 544, commit plan reading
the saved envelope 776, status reply 856, domain validation step 1,224,
loading the working copy from the saved slot 1,120, in-place rewrite 1,144,
combo republication 1,432, settings republication 1,200, sync exchange 1,064,
whole-copy read for the other half 1,328, save abandoning its candidate 1,104;
the working RPC's slave-thread mailbox 80. Three existing paths follow the new
call shape: settings readback reads the stored domain (it no longer asks QMK
for live values), settings publication ends in the pointing CPI setter (it
writes no EEPROM), and the boot PD cache is reached through
`noah_effective_pd_invalidate` (`publish_pd_cache` is kept out of line). The
ordinary manifest fails only on the six inherited handled-key paths
(1,952–2,304 bytes, unchanged); every path that passed on D-F31 passes.


## Automatic Live saves and adjustment keys — D-F32

Automatic saves reuse the working copy and candidate storage; no second
profile buffer is allocated. Four coalesced adjustment scalars and scheduling
state live in the owner, with completed keyboard-save status. No regression
budget changes.

An instrumented right-half build (`NOAH_RESOURCE_CHECKS=yes`, pinned GCC
15.2.0 image, BK `d280a4471e`) on the reviewed live-edit base `8251af1f` plus
this change links 4,340 bytes of `.data` and 198,096 bytes of `.bss` in
SRAM0–3: 202,436 combined, 1,340 below the unchanged 203,776-byte tripwire.
The fixed prefix is 202,448 and the boot linker/core-memory span is 59,696
bytes; after its 4,096-byte arena floor the linked margin is 55,600 bytes.
These are linked figures per half, not runtime free memory or high-water.
The 96-byte increase over the earlier D-F32 measurement includes that base's
review fixes as well as automatic-save/adjustment state. The 130,048-byte
working buffer is unchanged. The resource build stops at the ordinary
manifest's inherited failures before analysing the left half separately.

The owner manifest passes every path. New reviewed paths: automatic Save
admission 1,168 bytes; adjustment commit reading the saved envelope 1,392; creating an inherited
settings domain 1,496 (reading the authored defaults 1,264);
DPI adjustment queue 1,068; no-EEPROM CPI feedback 1,064; settings
republication preserving newer pending feedback 1,232. The compiler inlines
the owner adjustment into the runtime wrapper, and pending-value readback
into `projected_setting`; the manifest follows those linked frames. The
largest owner path remains peer chunk write 1,904, with repeated read 1,896
(the earlier D-F32 figures too). The ordinary manifest still fails only its
six inherited handled-key paths (1,952–2,304); no path that passed on the
live-edit base fails here. Neither budget was weakened. Both halves still
owe allocator/stack high-water and measured flash pauses on the keyboard.

## Keyboard restart — D-F34

The restart's state (whether a restart is due, when it was noted, its delay,
whether the other half acknowledged, the RPC registration) and the working
copy sync's stopped flag are the only new RAM; the 8-byte split frames live on
the stack. An instrumented right-half resource build (`NOAH_RESOURCE_CHECKS=yes`
`tools/build-firmware-pair.sh`, the pinned build image, BK `d280a4471e`),
against the same build of `dev` at `ad543e08`:

| | This change | `dev` |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,412 bytes | 4,400 |
| SRAM0–3 `.bss` | 198,136 bytes | 198,120 |
| SRAM0–3 `.data + .bss` | 202,548 bytes | 202,520 |
| Below the 203,776-byte tripwire | 1,228 bytes | 1,256 |
| Fixed linked prefix | 202,560 bytes | 202,528 |
| Linker/core-memory span at boot | 59,584 bytes | 59,616 |

These are linked figures per half, not runtime free memory or high-water.
No regression budget changes.

Stack (reviewed paths, same ELF). The owner manifest passes every path; its
largest stays peer chunk write at 1,912 bytes, and no path that `dev` reviews
grew. Eleven working-copy paths through `scan_working` dropped by 80 bytes:
the other half's scan of its working-copy mailbox now links out of line,
since a stopped half calls it too. The restart request is kept out of line
(`noinline`), because inlined into `noah_profile_channel_dispatch` it widened
the frame every Raw HID report passes through by 8 bytes. New reviewed paths:
the restart asking the other half from the Raw HID handler 624 bytes, the
reset from housekeeping 192, a stopped half answering the working copy's poll
1,200, and the restart RPC on the split slave thread 88. The ordinary manifest
fails only on its six inherited handled-key paths (1,960–2,312 bytes), each
the same as on `dev`. The resource build stops there, before the left half,
which links the same code and was not analysed separately. Both halves still
owe allocator and stack high-water on the keyboard.

## The reviewed stack paths against the linked firmware

The resource build now runs every gate on both halves, whatever an earlier
one found, and fails afterwards if any did. Both manifests were reviewed
against the current linked callers: no path carries an adjacency the linker
no longer makes or an undeclared indirect edge. Figures come from the
instrumented pair (`NOAH_RESOURCE_CHECKS=yes` with the vault's `verify`,
pinned GCC 15.2.0 image, BK `d280a4471e`); both halves give the same figure
for every path. `dev`'s own right half, built the same way, links a
byte-identical ELF, so `dev`'s figures are the existing paths' figures here.
SRAM0–3 `.data + .bss` is 202,548 bytes per half, 1,228 below the
203,776-byte tripwire; no firmware changed.

On `dev` the owner manifest (79 main-process and 4 split-slave paths) passes:
its largest paths are peer large chunk candidate write 1,912 and peer repeated
large chunk read 1,904 bytes of the 1,920-byte budget, then 1,792; its largest
split-slave path is mailbox decode at 608 of 768. The ordinary manifest (51
and 4) fails only by budget, on six handled-key paths: handled press and
handled release projection with fallback settlement 2,312 bytes each, bounded
deferred fallback settlement 2,168, held-back record replay 2,144, active
release decision 1,976 and pending multi-tap release decision 1,960. Its
largest passing paths are saved macro storage read 1,904 and matrix scan
bounded deferred fallback settlement 1,888; its split-slave paths reach 328.

Code beneath every key event ran unreviewed. The ordinary manifest now
declares all nine stage-table handlers that `process_record_user` calls
indirectly (it declared two), and the core plan's sink, through which a
streaming transition projects its 16 planned effects when the next is pushed
(auto-drain). New main-process paths; those that project end in the
fallback-settlement projection the handled paths use unless named otherwise:

| Path | Bytes | Against 1,920 |
| --- | ---: | ---: |
| Press preflight, missed-scan planning auto-drained | 2,824 | 904 over |
| Handled press, fallback settlement preempting a held pointing mode | 2,584 | 664 over |
| Press preflight, its plan projected | 2,528 | 608 over |
| Handled press, the fallback settlement's own plan auto-drained | 2,456 | 536 over |
| Release preflight, missed-scan planning auto-drained | 2,368 | 448 over |
| Pointing-mode press stage, a mode's tap | 2,120 | 200 over |
| Release preflight, its plan projected | 2,072 | 152 over |
| Matrix scan, active-scan planning auto-drained | 2,048 | 128 over |
| Layer-tap settlement before a press, auto-drained | 2,016 | 96 over |
| Pointing-mode release stage, a mode's tap | 2,016 | 96 over |
| Direct action stage | 2,016 | 96 over |
| Layer-tap settlement before a press, its plan projected | 1,760 | 160 inside |
| Layer-tap settlement before a press, behaviour lookup | 1,592 | 328 inside |
| Native overlap stage, its report | 1,184 | 736 inside |
| Release slot keycode stage | 1,096 | 824 inside |
| Idle custom key stage | 1,040 | 880 inside |

The ordinary manifest (67 and 4 paths) therefore fails on seventeen paths,
each a key-runtime path, by 40 to 904 bytes.

Against the physical stack the figures are frame sums from `main`, so they
leave out what an interrupt pushes onto the process stack (at least 64 bytes,
taken at the deepest point). The process stack (0x20040400–0x20040E00) lies
directly above the 1 KiB interrupt stack, so an overflow lands where the next
interrupt pushes its frames. Press preflight auto-drained is 264 bytes past
the 2,560-byte process stack, at least 328 with an interrupt; press preflight
with its plan projected is 32 bytes inside it, 32 past with an interrupt.
The existing 2,312-byte handled tail stays 184 bytes inside with an
interrupt. These are reachable call chains: the auto-drained press preflight
needs a press while enough keys are active that interrupt planning queues 16
effects, a tap among them, a pending fallback hold and a pointing-mode press.

The deepest direct-call projection beneath a fallback settlement is the
preemption, which releases a held pointing mode. The reviewed path is 2,584
bytes, but it composes the activate branch a release never takes (8 bytes
too many) and stops at the 6KRO report build (`macro_payload_output_keys`):
the host driver's send beneath it is called through a function pointer and
is not reviewed. Reachable as far as the report build, it is 16 bytes past
the process stack, 80 with an interrupt, and at least 88 (152) once the
report is sent. A settlement whose own plan auto-drains adds 144. Both apply to every
fallback-settlement path and compose with auto-drain above it. Auto-drain
needs a transition whose plan already holds 16 effects; for the fallback
settlement's own plan, more than 16 keys waiting on fallback holds at once.

The frames that carry these are the key runtime's plans, held on the process
stack: `process_record_user` 496 bytes (it inlines the release preflight with
its transition and core plans), `key_runtime_preflight_record` 440,
`noah_key_runtime_settle_pending_fallback_hold` 400,
`noah_key_runtime_scan` 504 and
`noah_key_runtime_settle_layer_taps_before_press` 456. Reducing them belongs
to the key-runtime refactor; no budget changes and no path was dropped.

The owner manifest adds the split slave thread's busy reply when its mailbox
is full: asking the store runtime's `storage_paused` callback 288 bytes and
encoding the busy frame 616, of 768. It passes every path on both halves.
These are reviewed-path estimates for the named paths only, not a global or
interrupt-stack maximum; stack high-water on both halves remains keyboard
evidence.

## One effect plan per transition

The key runtime's transitions each hold one effect plan of 16 effects
(196 bytes, `key_runtime_core_effect_plan_t`) where they held two: the
streaming transitions kept a core plan beside their transition plan only to
reach its sink, and the handled press and release copied a buffered core plan
into a transition plan. The reducer now plans straight into the caller's plan;
a streaming plan's push projects it through `key_runtime_transition_drain_plan()`
when it is full, a direct call where the sink was a declared indirect edge.
The plans sit in frames of their own (`noinline`), so no path carries one it
does not use: the before-release preflight's is no longer inlined into
`process_record_user`, nor the scan's into `matrix_scan`, and the handled
press's and release's planning returns before their plan is projected.
Instrumented pair (`NOAH_RESOURCE_CHECKS=yes` with the vault's `verify`, pinned
GCC 15.2.0 image, BK `d280a4471e`); both halves give the same figure for every
path. Against the previous record:

| | This change | Before |
| --- | ---: | ---: |
| SRAM0–3 `.data` | 4,412 bytes | 4,412 |
| SRAM0–3 `.bss` | 198,128 bytes | 198,136 |
| SRAM0–3 `.data + .bss` | 202,540 bytes | 202,548 |
| Below the 203,776-byte tripwire | 1,236 bytes | 1,228 |
| Fixed linked prefix | 202,552 bytes | 202,560 |
| Linker/core-memory span at boot | 59,592 bytes | 59,584 |

The frames that carried the plans: `process_record_user` 496 → 96 bytes,
`noah_key_runtime_settle_pending_fallback_hold` 400 → 216,
`key_runtime_preflight_record` 440 → the press preflight's
`key_runtime_preflight_settle_before_press` 264, the release preflight's
`key_runtime_preflight_settle_before_release` 248 (out of
`process_record_user`), `noah_key_runtime_scan` 504 → 40 with
`noah_key_runtime_scan_core` 320, `noah_key_runtime_settle_layer_taps_before_press`
456 → 72 with its plan's frame 216, and the handled press and release 232 → 216
and 232, their planning helpers 264 and 256 → 80 and 88.

The ordinary manifest's seventeen failing paths, against the 1,920-byte policy:

| Path | Before | This change |
| --- | ---: | ---: |
| Press preflight, missed-scan planning auto-drained | 2,824 | 2,032 (112 over) |
| Handled press, fallback settlement preempting a held pointing mode | 2,584 | 2,008 (88 over) |
| Press preflight, its plan projected | 2,528 | 1,792 |
| Handled press, the fallback settlement's own plan auto-drained | 2,456 | 1,832 |
| Release preflight, missed-scan planning auto-drained | 2,368 | 1,992 (72 over) |
| Handled press projection with fallback settlement | 2,312 | 1,736 |
| Handled release projection with fallback settlement | 2,312 | 1,752 |
| Handled release, bounded deferred fallback settlement | 2,168 | 1,592 |
| Held-back record replay into handled press projection | 2,144 | 1,568 |
| Pointing-mode press stage, a mode's tap | 2,120 | 1,544 |
| Release preflight, its plan projected | 2,072 | 1,752 |
| Matrix scan, active-scan planning auto-drained | 2,048 | 1,680 |
| Layer-tap settlement before a press, auto-drained | 2,016 | 1,624 |
| Pointing-mode release stage, a mode's tap | 2,016 | 1,440 |
| Direct action stage | 2,016 | 1,440 |
| Handled active release decision | 1,976 | 1,408 |
| Handled pending multi-tap release decision | 1,960 | 1,392 |

Fourteen now pass; no other path grew past the policy, and the ordinary
manifest's largest passing path is the handled press with its fallback
settlement's plan auto-drained, 1,832. Three still fail by budget. Each holds
two plans by design: the plan being projected and, beneath an output it
projects, the fallback settlement's own plan (216 bytes), which must hold its
effects until its planning loop ends. The rest of each chain is the reducer's
scan planning under an auto-drain (some 250 bytes) or the pointing mode's
release beneath the preemption (some 640), not plans. Against the physical
2,560-byte process stack the worst, press preflight auto-drained, is 528 bytes
inside, at least 464 with an interrupt's 64. The preemption, reachable as far
as the report build (it composes 8 bytes of a branch a release never takes),
is 2,000 bytes: 560 inside, 496 with an interrupt, and sending the report
takes at least 72 more. The owner
manifest passes every path on both halves; its largest stays peer large chunk
write at 1,912 bytes. These are reviewed-path estimates for the named paths
only, not a global or interrupt-stack maximum; stack high-water on both halves
remains keyboard evidence.

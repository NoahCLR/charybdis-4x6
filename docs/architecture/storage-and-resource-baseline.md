# Stage 00 Baseline

Measured on 2026-08-25 from userspace commit `058f7fdd` against sibling QMK
branch `noah-userspace-contracts` at `aac9f637ee`.

## Firmware And Test Baseline

| Evidence | Result |
| --- | --- |
| Full host suite | pass |
| Firmware compile | pass; ELF and UF2 produced |
| QMK fork contract | sampled auto-mouse API present on `noah-userspace-contracts` |
| Hardware live-link probe | not run; no matching board was visible during the transport audit |

The full host suite passed only after the sibling checkout moved from
`qmk-latest` to `noah-userspace-contracts`, which contains `auto_mouse_get_time_elapsed_at()`.

## Target Resource Map

All values in this section are per keyboard half. Each half runs the same image
on its own RP2040; the two halves do not share RAM.

### Physical Bank Map

| Linker region | Physical SRAM | Bytes | Current role |
| --- | --- | ---: | --- |
| `ram0` | SRAM0–3, word-striped | 262,144 | `.data`, `.bss`, small RAM-resident sections, and the remaining ChibiOS core-memory/newlib allocation span |
| `ram4` | SRAM4 | 4,096 | interrupt stack, main process stack, and core-0 RTOS state |
| `ram5` | SRAM5 | 4,096 | reserved core-1 interrupt and process stacks; its final 256 bytes are also exposed as the overlapping `ram7` boot region and are not additional memory |
| **Total per RP2040** | six physical banks | **270,336** | **264 KiB** |

The 51,000-byte, 26,000-byte, and 204,800-byte thresholds below are project
regression policies. None is the RP2040's physical capacity.

### Current Reconciled Measurement

Fresh ordinary ELF on 2026-08-27 after the effective key-feedback RGB
consumer:
`bastardkb_charybdis_4x6_noah.elf`.

| Resource | Measured | Policy or physical boundary | Remaining margin |
| --- | ---: | ---: | ---: |
| SRAM0–3 `.data` | 22,984 B | accounting input | — |
| SRAM0–3 `.bss` | 25,684 B | 26,000 B regression policy | 316 B policy margin |
| SRAM0–3 `.data + .bss` metric | 48,668 B | 51,000 B regression policy | 2,332 B policy margin |
| SRAM0–3 linker/core-memory span at boot | 213,472 B | 204,800 B conservative minimum policy | 8,672 B policy margin |
| Fixed linked occupancy across all SRAM banks | 56,128 B | 270,336 B physical SRAM | informational; includes alignment and reserved stacks, excludes runtime allocation |
| Main-process worst reviewed path | 1,880 B | 1,920 B reviewed-path policy | 40 B policy margin |
| Main process stack allocation | 2,560 B | physical stack boundary | 680 B beyond the worst reviewed path |
| Split-slave worst reviewed path | 336 B | 768 B reviewed-path policy within its 1,024 B usable stack | 432 B policy margin |

The initial Stage 00 ELF at userspace commit `058f7fdd` recorded 111,184 B
`.text`, 15,628 B `.rodata`, 22,872 B `.data`, 25,524 B `.bss`, a 48,396 B
`.data + .bss` metric, and a 213,744 B linker/core-memory span. Those values are
historical measurements, not current capacity limits.

The SRAM0–3 linker/core-memory span is the range from `__heap_base__` to
`__heap_end__`. ChibiOS initializes its monotonic core allocator from this
range, and the linked newlib `_sbrk_r` obtains allocation pages from that
allocator. Therefore 213,472 bytes is the maximum span at boot, not a measured
runtime-free or runtime-high-water value. Hardware telemetry is still required
to record runtime allocator use.

SRAM4 is the actually tight bank: its 4,096 bytes contain a 1,024-byte
interrupt stack, a 2,560-byte process stack, and 288 bytes of RTOS state,
leaving 224 bytes outside those reservations. The 40-byte number above is only
the margin to the deliberately stricter reviewed-path policy. The stack result
covers named reviewed paths, not every possible call graph or interrupt nesting.
Stage 02 must add profile decode, storage, protocol, and split-RPC paths as they
become reachable.

A nominal 4 KiB active buffer, or an active plus candidate pair, would violate
the current `.data + .bss` regression policy. A zero-initialized buffer placed
in `.bss` would also violate the BSS policy. Neither would exhaust the RP2040:
one nominal buffer would leave about 209,376 bytes in the SRAM0–3
linker/core-memory span, and two would leave about 205,280 bytes before runtime
allocation. Candidate bytes remain in the inactive EEPROM slot because this
supports power-loss-safe staging and avoids unnecessary duplication, not
because the hardware has only 2–3 KiB RAM available. Firmware should still
prefer bounded indexes and derived caches where that keeps ownership simple.

## Logical EEPROM Address Map

`WEAR_LEVELING_BACKING_SIZE` is 32,768 bytes. RP2040 wear leveling exposes
half of that as 16,384 bytes of logical EEPROM.

### Historical Map Before The Profile Partition

| Range | Bytes | Owner |
| --- | ---: | --- |
| `0x0000–0x0024` | 37 | QMK EECONFIG |
| `0x0025–0x0027` | 3 | VIA magic |
| `0x0028` | 1 | VIA layout options |
| `0x0029–0x0280` | 600 | Dynamic keymap: 5 × 10 × 6 × 2 |
| `0x0281–0x3FFF` | 15,743 | VIA macro region |

Current authored VIA macro defaults use approximately 258 bytes including
slot terminators.

### Original Eight-Layer Profile Partition

| Range | Bytes | Owner |
| --- | ---: | --- |
| `0x0000–0x0024` | 37 | QMK EECONFIG |
| `0x0025–0x0028` | 4 | VIA config |
| `0x0029–0x03E8` | 960 | Dynamic keymap: 8 × 10 × 6 × 2 |
| `0x03E9–0x1FFF` | 7,191 | VIA macros |
| `0x2000–0x2FFF` | 4,096 | Live-profile slot A |
| `0x3000–0x3FFF` | 4,096 | Live-profile slot B |

`DYNAMIC_KEYMAP_EEPROM_MAX_ADDR` becomes `0x1FFF`. The profile does not use
`VIA_EEPROM_CUSTOM_CONFIG_SIZE`, because that would move existing dynamic
keymap addresses and mix unrelated storage ownership.

Repartitioning the existing logical EEPROM does not grow the wear-level cache.
Increasing logical EEPROM is rejected for v1 because QMK allocates a RAM cache
equal to the logical EEPROM size.

## Slot Contract

Each slot contains a 32-byte storage header followed by at most 4,064 bytes of
canonical Profile Wire payload.

The legacy format-1 header remains readable and is independent of compiler
struct layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | storage magic `NP` |
| 2 | 1 | storage format version, `1` |
| 3 | 1 | schema major in the high nibble, minor in the low nibble |
| 4 | 2 | payload length, little-endian |
| 6 | 4 | generation counter, little-endian |
| 10 | 1 | origin half, `0` or `1` |
| 11 | 1 | flags; bit 0 is override-enabled and all others are reserved |
| 12 | 4 | payload CRC32, little-endian |
| 16 | 4 | canonical payload FNV-1a digest, little-endian |
| 20 | 4 | compiled-default digest, little-endian |
| 24 | 4 | action-ABI digest, little-endian |
| 28 | 2 | CRC16-CCITT over bytes 0 through 27, little-endian |
| 30 | 2 | commit marker `A5 5A`, written last |

The packed schema field intentionally limits each stored schema component to
four bits in storage format v1. A later schema outside that range requires a
new storage format rather than a noncanonical reinterpretation of this header.

Atomic logical Apply writes format 2:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | logical storage magic `NQ` |
| 2 | 1 | domain mask in bits 0–3, origin in bit 4, profile flag in bit 5; bits 6–7 reserved |
| 3 | 2 | payload length, little-endian |
| 5 | 4 | custom generation, little-endian |
| 9 | 4 | payload CRC32, little-endian |
| 13 | 4 | compiled-default digest, little-endian |
| 17 | 4 | action-ABI digest, little-endian |
| 21 | 4 | bound VIA generation, little-endian |
| 25 | 4 | bound canonical VIA digest, little-endian |
| 29 | 2 | CRC16-CCITT over bytes 0 through 28, little-endian |
| 31 | 1 | state marker: `5A` prepared, `A5` committed, `00` invalid |

Format 2 binds schema 1.0 and derives the domain mask and FNV-1a payload digest
while validating the canonical payload. The header retains the compiled-default and
action-ABI identities so a reflash cannot silently adopt incompatible sparse
profile data.

### Schema-2 PD geometry

Before D-F14, side-specific PD-enabled owner builds selected schema 2,
format-3 `NR` and a 5,088-byte payload ceiling. Identity byte 2 uses domain
bits 0–4, origin bit 5, override bit 6 and reserved bit 7; CRC and markers
retain `NQ` offsets. Format 3 accepted RGB v3, settings v2–v5, key-behavior
v1, sparse PD v2 and combo v1–v2 (32 pointing slots, D-F09). Host candidate and split
writers bind it to the same logical VIA generation as format 2.

The lower 8 KiB VIA allocation is unchanged. Slot A is `0x2000..0x33ff`, slot B
`0x3400..0x47ff`, each with a 32-byte header. Logical EEPROM is 18 KiB and
wear-level backing is 36 KiB. Geometry changes the flash base; never interpret
old-geometry contents in place. The old readback bridge build is retired;
existing backups and old firmware must be retained outside this build.
See [the domain contract](pd-mode-domain-v1.md) and the deliberately revised
feature-specific policy in [memory budgets](memory-budgets.md).

Inactive-slot commit ordering is:

1. invalidate its commit marker;
2. write the payload in bounded scan-context chunks;
3. write the non-commit header;
4. read back and verify payload and header checksums;
5. write and verify the prepared marker;
6. after the coordinator's decision, replace it with the commit marker;
7. publish only after the activation owner accepts the complete logical state.

At boot, firmware selects the newest valid compatible slot. If neither slot is
valid, it uses compiled defaults. Interrupted writes never invalidate the
previous valid slot.

Generation counters are strictly monotonic unsigned 32-bit values. Zero is
invalid and `UINT32_MAX` is an exhausted terminal generation: v1 never wraps to
one because a wrapped record would lose to the last-known-good record under the
accepted higher-counter authority rule. Equal counters with different
authority identities are reported as conflicts rather than selected silently.

## Fixed V1 Ceilings

| Surface | Ceiling |
| --- | ---: |
| Logical layers | schema and firmware maximum 8 |
| Key-behavior rows | 64 |
| Tap steps per behavior | 5 |
| Populated behavior steps | 128 aggregate |
| Combos | 32 |
| Keys per combo | 4 |
| Reusable RGB groups | 16 |
| RGB stage-group rows | 32 aggregate |
| Physical LEDs | compiled 58 |
| LEDs per group | compiled 58 |
| Canonical LED bitmap | 8 bytes |
| Retired user-macro slots (action ABI range) | 16 |
| Persisted user-macro payload (settings v1/v2 only) | 1,024 bytes aggregate |
| Storage slot | 4,096 bytes |
| Canonical profile payload | 4,064 bytes |

All per-domain ceilings are additionally bounded by the 4,064-byte aggregate
payload maximum. Advertising a ceiling does not promise that every independent
maximum can be reached simultaneously.

## Verification Commands

- `sh tests/host/run_all_host_tests.sh` — pass
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — pass
- `sh tests/host/run_firmware_memory_budget_checks.sh` — pass
- `sh tests/host/run_firmware_stack_budget_checks.sh` — pass
- `arm-none-eabi-size -A ../bastardkb-qmk/.build/bastardkb_charybdis_4x6_noah.elf`
- `arm-none-eabi-nm -S --size-sort -r ../bastardkb-qmk/.build/bastardkb_charybdis_4x6_noah.elf`

The sibling QMK checkout was inspected and used for build artifacts. No sibling
source file was edited by this project pass.

The 2026-09-15 atomic logical-Apply image keeps the owner at exactly 4,096 bytes
on the 32-bit target, meeting its unchanged engineering state policy. Its normal
left-half SRAM0–3 `.data + .bss` is 55,796 bytes against the 57,344-byte
regression tripwire, and its linker/core-memory span at boot is 206,344 bytes.
The reviewed-path stack check passes at 1,824/1,920 bytes for the largest named
main-process path and 328/768 bytes for the largest named split-slave path. The
memory figures are linked values per half; the stack figures cover the manifest
paths only and are not runtime high-water evidence.

### Schema-3 geometry (D-F14)

Format-5 `NT` keeps the `NR` header layout: identity byte 2 holds domain bits
0–4, origin bit 5 and override bit 6; payload length stays a 16-bit field
because a slot payload is at most 53,216 bytes. Storage addresses are 32 bits
(`noah_profile_storage_address_t`), since slot B ends at `0x22FFF`. Format 5
binds schema 3.0 and accepts only RGB v4, key-behavior v2, combo v3, settings
v6 and sparse PD v3.

| Range | Bytes | Owner |
| --- | ---: | --- |
| `0x00000–0x08FFF` | 36,864 | QMK EECONFIG, VIA config, dynamic keymap, VIA macros |
| `0x09000–0x15FFF` | 53,248 | Live-profile slot A |
| `0x16000–0x22FFF` | 53,248 | Live-profile slot B |

Logical EEPROM is 140 KiB and wear-level backing 280 KiB at the top of flash.
QMK mirrors the logical EEPROM in SRAM0–3; see
[memory budgets](memory-budgets.md#bigger-profile-storage--d-f14). Geometry
moves the flash base, so the previous 18 KiB contents are never read.

The macro expansion keeps that flash base and the 140 KiB RAM mirror unchanged.
VIA sync metadata schema 5 refuses schema 4, so the existing recovery path
resets VIA and seeds authored defaults before declaring authority. Logical
store format 5 uses `NT` magic; old `NS` headers are refused even if old payload
bytes happen to land on a new slot boundary. There is no in-place migration.
The 41-byte configuration and 1,920-byte keymap leave 34,903 macro-bank bytes.
All 128 slot terminators count toward that bank; a single macro can use the
remaining space without a separate compiled-program limit (feature bit 27).

### Schema-3 ceilings and the maximum profile

| Surface | Ceiling |
| --- | ---: |
| Logical layers | 16 |
| Key-behavior rows | 128 |
| Tap steps per behavior / populated steps | 5 / 640 aggregate |
| Combos / inputs per combo | 128 / 2–16 |
| VIA macros / custom keys | 128 / 128 |
| Names | 32 bytes of UTF-8 each |
| Reusable RGB groups / stage-group rows | 16 / 32 aggregate |
| Pointing slots | 32 |
| Canonical profile payload | 53,216 bytes |

Unlike the V1 ceilings above, every one of these is reachable at once.
`tests/fixtures/maximum_profile_v3.fixture`, written by
`tests/host/make_maximum_profile.py` from the byte specs, holds every table at
its maximum and every name at 32 bytes: 37,667 bytes (RGB 591, behaviours
13,828, combos 9,736, settings 9,380, pointing 4,104, plus 28 bytes of
header and envelopes). That leaves 15,549 bytes of the slot, more than the
12 KiB reserved for future payload. The fixture is the maximum for the
encodings, not a limit the firmware checks: a larger profile is refused only
by a table's own ceiling or the slot.

The owner validates one step a matrix scan, each step one read of at most 20
bytes. The maximum profile validates in 7,429 scans, a boot that adopts it
publishes after 9,912, and a peer's prepared copy spends 11,104 scans
validating without transfer progress, which the split harness's conservative
5 ms scan keeps inside the owner's 60-second no-progress window. Real
durations depend on the scan rate and are hardware acceptance evidence.
`run_profile_domain_owner_round_trip_tests.sh` saves, boots, publishes,
refuses over-limit and truncated copies of, and cuts power through a save of
this profile; `run_profile_split_reconciler_tests.sh` prepares and commits it
on the peer.

Earlier builds used eight-layer VIA geometry, schema-2 reconciliation
metadata, and an 18 KiB logical EEPROM with two 5 KiB profile slots. The old
five-layer snapshot bridge is retired. Existing portable files remain subject
to the source-evidence rules in [portable-profile-v1.md](portable-profile-v1.md).

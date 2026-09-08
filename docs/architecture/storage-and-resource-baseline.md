# Stage 00 Baseline

Measured on 2026-08-25 from userspace commit `058f7fdd` against sibling QMK
branch `sol` at `aac9f637ee`.

## Firmware And Test Baseline

| Evidence | Result |
| --- | --- |
| Full host suite | pass |
| Firmware compile | pass; ELF and UF2 produced |
| QMK fork contract | sampled auto-mouse API present on `sol` |
| Hardware live-link probe | not run; no matching board was visible during the transport audit |

The full host suite passed only after the sibling checkout moved from
`qmk-latest` to `sol`, which contains `auto_mouse_get_time_elapsed_at()`.

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

### Current Eight-Layer Profile Partition

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

The canonical header is independent of compiler struct layout:

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

Inactive-slot commit ordering is:

1. invalidate its commit marker;
2. write the payload in bounded scan-context chunks;
3. write the non-commit header;
4. read back and verify payload and header checksums;
5. write the commit marker atomically as the final operation;
6. publish only after the activation owner accepts the candidate.

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
| Logical layers | schema maximum 8; firmware initially advertises compiled 5 |
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
| Hardcoded macro slots | 16 |
| Persisted hardcoded macro payload | 1,024 bytes aggregate |
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

The temporary snapshot bridge retains the five-layer keymap at
`0x0029–0x0280` and 7,551-byte macro bank at `0x0281–0x1FFF`. Its VIA
reconciliation metadata remains schema 1; the eight-layer image uses schema 2.
Export before changing geometry. See [portable-profile-v1.md](portable-profile-v1.md)
for the migration, effective settings cache and recovery sequence. Neither
variant increases the 16 KiB logical EEPROM or its wear-level cache.

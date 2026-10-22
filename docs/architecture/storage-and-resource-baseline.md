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
allocation. Candidate bytes remain in the inactive slot because this
supports power-loss-safe staging and avoids unnecessary duplication, not
because the hardware has only 2–3 KiB RAM available. Firmware should still
prefer bounded indexes and derived caches where that keeps ownership simple.

## Logical EEPROM Address Map

`WEAR_LEVELING_BACKING_SIZE` is QMK's RP2040 default, 8,192 bytes, and wear
leveling exposes half of it as 4 KiB of logical EEPROM (D-F31). It holds only
QMK's eeconfig and its datablocks, with room for the user datablock that will
hold the edit mode; QMK's RAM mirror is the same 4 KiB. VIA is not supported,
so there is no VIA configuration, dynamic keymap or macro bank: keys and
macros are profile domains (D-F30), and the profile slots have a flash region
of their own ([Profile slot flash region](#profile-slot-flash-region-d-f29)).
From D-F29 to D-F31 the backing was 73,728 bytes, a 36 KiB VIA region at
`0x0000–0x8FFF`. The two maps below are historical, from the 32,768-byte
backing and its 16,384 logical bytes.

### Historical Map Before The Profile Partition

| Range | Bytes | Owner |
| --- | ---: | --- |
| `0x0000–0x0024` | 37 | QMK EECONFIG |
| `0x0025–0x0027` | 3 | VIA magic |
| `0x0028` | 1 | VIA layout options |
| `0x0029–0x0280` | 600 | Dynamic keymap: 5 × 10 × 6 × 2 |
| `0x0281–0x3FFF` | 15,743 | VIA macro region |

The authored VIA macro defaults used approximately 258 bytes including slot
terminators.

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

Repartitioning the existing logical EEPROM did not grow the wear-level cache.
Increasing logical EEPROM was rejected for v1 because QMK allocates a RAM cache
equal to the logical EEPROM size; D-F14 later grew it to 140 KiB within the
measured SRAM budget ([memory budgets](memory-budgets.md#bigger-profile-storage--d-f14)).

## Slot Contract

Each slot contains a 32-byte storage header followed by at most 4,064 bytes of
canonical Profile Wire payload.

Format 1 (`NP`) was the first header; current firmware refuses it. It was
independent of compiler struct layout:

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

1. invalidate it: on flash, erase its header sector before anything is
   written to the slot (D-F29); on byte-writable test media, clear its marker;
2. write the payload in bounded scan-context chunks;
3. write the non-commit header;
4. read back and verify payload and header checksums;
5. write and verify the prepared marker;
6. after the coordinator's decision, replace it with the commit marker;
7. publish only after the activation owner accepts the complete logical state.

At boot, firmware selects the newest valid compatible slot. If neither slot is
valid, it uses compiled defaults. Interrupted writes never invalidate the
previous valid slot. Boot selection is the only path that validates and adopts a
saved slot from scratch; it runs the header, checksum and shape checks in bounded steps,
and the store says what it found in each slot (`boot_slot_result`).

The store answers its own state questions, and its callers ask them rather
than read its fields: the committed record, whether a prepare is active,
whether the store is settled (no prepare and no outstanding destructive-write
reservation), the admitted candidate, whether the payload is fully staged,
whether it is durably prepared and waiting for a decision, and whether a final
marker awaits a boot scan (`profile_store.h`). They are static inline;
the compiler may keep one out of line as a small leaf of its own, and the reviewed stack paths are measured as built. Slot addressing (a slot's start, and its
payload's after the 36-byte header) has its one home there too; where those
addresses fall on flash pages stays with the flash backend (D-F29).

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

### Schema-4 geometry (D-F14, D-F28)

Store format 6 (D-F28) gives the payload length 32 bits. Its 36-byte slot
header (D-F30) has no room left for a magic, so the format is keyed into the header
instead:

| Byte | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | domain mask, seven known bits |
| 1 | 4 | payload length, uint32 |
| 5 | 1 | origin bit 0, override bit 1; other bits zero |
| 6 | 4 | generation |
| 10 | 4 | payload CRC32 |
| 14 | 4 | compiled-default digest |
| 18 | 4 | action-ABI digest |
| 22 | 11 | reserved zero; refused otherwise (22–29 held the bound VIA generation and digest until D-F31) |
| 33 | 2 | CRC16-CCITT over the format version (one byte, `6`) then bytes 0–32 |
| 35 | 1 | marker: erased `0xFF`, prepared `0x3C`, committed `0x18`, invalid `0x00` |

Format 5 (`NT`) had a two-byte magic, identity at byte 2, a 16-bit length at
byte 3, an unkeyed CRC and markers `0xA5`/`0x5A`. A slot with the `NT` magic
and either format-5 marker is an earlier format: it is refused before its
payload is read, never converted (Ark's D-L64), and status reports it (Profile Wire
state flag 9) while nothing current is committed. Any other header from an
earlier or later format fails the keyed CRC or carries a marker this format
never writes. Storage addresses are 32 bits (`noah_profile_storage_address_t`),
offsets into the slot region, and so are the store's read and write
callbacks. Each marker is a bit subset of the one before it, so on NOR flash
each step only clears bits (D-F29); format 6 committed with `0xC3` while the
slots were in EEPROM, before any release.

On flash the record is laid out by page, so that no 256-byte page is
programmed twice between erases (D-F29). The store's addresses stay as the
table above gives them; the flash backend places them:

| Slot flash offset | Bytes | Holds | Programmed |
| --- | ---: | --- | --- |
| `0x000` | 256 | header bytes 0–30; byte 31 stays erased | once, at the header write |
| `0x100`, `0x200`, `0x300` | 1 each, a page each | the marker's three cells | one per marker step, in order |
| `0x400` | to the slot's end | payload | each page once, in order |

The marker reads as the bitwise AND of its cells. Each step programs the next
cell with the step's value: prepared `0x3C` into the first, committed `0x18`
into the second; an abort's invalid `0x00` goes into the next unused one. As
each value is a bit subset of the one before, the AND is always the last
value written, so the marker sequence and its marker-last durability are as
on byte media. A half-programmed cell can only clear more bits of the AND,
which reads as no marker this format writes. The header and every cell lie in
the slot's first sector, so erasing it clears the record at once. The first
kilobyte is the record's, so a slot holds at most 130,048 payload bytes. Format 6 binds schema 4.0 and
accepts only RGB v4, key-behavior v2, combo v3, settings v8 (v6 when it
landed; D-F25 and D-F26 each added one), sparse PD v3, keymap v1 and macros
v1 (D-F30). Existing domain versions are unchanged.

Nothing in the format, the wire or the split copy stops at 64 KiB: host
tests carry a 70,000-byte payload through the blob codec, the candidate upload,
the split frames, the receiving half's staging of a split copy (chunks and a
reuse range past 64 KiB) and a store write, commit, boot and read.

The macro domain holds 34,903 bytes, the size the VIA bank had in its 36 KiB
region. All 128 slot terminators count toward it; a single macro can use the
remaining space without a separate compiled-program limit.

### Profile slot flash region (D-F29)

The two slots are a fixed 256 KiB region of flash, 1 MiB in, read in place
through XIP: no RAM buffer holds a slot. Addresses in the store are offsets
into the region.

| Flash offset | Region offset | Bytes | Owner |
| --- | --- | ---: | --- |
| `0x000000–` image end | — | about 272 KB today | firmware image (`__flash_binary_end`) |
| `0x100000–0x11FFFF` | `0x00000–0x1FFFF` | 131,072 | profile slot A: 1 KiB record (header and marker cells), 130,048-byte payload |
| `0x120000–0x13FFFF` | `0x20000–0x3FFFF` | 131,072 | profile slot B |
| `0x1FE000–0x1FFFFF` | — | 8,192 | wear-leveling backing (4 KiB logical EEPROM, D-F31) |

The build assumes 2 MiB of flash (pico-sdk's default, as wear leveling does).
The link fails if the image reaches `0x10100000`
(`users/noah/lib/compat/qmk_profile_flash.ld`), and the flash adapter does not
compile if the region reaches the wear-leveling backing. A UF2 writes only the
image, so the slots survive a firmware update. An EEPROM reset forgets them,
as formatting logical EEPROM did while the slots lived there: QMK formats
logical EEPROM at boot exactly when its magic is not valid, and makes the
magic valid early in formatting, before any userspace hook runs; so
`keyboard_pre_init` erases both header sectors whenever the magic is not
valid, before QMK's keyboard init and before anything reads a slot. Until
both erases are done the magic stays invalid, so a boot interrupted anywhere
in between forgets the slots again (`tests/host/qmk_profile_flash_test.c`
cuts power before, during and between the erases and before the magic
write). Old EEPROM slots are not migrated: they held format 5, which this
store refuses anyway.

Flash is NOR: a sector (4 KiB) erases to `0xFF`, a page program (256 bytes)
only clears bits, and no page is programmed again before its sector is
erased (the host test fakes refuse it). The backend (`profile_flash_store.c`)
keeps one 256-byte page buffer:

- a save's reset touches no flash: it names the target range, which from then
  on reads as erased, and every write to it waits until the header sector is
  erased, so the slot stops being a boot candidate before anything is written
  to it. Until that erase the slot holds what it held, an older profile than
  the active one, which is what a power cut at that moment leaves too; a save
  aborted before the erase leaves the slot untouched;
- writes collect in the page buffer and reach flash as one page program when
  the page fills, when a write moves to another page, or at a sync; reads
  overlay the buffered page;
- the store syncs before the header readback, after the prepared marker and
  after the commit marker, which is still written last.

Erase and program run with interrupts off while XIP is off, through pico-sdk's
RAM-resident `flash_range_erase` and `flash_range_program`, as QMK's
wear-leveling driver does: the keyboard pauses for the operation, so the pause
policy decides when one may run.

- **Erases run only on a quiet keyboard.** A sector erase (W25Q16JV: 45 ms
  typical, 400 ms at worst) runs at the start of a durable-I/O grant, at most
  one per grant, only when no key is held and the matrix has not changed for
  50 ms (`NOAH_QMK_PROFILE_FLASH_QUIET_MS`), and only for a save: the next
  sector of its range, while fewer than a sector of erased bytes lie ahead of
  its writes. An erase so starts only in a pause in typing; a key pressed
  during it is read when it ends, and only a press and release both inside
  the erase would be missed.
- **The writer waits for erased flash.** The profile step runs in a grant only
  while at least a page of erased flash lies ahead of the save's writes and
  the grant's flash time is under its 2 ms budget
  (`NOAH_PROFILE_FLASH_GRANT_BUDGET_US`), which an erase always exceeds. So an
  erase never shares a grant with the writer, and a save simply waits while
  the keyboard is busy. Should a write still reach unerased flash, the backend
  erases first rather than write it.
- **A grant programs at most one page.** A grant repeats the store's steps
  only until one erases or programs (D-F29, "Pacing"), and a step writes, in
  order and in store writes of at most 20 bytes, at most a page: a REUSE step
  copies up to 256 bytes of its range, a received split chunk is at most 106
  bytes, and each commit step writes one record and syncs it. So a step fills
  at most one page, and a grant programs at most one.

A save waits while the keyboard cannot safely erase. D-F32 excludes that
wait from the owner's host and split no-progress clocks: the waiting half
reports STORAGE_PAUSED (busy reason 9), which counts as peer progress.
A held key therefore delays a save without making the storage wait itself
expire it. Live starts only after 3 seconds without an edit or held key;
typing after that can pause later erases in the same way. Erase suspend is
not used. The pointing sensor keeps counting motion during a
pause, so pointing stutters rather than loses movement. A save of the maximum
profile erases 19 sectors and programs about 297 pages, with the header and
marker cells each programmed on their own page.

Each half measures its own pauses since boot: longest sector erase, longest
page program, the most flash time in one grant, and the last save's duration
and flash time. Profile Wire custom get `0x0D` reads them
([Profile Wire](profile-wire-v1.md#flash-pauses--get-value-0x0d)).

Power loss: host tests cut power at every erase and page program of a save,
with the operation either not started or half done, and the previous profile
stays active unless the commit marker was programmed; the interrupted slot is
reused by the next save (`tests/host/profile_flash_store_test.c`). The same
tests check every grant against the pause policy: one erase or one page
program, never both, and no erase on a busy keyboard.

### Schema-4 ceilings and the maximum profile

| Surface | Ceiling |
| --- | ---: |
| Logical layers | 16 |
| Key-behavior rows | 128 |
| Tap steps per behavior / populated steps | 5 / 640 aggregate |
| Combos / inputs per combo | 128 / 2–16 |
| Macros / custom keys | 128 / 128 |
| Names | 32 bytes of UTF-8 each |
| Reusable RGB groups / stage-group rows | 16 / 32 aggregate |
| Pointing slots | 32 |
| Canonical profile payload | 130,048 bytes |

Unlike the V1 ceilings above, every one of these is reachable at once.
`tests/fixtures/maximum_profile_v4.fixture`, written by
`tests/host/make_maximum_profile.py` from the byte specs, holds every table at
its maximum and every name at 32 bytes: 74,801 bytes (RGB 591, behaviours
13,828, combos 9,736, settings 9,669 with every custom-key shortcut (D-F26),
pointing 4,104, keymap 1,920, macros 34,903, plus 50 bytes of blob header and
six-byte envelopes). The slot payload is 130,048 bytes after the 1 KiB physical
record, containing the 36-byte logical header and marker cells (D-F30), leaving 55,247 bytes. Keeping the 12,288-byte future reserve
still leaves 42,959 bytes. The generator reads the payload ceiling from the
storage contract and refuses a fixture that cannot retain that reserve.
The fixture is the maximum for these encodings, not a separate firmware
payload limit: a larger profile is refused by a table's ceiling or the slot.

The owner validates one bounded step per matrix scan, reading at most twenty
bytes in that step. The keymap and macro domains add checksum and semantic
passes over their bytes, without a larger per-step workspace or read. The
owner/save/reboot and split suites exercise this complete fixture; actual
validation, prepare and boot duration depend on keyboard scan cadence and
remain hardware acceptance evidence.

Split framing version 4 retains bounded mailbox, duplicate-request and outbound
chunk buffers of at most 128 bytes, plus reuse source/progress state; it never
keeps a full profile in the reconciler. Its 32-bit state regression allowance
is 896 bytes (previously 768), which its 32-bit offsets fill exactly. D-F28's
wider offsets add 292 bytes of static state per half (the owner 260, the
compiled-profile handle 24, padding 8), so the owner's engineering allowance
rises from 11,264 to 11,520 bytes; the 209,920-byte static-data tripwire is
unchanged ([memory budgets](memory-budgets.md#the-new-base-32-bit-profile-lengths--d-f28-2026-10-19)). These allowances
are engineering policies; acceptance still requires fresh linked memory and
reviewed-path stack checks for each half, and hardware high-water evidence.
`run_profile_domain_owner_round_trip_tests.sh` saves, boots, publishes,
refuses over-limit and truncated copies of, and cuts power through a save of
this profile; `run_profile_split_reconciler_tests.sh` prepares and commits it
on the peer.

Earlier builds used eight-layer VIA geometry, schema-2 reconciliation
metadata, and an 18 KiB logical EEPROM with two 5 KiB profile slots. The old
five-layer snapshot bridge is retired. Existing portable files remain subject
to the source-evidence rules in [portable-profile-v1.md](portable-profile-v1.md).

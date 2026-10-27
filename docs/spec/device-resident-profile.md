# Device-resident profile

The keyboard holds its own profile: each half keeps its saved profile in its
own flash, runs it, and falls back to the compiled defaults built into the
firmware. This file covers which profile a half runs, which domains and
formats it admits, how the compiled defaults are composed and read, where and
how a saved profile is stored on one half, and the ceilings every profile
meets. The profile holds structured values, never C source: the authored
profile in the keymap folder is firmware source, and its compiled form is the
compiled defaults. The bytes a profile is made of are
[Profile Wire](profile-wire-v1/README.md)'s, its header and envelope
[blob and digests](profile-wire-v1/blob-and-digests.md)'s; how a save or an
Apply reaches both halves is the
[logical profile transaction](logical-profile-transaction-v1.md)'s and the
[split protocol](profile-split-v1.md)'s; the working copy and Save are
[live edits](profile-wire-v1/live-edits.md)'; the complete backup is the
[portable profile](portable-profile-v1.md); RAM per half is the
[resource policy](resource-policy.md)'s.

## Authority

A half runs one active profile: its working copy when it has unsaved edits,
otherwise its saved profile, otherwise its compiled defaults. A client writes
the profile only through Profile Wire; VIA is not built (D-F31,
[`0031`](../adr/0031-via-is-not-supported.md)).

At boot a half runs the saved profile [boot selection](#boot-selection)
chooses; a working copy never survives a replug or a restart (D-F32,
[`0032`](../adr/0032-live-edits-working-copy-and-save.md);
[live edits](profile-wire-v1/live-edits.md#the-working-copy)). The halves then
compare their saved generations ([split protocol](profile-split-v1.md#authority)).

Until it activates the profile its boot settles on, a half runs its compiled
defaults: from reset, through boot selection, validating the selected record
and waiting until both halves hold the same saved profile, to its activation
at a safe boundary ([owner states](authority-state-table.md#owner-states)).
Meanwhile profile status reports the active profile pending, never the
compiled defaults, so a client cannot mistake a booting keyboard for one with
nothing saved (D-F61, [active kind](profile-wire-v1/hid-channel.md#active-kind) 3).

After the boot, a half runs its compiled defaults when:

- no slot holds a record it can use: both slots are empty, interrupted,
  corrupt, in an earlier format, or bound to another build's digests;
- its saved record's override flag (header byte 5 bit 1) is clear. Every
  candidate the firmware stages sets it.

A slot it cannot read, two different saved profiles of one generation, or a
selected record that fails validation stop the half instead of falling back
([a stopped keyboard](profile-wire-v1/live-edits.md#a-stopped-keyboard)).
Profile status reports which profile runs: state flags 0 and 1 and the
active kind ([HID channel](profile-wire-v1/hid-channel.md#state-flags)).

A newly saved profile replaces the active one only at a safe boundary, and
only when its generation is strictly newer than the active one and its
compiled-default and action-ABI digests are the build's.

A build without the live-profile owner (`NOAH_LIVE_PROFILE_OWNER=no`) only
discovers: it runs boot selection over a read-only view of its slots, reports
what it found, runs its compiled defaults and never saves.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, `test_empty_boot_live_commit_and_timeout`, `test_boot_status_is_pending_until_activation`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the pending kind through the boot), `tests/host/run_active_profile_provider_tests.sh` (`active_profile_provider_test.c`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`profile_candidate_store_backend_test.c`, `test_override_disabled_commit_activates_compiled_fallback`), `tests/host/run_profile_store_runtime_tests.sh` (`profile_store_runtime_test.c`, `test_qmk_adapter_is_region_bounded_and_read_only`, `test_old_compiled_default_record_falls_back`).

## Profile domains

A profile is up to seven domains, one per [domain registry](profile-wire-v1/blob-and-digests.md#domain-registry)
row: lighting, key behaviours, combos, settings, pointing slots, the keymap
and the macros (D-F30,
[`0030`](../adr/0030-keymap-and-macros-belong-to-the-profile.md)). The
keymap and macro domains are the only copy of the layout and the macros the
firmware keeps.

A build admits:

| Mask | Domains |
| --- | --- |
| Required | pointing (`0x50`); a profile without it is refused `MISSING_DOMAIN` |
| Allowed | the domains its compiled defaults hold, plus combos (`0x30`) in a build with combos and settings (`0x40`) in a build with saves |

A declared domain outside the allowed mask is refused `UNSUPPORTED_DOMAIN`; a
declared mask that differs from the domains the blob holds is
`DOMAIN_MASK_MISMATCH` ([errors](profile-wire-v1/errors.md#validation-results)).
An admitted profile may omit any other domain; the half then runs that
domain from its compiled defaults. Each domain's file says what its absence
means: [lighting](rgb-domain-v1.md),
[key behaviours](profile-wire-v1/key-behavior-domain.md#absent-domain-and-compiled-defaults),
[combos](profile-wire-v1/combo-domain.md),
[settings](profile-wire-v1/settings-domain.md#compiled-defaults),
[keymap](profile-wire-v1/keymap-domain.md#absent-domain-and-compiled-defaults),
[macros](profile-wire-v1/macro-domain.md#absent-domain-and-compiled-defaults).

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`, `test_identity_capacity_and_masks`), `tests/fixtures/profile_validator_v1.fixture`, `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `test_real_authored_profile`, the build's compatibility), `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`, compiled fallback).

## Current-format admission

The firmware accepts only the formats it writes (D-F10,
[`0010`](../adr/0010-firmware-accepts-only-current-formats.md)): Profile
Wire 2.0, profile schema 4.3, each domain at its registry version, and store
format 6 (D-F28, [`0028`](../adr/0028-new-base-32-bit-profile-lengths.md)).
Nothing in another format is translated:

- a candidate or backup in another schema or domain version is refused
  ([blob header](profile-wire-v1/blob-and-digests.md#blob-header),
  [domain envelope](profile-wire-v1/blob-and-digests.md#domain-envelope));
- a slot in an earlier store format is never read past its header or
  converted ([boot selection](#boot-selection)). While no slot holds a usable
  record and one holds an earlier format, profile status sets state flag 9;
  the flag clears once a profile is saved.

A saved record is bound to the build that wrote it by its compiled-default
and action-ABI digests. The action-ABI digest includes the firmware version
and the layer count ([action-ABI digest](profile-wire-v1/blob-and-digests.md#action-abi-digest)),
and the compiled-default digest every byte of the compiled defaults, so after
a reflash that changes either, the saved profile is not used and the half
runs its compiled defaults until a client restores one (D-F12,
[`0012`](../adr/0012-one-complete-compiled-profile.md)). The slots survive a
UF2 update, which writes only the firmware image.

Every candidate a client sends, and every live edit, is held to where its
actions are placed; the saved profile adopted at boot, the other half's copy
and the compiled defaults are not
([placement](profile-wire-v1/blob-and-digests.md#where-an-action-may-be-placed)).

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_previous_layout_store_header_is_refused`, `test_retired_store_headers_never_become_authority`, `test_boot_rejects_old_compiled_default_identity`), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`, `test_blob_and_domain_rejections`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, flag 9), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`tests/fixtures/client-regression/`).

## Compiled defaults

The compiled defaults are one complete profile built from the authored
profile (D-F12): every registry domain whose encoder emits bytes, in registry
order, behind an eight-byte blob header. A domain's length is exactly what
its encoder emits; an encoder that emits nothing leaves its domain out of
that build's compiled defaults. Each registry row names the module whose
encoder writes it, so a row without one does not compile (D-F13,
[`0013`](../adr/0013-registry-row-names-its-domain-module.md)). The authored
macros are already in the macro bank encoding (`authored_macro_bank.h`), so
writing the compiled defaults compiles no macro.

Opening the compiled defaults encodes them once without keeping their bytes,
computing:

| Field | Meaning |
| --- | --- |
| byte length | the whole blob, at most 130,048 bytes |
| CRC32, payload digest | over the exact blob; the payload digest is the compiled-default digest |
| action-ABI digest | the build's |
| domain mask | the domains present |
| seven offset/length pairs | each domain's payload in the blob, 32 bits each, zero when absent |

The handle is that metadata and the seven pairs, at most 76 bytes; no
profile-sized buffer holds the compiled defaults.

The compiled reader serves any byte range of the blob from the handle: it
emits the blob header, the envelopes and payloads of the domains the range
intersects, and replays only those domains' encoders, never a preceding
domain's. A range may cross any header or domain seam; a range past the end
is refused. Runtime caches are filled from it when a profile becomes active,
so key events and lighting frames never run it. Readback of the compiled
defaults is GET `0x05` ([HID channel](profile-wire-v1/hid-channel.md#payload-readback--get-0x04-and-0x05)).

At boot the live-profile owner validates the compiled defaults with the
whole-profile validator, one bounded step a scan, before it reads a slot; if
they do not validate the owner stops in `COMPILED_ERROR`.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `test_real_authored_profile`: registry order, the 76-byte handle, byte-granular reads across every seam, one domain encoder per domain read), `tests/fixtures/compiled_profile_pd_v2.fixture`, `tests/fixtures/folded_keyboard_profile.json`, `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, `boot_empty`; `profile_owner_stopped_test.inc`, `test_unreadable_compiled_defaults_stop_the_owner`).

## Where the profile is stored

Each half keeps its saved profile in two profile slots, A and B, in a fixed
256 KiB region of its own flash, outside QMK's emulated EEPROM (D-F29,
[`0029`](../adr/0029-profile-slots-in-their-own-flash-region.md)). A slot is
read in place through XIP; no RAM copy of a slot exists. The build assumes
2 MiB of flash.

| Flash offset | Region offset | Bytes | Holds |
| --- | --- | ---: | --- |
| `0x000000`–image end | — | below 1 MiB | firmware image |
| `0x100000`–`0x11FFFF` | `0x00000`–`0x1FFFF` | 131,072 | profile slot A |
| `0x120000`–`0x13FFFF` | `0x20000`–`0x3FFFF` | 131,072 | profile slot B |
| `0x1FE000`–`0x1FFFFF` | — | 8,192 | QMK's wear-leveling backing |

The link fails if the firmware image reaches `0x10100000`, the region's XIP
address (`users/noah/lib/compat/qmk_profile_flash.ld`), and the flash adapter
does not compile if the region reaches the wear-leveling backing. Both slots
start on a 4 KiB erase sector. Storage addresses are 32-bit offsets into the
region.

Held by: `tests/host/run_profile_storage_layout_tests.sh` (`profile_storage_layout_test.c`, and the linker guard check), `tests/host/run_profile_flash_store_tests.sh` (`profile_flash_store_test.c`, `test_save_paces_and_reports_its_pauses`: read in place after a reboot).

### Logical EEPROM

QMK's logical EEPROM holds only QMK's eeconfig and its datablocks (D-F31): the
wear-leveling backing is QMK's RP2040 default of 8,192 bytes, exposing 4,096
bytes of logical EEPROM, mirrored in RAM. It leaves room for a user datablock
of at least 256 bytes beyond eeconfig. The user datablock is 32 bytes and
holds the [edit mode](profile-wire-v1/live-edits.md#edit-mode), outside
every profile. No profile byte lives in EEPROM.

Held by: `tests/host/run_profile_storage_layout_tests.sh` (compile guards: logical EEPROM size, room for a user datablock).

## The slot record

A slot is a record and its payload. In storage addresses a slot is its
36-byte header, whose last byte is the marker, then the canonical blob from
byte 36. Integers are little-endian.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | domain mask, one bit per registry row; bit 7 zero |
| 1 | 4 | payload length, uint32, `8..130,048` |
| 5 | 1 | flags: bit 0 origin half, bit 1 override; other bits zero |
| 6 | 4 | generation, nonzero |
| 10 | 4 | payload CRC32 |
| 14 | 4 | compiled-default digest |
| 18 | 4 | action-ABI digest |
| 22 | 11 | reserved, zero |
| 33 | 2 | CRC16 |
| 35 | 1 | marker |

The format is keyed into the header, which has no magic: the CRC16 is
CRC-16/CCITT (polynomial `0x1021`, initial `0xFFFF`, not reflected, no final
XOR) over the store format byte `6` followed by bytes 0–32. The schema is
implied by the format: the firmware's own, 4.3. A record an earlier firmware
wrote under format 6 with schema 4.0, 4.1 or 4.2 holds earlier domain versions, so
its validation refuses it (D-F62, D-F26); the payload digest is not stored and is computed
from the payload whenever it is read whole. The origin half is the physical
half that made the generation ([split protocol](profile-split-v1.md#physical-origin)).

| Marker | Meaning |
| ---: | --- |
| `0xFF` | erased: nothing written |
| `0x3C` | prepared: complete and checked, waiting for the decision; never selected at boot |
| `0x18` | committed: the saved profile |
| `0x00` | invalid: an abandoned save |

Each marker value is a bit subset of the one before it, so on NOR flash each
step only clears bits.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_header_reserved_bytes_are_zero`, `test_pd_header_all_identity_bits_and_boot_paths`, `test_payload_over_64_kib_round_trips`).

### The record on flash

On flash no 256-byte page is programmed twice between erases, so the backend
lays each slot's first kilobyte out by page:

| Slot flash offset | Bytes | Holds | Programmed |
| --- | ---: | --- | --- |
| `0x000` | 256 | header bytes 0–34 | once, at the header write |
| `0x100`, `0x200`, `0x300` | 1 each, a page each | the marker's three cells | one cell per marker step, in order |
| `0x400` | to the slot's end | payload | each page once, in order |

The marker reads as the bitwise AND of its three cells. The prepared step
programs `0x3C` into the first cell and the decision `0x18` into the second;
an abort programs `0x00` into the next unused one. Since each value is a bit
subset of the one before, the AND is the last value written, and a
half-programmed cell only clears more bits, which reads as no marker a save
writes. The header and every cell lie in the slot's first sector, so erasing
that sector clears the record at once. A slot holds at most 130,048 payload
bytes: 128 KiB less this first kilobyte.

Held by: `tests/host/run_profile_flash_store_tests.sh` (`profile_flash_store_test.c`, `test_marker_steps_each_program_their_own_cell`, `test_save_paces_and_reports_its_pauses`), `tests/host/run_profile_storage_layout_tests.sh`.

## Writing a slot

A save writes only the slot not holding the saved record (slot A when
nothing is saved), so the previous saved profile stays whole until the
new one's marker is committed. The store admits a candidate only when:

- boot selection has run, found no generation conflict, and no final marker
  waits to be confirmed;
- no other save is open;
- it is format 6, schema 4.3, `8..130,048` bytes, with only known domain-mask
  and flag bits and origin 0 or 1;
- its action-ABI and compiled-default digests are the build's;
- its generation is strictly greater than the saved one.

Then, in order:

1. The target slot leaves boot selection before anything is written to it:
   its header sector is erased before the first write (on byte-writable test
   media its marker is set to `0x00`).
2. The payload is written in order, each write at most 32 bytes at the next
   offset.
3. The payload's CRC32 and payload digest, accumulated while writing, must
   equal the candidate's.
4. Header bytes 0–34 are written and synced, then read back and compared.
5. The payload is read back, up to a page a step, and its CRC32 and digest
   checked again; its envelope is walked and its domains must equal the
   header's domain mask.
6. The prepared marker is written, synced and read back. The slot is now
   durably prepared and may still be aborted.
7. On the decision, the commit marker is written, synced and read back.

A failure before step 7 ends the save and leaves the previous profile
active. A write, sync or read-back failure in step 7 is
`DURABILITY_UNKNOWN`: the store admits no new save until a boot scan reads
the marker. An abort before step 7 writes `0x00` into the marker and syncs;
once step 7 has begun an abort is refused. Who decides and when is the
[logical profile transaction](logical-profile-transaction-v1.md)'s.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_commit_and_boot_selection`, `test_chunk_and_candidate_guards`, `test_payload_readback_reads_a_page_a_step`, `test_durable_prepare_is_not_authority_until_decided`, `test_durable_prepare_can_be_aborted_before_decision`, `test_checksum_mismatch_never_commits`, `test_final_marker_io_failure_is_durability_unknown`, `test_pd_header_rejects_bad_contracts_before_writing`, `test_power_loss_preserves_last_known_good`), `tests/host/run_profile_flash_store_tests.sh` (`profile_flash_store_test.c`, `test_power_loss_at_every_flash_operation`, `test_abort_invalidates_the_target`, `test_abort_before_any_erase_touches_nothing`).

## Boot selection

Boot selection reads each slot's header, then its payload, then its envelope,
one bounded read a step, and records what it found in each slot. A slot's
header is refused, first match wins:

| Header | Slot result |
| --- | --- |
| bytes 0–1 `NT` and byte 31 `0xA5` or `0x5A` (store format 5) | `EARLIER_FORMAT` |
| marker not `0x18` | `INVALID_HEADER` |
| bytes 31–32 `0xFF` and a valid format-keyed CRC16 at bytes 29–30 over bytes 0–28 (the 32-byte format-6 layout of earlier test builds) | `EARLIER_FORMAT` |
| CRC16 wrong | `CHECKSUM_MISMATCH` |
| any of bytes 22–29 nonzero (a test build's VIA binding) | `EARLIER_FORMAT` |
| any of bytes 30–32 nonzero, an unknown domain-mask or flag bit, a length outside `8..130,048`, or generation zero | `INVALID_HEADER` |
| action-ABI digest not the build's | `INCOMPATIBLE_ACTION_ABI` |
| compiled-default digest not the build's | `INCOMPATIBLE_COMPILED_DEFAULT` |

A header that passes has its payload read: a CRC32 other than the header's is
`CHECKSUM_MISMATCH`, and an envelope that does not walk, or whose domains
differ from the header's mask, is `INVALID_PAYLOAD`. Then:

| Slots | Result |
| --- | --- |
| a read failed in either slot | I/O error: the half stops |
| neither usable | no saved profile: compiled defaults |
| one usable | that slot |
| both usable | the higher generation; equal generations naming the same profile select B; equal generations that differ are a generation conflict and the half stops |

The selected record is then validated whole, the semantic checks included,
before it becomes active; placement is not checked. Before USB starts, a half
also runs boot selection once over a read-only view to read the
[keyboard name](profile-wire-v1/settings-domain.md#keyboard-name) of the
profile it will boot.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_commit_and_boot_selection`, `test_payload_and_header_validation`, `test_retired_store_headers_never_become_authority`, `test_equal_generation_metadata_divergence_conflicts`, `test_io_failures_are_not_treated_as_empty`, `test_first_commit_power_loss_falls_back_to_compiled_defaults`, `test_boot_selection_state_machine_is_scan_bounded`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`profile_candidate_store_backend_test.c`, adoption without placement), `tests/host/run_profile_store_runtime_tests.sh` (`profile_store_runtime_test.c`, `test_boot_keyboard_name_before_the_runtime_starts`, `test_equal_generation_divergence_is_reported`).

## Generations

Generations are unsigned 32-bit and strictly increase. The store's next
generation is the saved one plus one, or 1 when nothing is saved.
Zero is never a generation, and `UINT32_MAX` is terminal: the store offers no
next generation after it and never wraps. A store in a generation conflict
offers none either.

Held by: `tests/host/run_profile_store_tests.sh` (`profile_store_test.c`, `test_generation_conflict_and_rollover`).

## Flash pauses

Erasing and programming flash pause the keyboard: they run with interrupts
off and XIP off, through pico-sdk's RAM-resident `flash_range_erase` and
`flash_range_program`, as QMK's wear-leveling driver does. A sector is 4 KiB
and erases to `0xFF`; a page is 256 bytes and only clears bits.

Every flash operation of the profile store is reached from a durable-I/O
grant, which runs once per matrix scan on both halves and never from Raw HID
or a split callback. A grant, in order:

1. erases one sector, only if a save has fewer than 4,096 erased bytes ahead
   of its writes and the keyboard is quiet: no key held and the matrix
   unchanged for 50 ms (`NOAH_QMK_PROFILE_FLASH_QUIET_MS`). The master half's
   matrix holds both halves' rows; the slave half checks its own;
2. lets the store's writer run only while at least a page of erased flash
   lies ahead of its writes and the grant has used under 2,000 µs of flash
   time (`NOAH_PROFILE_FLASH_GRANT_BUDGET_US`), which an erase always
   exceeds; so an erase never shares a grant with the writer;
3. repeats the store step while it has local work in progress, stopping
   after a step that erased or programmed, used the split link or found
   nothing to do, after 32 steps (`NOAH_QMK_DURABLE_IO_STEPS_MAX`), or after
   1,000 µs (`NOAH_QMK_DURABLE_IO_STEP_BUDGET_US`).

So a grant erases at most one sector or programs at most one page, never
both. One step writes at most a page in order: a share of a reuse range, a
split chunk, or one record write. Writes collect in one 256-byte page buffer
and reach flash as one page program when the page fills, when a write moves
to another page, or at a sync; reads merge the buffered page, and bytes of
the save's range not yet erased read as `0xFF`. The store syncs after the
header, after the prepared marker and after the commit marker. A write that
would reach unerased flash erases first. A save's first step erases nothing:
until the header sector is erased the slot keeps what it held, and every
write waits for that erase.

A save therefore waits while the keyboard is busy. Grants in which the
writer may not run are left out of the owner's no-progress clocks, and a
half waiting this way answers busy reason 9, `STORAGE_PAUSED`
([errors](profile-wire-v1/errors.md#busy-reasons)). Each half measures its
own pauses ([GET `0x0D`](profile-wire-v1/hid-channel.md#flash-pauses--get-0x0d)).

Held by: `tests/host/run_qmk_durable_io_tests.sh` (`qmk_durable_io_test.c`), `tests/host/run_qmk_profile_flash_tests.sh` (`qmk_profile_flash_test.c`, `test_erases_wait_for_a_quiet_keyboard`, `test_idle_grants_do_not_inspect_the_matrix`), `tests/host/run_profile_flash_store_tests.sh` (`profile_flash_store_test.c`, `test_reset_takes_the_slot_and_erases_ahead_of_writes`, `test_writes_never_land_on_unerased_flash`, `test_save_paces_and_reports_its_pauses`).

## EEPROM reset

An EEPROM reset forgets the saved profiles. QMK formats logical EEPROM at
boot exactly when its magic is not valid, and makes the magic valid early in
formatting. So in `keyboard_pre_init`, before QMK's keyboard init and before
anything reads a slot, a half whose EEPROM magic is not valid erases both
slots' header sectors. Until both erases are done the magic stays invalid,
so a boot interrupted anywhere in between forgets them again. The half then
boots its compiled defaults.

Held by: `tests/host/run_qmk_profile_flash_tests.sh` (`qmk_profile_flash_test.c`, `test_eeprom_reset_forgets_the_saved_profiles`), `tests/host/run_profile_store_runtime_tests.sh` (`profile_store_runtime_test.c`, `test_eeprom_reset_forgets_the_saved_profile_before_start`).

## Bounded work

Storage and validation work is bounded per step, and an idle keyboard pays
almost none of it:

- a grant with no store work does no flash work and does not inspect the
  matrix;
- every store read is at most 32 bytes, and boot selection and adoption make
  at most one read a step;
- the whole-profile validator makes at most one read of at most 20 bytes a
  step;
- runtimes read decoded views or RAM copies made when a profile becomes
  active, never the compiled reader, on key events and lighting frames.

Held by: `tests/host/run_qmk_durable_io_tests.sh` (`qmk_durable_io_test.c`, `test_an_idle_store_gets_one_grant_and_one_step_per_scan`), `tests/host/run_qmk_profile_flash_tests.sh` (`test_idle_grants_do_not_inspect_the_matrix`), `tests/host/run_profile_store_tests.sh` (`test_commit_state_machine_is_scan_bounded`, `test_boot_selection_state_machine_is_scan_bounded`, `test_read_span_reads_within_the_io_bound`), `tests/host/run_profile_validator_work_budget_tests.sh` (`profile_validator_work_budget_test.c`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, `boot_empty`), `tests/host/run_profile_keymap_macros_runtime_tests.sh` (`profile_keymap_macros_runtime_test.c`).

## Ceilings and the maximum profile

Every ceiling holds at once (D-F14, [`0014`](../adr/0014-bigger-profiles.md)):

| Surface | Ceiling |
| --- | ---: |
| Layers | 16 |
| Behaviours / branches per behaviour / branches in all | 128 / tap depth `d` (5) / `128 × d` (640) |
| Combos / inputs per combo | 128 / 2–16 |
| Macros / macro bank | 128 / 34,903 bytes |
| Custom keys | 128 |
| Names | 32 bytes of UTF-8 each |
| Reusable lighting groups / stage-group rows | 16 / 32 |
| Physical LEDs / LED bitmap | 58 / 8 bytes |
| Pointing slots | 32 |
| Key-behaviour and lighting domains | 65,535 bytes each (16-bit internal offsets) |
| Profile payload | 130,048 bytes |

`tests/fixtures/maximum_profile_v4.fixture`, written by
`tests/host/make_maximum_profile.py` from the byte specs, holds every table
at its ceiling and every name at 32 bytes, in 75,825 bytes:

| Domain | Bytes |
| --- | ---: |
| Lighting `0x10` | 591 |
| Key behaviours `0x20` | 13,828 |
| Combos `0x30` | 9,736 |
| Settings `0x40` | 10,693 |
| Pointing `0x50` | 4,104 |
| Keymap `0x60` | 1,920 |
| Macros `0x70` | 34,903 |
| Blob header and seven envelopes | 50 |

The generator reads the payload ceiling from the storage layout and refuses a
fixture that does not leave 12,288 bytes below it, the reserve for later
additions. The fixture is the maximum for these encodings, not a separate
limit: a larger profile is refused by a table's ceiling or the payload
ceiling. The firmware saves, boots, publishes and copies it to the other
half, and refuses over-limit and truncated copies of it.

Held by: `tests/host/run_profile_storage_layout_tests.sh` (`profile_storage_layout_test.c`), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`profile_domain_owner_round_trip_test.c`, `test_maximum_profile`, `test_maximum_save_interrupted`; `make_maximum_profile.py --check`), `tests/fixtures/maximum_profile_v4.fixture`, `tests/host/run_profile_split_reconciler_tests.sh`.

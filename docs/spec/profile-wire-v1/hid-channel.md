# Profile Wire HID channel

How a client's reports reach the keyboard and come back: the Raw HID report,
the read envelope every GET shares, the capability, profile status, payload,
host OS and flash-pause pages, and the complete map of value ids. The master
half answers every report. Candidate writes and their status are
[candidate-transfer.md](candidate-transfer.md); live edits, Save, Revert, the
edit mode and Restart are [live-edits.md](live-edits.md); status codes and
every named result are [errors.md](errors.md); the profile bytes the payload
pages carry are [blob-and-digests.md](blob-and-digests.md).

## Raw HID reports

The firmware answers Raw HID itself (D-F31,
[`0031`](../../adr/0031-via-is-not-supported.md)): QMK's Raw HID interface,
usage page `0xFF60`, 32-byte reports both ways. Every report goes to the
Profile Wire handlers and is sent back exactly once, in place, as the reply,
with the same length.

| Byte | Field |
| ---: | --- |
| 0 | command: `0x08` get, `0x07` set, `0x09` save |
| 1 | channel, `0x00` for every Profile Wire value |
| 2 | value id |
| 3… | the value's own fields |

A report no handler takes is answered with command byte `0xFF` and every
other byte unchanged. That covers any other command or channel, any value id
the build does not route ([value ids](#value-ids)), and any report that is
not 32 bytes. An empty report is not answered. A client always sends and
receives exactly 32 bytes.

Every report, handled or not, counts as host activity: the USB re-enumeration
that publishes a new keyboard name waits until no report has arrived for one
second ([settings-domain.md](settings-domain.md)).

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the unhandled and retired-command cases), `tests/host/run_qmk_contract_checks.sh` (QMK's `RAW_EPSIZE` is 32), `tests/host/run_qmk_usb_product_name_tests.sh` (`qmk_usb_product_name_test.c`, host activity).

## Read envelope

Every GET uses this request. A value reads either a byte page or a wide page:

| Offset | Size | Byte page | Wide page |
| ---: | ---: | --- | --- |
| 0 | 1 | `0x08` | `0x08` |
| 1 | 1 | `0x00` | `0x00` |
| 2 | 1 | value id | value id |
| 3 | 1 | request id, nonzero | request id, nonzero |
| 4 | 1 | page | page, low byte |
| 5 | 1 | reserved, zero | page, high byte |
| 6 | 26 | reserved, zero | reserved, zero |

Wide pages are taken by GET `0x04`, `0x05`, `0x06`, `0x07` and `0x21`; every
other GET takes a byte page. The reply echoes bytes 0–4 and replaces the rest:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 5 | command, channel, value, request id and page (its low byte), echoed |
| 5 | 1 | status: `0` OK, `1` malformed, `2` unknown page, `3` unavailable ([errors.md](errors.md#read-status)) |
| 6 | 1 | payload length, `0..25`; `0` unless the status is OK |
| 7 | 25 | payload, then zero padding |

A request id of zero or a nonzero reserved byte answers status `1`. A wide
page's high byte is not echoed, so the request id correlates the reply. A
client matches a reply on all five echoed bytes, not on the command alone.
Capability and status pages always carry exactly 25 payload bytes.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, the malformed-request test; `qmk_profile_channel_test.c`, the wide-page payload test), `tests/fixtures/profile_wire_v1_reads.fixture`.

## Capability pages — GET `0x01`

A client reads these before anything else. Three byte pages, `0..2`; page 3
and above answer status `2`.

Page 0, the identity (see [README.md](README.md#version-identity)):

| Byte | Field |
| ---: | --- |
| 0 | capability layout, `3` |
| 1 | capability page count, `3` |
| 2–3 | Profile Wire major `2`, minor `0` |
| 4–5 | schema major `4`, minor `2` |
| 6 | report size, `32` |
| 7 | candidate chunk capacity: `20` when feature bit 1 is set, otherwise `0` |
| 8 | profile status page count, `2` |
| 9–12 | feature word, below |
| 13–16 | action-ABI digest; `0` until the live-profile owner reports |
| 17–20 | firmware version, never `0` |
| 21–24 | compiled-default digest; `0` until the owner reports |

Page 1, every ceiling that fits a byte:

| Byte | Field | This firmware |
| ---: | --- | --- |
| 0 | layers in the compiled bank | 16 |
| 1 | maximum layers | 16 |
| 2 | maximum behaviours | 128 |
| 3 | tap depth: branches per behaviour | 5, or the build's `KEY_BEHAVIOR_MAX_TAP_COUNT` |
| 4 | maximum combos | 128 |
| 5 | maximum inputs per combo | 16 |
| 6 | maximum reusable lighting groups | 16 |
| 7 | maximum lighting stage-group rows | 32 |
| 8 | physical LEDs | 58 with RGB Matrix, otherwise 0 |
| 9 | LED bitmap bytes | 8 |
| 10 | custom keys | 128 |
| 11 | macro slots | 128 |
| 12 | supported domain mask ([registry](blob-and-digests.md#domain-registry)); `0` until the owner reports | |
| 13 | maximum encoded bytes in a name | 32 |
| 14 | layer-mask width in bits | 32 |
| 15 | placement positions per layer: matrix rows × columns | |
| 16–24 | zero | |

Page 2, the wider ceilings; every byte count is 32 bits:

| Byte | Field | This firmware |
| ---: | --- | --- |
| 0–1 | maximum branches in the whole key-behaviour domain | 128 × tap depth |
| 2–5 | profile slot size in bytes | 131,072 |
| 6–9 | maximum profile payload | 130,048 |
| 10–13 | slot payload capacity | 130,048 |
| 14–17 | macro domain capacity in bytes | 34,903 |
| 18–24 | zero | |

The values come from one header, `users/noah/lib/compat/qmk_profile_capabilities.h`,
which the Raw HID channel and the contract probe (`tests/host/run_contract_probe.sh`)
both build from.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, the capability-page test; `qmk_profile_channel_test.c`, the capability hook in every build variant), `tests/fixtures/profile_wire_v1_reads.fixture`.

## The feature word

Capability page 0 bytes 9–12. It holds only what a build varies by; no
firmware behaviour depends on a bit, the word only describes the build (D-F28,
[`0028`](../../adr/0028-new-base-32-bit-profile-lengths.md)).

| Bit | Meaning |
| ---: | --- |
| 0 | split keyboard: this build is one half of a pair; a save copies the profile to the slave half and status reports it |
| 1 | saves: candidate writes with reuse and streaming chunks, durable commit and activation, live-edit writes, and settings, editor and host-OS reads (GET `0x07`, `0x08`, `0x0B`). Set once the live-profile owner reports, in a build that saves |
| 2 | pointing device: pointing keys and the pointing domain mean something on this build |
| 3–31 | zero |

Before the live-profile owner reports, the word holds bits 0 and 2 as built,
the chunk capacity, digests and supported domain mask are zero, and bit 1 is
clear. Each capability read takes the owner's current figures.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, the bit values; `qmk_profile_channel_test.c`, bit 1 against the chunk capacity and no unknown bit in any build variant).

## Profile status — GET `0x02`

Two byte pages; page 2 and above answer status `2`. Reading page 0 takes a
fresh snapshot of the half's profile state; page 1 reports the snapshot the
most recent page-0 read took. A client reads page 0, then page 1.

Page 0:

| Byte | Field |
| ---: | --- |
| 0 | status layout, `1` |
| 1 | status page count, `2` |
| 2–3 | state flags, below |
| 4–7 | source digest: the compiled-default digest |
| 8–11 | compiled-default digest |
| 12–15 | active digest: the active profile's payload digest; for the working copy, its revision |
| 16–19 | pending digest: a generation waiting to activate |
| 20–23 | saved-profile digest |
| 24 | active kind, below |

Page 1:

| Byte | Field |
| ---: | --- |
| 0–3 | active generation; for the working copy, its saved base's (`0` over the compiled defaults) |
| 4 | active origin half: `0` left, `1` right; `255` for a working copy over the compiled defaults |
| 5–8 | saved generation |
| 9 | saved generation's origin half |
| 10–13 | the slave half's saved generation |
| 14 | its origin half |
| 15–16 | candidate transaction id |
| 17–18 | last committed transaction id |
| 19–20 | conflict count: `1` while a generation conflict or concurrent commit is unresolved, otherwise `0` |
| 21 | candidate state ([candidate-transfer.md](candidate-transfer.md)) |
| 22 | last candidate error id ([errors.md](errors.md#candidate-error-ids)) |
| 23–24 | zero |

### State flags

| Bit | Flag | Meaning | Defined by |
| ---: | --- | --- | --- |
| 0 | active is compiled defaults | the keyboard runs its compiled defaults | [device-resident-profile.md](../device-resident-profile.md) |
| 1 | saved profile valid | this half holds a saved profile it can use | [device-resident-profile.md](../device-resident-profile.md) |
| 2 | candidate pending | an Apply's candidate is open, or a saved generation waits to activate | [candidate-transfer.md](candidate-transfer.md) |
| 3 | preview active | never set | |
| 4 | peer known | this half knows the slave half's saved state | [profile-split-v1.md](../profile-split-v1.md) |
| 5 | peer converged | both halves hold the same saved profile | [authority-state-table.md](../authority-state-table.md) |
| 6 | waiting for a safe boundary | a pending generation waits for a safe boundary to activate | [logical-profile-transaction-v1.md](../logical-profile-transaction-v1.md) |
| 7 | digests unavailable | the active digest and generation are not reported yet; zero is not a digest | this file |
| 8 | peer cleanup pending | a cancelled save's abort was never acknowledged by the slave half; no new save starts until it is, or a restart | [authority-state-table.md](../authority-state-table.md) |
| 9 | stored earlier format | a profile slot holds an earlier store format, never read or converted; the keyboard runs its compiled defaults, and the flag clears once a profile is saved (D-F28) | [device-resident-profile.md](../device-resident-profile.md) |
| 10–15 | zero | | |

The live-edit status has its own flag word (GET `0x20`,
[live-edits.md](live-edits.md)).

### Active kind

| Value | Active profile |
| ---: | --- |
| 0 | the compiled defaults; flag 0 is set |
| 1 | the saved profile |
| 2 | never reported |
| 3 | pending: the keyboard has not yet decided which profile runs. Flag 7 is set, flag 0 clear, and the active digest and generation are zero |
| 4 | the working copy: unsaved edits over the saved generation page 1 names ([live-edits.md](live-edits.md)) |

A half reports pending from reset until it activates the profile its boot
settles on (D-F61): while it validates its compiled defaults, selects and
validates its saved profile, waits until both halves hold the same saved
profile, and activates it. It runs its compiled defaults meanwhile
([owner states](../authority-state-table.md#owner-states)), and the last error
is `0`. Flags 1, 4 and 5 and the saved generation say what the boot has found
so far. A boot that finds nothing usable saved reports the compiled defaults
(kind 0) once it has decided; one waiting for a slave half that never answers
stays pending. Outside the boot, pending means the active profile's pointing
slots were never published: the half stopped, and the last error is `17`.

A build without the live-profile owner reports the compiled defaults with
flags 0 and 7 always set, flag 1 and the saved generation when its store
found a saved profile, and conflict count `1` on a generation conflict.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, the status-page test; `qmk_profile_channel_test.c`, the read-only and owner snapshots, page 1 paired with page 0, the working copy, the pending kind through the boot and after a failed activation, and flag 9), `tests/fixtures/profile_wire_v1_reads.fixture`, `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, `test_boot_status_is_pending_until_activation`).

## Payload readback — GET `0x04` and `0x05`

GET `0x04` reads this half's saved profile; GET `0x05` the compiled defaults
the firmware was built with. Both take a wide page: 16 bits of page reach any
payload up to 130,048 bytes (5,203 pages). Page 0 is metadata; page `n ≥ 1`
carries payload bytes from `(n − 1) × 25`, at most 25, the payload length byte
giving the count. A page past the end answers status `2`. Status `3` means
nothing is saved (GET `0x04`), the compiled defaults cannot be opened (GET
`0x05`), or a page's bytes could not be read.

Page 0:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `2` |
| 1 | 1 | bytes per page, `25` |
| 2 | 4 | payload length, uint32 |
| 6 | 4 | generation; `0` for the compiled defaults |
| 10 | 4 | payload digest (FNV-1a) |
| 14 | 4 | CRC32 of the payload |
| 18 | 1 | schema major |
| 19 | 1 | schema minor |
| 20 | 1 | domain mask |
| 21 | 1 | origin half; GET `0x04` only, otherwise `0` |
| 22 | 1 | record flags; GET `0x04` only, otherwise `0` |
| 23 | 2 | zero |

Pages carry no sequence number. A generation only increases, so a client that
reads page 0 again after the data pages and finds the same generation and
digest knows nothing was saved in between. The working copy is read with GET
`0x21` ([live-edits.md](live-edits.md)).

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the payload and compiled read tests).

## Host OS — GET `0x0B`

Present in builds that save (feature bit 1). Byte page `0` only; any other
page answers status `2`. The payload is two bytes:

| Byte | Field |
| ---: | --- |
| 0 | layout, `1` |
| 1 | detected host: `0` unknown, `1` macOS, `2` Windows, `3` Linux |

Unknown covers iOS, a host QMK cannot place, and a build without OS
detection. The value is QMK's current guess from USB, not the override the
settings domain stores (settings scalar 27, [settings-domain.md](settings-domain.md)),
and it may change after USB setup. It is volatile: no part of the profile, a
backup or a digest.

Held by: `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`), `tests/host/run_host_detection_tests.sh` (`host_detection_test.c`, QMK's detection to these values).

## Flash pauses — GET `0x0D`

Each half measures how long its profile-slot flash work stops the keyboard
(D-F29, [`0029`](../../adr/0029-profile-slots-in-their-own-flash-region.md)).
Byte page `0` is the master half's figures; page `1` the slave half's, as the
master half last pulled them over the split link; page 2 and above answer
status `2`. Page 1 answers status `3` until a pull has succeeded.

A valid page-1 read asks for one refresh and answers at once with the last
pulled figures; Raw HID never waits for the split link. The master half pulls
from its housekeeping, coalescing requests, at most once a second, failures
included; a failed pull keeps the previous figures and is not retried without
another page-1 read. Page 0 and malformed reads ask for nothing, and with no
reads there is no diagnostic traffic. To refresh, read page 1, give
housekeeping time to pull, and read it again. Cached figures carry no
freshness guarantee.

Each page's payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | flags: bit 0 figures present |
| 1 | 1 | physical half: `0` left, `1` right, `0xFF` not provisioned |
| 2 | 4 | longest 4 KiB sector erase, µs |
| 6 | 4 | longest 256-byte page program, µs |
| 10 | 4 | most flash time in one scan's storage step, µs |
| 14 | 4 | the last save on this half, from its start to its last sync, µs |
| 18 | 4 | flash time inside that save, µs |
| 22 | 2 | saves since boot |
| 24 | 1 | zero |

Every figure counts from boot. Interrupts are off during each erase and
program, so a pause is time the half did nothing else. A save's duration
includes the host's transfer and any wait for a quiet keyboard before an
erase. The figures are diagnostics: no part of the profile, a backup or a
digest.

Held by: `tests/host/run_qmk_profile_flash_tests.sh` (`qmk_profile_flash_test.c`, page 0, request checks, the pull and its rate limit), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, routing).

## Diagnostic values

GET `0x03` is the cadence recorder, answered only by builds with
`NOAH_PROFILE_PERFORMANCE_DIAGNOSTICS_ENABLE`
([split-activity-sync.md](../split-activity-sync.md#cadence-recorder)).
GET and SET `0x0A` are the bounded split-transaction capture, answered only by
builds with `SPLIT_TRANSACTION_DIAGNOSTICS`
([split-activity-sync.md](../split-activity-sync.md#split-transaction-recorder)). Every other build
answers them unhandled.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, the cadence route), `tests/host/run_split_activity_tests.sh` (`split_diagnostics_test.c`).

## Value ids

Every value the firmware routes on channel `0x00`. *Builds* says which builds
answer it: *all* (every build with Raw HID), *owner* (with the live-profile
owner), *saves* (feature bit 1), or a diagnostic build. A value a build does
not route, and every value not listed, is answered unhandled (`0xFF`).

| Command | Value | Operation | Builds | Specified in |
| --- | ---: | --- | --- | --- |
| get | `0x01` | capability pages 0–2 | all | [this file](#capability-pages--get-0x01) |
| get | `0x02` | profile status pages 0–1 | all | [this file](#profile-status--get-0x02) |
| get | `0x03` | cadence recorder | diagnostic | [this file](#diagnostic-values) |
| get | `0x04` | saved payload | all | [this file](#payload-readback--get-0x04-and-0x05) |
| get | `0x05` | compiled-defaults payload | all | [this file](#payload-readback--get-0x04-and-0x05) |
| get | `0x06` | native combo readback | all | [combo-domain.md](combo-domain.md) |
| get | `0x07` | settings readback | saves | [settings-domain.md](settings-domain.md) |
| get | `0x08` | editor pages from 1; page 0 answers status `2` | saves | [settings-domain.md](settings-domain.md) |
| get | `0x09` | unhandled | — | |
| get, set | `0x0A` | split-transaction capture | diagnostic | [split-activity-sync.md](../split-activity-sync.md) |
| get | `0x0B` | detected host OS | saves | [this file](#host-os--get-0x0b) |
| get | `0x0C` | unhandled | — | |
| get | `0x0D` | flash pauses of each half | all | [this file](#flash-pauses--get-0x0d) |
| set | `0x10` | candidate begin | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x11` | candidate chunk | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x12` | candidate validate | saves | [candidate-transfer.md](candidate-transfer.md) |
| save | `0x13` | candidate commit | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x14` | candidate abort | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x15`–`0x17` | unhandled | — | |
| get | `0x18` | candidate status pages 0–3 | saves | [candidate-transfer.md](candidate-transfer.md) |
| get | `0x19` | unhandled | — | |
| set | `0x1A` | unhandled | — | |
| set | `0x1B` | candidate reuse | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x1C` | candidate streaming chunk | saves | [candidate-transfer.md](candidate-transfer.md) |
| set | `0x20` | edit begin | saves | [live-edits.md](live-edits.md) |
| get | `0x20` | edit status pages 0–3 | owner | [live-edits.md](live-edits.md) |
| set | `0x21` | edit splice | saves | [live-edits.md](live-edits.md) |
| get | `0x21` | the active profile (working copy or saved profile), wide pages | owner | [live-edits.md](live-edits.md) |
| set | `0x22` | edit data | saves | [live-edits.md](live-edits.md) |
| get | `0x22` | runtime overrides and QMK's live values | owner | [live-edits.md](live-edits.md) |
| set | `0x23` | edit commit | saves | [live-edits.md](live-edits.md) |
| set | `0x24` | edit abort | saves | [live-edits.md](live-edits.md) |
| set | `0x25` | Save the working copy | saves | [live-edits.md](live-edits.md) |
| set | `0x26` | Revert the working copy | saves | [live-edits.md](live-edits.md) |
| get, set | `0x27` | edit mode (SET in builds that save) | owner | [live-edits.md](live-edits.md) |
| set | `0x28` | Restart both halves | owner | [live-edits.md](live-edits.md) |

No value answers a protocol-version, firmware-version, keymap, macro or EEPROM
command from another protocol; every such report is unhandled.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, routing per build variant and the unhandled cases), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`, GET `0x09` and GET `0x08` page 0), `tests/host/run_qmk_profile_edit_channel_tests.sh` (`qmk_profile_edit_channel_test.c`).

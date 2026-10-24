# Live edits

How a client changes the working copy over Profile Wire so an edit runs on
both halves at once without a save, then saves it, reverts it, or has the
keyboard save it in Live; the edit mode stored beside the profile; keyboard
adjustments; and how a stopped keyboard says so and is restarted
([D-F32](../../adr/0032-live-edits-working-copy-and-save.md),
[D-F34](../../adr/0034-stopped-keyboard-says-so-and-restarts.md)). The
Raw HID framing and read envelope are in [the HID channel](hid-channel.md);
every result code is in [errors](errors.md); Save writes through
[candidate transfer](candidate-transfer.md). How the slave half follows the
working copy, the edit mode and a restart is
[the split protocol](../profile-split-v1.md).

## The working copy

Each half has one RAM buffer the size of a profile slot's payload (130,048
bytes) for its working copy. While nothing is edited the keyboard runs its
saved profile, or the compiled defaults when nothing is saved, and the
buffer is unused. The first edit copies the saved profile into it in bounded
steps; from that edit's publication until Save, Revert, an Apply or a
restart, the working copy is the active profile and the keyboard has
unsaved edits. Boot always runs the saved profile: unsaved edits never
survive a replug or a restart.

Only a build with the live-profile owner answers SET and GET `0x20`–`0x28`;
a build without it leaves them unhandled (command byte `0xFF`). A build with
the owner but without saves answers the GETs and SET `0x28` and leaves SET
`0x20`–`0x27` unhandled.

Held by: `tests/host/run_profile_working_copy_tests.sh` (`profile_working_copy_test.c`, `test_keymap_edit_publishes_in_place`), `tests/host/run_qmk_profile_edit_channel_tests.sh` (`qmk_profile_edit_channel_test.c`).

## Session and revision

The working copy is named by a session and a revision. The session is a
32-bit value chosen afresh at every boot. The revision starts at zero each
boot and advances by one with every change to the working copy's bytes: a
published edit (a client's or a keyboard adjustment's), a Revert that
publishes, and a profile activated from outside it (an Apply, or a newer
profile from the other half). A save of the working copy itself does not
advance it, since the saved bytes are the working copy's. The pair never
repeats within a boot: an edit that would take the revision past
`0xFFFFFFFF` is refused `CAPACITY_EXCEEDED`. A client that sees another
session knows the keyboard restarted and its unsaved edits are gone.

Every edit begin, Save and Revert names the session and revision it was
made against; a different pair is refused `STALE`, and the reply carries
the current one.

Held by: `tests/host/run_profile_working_copy_tests.sh` (`test_stale_and_refused_edits_change_nothing`, `test_replaced_by_a_save_or_apply`), `tests/host/run_profile_owner_tests.sh` (`test_an_apply_replaces_the_working_copy`).

## An edit

An edit is up to eight splices, applied together as one revision or not at
all. A splice names a domain by its id, an offset into that domain's
payload, a count of bytes removed there and a count inserted; the inserted
bytes of all splices arrive as one stream, in splice order. Splices are in
domain order (the registry's, [blob and digests](blob-and-digests.md)), then
in offset order. Two splices in one domain must not overlap, and a later
one starts strictly after the earlier one's offset and at or after its end.

A splice may instead, as the only splice of its domain:

- **create** an absent domain (flag bit 0): offset and removal `0`, the
  whole payload inserted;
- **drop** a present one (flag bit 1): offset `0`, removal its whole length,
  nothing inserted. A required domain cannot be dropped.

Both flags at once, or any other bit, is `INVALID_SPLICE`. The blob header
and every domain header are the firmware's to write, in canonical order;
their lengths follow from the splices.

A **replacement** (begin flag bit 0) has no splices: its stream is a whole
new profile of at least a blob header, written straight into the buffer. It
needs the saved profile active; over unsaved edits it is refused
`NOT_CLEAN`.

Held by: `tests/host/run_profile_working_copy_tests.sh` (`test_create_and_drop_move_later_domains`, `test_resizing_macro_edit_and_working_reads`, `test_replace_needs_the_saved_profile`, `test_stale_and_refused_edits_change_nothing`).

## Insert staging and capacity

The inserted stream is staged at the top of the buffer, above both the
current and the resulting profile, so a refused edit leaves every byte of
the working copy as it was and no second buffer exists. An edit therefore
fits when its inserted bytes plus the larger of the current and resulting
profile fit the buffer. Begin refuses `CAPACITY_EXCEEDED` when the inserted
bytes alone cannot fit above the current profile; commit refuses it when
the resulting profile exceeds the build's payload ceiling or the buffer, or
does not leave room for the staging above it. A replacement fits when its
stream fits the buffer and the payload ceiling.

Held by: `tests/host/run_profile_working_copy_tests.sh` (`test_resizing_macro_edit_and_working_reads`, `test_stale_and_refused_edits_change_nothing`).

## Checking and publishing

Commit checks that every splice and inserted byte arrived (else
`INVALID_STATE`, and the edit stays open) and that the CRC-32 of the stream
matches (else the edit ends `CHECKSUM_MISMATCH`). It then plans the result
against the current profile, ending the edit with `UNSUPPORTED_DOMAIN` (a
domain unknown or not allowed for a client), `INVALID_SPLICE` (order,
overlap, a range outside its domain, create or drop rules, inserted counts
not summing to the begin's total, a required domain missing from the
result) or `CAPACITY_EXCEEDED`. Otherwise it answers `QUEUED`, and the
outcome is the edit status's last result for that transaction.

Over later scans the first edit loads the saved profile (phase `2`), then
each domain the edit touches is checked alone, exactly as a whole-profile
validation checks it, action references and placements included, and held
to the rules a client's candidate is held to (phase `3`). No other domain is
read and nothing is checksummed. A replacement is checked whole, envelope
and every domain, still without a checksum. A refusal ends the edit
`VALIDATION_REJECTED` (edit status page 2 says where) or `READ_ERROR`, and
changes nothing.

A checked edit waits for a safe boundary, the same one an activation waits
for, while typing goes on (phase `4`). Its bytes are rewritten inside the
publication, then the runtimes that read the changed domains are rebuilt; a
domain the edit did not touch keeps its tables, and settings are projected
into QMK again only when the settings domain changed. A publication that
fails ends the edit `PUBLICATION_FAILED`. The master half then sends the
same edit to the slave half ([the split protocol](../profile-split-v1.md)).

Held by: `tests/host/run_profile_working_copy_tests.sh` (`test_every_domain_is_checked_alone`, `test_edit_waits_for_a_safe_boundary`, `test_keymap_edit_publishes_in_place`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`), `tests/host/run_profile_owner_tests.sh` (`test_live_edit_reaches_both_halves_and_save_writes_both`).

## SET reports

Command `0x07`, channel `0`, the value at byte 2, a nonzero transaction id
at bytes 3–4. Multi-byte fields are little-endian, and every byte after a
value's fields is zero. A zero transaction id or a nonzero trailing byte is
answered `INVALID_ARGUMENT` and reaches nothing.

| Value | Bytes 5… |
| --- | --- |
| `0x20` edit begin | 5–8 session, 9–12 revision, 13 splice count (`1`–`8`; `0` for a replacement), 14 flags (bit 0 replacement), 15–18 inserted bytes in total |
| `0x21` splice | 5 index (from `0`, in order), 6 domain id, 7 flags (bit 0 create, bit 1 drop), 8–11 offset, 12–15 bytes removed, 16–19 bytes inserted |
| `0x22` data | 5–8 offset in the inserted stream, 9 count (`1`–`22`), 10… the bytes |
| `0x23` commit | 5–8 CRC-32 of the whole inserted stream ([blob and digests](blob-and-digests.md)) |
| `0x24` abort | nothing |
| `0x25` save | 5–8 session, 9–12 revision |
| `0x26` revert | 5–8 session, 9–12 revision |
| `0x27` edit mode | 5 mode: `0` Apply, `1` Live, `2` Live + review |
| `0x28` restart | 5–8 the confirmation `52 45 53 54` ("REST") |

Splices and data arrive in order. A repeated splice or data report whose
reply was lost is accepted again when it matches what was taken (a data
report's bytes are compared); otherwise, and for a gap, `OUT_OF_ORDER`. A
splice index at or past the begin's count is `INVALID_ARGUMENT`; data
reaching past the begin's total is `CAPACITY_EXCEEDED`. A splice, data or
commit report answers `INVALID_STATE` with no edit open, `BUSY` while the
edit is past receiving, and `WRONG_TRANSACTION` for another transaction's
edit. Every one of the four answers `BUSY` while the keyboard's own
adjustment edit is open.

Abort ends the open edit of its transaction with last result `ABORTED` and
answers `OK`, at any phase before publication; with no edit open it answers
`OK`, for another transaction's edit `WRONG_TRANSACTION`, and while its own
transaction's Revert waits `BUSY`.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_sets`), `tests/host/run_profile_working_copy_tests.sh` (`test_stale_and_refused_edits_change_nothing`).

### The reply

Every SET is answered in place: bytes 0–4 echoed, then

| Byte | Field |
| ---: | --- |
| 5 | result ([errors](errors.md)) |
| 6 | phase of the working copy |
| 7–10 | session |
| 11–14 | revision |
| 15–16 | edit status flags, as on [page 0](#edit-status-get-0x20) |
| 17 | restart only: `1` the slave half acknowledged, `0` it did not |
| 18–31 | zero |

When this half has no working copy to report on, bytes 6–16 are zero.
Phases: `0` idle, `1` receiving, `2` loading the saved profile, `3`
checking, `4` waiting for a safe boundary, `5` reverting.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_sets`, `test_restart`).

## Admission

Each edit, Save, Revert, mode change and candidate frame, and the keyboard's
own steps, is admitted by one rule per operation. A SET is first answered
`INVALID_ARGUMENT` when malformed, then `INVALID_STATE` when this half's
owner is not ready or has [stopped](#a-stopped-keyboard), then by its row,
refusals in the order answered. Terms: *adjusting*, a keyboard adjustment is
queued or its edit is open; *held*, a candidate (or its cleanup), a save or
a publication waiting for its boundary holds the profile; *apart*, the slave
half does not hold the same session and revision with nothing in progress,
heard within 3 s; *editing*, the working copy is not idle.

| Operation | Refused, in this order |
| --- | --- |
| Edit begin | The repeat of the begin already admitted is answered as before (`OK` when it matches, `BUSY` when it does not). Adjusting, held or apart: `STALE` when the pair is stale, else `BUSY`. Editing: `BUSY`. Then the begin's own checks: `INVALID_ARGUMENT` for unknown flags; `STALE`; `INVALID_ARGUMENT` for a splice count out of range, or a replacement with splices or shorter than a blob header; `NOT_CLEAN` for a replacement over unsaved edits; `CAPACITY_EXCEEDED` ([insert staging](#insert-staging-and-capacity)). |
| Save | While a save runs: `QUEUED` to a repeat of the client's own save, `BUSY` to anything else. Then `STALE`. Then adjusting, editing, held or apart: `BUSY`. |
| Revert | Adjusting or held: `BUSY`. It does not wait for the slave half, which follows. An edit still receiving is aborted; one past receiving answers `BUSY`. Then `STALE`. |
| Edit mode | `INVALID_ARGUMENT` for a mode other than `0`–`2`. Unsaved edits or adjusting: `NOT_CLEAN`. A save running, editing or apart: `BUSY`. No storage for the mode on this half: `READ_ERROR`. |
| Candidate frame | Begin, chunk, streaming chunk, reuse, validate and commit: `BUSY` (`MAILBOX_BUSY`) while a save runs or editing; a begin for a transaction other than the candidate's also while an adjustment is queued. Abort and reads never ([candidate transfer](candidate-transfer.md#the-admission-reply)). |
| Keyboard step | Publishing queued adjustments or starting an automatic save waits while editing, held or apart. |

Edit status flag 4 is the edit begin row asked without a request. A change
of edit mode is not held by an Apply or a publication, since the mode is
stored outside the profile, but an Apply copying to the slave half makes
the halves apart.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_admission_test.inc`: `test_admission_matrix`, `test_a_mode_change_waits_until_the_halves_agree`, `test_a_mode_change_waits_for_an_apply_copying_to_the_other_half`, `test_edit_status_flags_come_from_the_owner`).

## The edit lease

An edit still receiving (phase `1`) holds a lease on the master half: with
no begin, splice or data report accepted for 15 s, it is aborted with last
result `ABORTED`. The clock is the owner's, which stands still while
storage waits for a quiet keyboard. A client that lost track of its edit
can also Revert, which aborts an edit still receiving.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_an_abandoned_edit_is_aborted_and_revert_ends_one`).

## Save

Save writes the working copy as the next generation through the ordinary
candidate path, with a candidate built entirely of reuse from the working
copy (source kind `2`, [candidate transfer](candidate-transfer.md#reuse)) in
ranges of 1,024 bytes, so nothing is uploaded and the slave half fills its
copy from its own working copy. Validation (held to a saved profile's rules,
since unedited domains came from one), preparation on both halves, the
marker-last decision, convergence and every power-loss guarantee are an
Apply's. Its transaction is the candidate's: candidate status follows it as
it follows an Apply.

With no unsaved edits Save answers `OK` at once and writes nothing.
Otherwise it answers `QUEUED` and works through save phases (edit status
page 1): `1` checksumming the working copy, `2` begin, `3` reuse, `4`
validate, `6` waiting for the commit. When the checksums, length and domains
equal the saved profile's, it compares the bytes (`7`); the same bytes are
not written again: the keyboard runs its saved profile at the next safe
boundary (`8`) with nothing unsaved and the same revision, and the slave
half follows as after a Revert. Live's own save does the same. Phase `5` is
never reported.

When a save lands the saved profile holds the working copy's bytes: nothing
unsaved, the same revision. A save that fails leaves the working copy as it
was; its result (`VALIDATION_REJECTED`, or `PUBLICATION_FAILED` with the
candidate's error id) and transaction are on page 1, and a working copy
replaced before the save began writing ends it `ABORTED`. A save waits for
a quiet keyboard before each flash erase
([D-F29](../../adr/0029-profile-slots-in-their-own-flash-region.md)); that
wait counts toward no timeout on either half.

An Apply still works and replaces the working copy: unsaved edits are gone
once it activates, and the revision advances.

Held by: `tests/host/run_profile_owner_tests.sh` (`test_live_edit_reaches_both_halves_and_save_writes_both`, `test_save_survives_lost_replies_on_both_links`, `test_an_apply_replaces_the_working_copy`, `test_waiting_for_a_quiet_keyboard_never_ends_a_save`, `profile_owner_auto_save_test.inc`: `test_saving_the_saved_bytes_writes_nothing`).

## Revert

Revert discards the working copy at the next safe boundary on both halves
and runs the saved profile again, advancing the revision; no flash is
written. With no unsaved edits it answers `OK` and changes nothing; else
`QUEUED`, and the outcome is the edit status's last result.

Held by: `tests/host/run_profile_working_sync_tests.sh` (`profile_working_sync_test.c`, `test_adopt_edit_revert`), `tests/host/run_profile_owner_tests.sh` (`test_an_abandoned_edit_is_aborted_and_revert_ends_one`).

## GET reports

Command `0x08`, channel `0`, a nonzero request id at byte 3, the page at
byte 4 (bytes 4–5 for GET `0x21`), every other byte zero. A malformed
request answers status `1`, an unknown page status `2`, and a half with no
working copy to report on (an owner that never started, or is still
starting) status `3`. A stopped owner still answers every GET.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`).

### Edit status, GET 0x20

Pages `0`–`3`, 25-byte payloads.

| Page 0 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | phase |
| 2 | 2 | flags, below |
| 4 | 4 | session |
| 8 | 4 | revision |
| 12 | 4 | saved generation (`0`: the compiled defaults) |
| 16 | 1 | saved origin half |
| 17 | 1 | last result of an edit or Revert |
| 18 | 2 | its transaction |
| 20 | 2 | transaction of the edit in progress |
| 22 | 1 | edit mode |
| 23 | 1 | splices received |
| 24 | 1 | zero |

| Flag | Meaning |
| ---: | --- |
| 0 | unsaved edits |
| 1 | the keyboard has a slave half |
| 2 | the slave half holds this session and revision with nothing in progress, heard within 3 s |
| 3 | the slave half answers |
| 4 | a new edit would be refused now |
| 5 | the saved profile is the compiled defaults |
| 6 | a save is running |
| 7 | the halves' edit modes conflict |
| 8 | an edit or Revert waits for a safe boundary |
| 9 | a save on this half waits for a quiet keyboard |
| 10 | the slave half's save waits for a quiet keyboard |
| 11 | the slave half refused this working copy, the edit and then the whole copy: apart until Revert or an Apply, and Save is `BUSY` |
| 12 | writing the edit mode failed and is being retried |
| 13 | a keyboard adjustment awaits publication |
| 14 | this half [stopped](#a-stopped-keyboard) |
| 15 | the slave half answers that it stopped |

While flag 13 is set a new Apply, edit, Save and Revert are `BUSY` and a
mode change `NOT_CLEAN`; once it publishes, the adjustment is an unsaved
edit like any other. With flags 11 and 13 together the adjustment cannot
publish and holds Revert, so only a [restart](#restart) ends it.

| Page 1 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | the slave half's phase |
| 1 | 1 | its last result |
| 2 | 2 | its transaction, in progress or else last |
| 4 | 4 | its session |
| 8 | 4 | its revision |
| 12 | 1 | its flags: bit 0 unsaved edits, bit 1 its saved profile is the compiled defaults, bit 2 it stopped |
| 13 | 1 | save phase ([Save](#save)) |
| 14 | 1 | last save's result |
| 15 | 1 | last save's candidate error id (`0` none) |
| 16 | 2 | transaction of the save in progress, `0` when none |
| 18 | 2 | last save's transaction |
| 20 | 4 | inserted bytes received for the edit in progress |
| 24 | 1 | the slave half's edit mode |

| Page 2 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | last refused edit: validator result ([errors](errors.md)) |
| 1 | 1 | domain id |
| 2 | 1 | table id |
| 3 | 2 | row |
| 5 | 1 | step |
| 6 | 1 | field |
| 7 | 4 | byte offset in the resulting profile |
| 11 | 1 | detail kind |
| 12 | 2 | detail code |
| 14 | 4 | why the keyboard is not at a safe boundary now, below |
| 18 | 4 | length of the active profile |
| 22 | 3 | zero |

Safe-boundary reasons: bit 0 a key is pressed, 1 a tap series is open, 2 a
runtime holds a lease, 3 a release is deferred, 4 a lock or other
persistent intent, 5 an owned output is held, 6 a modifier or one-shot, 7 a
one-shot layer, 8 a macro, 9 a combo, 10 the slave half, 30 internal.

Page 3 reports automatic saves on this master half since boot. Its counters
identify an event when polled; compare the session too, since a restart
resets them. The last save's result, error and transaction stay on page 1;
only a successful automatic save advances the counter.

| Page 3 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | flags: bit 0 an automatic save is running, bit 1 the last finished save was automatic |
| 2 | 4 | automatic saves completed since boot |
| 6 | 4 | revision of the last completed automatic save |
| 10 | 4 | its saved generation |
| 14 | 2 | the quiet interval in ms, `3000` |
| 16 | 9 | zero |

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`), `tests/host/run_profile_owner_tests.sh` (`test_edit_status_flags_come_from_the_owner`, `profile_owner_auto_save_test.inc`: `test_auto_save_modes_and_adjustments`).

### The active profile, GET 0x21

A wide page at bytes 4–5, paged as the saved profile is (GET `0x04`,
[the HID channel](hid-channel.md)): pages `1`… carry 25 bytes each; a page
past the length answers status `2`, a failed read status `3`. Page 0:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | page size, `25` |
| 2 | 4 | length |
| 6 | 4 | session |
| 10 | 4 | revision |
| 14 | 4 | saved generation |
| 18 | 1 | saved origin half |
| 19 | 1 | domain mask |
| 20 | 1 | flags: bit 0 unsaved edits |
| 21 | 4 | zero |

It reads the active profile: the working copy, or the saved profile when
nothing is unsaved. Reading page 0 again after the bytes and finding the
same session and revision proves they are one revision. GET `0x04` still
reads the saved profile and `0x05` the compiled defaults.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`), `tests/host/run_profile_working_copy_tests.sh` (`test_resizing_macro_edit_and_working_reads`).

### Runtime, GET 0x22

Page 0, 24 bytes: QMK's live values beside the stored ones they override.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | combos master, stored |
| 1 | 1 | combos override: QMK's combo flag (`0` in a build without combos) |
| 2 | 1 | behaviours master, stored |
| 3 | 1 | behaviours override |
| 4 | 2 | default DPI |
| 6 | 2 | sniping DPI |
| 8 | 4 | RGB mode word |
| 12 | 4 | RGB colour word |
| 16 | 4 | default layers |
| 20 | 4 | keymap options |

Bytes 4–23 are QMK's live values in the settings scalars' encodings
([settings domain](settings-domain.md)); a build without the portable
profile leaves them zero.

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`).

### Edit mode, GET 0x27

Page 0, 11 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | this half's mode |
| 1 | 4 | its counter |
| 5 | 1 | the slave half's mode |
| 6 | 4 | its counter |
| 10 | 1 | flags: bit 0 the slave half answers, bit 1 the two disagree at the same counter, bit 2 both hold the same record |

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_gets`).

## Edit mode

The edit mode says how a client's edits reach the keyboard: `0` Apply, `1`
Live, `2` Live + review. A keyboard never set is in Live + review. It is
stored outside every profile, in QMK's EEPROM user datablock on each half,
never in a profile or a backup, as two 16-byte records:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | magic `45 4D` ("EM") |
| 2 | 1 | layout, `1` |
| 3 | 1 | mode |
| 4 | 4 | zero |
| 8 | 4 | counter, nonzero |
| 12 | 4 | CRC-32 of bytes 0–11 |

The valid record with the higher counter counts; a write goes to the other
record and is read back. A change takes a counter one above both halves';
the master half sends it to the slave half, and on reconnect the higher
counter wins. Equal counters that disagree are a conflict (flag 7) until the
next change. A change is answered `OK` and takes effect at once; each half
writes it when its keyboard is quiet (no key held, the matrix unchanged for
50 ms), and a failed write is retried after 1 s, the wait doubling up to
64 s, with flag 12 set until it succeeds.

A change is refused while the halves are apart because a half that has not
heard the other cannot know a counter above both, and a change made on one
half alone could lose to the other's higher counter on reconnect. With the
slave half unplugged it stays `BUSY` until that half is back (flag 3 clear,
or flag 15 if it stopped).

Held by: `tests/host/run_profile_working_sync_tests.sh` (`test_edit_mode_follows_the_higher_counter`, `test_edit_mode_is_written_on_a_quiet_keyboard`), `tests/host/run_profile_owner_tests.sh` (`test_a_mode_change_waits_until_the_halves_agree`). The 16-byte record layout has no test.

## Automatic saves

In Live the master half saves the working copy itself after 3 s without a
published revision and without a held key, the matrix also quiet for 50 ms,
whether or not a client is connected. Every held-key scan and every published
revision restarts the interval. It starts only when the keyboard step is
admitted ([admission](#admission)) and the edit modes do not conflict, through
the ordinary Save. There is at most one automatic attempt per revision: a failed
attempt leaves the working copy unsaved and is reported as any Save is, and
only an explicit Save, a new edit or a new adjustment tries again. Unplugging
before it completes loses the unsaved edits; a save already running has the
ordinary old-or-new outcome.

Live + review never saves automatically. Apply saves automatically only a
working copy made of keyboard adjustments alone; a published client edit
rules that copy out until Save, Revert or an Apply. No client keepalive or
disconnect message affects this.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_auto_save_test.inc`: `test_auto_save_modes_and_adjustments`, `test_ineligible_auto_saves_skip_admission_work`, `test_auto_save_hour_bound`, `test_auto_save_power_cuts`, `test_a_failed_automatic_save_is_reported_and_not_repeated`).

## Keyboard adjustments

The DPI keys (default and sniping, forward and reverse, Shift reversing
them) and the lighting keys (on, off, toggle, mode, hue, saturation, value,
speed, flags, and QMK's shared underglow keys) change QMK at once through
its setters that do not write its EEPROM, then queue the resulting value of
one of four adjustment scalars: default DPI, sniping DPI, RGB mode and RGB
colour ([settings domain](settings-domain.md)). Only the master half queues,
only with its owner ready, in every edit mode, and onto a saved profile as
well as a working copy.

Queued values coalesce to the latest per scalar. When the keyboard step is
admitted they publish as one edit of one splice per scalar, advancing the
revision, so a client's next edit learns of them as `STALE`. A value pressed
while a save runs publishes after it, as a later revision; the bytes being
saved never change. Until they publish, settings projection keeps the newer
queued values, so a finishing publication cannot undo the key's feedback.

A new Apply is `BUSY` while an adjustment is queued, since published after
it the adjustment would overwrite what the Apply set. An Apply already
admitted carries on; an adjustment pressed during it publishes after it
lands, on the profile it activated, and is saved as any adjustment is.

The settings domain is optional, yet an adjustment replaces bytes inside
it: when the active profile has none, the adjustment's edit creates it from
the authored defaults with the adjusted scalars in place, so every other
value is unchanged. A build without the authored defaults cannot, and such
a profile's adjustments stay local feedback. An adjustment edit the keyboard
cannot make is dropped, not retried.

Held by: `tests/host/run_qmk_profile_adjustments_tests.sh` (`qmk_profile_adjustments_test.c`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_auto_save_test.inc`: `test_auto_save_modes_and_adjustments`, `test_an_apply_and_an_adjustment_land_in_the_order_made`, `test_adjustments_without_a_settings_domain`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Toggles and the settings QMK holds

The profile is the only durable owner of the settings QMK holds: base RGB
mode and colour, default layers, keymap options, DPI and auto-mouse. They
are pushed into QMK at boot (the compiled defaults' once discovery finds
nothing saved), on activation and on a settings edit, always through QMK's
setters that do not write its EEPROM; settings readback (GET `0x07`)
reports the stored values.

A toggle is a runtime override beside its stored master, never an edit:
combos run only while the stored master and QMK's combo flag (flipped by
QMK's combo keys) both allow; behaviours only while the stored master and
the firmware's behaviours override both allow. An override starts allowed
at boot, is never saved, and reads back through GET `0x22`. RGB on and off
is a lighting adjustment, not a toggle.

Held by: `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`), `tests/host/run_participation_tests.sh` (`participation_test.c`, `test_overrides_beside_the_masters`).

## A stopped keyboard

A half's live-profile owner stops in a state only a restart leaves, since
then only a fresh read of flash is known safe. A stopped half keeps typing
but saves, edits, reverts and follows the other half no more: edits, Save,
Revert and the edit mode answer `INVALID_STATE`, and candidate frames are
unhandled ([candidate transfer](candidate-transfer.md#who-routes-candidate-frames)).
Reads go on: edit status, the working copy (GET `0x21`, so a client can keep
the unsaved edits) and profile status. Flag 14 says this half stopped; flag
15 that the slave half answers that it did. A save under way (flag 6),
waiting for a quiet keyboard (flags 9, 10) or a half catching up (flags 2,
3) is never stopped.

| Owner state | Reached when | What a restart does |
| --- | --- | --- |
| `STORAGE_ERROR` | a slot read, write or read-back failed: at boot, in a save, in a copy from the other half, or activating a record | reads flash afresh: the last whole saved profile runs |
| `DURABILITY_UNKNOWN` | the commit marker's write or read-back failed | boot reads the marker: the old or the new profile runs, and the halves reconcile |
| `POSTCOMMIT_AUTHORITY_LOST` | after the decision, the other half reported a newer profile | boot reconciliation compares the halves again |
| `CONCURRENT_COMMIT` | both halves committed one generation, or the other half prepared one that wins | boot reconciliation compares the halves again |
| `INTEGRATION_ERROR` | a runtime could not install, or the owner could not start | starts it again; a fault in the build returns at once |
| `GENERATION_CONFLICT` | at boot, two different saved profiles of one generation | nothing: the conflict is in flash and returns at once |
| `COMPILED_ERROR` | the compiled defaults did not validate | nothing: the fault is in the firmware |

A flag back at once after a restart means the fault is in flash or the
firmware. An EEPROM reset (bootmagic) erases the saved profiles and so
clears a conflict in flash, at the cost of the saved profile.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_only_states_a_restart_leaves_are_stopped`, `test_a_storage_failure_in_a_save_stops_the_master_half_and_says_so`, `test_a_half_that_cannot_read_its_slots_stops_and_is_heard`, `test_an_unknown_commit_marker_stops_until_a_restart_reads_it`, `test_two_profiles_of_one_generation_stop_the_owner`, `test_unreadable_compiled_defaults_stop_the_owner`, `test_authority_changing_after_the_decision_stops_the_owner`, `test_a_failed_runtime_install_stops_the_owner`, `test_a_slow_save_is_never_stopped`, `test_a_stop_during_an_adjustment_edit_refuses_edits_first`).

## Restart

SET `0x28` with bytes 5–8 `52 45 53 54` ("REST"), every later byte zero and
a nonzero transaction restarts both halves; anything else answers
`INVALID_ARGUMENT` and restarts nothing. It is taken in any owner state and
edit mode, mid-save or mid-edit, even when the owner never started, and
reads nothing of the profile. The firmware never restarts itself.

The master half asks the slave half on a split RPC of its own
([the split protocol](../profile-split-v1.md)), again every 5 ms until it
acknowledges or 250 ms have passed, then replies `OK` with byte 17 saying
whether it acknowledged. 50 ms after answering the master half resets (QMK's `soft_reset_keyboard`); the slave half resets 20 ms after it
acknowledged. A restart asked again before the reset asks nothing more and
answers as the first did. Both halves then start as from power: each reads
its saved profile, the halves reconcile, and the working copy has a new
session. Everything in RAM is gone, unsaved edits and toggles included; the
edit mode is stored and stays. A client that wants its unsaved edits back
reads the working copy first and sends it as one replacement edit once the
keyboard is back.

Held by: `tests/host/run_qmk_keyboard_restart_tests.sh` (`qmk_keyboard_restart_test.c`: `test_frames`, `test_both_halves_restart`, `test_a_busy_other_half_is_asked_again`, `test_an_unanswering_other_half_bounds_the_wait`), `tests/host/run_qmk_profile_edit_channel_tests.sh` (`test_restart`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_stopped_owners_and_restarts`).

### Why a restart is safe

A restart resets each half between two main-loop passes, so it is exactly
as safe as unplugging: a slot's commit marker is written last and a record
without it is never booted, so a reset anywhere in a save leaves the
previous saved profile, or the new one once the master half's marker is
durable ([the logical profile transaction](../logical-profile-transaction-v1.md)).
QMK handles Raw HID in its main loop after the scan, and housekeeping,
where each half resets, after both; every flash erase and program runs
inside the scan's durable-I/O step, so a reset never falls inside one. The
slave half's split callback only notes the request.

Held by: `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_a_restart_anywhere_in_a_save_keeps_a_whole_profile`), `tests/host/run_qmk_keyboard_restart_tests.sh` (`test_the_other_half_takes_only_a_restart_frame`).

### When the slave half does not acknowledge

With byte 17 `0` (the link is down, or the slave half does not answer) the
master half restarts alone. Each half's saved profile is still whole, and
the halves rejoin once the link answers: a prepared copy of a save in
flight on the slave half is released, or committed when the master half's
commit marker had landed ([the split protocol](../profile-split-v1.md)). If
the slave half still does not answer once the keyboard is back, only
unplugging the keyboard restarts both.

Held by: `tests/host/run_qmk_keyboard_restart_tests.sh` (`test_an_unanswering_other_half_bounds_the_wait`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_stopped_test.inc`: `test_stopped_owners_and_restarts`).

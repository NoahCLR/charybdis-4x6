# Candidate transfer

How a client sends a complete profile to the master half as a candidate and
has it saved on both halves (an Apply): begin, chunks, reuse and streaming,
validate, commit and abort, the admission reply each frame gets at once, and
the candidate operation status pages a client polls for the outcome. The
Raw HID framing, the read envelope and the capability pages are in
[the HID channel](hid-channel.md); every error id, admission value and busy
reason is in [errors](errors.md); the blob the candidate carries is in
[blob and digests](blob-and-digests.md). What the transaction guarantees on
both halves (prepare, the commit decision, activation, power loss and
cancellation) is [the logical profile transaction](../logical-profile-transaction-v1.md);
the copy between the halves is [the split protocol](../profile-split-v1.md).
A Save of the working copy uses this same path ([live edits](live-edits.md#save)).

## Who routes candidate frames

Only a build with saves (feature bit 1, see [the HID channel](hid-channel.md))
routes candidate frames and GET `0x18`, and only once its live-profile owner
is initialized. A build without them, or an owner that is not ready (still
starting, or [stopped](live-edits.md#a-stopped-keyboard)), leaves every
candidate frame unhandled: the reply's command byte is `0xFF`. Compiling the
codec alone routes nothing.

The receive callback does only exact frame decoding and one bounded copy
into a one-command mailbox. It never touches flash and never computes a
digest; validation, storage, activation and the copy to the other half run
later from the scan.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, `test_hook_rejects_unsupported_or_malformed_commands`), `tests/host/run_profile_candidate_transaction_tests.sh` (`profile_candidate_transaction_test.c`, `test_mailbox_and_begin_retries`). No test sends a candidate frame to an owner that is not ready.

## Frames

Every candidate frame is an exact 32-byte report on custom channel `0x00`.
A report of another length, or with a value this table does not list, is not
a candidate frame.

| Command | Value | Operation | Last-operation id |
| --- | ---: | --- | ---: |
| set `0x07` | `0x10` | begin | `1` |
| set `0x07` | `0x11` | chunk | `2` |
| set `0x07` | `0x12` | validate | `3` |
| save `0x09` | `0x13` | commit | `5` |
| set `0x07` | `0x14` | abort | `4` |
| set `0x07` | `0x1B` | reuse | `6` |
| set `0x07` | `0x1C` | streaming chunk | `2` |

Commit is the one frame sent as custom save `0x09`; value `0x13` under
command `0x07` is not a candidate frame.

Every frame starts with the same header; multi-byte fields are
little-endian throughout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | command: `0x07`, or `0x09` for commit |
| 1 | 1 | custom channel `0x00` |
| 2 | 1 | value |
| 3 | 2 | transaction id, nonzero |

Validate, commit and abort carry nothing else: bytes 5–31 are zero.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_exact_frame_codecs`, `test_malformed_frame_matrix`), `tests/fixtures/profile_candidate_v1.fixture`.

## The admission reply

Each frame is answered in place at once. Bytes 0–4 are echoed; bytes 5–31
become:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | admission: `QUEUED`, `MALFORMED`, `BUSY` or `REJECTED` (values in [errors](errors.md)) |
| 6 | 1 | candidate error id |
| 7 | 1 | byte offset in the frame where it is malformed, else `0xFF` |
| 8 | 24 | zero |

The checks run in this order:

1. While a live edit, a Revert or a Save holds the working copy, a begin,
   chunk, streaming chunk, reuse, validate or commit is answered `BUSY` with
   `MAILBOX_BUSY`; so is a begin naming a transaction other than the
   candidate's while a keyboard adjustment awaits publication
   ([admission](live-edits.md#admission)). Abort never is.
2. A malformed frame is answered `MALFORMED` with `MALFORMED_FRAME` and the
   offending byte's offset.
3. While the mailbox holds an unprocessed command, `BUSY` with
   `MAILBOX_BUSY`.
4. A streaming chunk failing its admission checks is answered `REJECTED`
   ([streaming chunks](#streaming-chunks)).
5. Otherwise the frame is copied into the mailbox and answered `QUEUED`,
   error `0`, offset `0xFF`.

`QUEUED` says only that the frame was taken. Its outcome, and every later
result, is read from [candidate operation status](#candidate-operation-status-get-0x18).

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_mailbox_and_begin_retries`, `test_streaming_admission_and_reuse_failure`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_admission_test.inc`, `test_admission_matrix`), `tests/fixtures/profile_candidate_v1.fixture`.

## Begin

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | schema major, `4` |
| 6 | 1 | schema minor, `0` |
| 7 | 1 | requested-domain mask: bit *n* is the *n*th domain of the registry ([blob and digests](blob-and-digests.md)): RGB, key behaviours, combos, settings, pointing, keymap, macros |
| 8 | 4 | blob length, `8`–`130048` |
| 12 | 4 | CRC-32 of the exact blob |
| 16 | 4 | FNV-1a digest of the exact blob |
| 20 | 4 | action-ABI digest the actions were encoded with |
| 24 | 8 | zero |

The frame is malformed when the mask sets a bit outside the registry
(offset `7`), the length is out of range (offset `8`), or any of bytes
24–31 is nonzero. There is no flags byte and no store-format byte: Profile
Wire 2.0 implies store format 6 ([D-F28](../../adr/0028-new-base-32-bit-profile-lengths.md)).

Processing a begin checks, in order, the schema version
(`INCOMPATIBLE_SCHEMA`), the mask against the domains this build supports
(`UNSUPPORTED_DOMAIN`), the action-ABI digest (`INCOMPATIBLE_ACTION_ABI`)
and the length against the build's ceiling (`CAPACITY_EXCEEDED`). Any of
these leaves the candidate idle. The mask may be zero, for the empty
profile. A begin accepted by these checks enters receiving at offset `0`.

When storage cannot take a new candidate yet (another prepare holds it),
the begin stays in the mailbox, flag 0 set, and is tried again each scan;
status meanwhile reports the candidate idle with `MAILBOX_BUSY`. It is
never poisoned for this.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_begin_compatibility_rejections`, `test_busy_begin_remains_queued_without_poisoning`, `test_malformed_frame_matrix`), `tests/fixtures/profile_candidate_v1.fixture`.

## Chunk

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 4 | byte offset in the blob |
| 9 | 1 | length, `1`–`20` |
| 10 | 20 | bytes, then zero to the end of the field |
| 30 | 2 | zero |

The frame is malformed when the length is out of range (offset `9`), the
chunk would end past `130048` (offset `5`), or any byte after the chunk's
bytes is nonzero.

Chunks are sequential. A chunk at the next offset is written and advances
it; reaching the declared length makes the candidate complete. A chunk
ending past the declared length is refused `CAPACITY_EXCEEDED`; one starting
past the next offset, or straddling it, `OUT_OF_ORDER`. A chunk entirely
below the next offset is a retry: the staged bytes are read back and
compared, and the same bytes are accepted with nothing written, while
different bytes poison the candidate with `CONFLICTING_RETRY`. A write or
read-back failure poisons it with `STORAGE_FAILURE`. Chunks to a poisoned
candidate answer `POISONED`, to another transaction `WRONG_TRANSACTION`,
outside receiving and complete `INVALID_STATE`.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_sequential_chunks_and_duplicate_rules`, `test_upload_over_64_kib`).

## Differential transfer: reuse and streaming

A client may build the candidate from ranges of the active profile (reuse)
and stream the rest without reading status between chunks, so an Apply
sends only what changed ([D-F18](../../adr/0018-differential-upload-builds-complete-candidate.md)).
Both come with saves; a client may still send every byte as ordinary chunks.
They only build the candidate: it is still complete, validated whole and
saved as any other.

### Reuse

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 4 | offset in the new candidate |
| 9 | 4 | offset in the source |
| 13 | 4 | length, `1`–`1024` |
| 17 | 4 | source generation |
| 21 | 4 | source digest field |
| 25 | 4 | source CRC field |
| 29 | 1 | source kind |
| 30 | 1 | source origin |
| 31 | 1 | zero |

The source names the active profile, which must be exactly this one:

| Kind | Source | Generation | Origin | Digest field | CRC field |
| ---: | --- | --- | --- | --- | --- |
| `0` | the compiled defaults | `0` | `255` | FNV-1a digest | CRC-32 |
| `1` | a saved profile | nonzero | `0` or `1`, the half that saved it | FNV-1a digest | CRC-32 |
| `2` | the working copy | its saved profile's (`0` over the compiled defaults) | its saved profile's (`255` over the compiled defaults) | its revision | its session |

The frame is malformed when the length is out of range (offset `13`), the
destination or source range would end past `130048` (offsets `5`, `9`), the
kind and origin do not fit the table (offset `29`), or byte 31 is nonzero.

Processing checks that the candidate is receiving, unpoisoned and this
transaction's, and that the destination is the next offset and the range
fits the declared length; otherwise `INVALID_STATE`, and nothing is copied.
Each read then checks the source against the active profile's kind, origin,
generation and both identity fields, and the build's compiled-defaults and
action-ABI identities; a mismatch, read or write failure poisons the
candidate with `STORAGE_FAILURE`. Bytes already copied stay; nothing is
retried. The source stays active and only the candidate is written.

A reuse is worked through over several scans, up to one flash page (256
bytes) per step in pieces of at most 20 bytes, and holds the mailbox until
the range is done. The next offset advances with each piece; the operation
sequence advances once, when the range ends. A cancellation that discards
the mailbox ([lease and cancellation](#lease-and-cancellation)) can end a
range part copied. The transaction holds a 20-byte copy buffer, never a
profile-sized one.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_reuse_is_bounded_and_validated`, `test_differential_transfer_frames`, `test_supersession_discards_a_partly_copied_reuse`, `test_streaming_admission_and_reuse_failure`).

### Streaming chunks

A streaming chunk has the chunk layout and reports last operation `2`. It
is admitted only when, besides the ordinary checks, the candidate is
receiving and unpoisoned (else `INVALID_STATE`), the transaction is the
candidate's (`WRONG_TRANSACTION`), the offset is exactly the next one
(`OUT_OF_ORDER`) and the chunk fits the declared length
(`CAPACITY_EXCEEDED`). A refusal is admission `REJECTED` with that error and
frame offset `5`. An admitted streaming chunk is processed as a chunk.

The mailbox still takes one command at a time. A client sends a bounded
batch, each frame after the previous was admitted, then reads page 0 to
prove the transaction, the next offset and the operation sequence moved as
expected before sending more. On `BUSY` it resends only frames that were
definitely not admitted; a lost reply is ambiguous and is resolved by
reading status, never by resending the batch. A storage failure poisons the
candidate, so a later streaming chunk cannot hide it.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_streaming_admission_and_reuse_failure`, `test_differential_transfer_frames`).

### What the other half copies

Ranges built by reuse are remembered, and the copy to the other half asks
it to fill those ranges from its own active profile, checked against the
same source identity; only the rest is sent, and all of it when the other
half's active profile is not that source. The other half still receives,
validates and prepares the complete candidate. The client sends nothing
different ([split protocol](../profile-split-v1.md), payload reuse).

Held by: `tests/host/run_profile_owner_tests.sh` (`test_committed_reuse_saves_both_halves_and_reboots`).

## What validation holds a candidate to

Validate starts only on a complete candidate (else `INVALID_STATE`, with
the next offset as the error's byte offset). It checks the CRC-32 and
digest against the begin, then the whole profile, in bounded steps from the
scan: state `3` while it runs, then `4` validated or `5` rejected with the
first error and its location ([page 3](#page-3-where-the-error-is)). A
candidate a client sends is held to the host rules, which include where
actions may be placed; the candidate a Save builds is held to a saved
profile's rules, since its unedited domains came from one
([live edits](live-edits.md#save)).

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_bounded_validation_and_error_locations`, `test_error_pages_follow_asynchronous_errors`).

## Commit

Commit is admitted for the validated candidate of its transaction. On a
split keyboard it enters `8` (preparing the other half), the commit decision
waits until the other half holds a prepared copy and this half reaches a
safe boundary, then storage writes the record with its commit marker last
(`6` committing), the other half is brought to the same generation (`9`)
and the new profile is activated at a safe boundary (`7`), after which the
candidate is idle again. A single half goes from validated straight to
committing. The previous profile stays active until activation. What each
state means to both halves is in
[the logical profile transaction](../logical-profile-transaction-v1.md) and
[the authority state table](../authority-state-table.md).

A repeated commit for the same transaction changes nothing while preparing,
committing, converging or activating, and after the commit succeeded (the
candidate idle, last committed transaction the same). A commit for a
candidate that is not validated answers `INVALID_STATE`.

Failures after the decision are not safe failures. A failed write or
read-back of the commit marker reports `DURABILITY_UNKNOWN`: the client
reads status (profile status and its generation) before trying again. A
failure to activate a durable profile reports `ACTIVATION_FAILED`. Losing
authority to the other half after the decision reports
`POSTCOMMIT_AUTHORITY_LOST` or `PEER_COMMIT_CONFLICT` and state `10`. Each
of these also stops the owner ([stopped](live-edits.md#a-stopped-keyboard)).

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_commit_activation_and_idempotent_retry`, `test_split_authorization_barriers`, `test_unknown_marker_durability_is_not_reported_as_safe_failure`, `test_activation_failure_is_durable_and_requires_status_clear`, `test_postcommit_authority_failure_is_terminal_and_retains_candidate`), `tests/host/run_profile_owner_tests.sh` (`test_commit_waits_for_a_safe_boundary_then_activates`, `test_decided_commit_is_never_cancelled`).

## Abort

An abort for the candidate's transaction before the decision releases the
candidate; nothing is written and the active profile stays. On a split
keyboard the other half's prepared copy is aborted first
([lease and cancellation](#lease-and-cancellation)). An abort with no
candidate succeeds and does nothing but record its transaction, so a
repeated abort never repeats storage work. An abort for another transaction
answers `WRONG_TRANSACTION`; one after the decision (committing, converging,
activating, authority failed) answers `INVALID_STATE` and changes nothing.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_abort_reset_and_no_timeout`), `tests/host/run_profile_owner_tests.sh` (`test_host_abort_is_coordinated_at_each_prepare_boundary`, `test_host_abort_ends_when_peer_never_acknowledges`).

## Retries and lost replies

A client correlates every result by transaction id and operation sequence,
never by the reply alone.

- A repeated begin whose every field matches the candidate's changes
  nothing. One that differs poisons the candidate with
  `CONFLICTING_RETRY`; repeating the original afterwards does not revive
  it. After the decision a differing begin is refused `CONFLICTING_RETRY`
  without touching the committed candidate. A begin for another
  transaction while a candidate exists answers `WRONG_TRANSACTION`.
- A repeated chunk is accepted when its bytes match ([chunk](#chunk)).
- A repeated commit or abort is a no-op ([commit](#commit), [abort](#abort)).
- A lost commit reply is resolved by reading status: transaction, digest,
  state, last operation and operation sequence on page 0, and the profile
  status's committed generation.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_begin_retry_requires_identical_metadata`, `test_conflicting_begin_preserves_durable_phases`, `test_sequential_chunks_and_duplicate_rules`, `test_commit_activation_and_idempotent_retry`).

## Lease and cancellation

Only the keyboard ends a candidate
([D-F48](../../adr/0048-only-the-keyboard-ends-a-candidate.md)). Before the
decision, every way a candidate ends releases it on this half and aborts
the other half's prepared copy; after the decision nothing cancels it. What
the client sees of each, on page 0:

| Cause | Error | When |
| --- | --- | --- |
| the client's abort | none | an abort frame for the candidate's transaction |
| the lease runs out | `TIMEOUT` | 15 s without processed work on the candidate; from the start of the copy to the other half, 60 s without correlated progress of that copy |
| the other half's saved profile is as new or newer | `PEER_SUPERSEDED` | a compatible saved generation on the other half at least the one this candidate would take |
| the other half's own prepare wins | `PEER_PREPARE_YIELDED` | both halves prepared at once and the other half's origin wins |
| the copy to the other half failed | `PEER_TRANSFER_FAILED` | the copy stopped for good after its retries; page 1 says what the other half answered last |

The lease runs on the owner's clock, which stands still while storage waits
for a quiet keyboard on either half; `BUSY` replies and retries do not
refresh it. Neither window expires a candidate that is committing,
activating or whose durability is unknown. A desktop timeout does not abort
anything by itself: the client sends abort. Supersession, yield and a failed
copy each advance the operation sequence once and may discard one admitted
but unprocessed command, so a queued commit cannot cross the decision.
Transaction id, digest, last operation and operation sequence stay readable
after any of them.

An abort or cancel waits for the other half to acknowledge its ABORT for at
most 15 s; then this half releases its side anyway, and profile status
state flag 8, peer cleanup pending, stays set until the other half confirms
([the HID channel](hid-channel.md)). Meanwhile a new candidate's copy cannot
start, so its commit waits in state `8` until its own 15 s lease ends it
with `TIMEOUT`. A reset or power loss discards the mailbox and the
candidate; the marker-last record keeps an unfinished candidate from ever
being booted ([the logical profile transaction](../logical-profile-transaction-v1.md)).

Held by: `tests/host/run_profile_owner_tests.sh` (`test_empty_boot_live_commit_and_timeout`, `test_prepare_uses_extended_no_progress_timeout`, `test_prepare_phase_progress_extends_only_once`, `test_waiting_for_a_quiet_keyboard_never_ends_a_save`, `test_peer_authority_supersedes_only_precommit_host_generation`, `test_simultaneous_hosts_choose_stable_physical_origin_before_durability`, `test_peer_store_write_failure_retries_then_fails_fast`, `test_prepared_peer_is_aborted_before_host_timeout_releases_candidate`, `test_host_abort_ends_when_peer_never_acknowledges`, `test_every_precommit_cancel_releases_the_candidate`), `tests/host/run_profile_candidate_transaction_tests.sh` (`test_explicit_precommit_expiry_never_crosses_durable_boundary`, `test_supersession_discards_a_partly_copied_reuse`).

## Candidate operation status, GET 0x18

GET `0x18` uses the ordinary read envelope ([the HID channel](hid-channel.md)):
a nonzero request id at byte 3, the page at byte 4, bytes 5–31 zero; a
malformed request answers status `1`, an unknown page status `2`. Every page
answers a 25-byte payload. It follows a Save exactly as it follows an Apply.

### Page 0: progress

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `2` |
| 1 | 1 | candidate state |
| 2 | 1 | last operation |
| 3 | 1 | flags: bit 0 a command waits in the mailbox, bit 1 the candidate is poisoned |
| 4 | 2 | transaction id of the candidate, or of the last processed command |
| 6 | 4 | next byte offset |
| 10 | 4 | declared length |
| 14 | 4 | declared FNV-1a digest |
| 18 | 1 | candidate error id |
| 19 | 2 | operation sequence: advances once per processed command |
| 21 | 2 | error sequence: advances whenever the error or its location changes |
| 23 | 2 | zero |

| Candidate state | | Candidate state | |
| ---: | --- | ---: | --- |
| `0` | idle | `6` | committing |
| `1` | receiving | `7` | activating |
| `2` | complete | `8` | preparing the other half, before the decision |
| `3` | validating | `9` | converging the other half, after the local commit |
| `4` | validated | `10` | authority failed after a durable local commit, not activated |
| `5` | rejected | | |

Last operation: `0` none, `1` begin, `2` chunk (and streaming chunk), `3`
validate, `4` abort, `5` commit, `6` reuse.

After an abort or a timeout, length, digest and next offset read `0`;
supersession, a yield and a failed copy keep them.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_operation_status_codec`, `test_abort_reset_and_no_timeout`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, `test_hook_rejects_unsupported_or_malformed_commands`), `tests/fixtures/profile_candidate_v1.fixture`.

### Page 3: where the error is

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | candidate error id, as on page 0 |
| 2 | 1 | domain id, or `0xFF` |
| 3 | 1 | table id, or `0xFF` |
| 4 | 2 | row, or `0xFFFF` |
| 6 | 1 | tap count index or step, or `0xFF` |
| 7 | 1 | field id, or `0xFF` |
| 8 | 4 | byte offset in the candidate, or `0xFFFFFFFF` |
| 12 | 2 | transaction id, as on page 0 |
| 14 | 2 | operation sequence, as on page 0 |
| 16 | 2 | error sequence, as on page 0 |
| 18 | 7 | zero |

A client reads page 3 after a page 0 that reports an error and keeps it only
when transaction, operation sequence and error sequence all match that page
0; otherwise it reads page 0 again. The operation sequence alone is not
enough: validation, commit and the other half replace the error between
commands, and only the error sequence moves with each replacement. Setting
the same error again does not move it.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_operation_status_codec`, `test_error_pages_follow_asynchronous_errors`, `test_bounded_validation_and_error_locations`), `tests/fixtures/profile_candidate_v1.fixture`.

### Page 1: the copy to the other half

Page 0 does not move while the other half prepares; page 1 shows the copy.
A half that cannot report it (its split state not yet initialized) answers
status `2`.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `2` |
| 1 | 1 | phase of the copy |
| 2 | 1 | the other half's last split status ([errors](errors.md)) |
| 3 | 1 | flags: bit 0 peer cleanup pending, bit 1 this half is the master half, bit 2 the decision (or the step after it) waits for a safe boundary on this half |
| 4 | 4 | bytes of the profile transferred, including those the other half copied from its own profile |
| 8 | 4 | bytes to transfer |
| 12 | 4 | split retries since boot |
| 16 | 4 | split transport failures since boot |
| 20 | 2 | the other half's `BUSY` replies in a row, saturating |
| 22 | 1 | its last busy reason other than admission ([errors](errors.md)); its internal reasons for copying a reuse range and for waiting on a quiet keyboard are reported as "store working" |
| 23 | 1 | its store state in its last `BUSY` |
| 24 | 1 | low nibble: its transfer owner in its last `BUSY`; high nibble: who held its storage then, `0` nobody, `1` a client's candidate, `2` the master half's copy |

Phases: `0` idle, `1` never reported, `2` beginning, `3` sending, `4` the
other half validating and writing its prepared copy, `5` the other half
prepared, `6` the other half committing, `7` aborting its copy, `8` the copy
stopped (byte 2 says why), `9` waiting for the link between steps. Store
states: `0` uninitialized, `1` idle, `2` receiving, `3` validating, `4`
preparing, `5` prepared, `6` committing, `7` committed, `8` rejected, `9`
reconcile required. Transfer owners: `0` none, `1` receiving a push, `2`
pulling.

Bytes 23–24 come from every `BUSY`, admission included, so they describe the
other half as it is; byte 22 keeps the last reason that was not admission.
Bytes 20–24 reset when a copy starts. A change of phase, offset or last
status is progress. No firmware decision depends on bytes 12–24.

Bit 2 is the decision waiting for no held key, pending tap, locked layer or
pointing mode, one-shot, macro or combo on this half. Typing still works,
so the person can release what holds it; if it does not clear within the
60 s window the save ends with `TIMEOUT` and nothing changes. After the
decision the save simply waits.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_peer_status_page`), `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`, `test_hook_rejects_unsupported_or_malformed_commands`), `tests/host/run_profile_owner_tests.sh` (`test_host_abort_ends_when_peer_never_acknowledges`, `test_commit_waits_for_a_safe_boundary_then_activates`).

### Page 2: reuse counters of the copy

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `2` |
| 1 | 1 | flags: bit 0 reuse became unavailable for this copy |
| 2 | 4 | bytes acknowledged as copied from the other half's own profile |
| 6 | 2 | reuse requests sent, including repeated progress polls and failures |
| 8 | 2 | correlated source-unavailable replies |
| 10 | 4 | scan visits of the copy while it ran |
| 14 | 4 | split requests during the copy, failed ones included |
| 18 | 4 | current transfer offset, as on page 1 |
| 22 | 3 | zero |

Counters saturate at their width, reset when a new copy starts and stay
readable after it ends. They include validation, preparation, commit and
retries, so a client takes differences at phase boundaries. A request is
QMK's complete split RPC exchange. Reused bytes advance on a range's
acknowledgement or source-unavailable reply. Page 1 and page 2 are each
coherent but not one capture: read page 1 before and after page 2 and drop
a sample that crosses a phase or offset change.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_peer_status_page`), `tests/host/run_profile_owner_tests.sh` (`test_committed_reuse_saves_both_halves_and_reboots`).

## Fixtures

`tests/fixtures/profile_candidate_v1.fixture` holds exact begin, chunk,
validate, commit and abort requests, queued and malformed acknowledgements,
and page 0 and page 3 status replies.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`test_exact_frame_codecs`, `test_operation_status_codec`).

# Profile Wire errors and results

Every code the keyboard answers with on Profile Wire, and what a refusal
says about where it happened: the read status, the unhandled reply, a
candidate's admission and error ids, the validation results behind them,
live-edit results, and the split protocol's statuses and busy reasons as
Profile Wire reports them. The frames that carry these codes are
[hid-channel.md](hid-channel.md), [candidate-transfer.md](candidate-transfer.md)
and [live-edits.md](live-edits.md); the split protocol that produces the peer
codes is [profile-split-v1.md](../profile-split-v1.md).

## Read status

Byte 5 of every GET reply ([read envelope](hid-channel.md#read-envelope)).
Any status but `0` carries an empty, zero payload.

| Status | Name | Meaning |
| ---: | --- | --- |
| 0 | OK | the payload follows |
| 1 | malformed | request id zero, a nonzero reserved byte, or a field the value refuses |
| 2 | unknown page | the page does not exist: past the value's page count, past the end of a payload, or a page a value does not answer (GET `0x08` page 0) |
| 3 | unavailable | the page exists but cannot be answered now: nothing saved (GET `0x04`), compiled defaults not open (GET `0x05`), a read failed, the slave half's flash figures not yet pulled (GET `0x0D` page 1), or the live-profile owner not ready (GET `0x20`–`0x22`, `0x27`) |

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`, `qmk_profile_channel_test.c`), `tests/host/run_qmk_profile_flash_tests.sh` (`qmk_profile_flash_test.c`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Unhandled

A report no handler takes comes back with command byte `0xFF` and every other
byte unchanged ([Raw HID reports](hid-channel.md#raw-hid-reports)). It means
the build does not route that command, channel or value, or the report is not
32 bytes; it is never a refusal of a routed value, which answers with a
status or result instead.

Held by: `tests/host/run_profile_wire_v1_tests.sh` (`qmk_profile_channel_test.c`).

## Candidate admission

A candidate SET or SAVE ([candidate-transfer.md](candidate-transfer.md)) is
answered at once, in place: bytes 0–4 echoed, then

| Byte | Field |
| ---: | --- |
| 5 | admission, below |
| 6 | candidate error id |
| 7 | the offending frame byte, or `0xFF` |
| 8–31 | zero |

| Admission | Name | Meaning |
| ---: | --- | --- |
| 0 | QUEUED | copied into the mailbox; not validation, durability or activation |
| 1 | MALFORMED | the frame is refused; error `1`, byte 7 the first bad byte |
| 2 | BUSY | the mailbox holds an unprocessed command, or a live edit, a Revert, a save or a pending keyboard adjustment holds the profile; error `2`, byte 7 `0xFF` |
| 3 | UNSUPPORTED | never sent: a build that does not save answers unhandled |
| 4 | REJECTED | a streaming chunk failed admission; error `4`, `5`, `9` or `10` names the check, byte 7 is `5` |

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`profile_candidate_transaction_test.c`), `tests/fixtures/profile_candidate_v1.fixture` (`begin-queued-ack`, `begin-malformed-ack`, `commit-queued-ack`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, BUSY while edits hold the profile).

## Candidate error ids

Candidate status pages 0 and 3 and profile status page 1 byte 22 carry the
candidate's last error; admission byte 6 uses the same ids. An error with no
location reports the location sentinels below.

| Id | Name | Meaning |
| ---: | --- | --- |
| 0 | `NONE` | no error |
| 1 | `MALFORMED_FRAME` | a frame's layout or a field's range is wrong |
| 2 | `MAILBOX_BUSY` | an earlier command is unprocessed, or the profile is held |
| 3 | `INVALID_TRANSACTION` | never reported: transaction id zero is a malformed frame |
| 4 | `WRONG_TRANSACTION` | the command names a transaction other than the open candidate's |
| 5 | `INVALID_STATE` | the candidate is not in a state that takes this command |
| 6 | `INCOMPATIBLE_SCHEMA` | BEGIN declares another schema |
| 7 | `UNSUPPORTED_DOMAIN` | BEGIN requests a domain the build does not admit |
| 8 | `INCOMPATIBLE_ACTION_ABI` | BEGIN declares another action-ABI digest ([blob-and-digests.md](blob-and-digests.md#action-abi-digest)) |
| 9 | `CAPACITY_EXCEEDED` | a length or write past the declared or maximum size |
| 10 | `OUT_OF_ORDER` | a write that is not the next offset: a gap or a partial overlap |
| 11 | `CONFLICTING_RETRY` | a repeated BEGIN or chunk with different bytes; the candidate is poisoned |
| 12 | `CHECKSUM_MISMATCH` | the staged blob's CRC32 or digest is not the declared one |
| 13 | `STORAGE_FAILURE` | a flash read, write or check failed |
| 14 | `VALIDATION_REJECTED` | the blob failed validation; the location says where ([validation results](#validation-results)) |
| 15 | `UNSUPPORTED_OPERATION` | the mailbox held an operation the transaction does not know |
| 16 | `POISONED` | the candidate was poisoned by an earlier error; only abort ends it |
| 17 | `ACTIVATION_FAILED` | the generation was saved but did not activate; also reported while the active profile's pointing slots are unpublished ([active kind](hid-channel.md#active-kind) 3) |
| 18 | `DURABILITY_UNKNOWN` | completing or confirming the commit marker failed; read status and reconcile the saved generation before retrying |
| 19 | `TIMEOUT` | the keyboard ended a candidate that made no progress before its commit ([candidate-transfer.md](candidate-transfer.md)) |
| 20 | `PEER_SUPERSEDED` | the slave half holds a compatible generation at or above the one the candidate reserved; the candidate was cancelled before commit |
| 21 | `PEER_PREPARE_YIELDED` | both halves prepared at once and this half lost the deterministic arbitration |
| 22 | `POSTCOMMIT_AUTHORITY_LOST` | this half saved durably, then lost authority over the slave half before activating; it does not activate |
| 23 | `PEER_COMMIT_CONFLICT` | the slave half committed a conflicting identity while the halves converged after commit |
| 24 | `PEER_TRANSFER_FAILED` | the copy to the slave half stopped before any commit, after its storage retries; candidate status page 1 names its last answer, and nothing was saved |

The cancellation and authority rules behind `19`–`24` are the
[logical profile transaction](../logical-profile-transaction-v1.md)'s and the
[authority state table](../authority-state-table.md)'s.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`profile_candidate_transaction_test.c`), `tests/fixtures/profile_candidate_v1.fixture` (`operation-error-response`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`, `19`–`24`).

## Where an error is

A candidate error carries its location; candidate status page 3 reports it
([candidate-transfer.md](candidate-transfer.md)). Each field holds a sentinel
when the error has no such location:

| Field | Width | Sentinel |
| --- | ---: | --- |
| domain id | 8 bits | `0xFF` |
| table id (the domain's own numbering) | 8 bits | `0xFF` |
| row index | 16 bits | `0xFFFF` |
| tap index (branch) | 8 bits | `0xFF` |
| field id (the domain's own numbering) | 8 bits | `0xFF` |
| byte offset in the blob | 32 bits | `0xFFFFFFFF` |

A domain's table and field ids are in its own file. The error sequence beside
them advances whenever the error or its location changes, including a
validation result no mailbox command preceded; setting the same error again
does not advance it. No source strings come back from the firmware.

Held by: `tests/host/run_profile_candidate_transaction_tests.sh` (`profile_candidate_transaction_test.c`), `tests/fixtures/profile_candidate_v1.fixture` (`operation-error-response`).

## Validation results

The names the blob codec and whole-profile validator refuse with. They do not
travel on the wire: a checksum failure is reported as candidate error `12`,
every other one as `14` with its location, and a live edit's as edit result
`12` with its location on edit status page 2 ([live-edits.md](live-edits.md)).
Domain files name their own further results.

Blob and action codec (`noah_profile_codec_v1_result_t`): `TRUNCATED`,
`CAPACITY_EXCEEDED`, `INVALID_MAGIC`, `INCOMPATIBLE_SCHEMA`, `RESERVED_FLAGS`,
`NONCANONICAL`, `UNKNOWN_DOMAIN`, `UNKNOWN_DOMAIN_VERSION`,
`DUPLICATE_DOMAIN`, `DOMAIN_ORDER`, `TRAILING_BYTES`, `INVALID_LENGTH`,
`UNKNOWN_ACTION_KIND`, `INVALID_OPERAND`, `READ_ERROR`, `RESERVED_FIELDS`,
`DUPLICATE_TARGET`, `ROW_ORDER`, `INVALID_TARGET`, `DUPLICATE_STEP`,
`STEP_ORDER`, `INVALID_TAP_INDEX`, `EMPTY_STEP`, `INVALID_ACTION`,
`INVALID_HOLD_MODE`, `INVALID_REPEAT_RATE`, `ROW_LENGTH`, `COUNT_MISMATCH`,
`WRONG_DOMAIN`, and the caller faults `INVALID_ARGUMENT` and
`OUTPUT_TOO_SMALL`. The header, envelope and action ones are defined in
[blob-and-digests.md](blob-and-digests.md); the row and step ones in
[key-behavior-domain.md](key-behavior-domain.md).

Whole-profile validator (`noah_profile_validator_v1_result_t`):

| Name | Meaning |
| --- | --- |
| `READ_ERROR` | the profile could not be read |
| `INCOMPATIBLE_SCHEMA` | another schema |
| `INCOMPATIBLE_ACTION_ABI` | another action-ABI digest |
| `CAPACITY_EXCEEDED` | over a size or count ceiling |
| `CHECKSUM_MISMATCH` | the CRC32 or digest is not the declared one |
| `INVALID_BLOB` | the header or envelope is refused |
| `UNSUPPORTED_DOMAIN` | a domain the build does not admit |
| `MISSING_DOMAIN` | a required domain is absent (the pointing domain) |
| `DOMAIN_MASK_MISMATCH` | the domains present are not the declared mask |
| `INVALID_DOMAIN` | a domain's payload is refused |
| `INVALID_REFERENCE` | an action names something that does not exist, or stands where it may not ([placement](blob-and-digests.md#where-an-action-may-be-placed)) |

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_profile_candidate_store_backend_tests.sh` (`profile_candidate_store_backend_test.c`, the mapping to `12` and `14`).

## Live-edit results

Byte 5 of the reply to every live-edit SET, Save, Revert, edit-mode SET and
Restart, and the edit status's last result
([live-edits.md](live-edits.md), which gives the order they are answered in).

| Result | Name | Meaning |
| ---: | --- | --- |
| 0 | `OK` | done |
| 1 | `QUEUED` | accepted; the outcome is the edit status's last result |
| 2 | `STALE` | the session or revision named is not the current one; the reply carries the current pair |
| 3 | `BUSY` | another edit, a Revert, a save, an Apply or a pending keyboard adjustment holds the working copy, or the halves do not yet agree |
| 4 | `INVALID_ARGUMENT` | a malformed report (transaction id zero, nonzero padding) or a value the owner refuses |
| 5 | `INVALID_STATE` | this half's owner is not ready or has stopped |
| 6 | `WRONG_TRANSACTION` | the report names a transaction other than the edit in progress |
| 7 | `OUT_OF_ORDER` | a splice or data report out of sequence, or a repeat that does not match what was taken |
| 8 | `CAPACITY_EXCEEDED` | the result or its staging would not fit the slot payload |
| 9 | `CHECKSUM_MISMATCH` | the inserted stream's CRC-32 is not the declared one |
| 10 | `UNSUPPORTED_DOMAIN` | a splice names a domain the build does not admit |
| 11 | `INVALID_SPLICE` | splices overlap, are out of order, or create or drop a domain wrongly |
| 12 | `VALIDATION_REJECTED` | a touched domain would be invalid; nothing changed; edit status page 2 says where |
| 13 | `NOT_CLEAN` | needs no unsaved edits: an edit-mode change, or a replacement, over unsaved edits or an unsaved keyboard adjustment |
| 14 | `READ_ERROR` | the profile could not be read, or this half has nowhere to store the edit mode |
| 15 | `ABORTED` | the edit was abandoned: aborted, left unfinished past its lease or by a Revert, or a save, an Apply or the slave half's newer profile replaced the profile it was made against |
| 16 | `PUBLICATION_FAILED` | the checked edit could not be published |

Held by: `tests/host/run_qmk_profile_edit_channel_tests.sh` (`qmk_profile_edit_channel_test.c`), `tests/host/run_profile_working_copy_tests.sh` (`profile_working_copy_test.c`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`).

## Split statuses

The slave half's last answer to the master half, as candidate status page 1
byte 2 reports it ([candidate-transfer.md](candidate-transfer.md)). The split
protocol defines them ([profile-split-v1.md](../profile-split-v1.md)).

| Status | Name |
| ---: | --- |
| 0 | `OK` |
| 1 | `INVALID_FRAME` |
| 2 | `INCOMPATIBLE` |
| 3 | `STALE` |
| 4 | `CONFLICT` |
| 5 | `CORRUPT` |
| 6 | `BUSY` |
| 7 | `RANGE_ERROR` |
| 8 | `DIGEST_MISMATCH` |
| 9 | `STORAGE_ERROR` |
| 10 | `VALIDATION_ERROR` |
| 11 | `SOURCE_UNAVAILABLE`: the slave half cannot copy a reuse range from its own profile, so the bytes are sent instead |

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`profile_split_reconciler_test.c`).

## Busy reasons

Why the slave half answered BUSY, as the split protocol carries it, and as
candidate status page 1 byte 22 reports the last one that was not `1`.

| Reason | Name | Meaning |
| ---: | --- | --- |
| 0 | `UNSPECIFIED` | no reason given |
| 1 | `ADMITTED` | queued; the answer comes on a retry |
| 2 | `MAILBOX_FULL` | an earlier request is still unprocessed; if it persists, the slave half is not getting scan time |
| 3 | `OTHER_COPY` | its store holds a different copy |
| 4 | `NO_LEASE` | its store is not receiving this copy |
| 5 | `STORE_WORKING` | its store is validating, preparing or committing |
| 6 | `PULLING` | it is pulling a profile itself |
| 7 | `CONVERGENCE_ONLY` | it only converges just now |
| 8 | `COPYING` | it is copying a reuse range; the offset is its progress |
| 9 | `STORAGE_PAUSED` | its save waits for a quiet keyboard before an erase, or for flash time; counted as progress, not a stall |

Profile Wire reports `8` and `9` as `5`, so candidate status page 1 carries
only `0`–`7`. The split protocol's use of each is
[profile-split-v1.md](../profile-split-v1.md)'s.

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`profile_split_reconciler_test.c`), `tests/host/run_profile_store_runtime_owner_tests.sh` (`profile_store_runtime_owner_test.c`); the fold of `8` and `9` into `5` on Profile Wire: no test.

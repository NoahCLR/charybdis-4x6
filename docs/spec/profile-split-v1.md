# Profile Split Protocol V1

The split protocol's profile part: how the master half and the slave half
compare their saved profiles, copy a saved profile or a candidate from one
half to the other, keep the working copy and the edit mode the same on both,
and restart together. What an Apply or a Save commits, and when, is
[`logical-profile-transaction-v1.md`](logical-profile-transaction-v1.md)'s;
the owner states around it are
[`authority-state-table.md`](authority-state-table.md)'s; slots, markers and
flash pacing are [`device-resident-profile.md`](device-resident-profile.md)'s;
digests and the domain mask are
[`profile-wire-v1/blob-and-digests.md`](profile-wire-v1/blob-and-digests.md)'s;
the split link itself and runtime state are
[`split-activity-sync.md`](split-activity-sync.md)'s.

## Split RPCs

Each protocol is one QMK split RPC, registered in this order in
`SPLIT_TRANSACTION_IDS_USER` (`users/noah/config.h`). Both halves run one
build, so they agree on every id.

| Order | RPC | Carries |
| ---: | --- | --- |
| 1 | `PUT_SPLIT_RUNTIME_BASE_SYNC` | runtime state ([split-activity-sync.md](split-activity-sync.md#runtime-state-packets)) |
| 2 | `PUT_SPLIT_COMBO_FEEDBACK_SYNC` | runtime state |
| 3 | `PUT_SPLIT_KEY_FEEDBACK_SEMANTIC_SYNC` | runtime state |
| 4 | `PUT_SPLIT_KEY_FEEDBACK_BRANCH_SYNC` | runtime state |
| 5 | `PUT_PROFILE_SPLIT_SYNC` | the saved-profile frames below |
| 6 | `PUT_PROFILE_FLASH_DIAGNOSTICS` | flash pause figures ([hid-channel.md](profile-wire-v1/hid-channel.md)) |
| 7 | `PUT_PROFILE_WORKING_SYNC` | [working copy sync](#working-copy-sync) |
| 8 | `PUT_KEYBOARD_RESTART` | [keyboard restart](#keyboard-restart) |

- The master half is the only initiator. It sends from the scan's durable-I/O
  step, at most one exchange per step, blocking for one RPC; every
  `PUT_PROFILE_SPLIT_SYNC` or `PUT_PROFILE_WORKING_SYNC` request is reported
  to the durable-I/O grant, which then gives that scan no further step.
- A request goes at its own length and its reply is read at the length the
  request asks for. A length outside the protocol's bounds is refused unsent.
  `RPC_M2S_BUFFER_SIZE` and `RPC_S2M_BUFFER_SIZE` are 128, the largest frame
  either way; every other split RPC keeps its own small frames.
- On the slave half the callback runs on the split slave thread. It only
  copies a request into its protocol's one-frame mailbox and answers from the
  last published reply; the scan does the work. A request the protocol
  refuses (wrong length, bad framing, no endpoint) is answered with zeros, so
  a stale reply buffer is never read as an answer.
- `PUT_PROFILE_SPLIT_SYNC` and `PUT_PROFILE_WORKING_SYNC` are registered
  together, once per boot, after the profile owner has built both endpoints;
  a second registration is refused. `PUT_KEYBOARD_RESTART` is registered from
  housekeeping, apart from the owner.

Held by: `tests/host/run_qmk_profile_split_transport_tests.sh` (`qmk_profile_split_transport_test.c`), `tests/host/run_qmk_keyboard_restart_tests.sh` (`qmk_keyboard_restart_test.c`).

## Framing

- Byte 0 of every `PUT_PROFILE_SPLIT_SYNC` frame is the framing version, `4`.
  A frame of any other version is refused; a half on another build reads its
  sibling's frames as invalid and reports it unreadable.
- Every frame is 32 bytes except a payload chunk and a payload reuse. A
  payload chunk is its 21-byte header, the chunk and the CRC: 22 + n bytes for
  an n-byte chunk, never under 32 (a shorter chunk is zero-padded). A payload
  reuse is 44 bytes. The largest frame is 128 bytes, so a chunk carries at
  most 106 bytes, pushed as a request or pulled as a reply (D-F28,
  [0028](../adr/0028-new-base-32-bit-profile-lengths.md); D-F19,
  [0019](../adr/0019-peer-copy-reuses-what-the-other-half-has.md)).
- The last byte is a CRC8, polynomial `0x07`, initial value 0, over every
  byte before it. Multi-byte values are little-endian. Every offset and length
  is 32 bits. Every reserved or unused byte is zero, and a frame with a
  nonzero one is refused.
- A decoder takes exactly one frame of the length its kind declares.
- Every request asks for a 32-byte reply except a payload request, which asks
  for exactly the frame of the chunk it pulls: what remains of the payload
  from its offset, up to 106 bytes. The reply starts the buffer; a busy or
  error answer is a 32-byte frame there with the rest zero. A reply with
  anything nonzero past its frame is refused as torn.

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`profile_split_foundation_test.c`: golden frames, malformed and sized frames, reply buffers, payloads over 64 KiB).

## Frame kinds

| Kind | Id | Layout | Purpose |
| --- | ---: | --- | --- |
| `METADATA` | `1` | descriptor | announce this half's saved profile; a request and its reply |
| `PREPARE_BEGIN` | `2` | descriptor | open the receiver's copy of exactly this record |
| `PAYLOAD_CHUNK` | `3` | transfer | carry profile bytes, pushed or as a pulled reply |
| `PREPARE_COMMIT` | `4` | descriptor | commit the received copy, marker last |
| `ABORT` | `5` | descriptor | abandon the correlated copy |
| `ACK` | `6` | transfer | accepted progress (`OK`) or `BUSY` |
| `ERROR` | `7` | transfer | a terminal or retryable error |
| `PAYLOAD_REQUEST` | `8` | transfer, no chunk | pull bytes of the other half's saved profile |
| retired | `9` | none | refused like an unknown kind (D-F31, [0031](../adr/0031-via-is-not-supported.md)) |
| `PREPARE_DURABLE` | `10` | descriptor | validate the received copy and make only its prepared marker durable |
| retired | `11` | none | refused like an unknown kind (D-F31) |
| `PAYLOAD_REUSE` | `12` | reuse | fill a range of the copy from the receiver's own active profile |

A descriptor kind other than `METADATA` requires status `OK` and a readable
descriptor with a profile. An `ACK` carries `OK` or `BUSY` only; an `ERROR`
carries any status but those two. `PREPARE_BEGIN` is a copy's first
exchange.

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`profile_split_foundation_test.c`).

## Status codes

| Status | Id | Sent when |
| --- | ---: | --- |
| `OK` | `0` | success |
| `INVALID_FRAME` | `1` | the request did not decode (generation and digest zero), or its descriptor was invalid metadata |
| `INCOMPATIBLE` | `2` | schema, compiled-default digest, action-ABI digest, size or domain mask is not this build's |
| `STALE` | `3` | the copy is older than the receiver's saved profile, or a pull names a profile the sender no longer holds |
| `CONFLICT` | `4` | equal generation from the other origin; an `ABORT` for a copy already committed |
| `CORRUPT` | `5` | equal generation and origin that disagree; a repeated chunk whose bytes differ; a committed record that reads back differently |
| `BUSY` | `6` | in `ACK` only |
| `RANGE_ERROR` | `7` | a gap, partial overlap or out-of-range chunk |
| `DIGEST_MISMATCH` | `8` | defined; this build never sends it |
| `STORAGE_ERROR` | `9` | a flash read or write failed, or durability is unknown |
| `VALIDATION_ERROR` | `10` | whole-profile validation refused the copy |
| `SOURCE_UNAVAILABLE` | `11` | answering a `PAYLOAD_REUSE` only ([Payload reuse](#payload-reuse)) |

How the sender takes an error:

- `INVALID_FRAME` with generation and digest zero is a lost exchange, like a
  garbled reply: every error about a copy names its generation and digest.
- `STALE` or `BUSY` sends it back to the metadata poll after the backoff.
- `STORAGE_ERROR` for a prepared push not yet authorized to commit restarts
  the copy (see [Sending a prepared push](#sending-a-prepared-push)).
- Anything else stops the reconciler with that status. A stopped reconciler
  resumes the metadata poll once this half's own saved profile changes.

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`profile_split_reconciler_test.c`), `tests/host/run_profile_peer_store_backend_tests.sh` (`profile_peer_store_backend_test.c`).

## Descriptor frame

`METADATA`, `PREPARE_BEGIN`, `PREPARE_DURABLE`, `PREPARE_COMMIT` and `ABORT`:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | kind |
| 2 | 1 | status |
| 3 | 1 | flags, below |
| 4 | 1 | schema major |
| 5 | 1 | schema minor |
| 6 | 1 | domain mask of the payload |
| 7 | 4 | payload length |
| 11 | 4 | generation |
| 15 | 4 | payload CRC32 |
| 19 | 4 | payload FNV-1a digest |
| 23 | 4 | compiled-default digest |
| 27 | 4 | action-ABI digest |
| 31 | 1 | CRC8 |

| Flag bit | Meaning |
| ---: | --- |
| 0 | has a saved profile |
| 1 | readable |
| 2 | refused when set |
| 3 | origin half: 0 left, 1 right |
| 4 | the record's override flag, its one persistent flag |
| 5 | `METADATA` only: the half that sent it holds no prepared push ([An abandoned prepared copy](#an-abandoned-prepared-copy)); zero in every other kind |
| 6–7 | refused when set |

A descriptor is valid only in one of three forms:

- Unreadable: every field zero, flag bit 5 aside.
- Readable compiled defaults: no saved profile; schema exactly this build's;
  generation, length, CRC, digest, domain mask, flags and origin zero; the
  compiled-default and action-ABI digests present.
- Readable saved profile: schema exactly this build's, nonzero generation,
  origin 0 or 1, a length from the blob header's size up to the maximum
  profile, a domain mask with only known domain bits, and only the override
  flag.

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`profile_split_foundation_test.c`: descriptor flags, golden frames).

## Transfer, acknowledgement and error frame

`PAYLOAD_CHUNK`, `ACK`, `ERROR` and `PAYLOAD_REQUEST`:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | kind |
| 2 | 1 | status |
| 3 | 1 | reserved, zero |
| 4 | 4 | generation of the copy |
| 8 | 4 | payload digest of the copy |
| 12 | 4 | offset: the chunk's, or the acknowledged next offset |
| 16 | 4 | payload length |
| 20 | 1 | chunk length: `1..106` in a chunk, `0` otherwise |
| 21 | n | chunk bytes, then zero padding to byte 30 when n < 10 |
| 21 + n, at least 31 | 1 | CRC8 |

- A chunk requires `OK`, a nonzero generation, a length of at least the blob
  header, a nonempty chunk, and `offset + n` within the payload.
- A payload request requires `OK`, a nonzero generation and an offset inside
  the payload. Its reply is a chunk at exactly that offset, as long as its
  buffer allows, or an `ERROR`: `STALE` when this half's saved profile is no
  longer the named one, `STORAGE_ERROR` when the read fails.
- Generation, digest and length correlate every frame of one copy; the
  copy's other identity travels in its `PREPARE_BEGIN` descriptor.

An `ACK` with `BUSY` says why in the chunk's bytes, its chunk length still
zero; every other frame keeps them zero.

| Offset | Field | Values |
| ---: | --- | --- |
| 21 | busy reason | table below |
| 22 | receiver's store state | `0` uninitialized, `1` idle, `2` receiving, `3` validating, `4` preparing, `5` prepared, `6` committing, `7` committed, `8` rejected, `9` reconcile required |
| 23 | receiver's transfer owner | `0` none, `1` receiving a push, `2` pulling |
| 24 | holder of the receiver's storage admission | `0` none, `1` host, `2` peer |

| Busy reason | Id | Meaning |
| --- | ---: | --- |
| unspecified | `0` | |
| admitted | `1` | queued; the answer comes on a retry |
| mailbox full | `2` | an earlier request is still unprocessed |
| other copy | `3` | the store holds a different copy |
| no lease | `4` | the store is not receiving this copy |
| store working | `5` | validating, preparing or committing |
| pulling | `6` | the receiver is pulling a profile itself |
| convergence only | `7` | the receiver only converges just now |
| copying | `8` | copying a `PAYLOAD_REUSE` range; the offset is its progress |
| storage paused | `9` | the receiver's storage step is not running (below) |

The offset of a `BUSY`:

- written by the callback (admitted, mailbox full, storage paused): the
  request's own offset, `0` for a descriptor kind, with bytes 22–24 from the
  receiver's last scan;
- written by the scan for a copy: the store's next offset when the store
  holds that copy, else `0`;
- while copying a reuse range: how far the copy got.

Storage paused replaces mailbox full when the receiver's storage step may not
run: its save waits for a quiet keyboard before the next erase, or the scan's
flash time is spent ([device-resident-profile.md](device-resident-profile.md)).
That is not a stall: the receiver's own timers stand still meanwhile, and the
sender counts each storage-paused reply as progress toward the save's
deadlines (D-F32, [0032](../adr/0032-live-edits-working-copy-and-save.md)).
Profile Wire candidate status page 1 reports reasons 8 and 9 as 5
([candidate-transfer.md](profile-wire-v1/candidate-transfer.md)).

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`profile_split_foundation_test.c`), `tests/host/run_profile_split_reconciler_tests.sh` (`profile_split_reconciler_test.c`), `tests/host/run_profile_owner_tests.sh` (`profile_owner_test.c`: `test_waiting_for_a_quiet_keyboard_never_ends_a_save`).

## Payload reuse

A `PAYLOAD_REUSE` asks the receiver to fill `length` bytes of the copy at
`offset` from its own active profile at `source offset`, instead of
receiving them (D-F19).

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | kind, `12` |
| 2 | 1 | status, `OK` |
| 3 | 1 | reserved, zero |
| 4 | 4 | generation of the copy |
| 8 | 4 | payload digest of the copy |
| 12 | 4 | destination offset in the copy |
| 16 | 4 | payload length |
| 20 | 4 | length, at least 1 and within the copy |
| 24 | 4 | source offset; source offset + length within the maximum profile |
| 28 | 4 | source generation |
| 32 | 4 | source digest |
| 36 | 4 | source CRC32 |
| 40 | 1 | source kind |
| 41 | 1 | source origin |
| 42 | 1 | reserved, zero |
| 43 | 1 | CRC8 |

| Source kind | Id | Generation and origin | Digest and CRC32 |
| --- | ---: | --- | --- |
| compiled defaults | `0` | generation 0, origin `255` | the compiled profile's |
| saved profile | `1` | nonzero generation, origin 0 or 1 | the saved profile's |
| working copy | `2` | its base's: either of the two above | revision in digest, session in CRC32 |

The source identity is Profile Wire's candidate `REUSE` source
([candidate-transfer.md](profile-wire-v1/candidate-transfer.md)).

Receiving a range:

- The receiver takes a range only for the copy it is receiving, starting at
  or before its next offset; a later start is a gap and rejects the copy.
- It copies from its next offset to the range's end, at most one flash page
  (256 bytes) per scan, in reads and writes of at most 20 bytes, checking the
  source's whole identity and its compiled-default and action-ABI digests on
  every read. Meanwhile it answers `BUSY`, reason copying, with its progress;
  once staged it answers `ACK` with the range's end.
- Admission caches the first `BUSY`; copying starts on a later scan. Any new
  request ends an unfinished range where it stands, so the store's next
  offset is always the truth. Copying keeps the receive lease alive.
- When its active profile is not the named source, or stops being it, the
  receiver answers `ERROR` `SOURCE_UNAVAILABLE` with how far it got. The copy
  stays open at that offset, and the sender sends the rest of the copy as
  bytes. The whole-copy checksum and validation before the prepared marker
  are unchanged, so a reuse cannot make an invalid copy durable. The active
  profile's slot is pinned while a copy is staged, so the slot being written
  is never the source.

Sending ranges:

- While a client uploads a candidate, each `REUSE` step it stages is
  recorded beside the candidate; contiguous steps from one source merge, up
  to 16 ranges, and the rest is sent as bytes. A Save's candidate is built by
  reusing the working copy, so its copy to the slave half names source kind 2.
- A prepared push asks for the first range ending past its offset, sends
  chunks up to it and a reuse for it, resuming mid-range when the receiver's
  offset is inside one. A range the frame cannot carry, or a source refused
  once, sends the rest of that copy as bytes.
- After this half's own commit marker the staged candidate is gone; a copy
  that has to start over is sent from the saved record as bytes. Background
  repair always sends bytes.
- Profile Wire candidate status page 2 reports reused bytes, reuse exchanges,
  source-unavailable replies, prepared-push scans and exchanges
  ([candidate-transfer.md](profile-wire-v1/candidate-transfer.md)).

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`profile_split_reconciler_test.c`: `test_reuse_*`, `test_an_unencodable_reuse_plan_sends_bytes`, `test_wide_reuse_through_the_reconciler`), `tests/host/run_profile_peer_store_backend_tests.sh` (`test_reuse_fills_ranges_from_the_active_profile`), `tests/host/run_profile_owner_tests.sh` (`test_committed_reuse_saves_both_halves_and_reboots`).

## Requests and replies

The slave half's callback and scan answer `PUT_PROFILE_SPLIT_SYNC`:

- A request that does not decode is answered `ERROR` `INVALID_FRAME` with
  generation and digest zero.
- `METADATA` is answered at once with this half's published `METADATA`
  frame and also queued for the scan. Before a frame is published it is
  answered `BUSY`, admitted.
- Any other request is answered from the cached reply when it is
  byte-identical to the request that reply answered and nothing waits in the
  mailbox; else it is queued and answered `BUSY`, admitted; with the mailbox
  full it is answered `BUSY`, mailbox full or storage paused. Requests are
  taken in the order they came: nothing is answered from the cache while a
  request waits.
- The scan takes one request from the mailbox and caches its reply, which a
  byte-identical retry collects. A cached `BUSY` is dropped on the next scan
  unless a reuse range or a store step will replace it, so the retry is
  admitted again.
- An `ACK` or `ERROR` sent as a request is answered `INVALID_FRAME`.
- The scan publishes this half's store state, transfer owner and admission
  each scan for the callback's `BUSY` replies.

The master half retries a request byte for byte until a reply for it
arrives. A reply that fails its framing or correlation is a lost exchange:
the sender invalidates its view of the slave half, goes back to the metadata
poll after the backoff, and a prepared push resumes where it was.

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`test_receive_refuses_anything_but_one_frame`, `test_lost_reply_after_admission_is_idempotent`, `test_a_cached_reply_waits_behind_a_queued_poll`, `test_prepared_push_survives_one_lost_exchange_anywhere`, `test_prepared_push_survives_one_garbled_request_anywhere`, `test_prepared_push_survives_one_replayed_reply_anywhere`).

## Authority

Each half compares its own descriptor with the other half's:

| Condition, in order | Result |
| --- | --- |
| either descriptor invalid | invalid metadata |
| this half unreadable | local unreadable |
| the other half unreadable | peer unreadable |
| schema, compiled-default or action-ABI digest differ | incompatible |
| neither has a saved profile | compiled converged |
| only one has a saved profile, or one has the higher generation | that half newer |
| equal generation, different origin | concurrent commit |
| equal generation and origin, any of CRC, digest, length, domain mask or flags differ | corrupt same tuple |
| otherwise | committed converged |

- Unreadable, invalid and incompatible fail closed. Incompatible, concurrent
  commit and corrupt same tuple stop the reconciler with `INCOMPATIBLE`,
  `CONFLICT` and `CORRUPT`; neither half's record is overwritten.
- A newer local profile is pushed; a newer profile on the other half is
  pulled with `PAYLOAD_REQUEST`. The transport role never decides which
  profile wins.
- Each comparison is published as one coherent snapshot. The activation
  observer counts the slave half unresolved unless the snapshot is compiled
  or committed converged with no transfer pending, and treats a failed read
  as unresolved. Invalid metadata is itself published, so an older converged
  result never outlives a malformed observation.
- This half's descriptor is read every scan, also during backoff. A changed
  value is validated, and an invalid one becomes unreadable; the change
  republishes the `METADATA` reply and the authority before any deadline
  returns.
- `PREPARE_BEGIN` is recorded as the other half's provisional intent,
  separately from its last saved descriptor, and never published as
  authority. How two provisional candidates are arbitrated is
  [`logical-profile-transaction-v1.md`](logical-profile-transaction-v1.md)'s.

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`test_authority_decision_table`, `test_authority_publication_and_activation_observer`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_concurrent_commit_stops_without_overwrite`, `test_same_tuple_corruption_stops_without_overwrite`, `test_incompatible_firmware_stops_without_write`, `test_refresh_publishes_new_local_authority_during_backoff`, `test_descriptor_changes_are_validated_before_backoff`).

## Receiving a copy

The receiver stores a copy through the one candidate store the host's Apply
uses, holding its storage admission as `PEER`.

- It keeps the sender's generation, origin, override flag, checksums,
  digests and domain mask; it makes up no identity of its own.
- It admits only a strictly newer, compatible record whose domain mask holds
  every required domain and no unknown one. The same record already saved
  answers `ACK` at its end; an older one `STALE`; an equal generation
  `CORRUPT` from the same origin or `CONFLICT` from the other. A store holding
  another unfinished copy answers `BUSY`, other copy.
- Chunks are sequential. A fully repeated chunk is accepted when its staged
  bytes compare equal; a gap, partial overlap or conflicting repeat rejects
  the copy before any further write. A chunk or reuse for a copy the store is
  not receiving is answered `BUSY`, other copy or no lease.
- `PREPARE_DURABLE` after the last byte validates the whole copy and makes
  only its prepared marker durable, then answers `ACK` at the full length. A
  `PREPARE_COMMIT` for a prepared copy commits it marker last. A
  `PREPARE_COMMIT` for a fully received copy that was never prepared
  validates and commits it in one go (repair).
- Success is reported only after the marker reads back and the stored record
  equals the copy field by field, the local slot aside; otherwise `CORRUPT`.
- A commit whose durability is unknown leaves the store needing
  reconciliation and is answered `STORAGE_ERROR`; it is never aborted or
  released.
- A copy that ends rejected always gives its storage admission back, also
  when the store already closed its prepare on a failed write or a failed
  abort.
- The receive lease: a copy still being received is aborted, and its
  admission released, 3 s (`NOAH_PROFILE_SPLIT_PREPARE_LEASE_MS`) after the
  last request for that copy, whatever else crosses the link (D-F44,
  [0044](../adr/0044-stale-peer-copy-cannot-hold-off-later-copies.md)). The
  lease never ends a copy being validated, prepared, committed or of unknown
  durability.
- `ABORT` ends a copy still receiving, validating or prepared and answers
  `ACK`. It answers `BUSY` for another copy or one past that point, `ERROR`
  `CONFLICT` for the same copy already committed, and `ACK` when no such copy
  is held (cancellation is idempotent).
- A committed copy is published to the owner only while this half still
  holds its storage for it: as the step that committed it returns, then after
  each later step until it activates and the admission is released. The store
  stays committed afterwards to answer a retry of the finished copy, so that
  state alone is never published.

Held by: `tests/host/run_profile_peer_store_backend_tests.sh` (`profile_peer_store_backend_test.c`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_stale_receive_lease_expires_while_the_link_stays_busy`, `test_receiver_recovers_when_its_abort_write_fails`, `test_receiver_recovers_after_a_storage_failure`, `test_cancel_after_prepare_rejection_is_idempotent`, `test_cancel_refuses_matching_durable_peer`), `tests/host/run_profile_owner_tests.sh` (`test_an_apply_after_taking_the_other_halfs_copy_lands`, `test_a_save_after_taking_the_other_halfs_copy_lands`).

## Repair and pull

Without a prepared push, the master half repairs whatever differs:

- It polls `METADATA`, every 1,000 ms while the halves are converged.
- A newer local saved profile is pushed as bytes: `PREPARE_BEGIN`, which
  answers at the receiver's offset, chunks, then `PREPARE_COMMIT`.
- A newer saved profile on the slave half is pulled: the master half opens
  its own copy, sends `PAYLOAD_REQUEST`s from its next offset, writes each
  pulled chunk, then validates and commits it marker last.
- While the host's Apply holds this half's storage (and while it activates
  at boot) the reconciler only converges: it still polls, serves pulls and
  pushes the host's record, but starts no pull, and answers every request
  but `METADATA`, `PAYLOAD_REQUEST` and `ABORT` with `BUSY`, convergence
  only.
- The slave half invalidates its view of the master half 3 s
  (`NOAH_PROFILE_SPLIT_PEER_TIMEOUT_MS`) after the last request it took.

Each scan the reconciler does at most one exchange, one received chunk, one
scan's share of a reuse range, one validation step (reads of at most 20
bytes) or one commit step; the durable-I/O grant decides how many steps a
scan runs ([device-resident-profile.md](device-resident-profile.md)).

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`test_compiled_convergence`, `test_newer_master_pushes_exact_record`, `test_newer_slave_is_pulled_without_role_authority`, `test_disconnect_invalidates_then_reconnects`, `test_convergence_only_*`, `test_maximum_profile_pull_is_bounded`, `test_converged_steady_state_cost_is_bounded_by_poll_deadline`, `assert_one_scan_budget`).

## Sending a prepared push

An Apply or a Save copies its candidate to the slave half as a prepared push;
when the owner starts one, authorizes its commit and cancels it is
[`logical-profile-transaction-v1.md`](logical-profile-transaction-v1.md)'s.

1. `PREPARE_BEGIN` with the candidate's descriptor, then chunks and reuse
   ranges from the receiver's offset.
2. `PREPARE_DURABLE`. The push then pauses, sending nothing, with every byte
   acknowledged and the receiver's prepared marker durable.
3. Only the owner's authorization sends `PREPARE_COMMIT`. A converged
   metadata poll on the old profile resumes the push at `PREPARE_BEGIN`.

Pacing:

- Backoff starts at 50 ms and doubles to 1,000 ms; progress resets it.
- The first `BUSY` is admission: the sender collects again after 5 ms. While
  the receiver still holds the request (admitted, mailbox full, copying and
  advancing, storage paused) it keeps that pace up to eight times in a row,
  then falls back to the backoff.
- A correlated `BUSY`, store working, at the full offset, from a receiver
  receiving this push and holding `PEER` admission, in a validating,
  preparing or committing state, is polled every 25 ms without backoff. Only
  a strictly later store state counts as progress toward the owner's
  no-progress deadline; repeats, backward states and uncorrelated replies do
  not.

Recovery:

- A chunk that meets `BUSY` after admission at another offset, or until the
  backoff reaches 1,000 ms, restarts the copy at `PREPARE_BEGIN`, which
  resumes a live lease at the receiver's offset and recreates a lost one. An
  authorized `PREPARE_COMMIT` that meets a correlated `BUSY` short of the
  full length restarts the same way; after this half's own commit marker the
  copy is then read from the saved record.
- A correlated `STORAGE_ERROR` before the commit is authorized resends the
  copy from `PREPARE_BEGIN`, up to two times; then the reconciler stops, and
  the owner ends the Apply at once with `PEER_TRANSFER_FAILED`
  ([errors.md](profile-wire-v1/errors.md)) (D-F44).
- A half that loses the master role mid-push restarts at `PREPARE_BEGIN`
  when it is master again; it never assumes the receiver kept its lease. On
  any role change a copy this half receives or pulls is aborted if it still
  can be.
- Cancelling before `PREPARE_BEGIN` was handed to the transport ends the push
  locally. Later it sends `ABORT`: an `ACK`, or a correlated `STORAGE_ERROR`
  (the receiver rejected the copy and released its storage), ends it. After
  15 s (`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS`) without either, the
  push is released and the `ABORT` becomes peer cleanup: retried at most once
  a second in idle slots, reported as *peer cleanup pending*
  ([hid-channel.md](profile-wire-v1/hid-channel.md)), and no new prepared
  push or transfer starts until it is acknowledged (D-F41,
  [0041](../adr/0041-cancelled-save-ends-in-bounded-time.md)). A push whose
  commit is authorized cannot be cancelled.

Held by: `tests/host/run_profile_split_reconciler_tests.sh` (`test_prepared_push_*`, `test_authorized_commit_survives_one_replayed_reply`, `test_store_completion_polling_requires_the_matching_full_copy`, `test_maximum_candidate_resolves_within_owner_no_progress_window`, `test_prepared_abort_is_bounded_when_peer_goes_silent`, `test_prepared_abort_is_bounded_when_peer_stays_busy`, `test_receiver_durable_prepare_survives_sender_pause`), `tests/host/run_profile_owner_tests.sh` (`test_peer_store_write_failure_retries_then_fails_fast`, `test_host_abort_ends_when_peer_never_acknowledges`).

## An abandoned prepared copy

A prepared copy is ended only by its push: `PREPARE_COMMIT` commits it and
`ABORT` releases it; the receive lease never times it (D-F34,
[0034](../adr/0034-stopped-keyboard-says-so-and-restarts.md)). A push lives
in its sender's RAM, so a sender that restarts forgets it without ending it.
Flag bit 5 of a `METADATA` request closes that gap.

- The master half sets bit 5 in every poll while it holds no prepared push:
  from boot, and from the end of a push (converged, `ABORT` acknowledged or
  answered with a storage error, cancelled before `PREPARE_BEGIN` was sent,
  or released unacknowledged) until the next push begins. From the start of a
  push to its end the bit is clear. A clear bit claims nothing; replies and
  every other kind send it clear.
- The receiver releases its copy, writing its invalid marker and giving back
  its `PEER` admission, when it takes a poll with bit 5 set whose descriptor
  is readable and is not the copy, while its store is exactly prepared and it
  is not pulling. A sender whose saved profile is the copy decided to commit
  it; its repair commits the copy, `PREPARE_BEGIN` answering at the copy's end.
- A committing copy, or one of unknown durability, is never released. A poll
  that arrives while the copy is still validating or preparing is taken once
  that step ends. Nothing else releases a prepared copy: not time, nor the
  sender's silence; only a role change aborts it.
- The claim is exact: the mailbox holds one request, the sender sends one at
  a time, and nothing is answered from the cache while a request waits, so a
  poll is taken before any request of a push begun after it.
- A build without bit 5 refuses a poll that sets it, as it refuses any
  unknown flag, and never sets it; so the framing version stays 4.

Held by: `tests/host/run_profile_split_foundation_tests.sh` (`test_protocol_no_prepared_push_flag`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_a_restarted_sender_settles_the_prepared_copy`, `test_a_paused_sender_keeps_the_prepared_copy`, `test_a_sender_holding_the_push_keeps_the_prepared_copy`, `test_a_committing_copy_is_never_released`, `test_a_cached_reply_waits_behind_a_queued_poll`).

## Physical origin

A saved profile's origin is the half that made it, never the transport
role. It comes from `NOAH_PHYSICAL_HALF` (`left` origin 0, `right` origin
1), built into each half's image; the right half is also `FORCE_MASTER` and
the left `FORCE_SLAVE`. The live-profile owner is built only into an image
with a provisioned half. The reconciler preserves `origin_half` across role
changes.

Held by: `tests/host/run_qmk_physical_half_tests.sh` (`qmk_physical_half_test.c`), `tests/host/run_split_transport_build_tests.sh` (`split_transport_build_test.py`: `test_pair_flags_and_artifact_names`), `tests/host/run_profile_split_reconciler_tests.sh` (`test_role_change_restarts_and_preserves_physical_origin`).

## Working copy sync

Live edits change each half's working copy in RAM; nothing here writes flash
or touches a slot, so it has its own RPC, `PUT_PROFILE_WORKING_SYNC`, and its
own one-frame mailbox (D-F32). What an edit, a Save and a Revert are is
[`profile-wire-v1/live-edits.md`](profile-wire-v1/live-edits.md)'s.

Requests are 32 bytes; a data frame is its bytes' size, at most 128. Every
frame ends with a CRC-16/CCITT (polynomial `0x1021`, initial `0xFFFF`) of
everything before it, little-endian.

| Offset | Size | Request field |
| ---: | ---: | --- |
| 0 | 1 | kind |
| 1 | 2 | sequence |
| 3 | 2 | the edit's transaction on the receiving half |
| 5 | 4 | session |
| 9 | 4 | revision |
| 13 | … | the kind's fields |
| last 2 | 2 | CRC-16 |

| Kind | Id | Fields from byte 13 |
| --- | ---: | --- |
| status | `1` | none |
| begin | `2` | splice count, flags, inserted bytes (4); session and revision are the base |
| splice | `3` | index, domain id, flags, offset (4), removed (4), inserted (4) |
| data | `4` | offset (4), count (1–108), the bytes |
| commit | `5` | CRC-32 of the inserted stream |
| abort | `6` | none; the transaction names the edit |
| revert | `7` | none; the receiver drops its working copy, whatever it was |
| adopt | `8` | saved generation (4), origin, compiled flag; session and revision are the identity to take |
| mode | `9` | counter (4), mode |

Every reply is 32 bytes:

| Offset | Size | Reply field |
| ---: | ---: | --- |
| 0 | 1 | the request's kind with bit 7 set |
| 1 | 2 | the request's sequence |
| 3 | 1 | result: a working-copy result ([errors.md](profile-wire-v1/errors.md)), `0xFE` admitted or `0xFF` busy |
| 4 | 4 | session |
| 8 | 4 | revision |
| 12 | 1 | phase |
| 13 | 1 | last result |
| 14 | 2 | transaction in progress, else the last |
| 16 | 1 | flags: bit 0 unsaved edits, bit 1 its saved profile is the compiled defaults, bit 2 its owner stopped |
| 17 | 4 | saved generation |
| 21 | 1 | saved origin |
| 22 | 4 | edit mode counter |
| 26 | 1 | edit mode |
| 27 | 3 | zero |
| 30 | 2 | CRC-16 |

Exchange:

- The master half is the only initiator. The slave half's callback copies a
  request into the mailbox and answers `0xFE`; with the mailbox full it
  answers `0xFF`; a request whose kind and sequence match the last published
  reply gets that reply. `0xFE` and `0xFF` are not replies: the request is
  sent again, same kind and sequence, every 2 ms until one arrives.
- The master half polls status every 100 ms while the halves differ or an
  edit is on its way, every 2 ms while it waits for an edit to land, and
  every second while they agree. A half not heard from for 3 s is unknown
  again.

Bringing the slave half to the master half's working copy:

- Nothing is sent while the halves' saved profiles differ; the reconciler
  sees to those first.
- After each edit the master half publishes, it sends the same edit (begin,
  splices, data, commit) and polls until the slave half reports the same
  session and revision. The slave half checks it by a saved record's rules,
  not a candidate's, and publishes it at its own safe boundary.
- That is sent as an edit only when the halves were last seen agreeing at the
  revision just before it. The agreement is forgotten whenever the master
  half sends revert or adopt, or loses the slave half for 3 s.
- In every other disagreement it brings the slave half back: revert a working
  copy that is not its own, adopt its session and revision over the same
  saved profile, or adopt the revision before it and receive the whole
  working copy as a replacement.
- An edit the slave half refuses for the edit's own sake (anything but stale,
  busy, invalid state, wrong transaction or not clean) is not sent again: the
  whole working copy follows, once. If that is refused too, the halves stay
  apart until a Revert or an Apply, and edit status reports `PEER_REFUSED`.
- A Revert, an Apply or a newer profile on the master half ends an edit on
  its way; an unfinished edit on the slave half is aborted.
- New edits wait until the slave half holds the same session and revision,
  so at most one edit is ever behind
  ([live-edits.md](profile-wire-v1/live-edits.md)).

A stopped half (D-F34) keeps answering: a status poll with `OK`, anything
else with `INVALID_STATE`, every reply with flag bit 2, and acts on nothing.
The master half then sends it only a status poll each second and reports
`PEER_STOPPED`. A stopped master half sends nothing.

The edit mode record travels on the same RPC. Each reply carries the slave
half's counter and mode; the master half takes a higher one and sends its
own when the slave half's is lower. A setting from a client gets a counter
one above both halves'. A half holds a new record at once and writes it when
its keyboard is quiet (no key held and the matrix unchanged for 50 ms), as
QMK's EEPROM is wear-levelled flash that may erase. A failed write is retried
after 1 s, the wait doubling to 64 s, and reported as `MODE_NOT_STORED` until
one succeeds.

Held by: `tests/host/run_profile_working_sync_tests.sh` (`profile_working_sync_test.c`), `tests/host/run_qmk_profile_split_transport_tests.sh` (`qmk_profile_split_transport_test.c`).

## Keyboard restart

A client's restart of both halves (Profile Wire SET `0x28`,
[live-edits.md](profile-wire-v1/live-edits.md)) reaches the slave half on
`PUT_KEYBOARD_RESTART` (D-F34). Each half registers it from housekeeping,
apart from the profile owner, so a half takes it whatever its owner's state.

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | kind: `0x01` request, `0x81` acknowledgement |
| 1 | 4 | `52 45 53 54` ("REST") |
| 5 | 1 | version, `1` |
| 6 | 2 | CRC-16/CCITT of bytes 0–5, little-endian |

- Each frame is one fixed byte sequence. Any other request, or a reply
  buffer not 8 bytes, is answered with zeros and changes nothing.
- The slave half's callback notes the restart and acknowledges; its
  housekeeping resets the half 20 ms later, between main-loop passes, so never
  inside a flash erase or program. A request repeated before then is
  acknowledged again and keeps the first time.
- The master half asks every 5 ms until it is acknowledged or 250 ms have
  passed, then restarts itself 50 ms after the request either way. Asked
  again before then, it asks nothing and answers as the first time.
- A master half that restarts alone forgets a save in flight. The slave
  half's copy ends by itself: a copy still being received when its lease
  runs out, a prepared one at the first poll it takes once the restarted half
  has read its saved profile ([An abandoned prepared copy](#an-abandoned-prepared-copy)).

Held by: `tests/host/run_qmk_keyboard_restart_tests.sh` (`qmk_keyboard_restart_test.c`).

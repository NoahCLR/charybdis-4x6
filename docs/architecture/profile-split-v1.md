# Profile Split Protocol V1

> Current firmware accepts only the formats it writes (D-F10, D-F14, D-F28):
> profile schema 4.0; RGB v4, key behaviors v2, combos v3, settings v8 and
> sparse PD v3, keymap v1 and macros v1; a slot-sized profile payload; and logical store format 6. The
> profile is the only store (D-F31). Profile Wire is 2.0; split
> framing is version 4 (VIA removed, D-F31; 32-bit offsets and lengths since
> version 3, D-F28, after version 2's
> sized payload chunks and reuse ranges, D-F19).
> Older profile/store formats and the legacy GET 9 source page are rejected.
> A backup in an earlier format is refused, not translated.

Status: accepted internal split foundation and distributed commit barrier with
gated owner registration; normal mutation exposure and hardware acceptance
remain open

This is the split protocol for the durable live profile, the only store
since D-F31. Byte 0 of every frame is the framing version,
`4`; a frame of any other version is refused. Every frame except a payload
chunk or a payload reuse is exactly 32 bytes. A payload chunk is its 21-byte
header, the chunk and the CRC: 22 + n bytes for an n-byte chunk, at least 32
(a shorter chunk is zero-padded to 32). A payload reuse is 44 bytes. The last
byte of every frame is CRC8 polynomial `0x07` over the bytes before it,
multi-byte values are little-endian, and every reserved or unused byte is
zero. Every offset and length in a frame is 32 bits (D-F28). A chunk carries
at most 106 bytes, the largest frame being 128 bytes, whether it is pushed as
a request or pulled as the reply to a payload request.

Every request asks for a 32-byte reply except a payload request, which asks
for exactly the frame of the chunk it pulls: what remains of the payload from
its offset, up to 106 bytes, so 22 + n bytes and at least 32. Its reply starts
the buffer, and a busy or error answer is a 32-byte frame there with the rest
zero; a reply with anything nonzero past its frame is refused as torn.

Version 1 had only 32-byte frames and 14-byte chunks; version 2 had 16-bit
offsets and lengths, 110-byte request chunks and 14-byte reply chunks.
Version 3 first kept 32-byte replies, whose pulled chunks carried 10 bytes:
pulling a maximum profile took about 7,600 requests instead of 718. Both
halves run one build, so a half on an earlier version simply fails to decode
its sibling's frames and reports it unreadable; nothing durable is at risk.

## Frame Kinds And Status

| Kind | Id | Purpose |
| --- | ---: | --- |
| metadata | `1` | announce readable local durable state and compatibility |
| prepare begin | `2` | open exact remote-record staging |
| payload chunk | `3` | transfer canonical profile bytes |
| prepare commit | `4` | request validation and marker-last durable commit |
| abort | `5` | abandon the correlated prepare |
| acknowledgement | `6` | report accepted progress or busy state |
| error | `7` | report a stable terminal or retryable error |
| payload request | `8` | let the current QMK master pull bytes from a newer sibling |
| retired | `9` | was the VIA bind; refused since D-F31 |
| prepare durable | `10` | validate the received copy and persist only its prepared marker |
| retired | `11` | was the VIA bind request; refused since D-F31 |
| payload reuse | `12` | fill a range of the copy from the receiver's own active profile |

Status ids are OK `0`, invalid frame `1`, incompatible `2`, stale `3`,
conflict `4`, corrupt `5`, busy `6`, range error `7`, digest mismatch `8`,
storage error `9`, validation error `10`, and source unavailable `11` (only in
answer to a payload reuse, below). Descriptor operations other than
metadata require OK. Acknowledgement accepts only OK or busy; error frames
accept neither.

A busy acknowledgement says why in the unused chunk bytes: byte 21 is the busy
reason, 22 the receiver's store state, 23 its transfer owner and 24 who holds
its storage admission, with the chunk length still zero. A reply the split
callback writes itself (admission, mailbox full) takes 22–24 from the
receiver's last scan (the values are listed with candidate status page 1 in
[Profile Wire](profile-wire-v1.md)). Every other frame keeps those bytes zero.
Its offset is the receiver's progress only when the store holds the requested
copy, and `0` otherwise: another copy's offset past the request's length
cannot be encoded, and turned a busy reply into an invalid frame.

Busy reason `9`, **storage paused** (D-F32), replaces mailbox full when the
receiver's storage step is not running: a save there waits for a quiet
keyboard before its next erase (D-F29), or the scan's flash time is spent.
That wait is not a stall. The receiver's own timers do not count it (its
owner's clock stands still while its storage step may not run), and the
sender counts each storage-paused reply as progress toward the save's
no-progress and host deadlines, however long the typing goes on. Candidate
status page 1 reports it as reason `5` (store working), within the reasons
clients know.

Payload reuse also names the working copy as its source (kind `2`, D-F32),
so Save's copy to the other half is filled from that half's own working copy
(Payload Reuse, below).

## Descriptor Frame

Metadata, prepare begin, prepare commit, and abort use this layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | frame kind |
| 2 | 1 | status |
| 3 | 1 | bit 0 has committed profile; bit 1 metadata readable; bit 2 reserved zero (a VIA-bound record until D-F31); bit 3 stable physical origin half; bit 4 the persistent override flag; bits 5–7 zero |
| 4 | 1 | schema major, exactly the current schema major |
| 5 | 1 | schema minor, exactly the current schema minor |
| 6 | 1 | Profile Wire domain mask derived from the canonical payload |
| 7 | 4 | canonical payload length, uint32 |
| 11 | 4 | durable generation counter |
| 15 | 4 | payload CRC32 |
| 19 | 4 | canonical payload FNV-1a digest |
| 23 | 4 | compiled-default digest |
| 27 | 4 | action-ABI digest |
| 31 | 1 | frame CRC8 |

Version 2 kept the persistent flags and origin half in bytes 6 and 7 and the
domain mask in byte 30 beside a 16-bit length; version 3 packs both into the
flags byte (the store allows one persistent flag, override) to make room for a
32-bit length.

A readable compiled-default descriptor has no committed profile. Its
generation, payload length, payload identities, persistent flags, and origin
are all zero, while schema and firmware compatibility digests remain present.
An unreadable descriptor is entirely zero apart from the frame header. A
committed descriptor requires a nonzero generation, a blob-sized payload, a
valid physical origin, and only known persistent flags. Its domain mask may
contain only advertised RGB, key-behavior, combo, settings and PD bits and must match the domains found by
whole-profile validation before commit.

## Transfer, Acknowledgement, And Error Frame

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | frame kind |
| 2 | 1 | status |
| 3 | 1 | reserved zero |
| 4 | 4 | correlated durable generation |
| 8 | 4 | correlated payload digest |
| 12 | 4 | payload offset or acknowledged next offset, uint32 |
| 16 | 4 | total payload length, uint32 |
| 20 | 1 | chunk length: `1..106` in a payload chunk, `0` otherwise |
| 21 | n | payload bytes, then zero padding to byte 30 when n < 10 |
| last | 1 | frame CRC8: byte 31, or byte 21 + n of a longer chunk |

Payload chunks require OK, a nonzero generation, a valid complete profile
length, a nonempty chunk, and an in-range `offset + length`. Acknowledgement
and error frames carry no chunk bytes. Generation plus digest correlates
transfer progress inside the one active peer prepare; the prepare descriptor
retains the complete origin, CRC, flags, schema, and compatibility identity.

`PREPARE_BEGIN` may describe either an already durable sender record during
normal repair or a semantically validated but still provisional host candidate
during D-022 deployment. Receiving it records provisional intent separately;
it never overwrites the last durable `peer_descriptor` or publishes the intent
as peer authority. Once every byte is acknowledged, a provisional sender pauses
in `PUSH_PREPARED`. Only a later owner authorization may send
`PREPARE_COMMIT`. `ABORT` is idempotent and releases the correlated peer lease
before the sender releases its own precommit candidate. An ABORT for an exact
candidate that was never admitted or was already discarded also succeeds, but
it cannot cancel a different live transfer or an uncertain durable phase.

A payload request has the same correlation, offset, and total length fields but
no chunk. Its response is a payload chunk beginning at the exact requested
offset, as long as the requester's reply buffer: up to 106 bytes. QMK custom RPC is initiated only by the current transport master, so
this explicit pull prevents USB role from becoming durable profile authority.

## Payload Reuse

A payload reuse asks the receiver to fill `length` bytes of the copy at
`offset` from its own active profile, at `source offset`, rather than receive
them. It is 44 bytes:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | framing version, `4` |
| 1 | 1 | kind, `12` |
| 2 | 1 | status, OK |
| 3 | 1 | reserved zero |
| 4 | 4 | correlated durable generation |
| 8 | 4 | correlated payload digest |
| 12 | 4 | destination offset in the copy, uint32 |
| 16 | 4 | total payload length, uint32 |
| 20 | 4 | length, at least 1 and within the copy, uint32 |
| 24 | 4 | source offset, uint32 |
| 28 | 4 | source generation |
| 32 | 4 | source FNV-1a digest |
| 36 | 4 | source CRC32 |
| 40 | 1 | source active kind: compiled `0`, committed `1` |
| 41 | 1 | source origin: compiled `255`, committed `0..1` |
| 42 | 1 | reserved zero |
| 43 | 1 | frame CRC8 |

The source identity is Profile Wire's candidate REUSE source (compiled sources
have generation zero). The receiver holds a reuse like a chunk: only for the
copy it is receiving, at or before its next offset (a later start is a gap and
rejects the copy). It copies from its next offset to the range's end, at most
106 bytes a scan in reads and writes of at most 20, checking the source's
whole identity and its compiled-default and action-ABI digests on every read,
as Profile Wire's REUSE does. Meanwhile it answers BUSY with reason `8`
(copying) and its progress as the offset, and the ACK with the range's end
once staged. Any new request ends an unfinished range where it stands, so the
store's next offset is always the truth. The range keeps the receive lease
alive while it copies, since the sender's polls meet the cached reply.

Mailbox admission records the range and caches its initial BUSY reply; copying
starts on a later scan, after the decoded request and wire buffers retire.
Each copy step finishes its active-source read before building the progress
reply. Reply builders fill frames in place, and the dispatcher, handlers and
reuse reply boundary remain separate compiled calls so LTO cannot combine
their workspaces on the process stack. The live-owner stack manifest covers
the receive decoder, chunk writes/retries, reuse readers and progress replies,
as well as the split-slave mailbox and sender exchange paths.

When its active profile is not the named source, or stops being it, the
receiver answers ERROR `11` (source unavailable) with how far it got. That is
not a rejection: the copy stays open at that offset, and the sender sends the
rest of the copy as bytes. The receiver's whole-candidate checksum and semantic
validation before the prepared marker are unchanged, so a reuse can never
make an invalid copy durable. The active profile's backing slot is pinned
while a copy is staged, so the slot being written is never the source.

The sender takes its ranges from the host candidate: while the host uploads,
each Profile Wire REUSE step it stages is recorded beside the candidate, and
contiguous steps from one source merge (up to 16 ranges; the rest is sent as
bytes). A prepared push asks for the first range ending past its offset,
sends chunks up to it and a reuse for it, resuming mid-range when the
receiver's offset is inside one. After this half's commit marker the staged
candidate is gone and a copy that has to start over is sent from the
committed record as bytes. Background repair always sends bytes.

While the receiver still holds the sender's request (admitted, behind a full
mailbox, or copying and advancing) the sender keeps collecting at the 5 ms
admission pace, up to eight polls in a row without progress, before the
ordinary 50–1,000 ms backoff.

After the complete payload arrives, a correlated `STORE_WORKING` reply for
that exact generation, digest and length, at the full offset, with remote-push
ownership and peer storage admission, is ordinary asynchronous work. During
`PUSH_DURABLE` and `PUSH_COMMIT`, validating, preparing and committing stores
are polled every 25 ms. They do not enter exponential failure backoff just
because semantic validation takes more than one poll. The validator and
marker-last write budgets are unchanged. The cached reply is refreshed when
validation advances to preparation. The sender counts only a strictly newer
correlated store phase as progress for this copy. That advances the owner's
60-second no-progress deadline once per phase, allowing a large profile to
finish validation and readback without changing either per-scan budget.
Repeated or backwards BUSY phases, mismatched generation/digest/length,
partial offsets, another transfer owner or non-peer admission do not advance
it. A stalled phase still times out after sixty seconds. Other BUSY replies
and transport failures retain the existing backoff.

Profile Wire candidate status page 2 reports acknowledged reused bytes, reuse
RPC attempts, source-unavailable replies, prepared-push reconciler invocations
and RPC attempts. This distinguishes local copying from byte fallback without
changing the page-1 client vocabulary; see [the status layout](profile-wire-v1.md).
Diagnostic-only counters remain outside the ordinary reconciler status
snapshot, so scan-time callers do not carry a larger workspace into nested
receive/write paths. The owner reads them in its main-context HID status query.

## Authority And Activation

The comparator follows D-014:

- unreadable, malformed, unsupported, or incompatible metadata fails closed;
- no committed profile on either compatible half means compiled convergence;
- a profile on only one half or a higher generation makes that half newer;
- equal generation with different origins is a concurrent-commit conflict;
- equal generation and origin with any record disagreement is corruption;
- only exact committed records are durably converged.

The peer observer reports zero unresolved peers only for compiled or committed
convergence with no transfer in progress. An uninitialized or incoherent
publication is a failed observation, which the activation policy already
treats as unresolved.

For a peer-required live deployment, exact convergence is a two-phase barrier:

1. transfer the validated local candidate into the peer inactive slot and
   pause before either marker;
2. commit the local marker, immediately publish that exact durable descriptor,
   then authorize the peer marker;
3. exchange fresh metadata and require both durable descriptors to equal the
   just-committed candidate with no transfer pending;
4. only then request provider activation on either half.

The activation observer compares its coherent authority snapshot with the
owner's current committed descriptor. A stale previously converged publication
therefore cannot authorize a newly durable generation. Higher-generation
simultaneous provisional candidates win before durability; equal generations
use stable physical origin as the deterministic precommit tiebreaker. Durable
equal-generation/different-origin records remain D-014 conflicts and are never
silently overwritten.

## Durable Peer Receiver

`profile_peer_store_backend` reuses the one candidate-store/validator/provider
owner used by host candidates. It preserves the sender's generation, physical
origin, persistent flags, checksums, compatibility identities, and declared
domain mask instead of manufacturing a local identity. Validator flags remain
zero because they are a different namespace from persistent store flags.

The receiver admits only a strictly newer compatible record, accepts a fully
repeated chunk only after its staged bytes compare equal, and rejects gaps,
partial overlaps, conflicting retries, stale generations, concurrent origins,
and same-tuple record disagreement before destructive writes. Validation and
marker-last commit advance through the existing bounded scan-step interfaces.
Success is reported only after marker readback and an explicit field-by-field
comparison of the durable record, excluding only the local slot number.

The store now treats compiled-default digest as boot and candidate
compatibility, derives the domain mask from the checksummed canonical payload,
and latches an unconfirmed final marker as reconciliation-required. No later
prepare can invalidate either slot until a conclusive boot selection clears
that latch. The gated owner does not publish a boot-discovered exact record
until it has also advanced the bounded whole-profile adoption validator for
that exact selected record; the ordinary shell remains read-only and never
activates the record.

## Scan-Owned Reconciler And QMK Adapter

`profile_split_reconciler` is caller-owned and payload-independent. Its QMK
callback validates one exact frame, copies at most one mailbox frame, and
returns metadata, a correlated busy response, or the cached result of a prior
scan step. It never reads or writes EEPROM and never runs validation or commit.

The local descriptor callback is still queried on every scan, including during
retry backoff. The owner validates committed descriptors when publishing them;
the reconciler compares every descriptor field with its last normalized value
and validates changed values before accepting them. A failed callback or invalid
value becomes an unreadable descriptor. Changes publish metadata and authority
before any deadline return; unchanged values need neither revalidation nor a
replacement copy. This does not cache payload bytes or defer local changes.

Matrix scan performs at most one transport exchange, one received chunk or
one scan's share of a reuse range (at most 106 bytes, stored in writes of at
most 20), or one validator/marker-last commit step of at most 20 bytes. It supports newer-local
push, newer-peer pull, staged-source prepare/pause/commit/abort,
byte-identical retries, 50–1,000 ms backoff, disconnect/reconnect, prepared-
sender preservation across role changes, passive-peer expiry, and terminal
conflict/corruption/incompatibility states. Every peer loss republishes
fail-closed authority. The QMK adapter appends `PUT_PROFILE_SPLIT_SYNC`, sends each request at
its own length and reads the reply length the request asks for, and can be
registered only after a real owner initializes the reconciler.
`users/noah/config.h` raises both of QMK's RPC buffers to 128 bytes, for the
largest pushed request and the largest pulled reply. QMK carries each RPC's
own lengths, so every other split RPC still sends and reads its own small
frames.

An abandoned inbound provisional prepare has its own passive lease expiry. The
receiver aborts that incomplete inactive-slot candidate and releases `PEER`
admission 3 seconds after the last request for that copy, whatever else crosses
the link. It used to time the lease from any frame, so metadata polls, or a
newer copy's retries that the stale lease itself answered busy, kept it alive
for as long as the halves stayed connected; only a restart cleared it. A lease
no transfer owns any more expires the same way.

A sender whose chunk keeps meeting busy, after the admission reply, at another
offset or all the way to the longest backoff, restarts at `PREPARE_BEGIN`. That
is idempotent for a live lease, which resumes at the receiver's offset, and
re-creates a dropped one, where retrying the chunk would never end.

A copy the receiver rejects always returns its storage admission. The store
closes its own prepare when a write or read fails mid-copy, and an abort
whose invalidating write fails closes it too; the peer store then had nothing
left to abort, kept the `PEER` admission, and refused every later copy as
"prepare in progress" until the half was power cycled. It now releases the
admission whenever the rejected copy leaves no prepare open. A sender whose
ABORT is answered with a correlated storage error treats the cancel as done,
since the receiver has then rejected the copy and released its storage.

A receiver that cannot store a prepared copy, typically one failed flash write,
answers a correlated storage error. Until the commit is authorized nothing is
durable on either half, so the sender sends the copy again from
`PREPARE_BEGIN`, up to two times, before it stops with
that status. The owner then ends the host's Apply at once with candidate error
`24` rather than waiting out its no-progress timeout. If both halves change
transport roles, an outbound prepared source retains its candidate correlation
but restarts at `PREPARE_BEGIN` before resuming chunks or an authorized commit;
it never assumes the new receiver retained volatile prepare state. The same
rebuild applies when an authorized commit meets a receiver that no longer holds
the lease, typically because it rebooted while the link was down: the copy
restarts at `PREPARE_BEGIN`, and after its own
commit marker the sender reads the copy from its committed record because the
staged candidate no longer exists.

The reconciler preserves `origin_half`; it never derives it from current USB
role. On this `MASTER_RIGHT` board, upstream QMK falls back to
`is_keyboard_left() == !is_keyboard_master()` without a hand pin or `EE_HANDS`,
so that fallback is not a durable physical identity. D-018 now builds left
with `FORCE_SLAVE` plus `NOAH_PHYSICAL_HALF=left` and right with `FORCE_MASTER`
plus `NOAH_PHYSICAL_HALF=right`. The physical setting overrides handedness in
  flash independently of transport role. Production live mutation remains off.
  The side-specific D-021 engineering owner consumes that fail-closed identity
  boundary and registers the split transport exactly once. The additional
  mutation gate exposes the path only in labeled hardware-test artifacts.

## Working copy sync (D-F32)

Live edits change each half's working copy in RAM (see
[Profile Wire](profile-wire-v1.md#live-edits)); nothing on this path writes
flash or touches a slot, so it has a split RPC of its own,
`PUT_PROFILE_WORKING_SYNC` (QMK id 7), separate from the durable reconciler,
and its own one-frame mailbox. The USB half is the only initiator. On the
other half the RPC callback only copies a request into the mailbox and
answers from the last published reply; its scan does the work and publishes
the next reply. A request is retried with the same sequence number every
2 ms until a reply for it arrives (`0xFE` admitted and `0xFF` busy are not
replies).

Request frames are 32 bytes, a data frame longer (up to 128): byte 0 kind,
1–2 sequence, 3–4 the edit's transaction on the receiving half, 5–8 session,
9–12 revision, then the kind's fields, and a CRC-16 (CCITT) of everything
before it in the last two bytes.

| Kind | Id | Fields from byte 13 |
| --- | ---: | --- |
| status | `1` | none |
| begin | `2` | splice count, flags, inserted bytes (4); session and revision are the base |
| splice | `3` | index, domain id, flags, offset (4), removed (4), inserted (4) |
| data | `4` | offset (4), count (1–108), the bytes |
| commit | `5` | CRC-32 of the inserted stream |
| abort | `6` | none; the transaction names the edit |
| revert | `7` | none; the receiver discards its working copy, whatever it was |
| adopt | `8` | saved generation (4), origin, compiled flag; session and revision are the identity to take |
| mode | `9` | counter (4), mode |

Every reply is 32 bytes: byte 0 kind with bit 7 set, 1–2 the sequence, 3 the
result (the working copy's codes, Profile Wire), then the receiver's state:
4–7 session, 8–11 revision, 12 phase, 13 last result, 14–15 transaction (in
progress, else last), 16 flags (bit 0 unsaved edits, 1 its saved profile is
the compiled defaults), 17–20 saved generation, 21 saved origin, 22–25 mode
counter, 26 mode, 30–31 CRC-16.

The USB half polls every 100 ms while the halves differ or an edit is on its
way, every second while they agree, and forgets the other half after 3 s
without a reply. After each edit it publishes it sends the same edit (begin,
splices, data, commit) and polls until the other half reports the same
session and revision; the other half checks it with the other half's own
rules (a saved record's, not a host candidate's) and publishes it at its own
safe boundary. That is sent as an edit only when the halves were last seen
agreeing at the revision just before it. That agreement is forgotten
whenever the USB half sends revert or adopt, or forgets the other half (3 s
without a reply): both reset the other half's bytes to its saved profile, so
an identity it held before no longer says what bytes it holds. In every
other disagreement it brings the other half back: revert a working copy that
is not its own, adopt its session and revision over the same saved profile,
or adopt the revision before it and receive the whole working copy as a
replacement. A Revert, an Apply or a newer profile on the USB half ends an
edit on its way at once; the other half's unfinished edit is aborted.

An edit the other half refuses is not sent again: the USB half sends the
whole working copy instead, once (reverting and adopting the other half
first). If that is refused too, the halves stay apart until a Revert or an
Apply: Save answers BUSY meanwhile, and Profile Wire's edit status says so
(flag bit 11), so the client can offer Revert. Until the saved profiles
agree, which the durable reconciler sees to, nothing is sent. New edits on
the USB half wait until the other half holds the same session and revision
(Profile Wire's BUSY), so at most one edit is ever behind; Revert does not
wait.

The mode record travels both ways on the same RPC: each reply carries the
other half's counter and mode, the USB half adopts a higher one and sends a
lower one its own. A half takes a new record at once and writes it later,
when its keyboard is quiet (no key held and the matrix unchanged for 50 ms,
the rule before a slot erase, D-F29), because QMK's EEPROM is wear-levelled
flash that may erase. A failed write is tried again after 1 s, the wait
doubling to 64 s, and reported (edit status flag bit 12) until one succeeds.

## Remaining Exposure And Acceptance Pieces

- allocator/stack high-water evidence on both physical halves;
- the USB-orientation, reconnect, role-swap, interruption, and contention
  hardware matrix;
- real-device recovery evidence for `AUTHORITY_FAILED` after an injected
  postcommit fence loss;
- promotion of mutation routing and write/commit/activation/peer capability
  advertising beyond the explicit engineering gate.

Until those pieces pass, mutation, activation, and peer-reconciliation
capability bits remain disabled in ordinary firmware. The engineering mutation
pair advertises them together for the two-half acceptance matrix.

## No binding

Until D-F31 every copy carried the VIA generation and digest its record was
bound to, sent before PREPARE_BEGIN as LOGICAL_BIND (kind 9) or fetched before
a pull with LOGICAL_BIND_REQUEST (kind 11). The profile is now the only store:
PREPARE_BEGIN is a copy's first exchange, both kinds are refused like any
unknown kind, and descriptor flag bit 2, which marked a record bound to VIA,
is refused when set. Framing version 4 also distinguishes builds whose QMK
split call IDs moved down by two when the VIA calls were removed. Both halves
must be flashed together; earlier framing is refused as incompatible.

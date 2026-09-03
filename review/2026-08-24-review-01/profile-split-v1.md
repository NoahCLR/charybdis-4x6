# Profile Split Protocol V1

Status: accepted internal split foundation and distributed commit barrier with
gated owner registration; normal mutation exposure and hardware acceptance
remain open

This is a dedicated sibling protocol for the durable live profile. It does not
extend the VIA region reconciler. Every frame is exactly 32 bytes, multi-byte
values are little-endian, byte 31 is CRC8 polynomial `0x07` over bytes 0–30,
and every reserved or unused byte is zero. The maximum payload chunk is 14
bytes.

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

Status ids are OK `0`, invalid frame `1`, incompatible `2`, stale `3`,
conflict `4`, corrupt `5`, busy `6`, range error `7`, digest mismatch `8`,
storage error `9`, and validation error `10`. Descriptor operations other than
metadata require OK. Acknowledgement accepts only OK or busy; error frames
accept neither.

## Descriptor Frame

Metadata, prepare begin, prepare commit, and abort use this layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | protocol version, `1` |
| 1 | 1 | frame kind |
| 2 | 1 | status |
| 3 | 1 | bit 0 has committed profile; bit 1 metadata readable |
| 4 | 1 | schema major, exactly Profile Wire v1 major |
| 5 | 1 | schema minor, exactly Profile Wire v1 minor |
| 6 | 1 | persistent profile flags |
| 7 | 1 | stable physical origin half, `0` or `1` |
| 8 | 2 | canonical payload length |
| 10 | 4 | durable generation counter |
| 14 | 4 | payload CRC32 |
| 18 | 4 | canonical payload FNV-1a digest |
| 22 | 4 | compiled-default digest |
| 26 | 4 | action-ABI digest |
| 30 | 1 | Profile Wire domain mask derived from the canonical payload |
| 31 | 1 | frame CRC8 |

A readable compiled-default descriptor has no committed profile. Its
generation, payload length, payload identities, persistent flags, and origin
are all zero, while schema and firmware compatibility digests remain present.
An unreadable descriptor is entirely zero apart from the frame header. A
committed descriptor requires a nonzero generation, a blob-sized payload, a
valid physical origin, and only known persistent flags. Its domain mask may
contain only the RGB and key-behavior bits and must match the domains found by
whole-profile validation before commit.

## Transfer, Acknowledgement, And Error Frame

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | protocol version, `1` |
| 1 | 1 | frame kind |
| 2 | 1 | status |
| 3 | 1 | reserved zero |
| 4 | 4 | correlated durable generation |
| 8 | 4 | correlated payload digest |
| 12 | 2 | payload offset or acknowledged next offset |
| 14 | 2 | total payload length |
| 16 | 1 | chunk length, `0..14` |
| 17 | 14 | payload bytes followed by zero padding |
| 31 | 1 | frame CRC8 |

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
offset. QMK custom RPC is initiated only by the current transport master, so
this explicit pull prevents USB role from becoming durable profile authority.

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

Matrix scan performs at most one transport exchange, one bounded payload
read/write, or one validator/marker-last commit step. It supports newer-local
push, newer-peer pull, staged-source prepare/pause/commit/abort,
byte-identical retries, 50–1,000 ms backoff, disconnect/reconnect, prepared-
sender preservation across role changes, passive-peer expiry, and terminal
conflict/corruption/incompatibility states. Every peer loss republishes
fail-closed authority. The QMK adapter appends `PUT_PROFILE_SPLIT_SYNC`, checks
both 32-byte RPC directions, and can be registered only after a real owner
initializes the reconciler.

An abandoned inbound provisional prepare has its own passive lease expiry. The
receiver aborts that incomplete inactive-slot candidate and releases `PEER`
admission when its correlated sender activity expires. If both halves change
transport roles, an outbound prepared source retains its candidate correlation
but restarts at `PREPARE_BEGIN` before resuming chunks or an authorized commit;
it never assumes the new receiver retained volatile prepare state.

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

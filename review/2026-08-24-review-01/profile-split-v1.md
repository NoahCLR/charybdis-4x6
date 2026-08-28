# Profile Split Protocol V1

Status: accepted internal split foundation and isolated durable peer receiver;
production transport/reconciliation remains unimplemented

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
that latch. A boot-discovered exact record is not called idempotently validated
until a future boot owner has also rerun whole-profile validation.

## Deliberately Missing Production Pieces

- QMK transaction id and RPC callback registration;
- a scan-owned retry, reconnect, and role-change reconciler;
- protocol acknowledgement emission after the landed marker-last receiver;
- boot discovery publication and reset convergence;
- production activation-policy installation, diagnostics, and capabilities.

Until those pieces and their hardware matrix pass, mutation, activation, and
peer-reconciliation capability bits remain disabled.

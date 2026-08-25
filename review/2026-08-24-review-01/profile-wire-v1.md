# Profile Wire V1

Status: accepted Stage 00 wire contract

All multi-byte integers use little-endian byte order. All reserved bytes and
reserved flag bits must be zero. Encoders emit one canonical representation;
decoders reject noncanonical duplicates, ordering, padding, and unused data.

## Canonical Profile Blob

The blob is independent of Raw HID framing and EEPROM slot metadata.

### Blob Header — 8 Bytes

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `NLP1` |
| 4 | 1 | schema major, `1` |
| 5 | 1 | schema minor, initially `0` |
| 6 | 1 | domain count |
| 7 | 1 | flags; bit 0 means canonical encoding, all others reserved |

The transport candidate length or storage header supplies total blob length.
The maximum is 4,064 bytes.

### Domain Envelope — 4 Bytes Plus Payload

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | domain id |
| 1 | 1 | domain version |
| 2 | 2 | payload length |
| 4 | N | domain payload |

Domains appear once, in ascending id order, with no padding or trailing bytes.
Unknown required domains reject the candidate. Unknown optional domains are
allowed only after a future schema-minor rule explicitly defines skippability;
v1.0 rejects every unknown domain.

Initial domain ids:

| Id | Domain | Version |
| ---: | --- | ---: |
| `0x10` | Milestone A RGB | 1 |
| `0x20` | Milestone A key behaviors | 1 |
| `0x30` | Later combos | reserved |
| `0x40` | Later hardcoded macros | reserved |
| `0x50` | Later live defaults | reserved |

## Action Encoding

Actions use a four-byte tagged value:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | action kind |
| 1 | 1 | flags, initially zero |
| 2 | 2 | operand |

Initial action kinds:

| Kind | Operand |
| ---: | --- |
| 0 | none; operand must be zero |
| 1 | 16-bit standard QMK keycode under the advertised action-ABI digest |
| 2 | logical layer id, momentary |
| 3 | logical layer id, lock |
| 4 | stable PD-mode id, momentary |
| 5 | stable PD-mode id, lock |
| 6 | VIA macro slot |
| 7 | hardcoded macro slot |

Userspace-owned actions are never encoded as raw custom-keycode enum values.
Unsupported action kinds or operands reject the complete candidate.

## Key-Behavior Domain

The payload begins with:

| Size | Field |
| ---: | --- |
| 1 | row count, maximum 64 |
| 1 | populated step count, maximum 128 |
| 2 | reserved |

Each row is length-delimited and contains:

- target action identity;
- `tap_hold_term`, `longer_hold_term`, and `multi_tap_term` as `u16`;
- row flags, currently bit 0 `keeps_auto_mouse_anchored`;
- populated-step count;
- populated step records in strictly ascending tap index.

The exact row encoding is:

| Size | Field |
| ---: | --- |
| 2 | row-body length; excludes this length field |
| 4 | target semantic action |
| 2 | `tap_hold_term`; zero selects the compiled default |
| 2 | `longer_hold_term`; zero selects the compiled default |
| 2 | `multi_tap_term`; zero selects the compiled default |
| 1 | flags; bit 0 keeps auto mouse anchored |
| 1 | populated-step count for this row |

Rows are sorted lexicographically by the target action's four canonical bytes.
Duplicate targets reject the domain.

Each step contains a tap index, presence mask, and only the present branches in
tap/hold/long-hold order. A tap branch stores one action. Hold and long-hold
branches each store mode, `u8` repeat rate, and action.

The exact step encoding is:

| Size | Field |
| ---: | --- |
| 1 | zero-based tap index |
| 1 | presence mask: bit 0 tap, bit 1 hold, bit 2 long hold |
| 4 | tap action, only when bit 0 is set |
| 6 | hold branch, only when bit 1 is set |
| 6 | long-hold branch, only when bit 2 is set |

Each six-byte hold branch is `mode:u8`, `repeat_hz:u8`, then one four-byte
semantic action. Wire hold-mode ids are stable and deliberately do not reuse
the native C enum: `1` press-and-hold-until-release, `2`
tap-at-hold-threshold, `3` repeat-while-held, and `4`
tap-on-release-after-hold. The internal immediate-press mode has no wire id.

Authorable hold modes are:

- press and hold until release;
- tap at threshold;
- repeat while held;
- tap on release after hold.

The internal immediate-press mode is derived runtime state and is forbidden on
wire. Repeat rate must be zero for non-repeat modes and `1..100` for repeat.
Rows are sorted by their canonical target-action bytes and target identities
must be unique.

## RGB Domain

Domain `0x10` version `1` begins with this exact 16-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | RGB payload format, `1` |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask |
| 4 | 1 | reusable-group count, maximum 16 |
| 5 | 1 | layer-color count, maximum 8 |
| 6 | 1 | layer-group row count |
| 7 | 1 | PD-color count, maximum 6 |
| 8 | 1 | PD-group row count |
| 9 | 1 | combo-group row count |
| 10 | 1 | tap-branch color count, maximum 4 |
| 11 | 1 | key-feedback group row count |
| 12 | 1 | physical LED count, exactly 58 |
| 13 | 1 | group-bitmap bytes, exactly 8 |
| 14 | 2 | reserved, zero |

Stage bits 0 through 4 select layer, auto-mouse, PD-mode, combo, and
key-behavior feedback respectively. Bits 5 through 15 are reserved. The four
group-row counts have an aggregate maximum of 32.

Records then appear without padding in this fixed order:

| Section | Bytes per record | Layout |
| --- | ---: | --- |
| reusable group | 9 | `id, bitmap[8]` |
| layer color | 5 | `layer_id, h, s, v, mode` |
| layer group | 5 | `selector, h, s, v, group_id` |
| auto-mouse fade | 4 fixed | `mode, h, s, v` |
| PD color | 5 | `pd_id, h, s, v, locality` |
| PD group | 5 | `selector, h, s, v, group_id` |
| combo feedback | 4 fixed | `h, s, v, locality` |
| combo group | 4 | `h, s, v, group_id` |
| tap-branch color | 3 | `h, s, v` |
| key feedback | 11 fixed | committed/hold/long-hold HSV, tap policy, locality |
| key group | 5 | `semantic, h, s, v, group_id` |

For the compiled 58-LED board, every reusable group is a stable profile-local
`u8` id plus an eight-byte bitmap. Bits 58 through 63 must be zero. Inline
source groups receive generated anonymous ids during canonicalization. Unique
bitmaps sort lexicographically by their eight unsigned bytes and receive
consecutive ids from zero; group names and declaration syntax never enter the
wire representation.

Rows reference dictionary ids rather than embedding native
`rgb_led_group_t`. Row order is preserved because later overlapping rows can
repaint earlier rows. HSV uses three bytes. Selectors and enums use explicit
one-byte ids. Layer and PD selectors use `0xff` for all. Key group semantics
are pending `0`, committed `1`, hold `2`, long hold `3`, and all `0xff`.

Layer modes are all keys `0` and mapped keys only `1`. Locality is both `0`,
left `1`, right `2`, key half `3`, or keys only `4`. Auto-mouse modes are real
destination `0`, end color only at base-effect positions `1`, or end color on
all keys `2`. Tap-commit policy is off `0` or non-base taps `1`. Stable PD ids
are drag-scroll `0`, volume `1`, brightness `2`, zoom `3`, arrow `4`, and
pinch `5`.

Stage-enable bits are valid only for features advertised as compiled. The
key-feedback tap-commit policy is behavior-affecting and therefore uses the
strict activation boundary even though it lives in the RGB domain.

Layer colors cover every logical layer exactly once, PD colors cover every
supported stable PD id exactly once, and tap-branch colors match the compiled
tap count. Identity tables sort by id; renderer group rows and tap-branch
colors preserve authored order. The full validation and canonicalization
contract is mirrored in
`tools/charybdis-profile-studio/live-link/rgb-domain-v1.md`.

## Digests And Checksums

- Transport and storage CRC32 detect corruption of the exact canonical blob.
- Canonical payload digest is FNV-1a 32-bit in v1 for cheap comparison with the
  existing split tooling. CRC and digest are separate fields.
- Source digest is the canonical blob digest produced from the three files.
- Compiled-default digest is the canonical blob digest materialized into the
  firmware.
- Action-ABI digest covers supported standard QMK values, stable userspace
  action ids, layer ids, PD ids, macro capacities, and relevant schema ceilings.

Digest collisions are acceptable for drift display but never replace CRC,
length, schema, capacity, and semantic validation before activation.

## VIA Raw HID Profile Channel

The project uses VIA's existing custom channel (`channel_id = 0`) and 32-byte
reports. The extension host always sends and receives exactly 32 bytes.

### Read Request And Response Envelope

Capability and status reads use this exact request header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | VIA command `0x08`, custom get |
| 1 | 1 | custom channel `0x00` |
| 2 | 1 | value id |
| 3 | 1 | nonzero request id |
| 4 | 1 | zero-based page |
| 5 | 27 | reserved; all zero |

The response echoes bytes 0 through 4, then contains a stable status byte,
payload length, and payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 5 | echoed command, channel, value, request id, and page |
| 5 | 1 | status: `0` OK, `1` malformed, `2` unknown page, `3` unavailable |
| 6 | 1 | payload length, at most 25 |
| 7 | 25 | payload followed by canonical zero padding |

A successful v1 capability or status page has exactly 25 payload bytes. The
desktop correlates all five echoed bytes; a broad command-only match is not
valid for this channel.

Capabilities use two pages. Page 0 contains the response-layout version, page
count, protocol/schema versions, report and chunk sizes, status-page count,
feature flags, action-ABI digest, firmware version, and compiled-default
digest. Page 1 contains compiled and maximum layer/behavior/combo/RGB/macro
capacities plus the 4,064-byte payload, 4,096-byte slot, and 7,551-byte VIA
macro bounds. Feature bits distinguish schema/storage knowledge from candidate
write, commit, preview, activation, and peer support, so read-only firmware
does not advertise write operations prematurely.

Capability feature bits are:

| Bit | Meaning |
| ---: | --- |
| 0 | read surface |
| 1 | persistent storage layout |
| 2 | RGB domain schema |
| 3 | key-behavior domain schema |
| 4 | split keyboard |
| 5 | candidate writes |
| 6 | persistent commit |
| 7 | volatile RGB preview |
| 8 | runtime activation |
| 9 | peer reconciliation |
| 10 | action-ABI digest available |
| 11 | compiled-profile digest available |

Supported-domain-mask bit 0 is RGB and bit 1 is key behaviors. A domain bit
must agree exactly with its schema feature bit. Candidate chunk capacity is
zero exactly when candidate writes are absent and otherwise is `1..20`.
Commit and runtime activation require candidate writes. RGB preview also
requires the RGB domain. Peer reconciliation requires both split-keyboard and
persistent-commit support. Studio rejects inconsistent combinations before it
offers a live operation.

Status also uses two pages. Page 0 reports state flags and source, compiled,
active, pending, and committed digests. Page 1 reports active, committed, and
peer generation identities plus transaction, conflict, validation, and error
state. Until the store and generated compiled digest land, firmware truthfully
reports `compiled-only` with `digests unavailable`; zero is not presented as a
real digest.

### Candidate Mutation Envelope

Candidate mutations use VIA custom set command `0x07`, custom channel `0x00`,
and an exact 32-byte report. The only v1 candidate operations admitted before
commit is implemented are:

| Value id | Operation |
| ---: | --- |
| `0x10` | begin candidate |
| `0x11` | write candidate chunk |
| `0x12` | validate candidate |
| `0x14` | abort candidate |

Value `0x13` remains reserved for the later durable commit owner. The preview,
rollback, and committed-blob read values remain reserved as `0x15`, `0x16`,
and `0x17`. A firmware build must not route these mutation frames or advertise
candidate-write capability merely because the standalone codec and coordinator
are compiled.

All mutation requests start with:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | VIA command `0x07`, custom set |
| 1 | 1 | custom channel `0x00` |
| 2 | 1 | operation value id |
| 3 | 2 | nonzero transaction id |

Begin candidate uses this complete layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | schema major |
| 6 | 1 | schema minor |
| 7 | 1 | requested-domain mask; bits 0 RGB and 1 key behaviors |
| 8 | 1 | flags, initially zero |
| 9 | 2 | canonical blob length, `8..4064` |
| 11 | 4 | CRC32 of the exact canonical blob |
| 15 | 4 | FNV-1a digest of the exact canonical blob |
| 19 | 4 | action-ABI digest used to encode actions |
| 23 | 9 | reserved, all zero |

The requested-domain mask may be zero for the canonical empty profile and may
contain no bits other than 0 and 1. Compatibility with the build's advertised
schema, domain mask, action ABI, and capacity is checked by the scan owner
before storage work begins.

Candidate chunk uses this complete layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 2 | zero-based byte offset in the candidate blob |
| 7 | 1 | chunk length, `1..20` |
| 8 | 20 | chunk bytes, followed by zero padding |
| 28 | 4 | reserved, all zero |

Chunks are sequential. A retry of an already staged `{transaction, offset,
length, bytes}` tuple is accepted after the scan owner reads and compares the
staged bytes. A retry with different bytes rejects and poisons that candidate.
Partial overlaps, gaps, writes beyond the declared length or 4,064-byte bound,
and nonzero padding are rejected.

Validate and abort contain only the five-byte common header; bytes 5 through
31 are reserved and zero. Validate can start only after every declared byte is
staged. It performs digest and semantic validation incrementally from scan
context. Abort is idempotent; retrying a successful or no-op abort never
repeats storage work.
There is no autonomous candidate timeout in v1. A host transport timeout does
not implicitly abort a transaction. Reset or power loss discards the volatile
mailbox/transaction owner, and the storage marker-last rule keeps an incomplete
candidate ineligible for boot.

The custom-set callback performs only exact frame validation and one bounded
mailbox copy. It never reads or writes EEPROM and never computes a payload
digest. Its immediate response preserves bytes 0 through 4 and replaces bytes
5 through 31 with:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | admission: `0` queued, `1` malformed, `2` busy, `3` unsupported |
| 6 | 1 | stable candidate error id |
| 7 | 1 | malformed-frame byte offset, or `0xFF` |
| 8 | 24 | reserved, all zero |

`queued` acknowledges only the bounded copy. It is not validation, durability,
commit, or activation success.

### Candidate Operation Status

Candidate operation status uses custom get value `0x18`, request page zero,
and the normal read request/response envelope. It exists only in builds that
route and advertise candidate writes. Its successful 25-byte payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `1` |
| 1 | 1 | candidate state |
| 2 | 1 | last operation |
| 3 | 1 | flags: bit 0 mailbox pending, bit 1 candidate poisoned |
| 4 | 2 | candidate or last affected transaction id |
| 6 | 2 | next sequential byte offset |
| 8 | 2 | declared candidate length |
| 10 | 4 | declared FNV-1a digest |
| 14 | 1 | stable candidate error id |
| 15 | 1 | error domain id, or `0xFF` |
| 16 | 1 | error table id, or `0xFF` |
| 17 | 2 | error row index, or `0xFFFF` |
| 19 | 1 | error tap/step index, or `0xFF` |
| 20 | 1 | error field id, or `0xFF` |
| 21 | 2 | candidate byte offset, or `0xFFFF` |
| 23 | 2 | operation sequence, incremented after each processed mailbox item |

Candidate states are `0` idle, `1` receiving, `2` complete, `3` validating,
`4` validated, and `5` rejected. Last-operation ids are `0` none, `1` begin,
`2` chunk, `3` validate, and `4` abort. Error ids are stable:

| Id | Error |
| ---: | --- |
| 0 | none |
| 1 | malformed frame |
| 2 | mailbox busy |
| 3 | invalid transaction |
| 4 | wrong transaction |
| 5 | invalid state |
| 6 | incompatible schema |
| 7 | unsupported domain |
| 8 | incompatible action ABI |
| 9 | capacity exceeded |
| 10 | out of order, gap, or partial overlap |
| 11 | conflicting retry |
| 12 | checksum or digest mismatch |
| 13 | storage failure |
| 14 | semantic validation rejected |
| 15 | unsupported operation |
| 16 | candidate poisoned |

Errors which have no domain/table/row/tap/field location use the sentinels
above. The operation sequence lets a host distinguish a newly processed
result from an older polled status without inventing another transaction id.

The executable cross-language golden reads live in
`tests/fixtures/profile_wire_v1_reads.fixture` and are consumed by both the C
host codec suite and the Profile Studio JavaScript suite. Exact candidate
begin, chunk, validate, abort, acknowledgement, and operation-status reports
live in `tests/fixtures/profile_candidate_v1.fixture` and are consumed by the
standalone firmware C codec/coordinator suite.

Initial value ids:

| VIA command | Value id | Operation |
| --- | ---: | --- |
| custom get | `0x01` | capabilities page |
| custom get | `0x02` | profile status page |
| custom set | `0x10` | candidate begin |
| custom set | `0x11` | candidate chunk |
| custom set | `0x12` | candidate validate/prepare |
| custom save | `0x13` | candidate commit |
| custom set | `0x14` | candidate abort |
| custom set | `0x15` | RGB preview chunk/control |
| custom set | `0x16` | RGB preview rollback |
| custom get | `0x17` | committed-profile read chunk |

Every mutating operation includes a nonzero transaction id. Candidate begin
declares schema, length, CRC, digest, and requested domain mask. Chunks include
transaction id, offset, explicit length, and at most the remaining report
payload. Firmware validates every frame length before reading any field.

Retries are idempotent when transaction id, offset, length, and bytes match.
Conflicting retransmission rejects the transaction. Commit acknowledgement loss
is resolved by reading status and comparing transaction, generation, and
digest.

USB receive context performs framing and bounded copying only. Decode,
semantic validation, EEPROM work, safe activation, and split convergence run
from scan context.

## Error Model

Responses use stable error ids and include the transaction id when present:

- incompatible protocol or schema;
- unsupported domain or action ABI;
- malformed length, order, reserved field, or checksum;
- capacity exceeded;
- duplicate identity or invalid cross-reference;
- invalid behavior timing, mode, repeat rate, or action;
- invalid RGB selector, enum, group, bitmap, or stage flag;
- no candidate, wrong transaction, or conflicting retry;
- validation rejected;
- waiting for safe boundary;
- storage failure;
- peer pending or divergent;
- busy or transport contention.

Structured validation details identify domain, table, row, tap index, field,
and error id without returning source strings from firmware.

## Golden Fixtures Required Before Stage 02 Completion

- empty header with zero domains;
- representative full RGB domain;
- representative behavior with tap, repeat hold, long hold, timing overrides,
  semantic layer/PD/macro actions, and auto-mouse anchoring;
- current compiled-default profile;
- maximum valid behavior and RGB records under the aggregate payload limit;
- truncated envelope and record;
- overlong, duplicate, out-of-order, reserved-bit, and unknown-domain input;
- invalid action ABI and invalid cross-reference;
- canonical C/JavaScript byte-for-byte round trips.

# Profile Wire V1

> Current firmware accepts only the formats it writes (D-F10, D-F14): profile
> schema 3.0; a 65,504-byte custom payload; and logical store format 4
> (`NS`). Every save binds a nonzero VIA generation and digest. HID and split
> framing remain v1. Older profile/store formats and the legacy GET 9 source
> page are rejected. Backup translation belongs to the client, before a
> current-format Apply.

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
| 4 | 1 | schema major, `3` |
| 5 | 1 | schema minor, initially `0` |
| 6 | 1 | domain count |
| 7 | 1 | flags; bit 0 means canonical encoding, all others reserved |

The transport candidate length or storage header supplies total blob length.
Schema 3.0 is capped at 65,504 bytes, one 64 KiB slot less its 32-byte header
(D-F14); schemas 1 and 2 are refused.

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

Accepted domains (`users/noah/lib/profile/schema/profile_domain_registry.h`):

| Id | Domain | Version |
| ---: | --- | ---: |
| `0x10` | RGB | 4 |
| `0x20` | Key behaviors | 2 |
| `0x30` | Combo overrides | 3 |
| `0x40` | Settings, layer records, layer names, VIA macro names and custom-key names | 6 |
| `0x50` | Sparse pointing-mode slots | 3 |

### Firmware domain ownership

The domain registry owns each current domain's ID, version, canonical order
and module name; a domain's index is its row position and its mask bit is
`1 << index`. Blob encoding/decoding, candidate masks,
whole-profile validation, compiled domain envelopes, storage and provider mask
accounting use it. The validator and compiled defaults reach each domain only
through a switch generated from the rows, calling `validate_<module>` and
`noah_profile_<module>_compiled_v1_write` directly (D-F13). Build feature
admission is separate: knowing a wire domain does not require every
feature-gate build to enable its runtime.

One envelope walker checks magic, schema, flags, domain count, current
versions, ordering, payload extents and completion. Its interface consumes
one already-read blob/domain header; it performs no reader I/O or semantic
validation. The synchronous store check, stepped boot scan, prepared commit
check and whole-profile validator therefore share shape policy while keeping
their own read budgets and transaction scheduling. Storage still checks
checksums and declared masks; the validator still owns semantic references
and runtime placement admission.

Each domain module owns its record shape and its compiled encoder. RGB record
geometry is shared by the decoder and compiled writer. RGB and key behaviors retain their
incremental decoders; settings retains its byte consumer. Combo row iteration
is shared by stepped validation and cache publication, reading a row in 12-
and 16-byte grants. Sparse pointing iteration is shared by whole-buffer
validation, stepped validation and cache publication, with at most one read
of 20 bytes per step. Compiled pointing output uses the same sparse cursor's
ordering/presence checks. An iterator exposes a record only after validating
it; an omitted pointing slot remains disabled and unnamed.

A validated profile keeps decoded views for RGB and key behaviours only.
Combo, settings and pointing publication find their payload with
`noah_profile_blob_v1_find_domain`, which walks the envelope through the
snapshot's reader on the cold path, after checking the snapshot's domain mask.
The combo codec reads from that payload range; its row count is fixed by the
range length.

The registry/traversal change (D-F11) preserves wire bytes, digests, slot
geometry and feature admission. The subsequent complete compiled profile
(D-F12, below) deliberately changes compiled-default identity. The owner
round-trip harness saves and reboots both the complete authored five-domain
profile and a profile with populated imported combo/settings overrides,
checking every published domain and runtime view. Hardware eligibility and the independent VIA adapter
are injected; this host harness does not establish physical acceptance. The
reviewed stack manifest follows the shared iterators through PD publication,
combo cache EEPROM reads and host PD record validation; compiler-inlined
settings readback/publication paths are represented by their linked callers.

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
| 4 | stable PD-mode id, momentary: `0..31` on 32-slot firmware (native `PD_SLOT_n` = `0x7e80 + n`) |
| 5 | stable PD-mode id, lock: `0..31` on 32-slot firmware (native `PD_SLOT_n_LOCK` = `0x7ea0 + n`) |
| 6 | VIA macro slot |
| 7 | custom key `0..127` (`CUSTOM_KEY_n`, native `0x7f00 + n` since D-F14; before it `0..63` at `0x7e40 + n`, now inert); under earlier action vocabularies this kind named the retired user macros, and a digest mismatch keeps the two apart |

Userspace-owned actions are never encoded as raw custom-keycode enum values.
Unsupported action kinds or operands reject the complete candidate.

Layer operands and native layer keycodes name the sixteen-layer bank, `0..15`.
`LT()` and `LM()` encode their layer in four bits, so every value they can
express is a bank layer. The five-bit layer actions (`MO()`, `TG()`, `TT()`,
`OSL()`, `TO()` and the layer locks) refuse layers 16 and above.

## Key-Behavior Domain

Version 2 (D-F14). Its ceilings follow the firmware's one supported tap
depth `d` (five; capability pages 1 and 2 report it): at most 128 rows, at most
`d` steps in a row, and at most `128 × d` populated steps in all (640 at
depth five). The payload begins with:

| Size | Field |
| ---: | --- |
| 1 | row count, maximum 128 |
| 1 | reserved, zero |
| 2 | populated step count, `u16`, maximum `128 × d` |

Each row is length-delimited and contains:

- target action identity;
- `tap_hold_term`, `longer_hold_term`, and `multi_tap_term` as `u16`;
- row flags: bit 0 `keeps_auto_mouse_anchored`, bit 1 disabled;
- populated-step count;
- the layers the row may act on;
- populated step records in strictly ascending tap index.

The exact row encoding is:

| Size | Field |
| ---: | --- |
| 2 | row-body length; excludes this length field |
| 4 | target semantic action |
| 2 | `tap_hold_term`; zero selects the compiled default |
| 2 | `longer_hold_term`; zero selects the compiled default |
| 2 | `multi_tap_term`; zero selects the compiled default |
| 1 | flags; bit 0 keeps auto mouse anchored, bit 1 disabled; other bits zero |
| 1 | populated-step count for this row, at most `d` |
| 4 | allowed layers, `u32`, one bit per layer; bits at or above the layer count zero, an empty mask allowed |

A disabled row keeps its steps, timing and allowed layers; a press takes the
key's normal action while it is disabled or its source layer is not allowed
([participation policy](participation-policy.md)). Compiled defaults and
translated version-1 rows are enabled and allowed on every bank layer.
Translation from version 1 moves the step count to bytes 2–3 and inserts
`ff ff 00 00` after each row's step count, growing its body length by four.

Rows are sorted lexicographically by the target action's four canonical bytes.
Duplicate targets reject the domain.

Each step contains a tap index, presence mask, and only the present branches in
tap/hold/long-hold order. A tap branch stores one action. Hold and long-hold
branches each store mode, `u8` repeat rate, and action.

The exact step encoding is:

| Size | Field |
| ---: | --- |
| 1 | zero-based tap index, below `d` |
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

The firmware finds a key's row through an index: at publication it walks the
row lengths once (one two-byte read per row) into the row offsets, then a key
press binary-searches them, reading one four-byte target per probe, and decodes
only the matching row's fixed fields and the step it needs. Steps before it are
skipped by their presence masks. The index lives beside each runtime bank, not
in the snapshot a lookup copies.

## RGB Domain

Domain `0x10` version `4` begins with this exact 16-byte header
(see [RGB domain](rgb-domain-v1.md)):

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | RGB payload format, `4` |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask |
| 4 | 1 | reusable-group count, maximum 16 |
| 5 | 1 | layer-color count, maximum 16 |
| 6 | 1 | layer-group row count |
| 7 | 1 | PD-color count, exactly 32 when compiled |
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
one-byte ids. Layer and PD selectors use `0xff` for all; otherwise a layer
selector is below 16 and a PD selector below 32. Key group semantics
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
[RGB domain v1](rgb-domain-v1.md).

## Digests And Checksums

- Transport and storage CRC32 detect corruption of the exact canonical blob.
- Canonical payload digest is FNV-1a 32-bit in v1 for cheap comparison with the
  existing split tooling. CRC and digest are separate fields.
- The legacy source-digest status field reports the firmware's compiled-profile
  digest. Ark reads it from the keyboard and never hashes source files.
- Compiled-default digest is the canonical blob digest materialized into the
  firmware.
- Action-ABI digest covers supported standard QMK values, stable userspace
  action ids, layer ids and the layer count, PD ids, macro capacities, and
  relevant schema ceilings. It is independent of authored rows; an empty
  profile uses the same vocabulary as a populated one. The sixteen-layer bank
  (D-F14) therefore has its own digest, and stored profiles from earlier
  firmware are refused by it.

Digest collisions are acceptable for drift display but never replace CRC,
length, schema, capacity, and semantic validation before activation.

## VIA Raw HID Profile Channel

The project uses VIA's existing custom channel (`channel_id = 0`) and 32-byte
reports. The extension host always sends and receives exactly 32 bytes.

### Native Combo Readback — GET Value `0x06`

This optional read uses the envelope below. It reports the connected half's
effective native combo definitions. It observes the active `0x30` override when
present, otherwise the compiled table. Its digest also covers transient global
enable state, so it is separate from canonical profile identity.
Older firmware returns VIA unhandled (`0xff`); clients must show unsupported
rather than infer an empty table. Firmware built without combos reports a
valid disabled empty table. No SET or SAVE operation exists for this value.

The request takes a 16-bit page, its low byte at request byte 4 and its high
byte at byte 5 (feature bit 19). Every successful page has exactly 25 payload
bytes. Metadata is page 0:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | readout version, `3` |
| 1 | 1 | row count, `0..128` |
| 2 | 1 | maximum inputs per row, `16` |
| 3 | 1 | layer count, `1..16` |
| 4 | 1 | current global combo enable state, `0` or `1` |
| 5 | 1 | flags: bit 0 no timer, 1 strict timer, 2 custom trigger hook, 3 custom release hook, 4 custom repress hook, 5 fixed reference layer |
| 6 | 4 | FNV-1a 32-bit digest of page-0 bytes 0..5 and 10..24, then page 1, then every row's two pages in order |
| 10 | 2 | default combo window in milliseconds: the window of every row that follows it |
| 12 | 2 | combo hold threshold in milliseconds |
| 14 | 1 | pages per row, `2` |
| 15 | 10 | reserved, zero |

Page 1 holds the combo reference layer of each layer, one byte per layer for
`layer count` layers, the rest zero. With a fixed reference layer every entry
is that layer.

Row `n` occupies pages `2 + 2n` (page A) and `3 + 2n` (page B):

| Page | Offset | Size | Field |
| --- | ---: | ---: | --- |
| A | 0 | 1 | zero-based row index |
| A | 1 | 1 | input count, `2..16` |
| A | 2 | 2 | native output keycode; zero means a firmware callback |
| A | 4 | 2 | combo window in milliseconds, including per-combo hook result |
| A | 6 | 1 | flags: bit 0 must hold, 1 must tap, 2 press in order, 3 follows the default window, 4 disabled |
| A | 7 | 4 | allowed-layer mask, uint32, one bit per layer; bits at or above the layer count zero |
| A | 11 | 14 | native inputs 0..6 in declared order, uint16 each; unused entries zero |
| B | 0 | 18 | native inputs 7..15, uint16 each; unused entries zero |
| B | 18 | 7 | reserved, zero |

A table has `2 + 2 × row count` pages; later pages return status 2. A
compiled table reports every combo enabled with every bank layer allowed.
Version 2's eight-entry reference list in the metadata is gone; references are
the counted page 1.

Combo readout uses version 3 only (D-F14; it replaces version 2) and domain
`0x30` version 3 only. A live row follows the header's default window when its
stored window is zero. A compiled combo
follows `COMBO_TERM` unless its authored `COMBO_WINDOW` supplies a window.
Without the owner, a per-combo term hook is the user's own and rows never
report following. Firmware built without combos reports zero for both values.

Flag bit 2 means a user's own trigger hook, which a client cannot see into.
The firmware's participation hook (D-F14) does not set it: its decisions are
the profile's own controls, read back with the rest.

Inputs are distinct nonzero keycodes. Timing zero is preserved verbatim. Input
order matters when the order flag is set. Native codes use the connected
firmware's action ABI; custom source names and callback bodies are not sent.
Additional trigger/release predicates and transient per-combo engine state
are not serialized. The hold requirement follows QMK's effective precedence,
including its suppression under no-timer mode.
Fixed-reference mode reads the reference layer directly even when it equals
the selected layer; transparent keys do not inherit in this lookup. Without
this flag, an identity reference uses the effective key from the active stack.

Clients read metadata, the reference page, both pages of every row, then
metadata again. Accept only equal metadata and a matching digest, with at most
two complete attempts (134 GETs at this firmware's 32 rows). This detects
ordinary changes, not an atomic snapshot or an active layer stack. Failure clears prior combo rows without discarding
independent profile or base-lighting reads. Firmware uses a bounded cold-path
table scan and a 50-byte row scratch buffer; it allocates no persistent cache.
Malformed requests use status 1, unknown pages status 2, and tables that cannot
be represented within these limits status 3. Error payloads are empty and zero.

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

Capabilities use three pages (response layout `2`, D-F14). Page 0 contains
the response-layout version, page count, protocol/schema versions, report and
chunk sizes, status-page count, feature flags, action-ABI digest, firmware
version, and compiled-default digest. Page 1 contains compiled and maximum
layer/behavior/combo/RGB/macro capacities, the custom-key slot count (payload
byte 11), the selected schema's payload capacities and the advertised VIA
macro bound. Feature bits distinguish schema/storage knowledge from candidate
write, commit, preview, activation, and peer support, so read-only firmware
does not advertise write operations prematurely.

| Page 1 byte | Field |
| ---: | --- |
| 0 | compiled layer count |
| 1 | maximum logical layers |
| 2 | maximum behaviour rows |
| 3 | supported tap depth (maximum tap steps per behaviour) |
| 4 | zero; the populated-step ceiling is on page 2 |
| 5 | maximum combos |
| 6 | maximum inputs per combo |
| 7 | maximum reusable RGB groups |
| 8 | maximum RGB stage-group rows |
| 9 | physical LED count |
| 10 | LED bitmap bytes |
| 11 | custom-key slots |
| 12 | VIA macro slots |
| 13–14 | maximum profile payload |
| 15–16 | slot payload capacity |
| 17–18 | zero; the slot size is on page 2 |
| 19–20 | VIA macro bank bytes |
| 21 | supported domain mask |
| 22–24 | zero |

Page 2 carries the fields wider than a byte:

| Page 2 byte | Field |
| ---: | --- |
| 0–1 | maximum populated behaviour steps |
| 2 | supported tap depth (equal to page 1 byte 3) |
| 3–6 | profile slot size in bytes |
| 7 | maximum encoded bytes per name |
| 8 | layer-mask width in bits, `32` |
| 9 | placement positions per layer (matrix rows × columns) |
| 10–24 | zero |

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
| 12 | atomic logical apply |
| 13 | retired, never advertised |
| 14 | owned layer keys: `TG()`, `TO()`, `TT()` and `OSL()` act through userspace layer ownership, so a host may offer them in behaviours and combos where the placement rules allow (`TT()` and `OSL()` joined the bit on the same unreleased branch; every flashed build that sets it has all four) |
| 15 | behaviour QMK functions: a behaviour sends QMK and keyboard keycodes past the layer keycodes and below the user range (the Charybdis DPI and sniping keys, RGB Matrix, Magic, `QK_BOOT`…) as a synthetic QMK record, so they run as they do on a key, and a key whose own keycode is one keeps a plain key's fallback hold; a host may offer them in a behaviour's target, tap and hold. Without it, the engine sends them as report keys, keeping only the low byte, and a host must refuse them there |
| 16 | custom keys and keycode blocks: userspace keycodes sit in fixed blocks (pointing holds `0x7e80`, pointing locks `0x7ea0`, layer locks `0x7ec0`, custom keys `0x7f00` since D-F14, each reserved beyond what is supported), action kind 7 is a custom key, and the settings domain names the 128 custom keys. It comes with its own action ABI digest; a host knowing that digest may offer custom keys as keys and combo outputs, never as behaviour steps |

| 17 | physical gesture timing: handled physical keys measure holds from physical press and repeats from physical release to next press; eligible records buffered by combo/tapping retain their series. Authored `LT()` rows bypass native QMK tapping; plain `LT()` keys retain it. An authored `LT()` row's layer is its hold: without an authored first hold it holds `MO()` from the tap-hold term, never from the press (joined the bit on the same unreleased branch). Combo outputs retain their delivery/origin timing contract. This is a runtime capability, not a profile format change |

| 18 | runtime-owned tapping: every key handled by userspace bypasses native QMK tapping, including authored MT/OSM rows and intrinsic TT/OSL ownership. An authored MT/OSM row without a first hold keeps the key's modifier hold past the tap-hold term, and keys pressed while such a runtime-owned LT/MT/OSM key is undecided wait for its tap or hold (both joined the bit on the same unreleased branch). Unhandled LT/MT/OSM keep native QMK tapping. Extends bit 17 without changing its physical timestamp contract or profile bytes |

| 19 | wide pages: the payload, compiled, combo and settings readbacks (GET `0x04`, `0x05`, `0x06`, `0x07`) take a 16-bit page, request byte 4 its low byte and byte 5 its high byte, with bytes 6–31 reserved; their responses echo bytes 0–4 and the request id correlates them |

| 21 | canonical UTF-8 macro text and settings scalar 27 Host settings and GET 0x0B OS readback; require this bit before writing either extension. See [runtime flow](runtime-flow.md) |

| 22 | host layouts: settings scalar 27 bits 16–23 name a layout from the [host layout catalogue](host-layouts-v1.md) and bit 24 marks a macOS ISO keyboard; macro text is typed through that layout. Require this bit before writing bits 16–24. See [runtime flow](runtime-flow.md) |

| 23 | macro input protection: automatic protection when the selected layout needs Unicode entry, with optional per-macro `01 05 01` (On) or `01 05 02` (Off) prefixes. Require this bit before writing a prefix. New physical presses are ignored until released; earlier releases remain admitted. See [runtime flow](runtime-flow.md) |

| 20 | participation controls: behaviour and combo participation at the master, layer, definition and placement scopes ([participation policy](participation-policy.md)) |

Supported-domain-mask bits 0–3 are RGB, key behaviors, combos and portable
settings respectively. RGB and behavior domain bits must agree exactly with
their schema feature bits. Candidate chunk capacity is
zero exactly when candidate writes are absent and otherwise is `1..20`.
Commit and runtime activation require candidate writes. RGB preview also
requires the RGB domain. Peer reconciliation requires both split-keyboard and
persistent-commit support. The live client rejects inconsistent combinations
before it offers a live operation.

Status also uses two pages. Page 0 reports state flags and source, compiled,
active, pending, and committed digests. Page 1 reports active, committed, and
peer generation identities plus transaction, conflict, validation, and error
state. Until the store and generated compiled digest land, firmware truthfully
reports `compiled-only` with `digests unavailable`; zero is not presented as a
real digest.

State flags (page 0, bytes 2–3). Bits 9–15 are reserved and must be zero.

| Bit | Flag |
| ---: | --- |
| 0 | active profile is the compiled default |
| 1 | committed record valid |
| 2 | candidate pending |
| 3 | preview active |
| 4 | peer known |
| 5 | peer converged |
| 6 | waiting for a safe activation boundary |
| 7 | digests unavailable |
| 8 | peer cleanup pending |

Peer cleanup pending (added 2026-09-23) means a cancelled save's split ABORT
was never acknowledged, so the USB half released its own side after
`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS` (see the
[authority state table](authority-state-table.md#cancelled-prepare-peer)). No
marker exists on either half, and durable authority is unchanged. Until the
peer acknowledges the retried ABORT, the keyboard starts no new save or split
transfer; a peer that never does needs a restart. A decoder that predates the
bit rejects the status while it is set, as it rejects any unknown flag.

### Payload Readback — GET Values `0x04` And `0x05`

GET `0x04` reads the committed payload and `0x05` the compiled defaults the
firmware was built with, through the envelope above with a wide page (feature
bit 19): a 65,504-byte payload needs 2,621 pages. Page 0 is metadata; page
`n` from 1 carries payload bytes `(n - 1) × 25` onward, at most 25, with the
payload length byte giving the count. A page past the end answers status `2`;
status `3` means nothing is committed, or the compiled defaults cannot be
opened. Page 0's 25-byte payload, little-endian:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version `1` |
| 1 | 1 | page payload size `25` |
| 2 | 2 | payload length |
| 4 | 4 | generation; `0` for the compiled defaults |
| 8 | 4 | FNV-1a payload digest |
| 12 | 4 | CRC32 of the payload |
| 16 | 1 | schema major |
| 17 | 1 | schema minor |
| 18 | 1 | domain mask |
| 19 | 1 | origin half; committed record only |
| 20 | 1 | record flags; committed record only |
| 21 | 4 | reserved, zero |

### Candidate Mutation Envelope

Candidate staging mutations use VIA custom set command `0x07`; durable commit
uses VIA custom save command `0x09`. Both use custom channel `0x00` and an exact
32-byte report:

| Value id | Operation |
| ---: | --- |
| `0x10` | begin candidate |
| `0x11` | write candidate chunk |
| `0x12` | validate candidate |
| `0x13` | durably commit and request safe activation |
| `0x14` | abort candidate |

Custom set values `0x15`, `0x16`, `0x17` and `0x1A` belong to the logical VIA
staging channel, specified in
[Logical Profile Transaction V1](logical-profile-transaction-v1.md#cancellation-and-lease-ownership);
the complete map is under Value Ids below. A firmware build must not route
candidate mutation frames or advertise candidate-write capability merely
because the standalone codec and coordinator are compiled.

All mutation requests use this correlation header (byte 0 is `0x07` except
for commit, which uses `0x09`):

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | VIA command: custom set `0x07` or commit custom save `0x09` |
| 1 | 1 | custom channel `0x00` |
| 2 | 1 | operation value id |
| 3 | 2 | nonzero transaction id |

Begin candidate uses this complete layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | schema major |
| 6 | 1 | schema minor |
| 7 | 1 | requested-domain mask; bits 0 RGB, 1 key behaviors, 2 combos, 3 settings, and schema-2 bit 4 PD |
| 8 | 1 | flags, initially zero |
| 9 | 2 | canonical blob length, `8..65504`, schema 3.0 only |
| 11 | 4 | CRC32 of the exact canonical blob |
| 15 | 4 | FNV-1a digest of the exact canonical blob |
| 19 | 4 | action-ABI digest used to encode actions |
| 23 | 1 | store format, exactly `4` |
| 24 | 4 | nonzero bound VIA generation |
| 28 | 4 | nonzero bound VIA digest |

The requested-domain mask may be zero for the canonical empty profile and may
contain only the domains advertised by that firmware. Compatibility with the build's advertised
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
Partial overlaps, gaps, writes beyond the declared length or selected schema bound,
and nonzero padding are rejected.

Validate and abort contain only the five-byte common header; bytes 5 through
31 are reserved and zero. Commit uses the same body-free shape but byte 0 is
custom save `0x09` rather than custom set `0x07`. Validate can start only after
every declared byte is staged. It performs digest and semantic validation
incrementally from scan context.

Commit is admitted only for the matching validated transaction. The scan owner
writes and reads back the non-marker header, streams one payload readback,
checks canonical top-level shape, writes the two-byte commit marker last, reads
that marker back, and only then requests provider activation. Each scan step
performs exactly one EEPROM operation of at most 20 bytes. Behavior activation
may remain in the activating state until the safe-boundary predicate clears;
the prior generation remains active meanwhile. A duplicate commit for the same
transaction is a no-op while commit/activation is progressing and after final
success. Once durable commit has entered activation, abort cannot undo it.
Failure while completing or confirming the final marker is reported as
durability unknown, not as a safe failure: the live client must reconcile committed
generation and digest before retrying.

Abort is idempotent before durable commit; retrying a successful or no-op abort
never repeats storage work.
The device owner expires ordinary host staging after 15 seconds without
processed precommit work; for a candidate bound to a VIA identity, each
logical VIA staging frame the owner admits counts as that work. Every
precommit cancel of such a candidate also ends its VIA staging, and the host
cannot end that staging itself; see
[Logical Profile Transaction V1](logical-profile-transaction-v1.md#cancellation-and-lease-ownership).
Once a peer-required candidate enters its distributed
prepare, it uses a separate 60-second no-progress window; only acknowledged
split payload progress refreshes that window, not BUSY replies or retries. A
matching host abort or barrier timeout cancels and confirms the peer's
provisional prepare before releasing the local candidate. Neither policy
expires marker-last commit, activation, or durability-unknown state. Device-side
inactivity reports error `19`; a desktop transport timeout still does not
itself command an abort. Reset or power loss discards the volatile
mailbox/transaction owner, and the storage marker-last rule keeps an incomplete
candidate ineligible for boot.

When coherent split authority reveals a compatible peer generation greater
than or equal to the generation reserved by a host candidate, the owner cancels
that host candidate before commit can begin. One acknowledged queued command
may be discarded by this scan-owned cancellation so a queued commit cannot
cross the authority boundary. Transaction id, digest, last operation, and
operation sequence remain available for correlation, and error `20` identifies
the peer supersession. Committing, activating, and durability-unknown states
cannot be canceled by this rule.

The candidate mutation receive callback performs only exact custom-set or
custom-save frame validation and one bounded mailbox copy. It never reads or
writes EEPROM and never computes a payload digest. Its immediate response
preserves bytes 0 through 4 and replaces bytes 5 through 31 with:

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

Page 1 (added 2026-09-23) reports the save's copy to the other half, which
page 0 cannot show: during `PREPARING_PEER` page 0 stays identical until the
peer is ready. Builds with the live-profile owner answer it; others answer
`UNKNOWN_PAGE`, which the host treats as "not reported". Its 25-byte payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `1` |
| 1 | 1 | peer phase |
| 2 | 1 | the peer's last split status |
| 3 | 1 | flags: bit 0 peer cleanup pending, bit 1 this half is transport master, bit 2 waiting for a safe boundary |
| 4 | 2 | bytes of the profile transferred to the peer |
| 6 | 2 | bytes to transfer |
| 8 | 4 | split retries since boot |
| 12 | 4 | split transport failures since boot |
| 16 | 2 | the peer's BUSY replies in a row (saturating) |
| 18 | 1 | why the peer last held off: its last BUSY reason other than routine admission |
| 19 | 1 | the peer's store state in its last BUSY |
| 20 | 1 | the peer's transfer owner in its last BUSY |
| 21 | 1 | who held the peer's storage admission in its last BUSY: `0` none, `1` host, `2` peer |
| 22 | 3 | reserved, zero |

Flag bit 2 (added 2026-09-25) means the commit decision, or the VIA ACCEPT
that follows it, is waiting for this half to go idle: no held key, pending
tap, locked layer or pointer mode, one-shot, macro or combo. Input still works
in this state, so the user can release or unlock what holds it. If it does not
clear before the 60 s pre-decision no-progress timeout, the save is cancelled
with `TIMEOUT` and nothing changes; after the decision the save simply waits. Hosts that predate the bit reject the page as
incompatible; page 1 is advisory, so their Apply is unaffected.

Busy reasons are `0` unspecified, `1` admitted (queued; the answer comes on a
retry), `2` mailbox full (an earlier request is still unprocessed, which, if it
persists, means the peer is not getting scan time), `3` the peer's store holds
a different copy, `4` the store is not receiving this copy, `5` the store is
validating, preparing or committing, `6` the peer is pulling a profile itself,
and `7` the peer only converges just now. Store states are `0` uninitialized,
`1` idle, `2` receiving, `3` validating, `4` preparing, `5` prepared, `6`
committing, `7` committed, `8` rejected and `9` reconcile required; transfer
owners are `0` none, `1` receiving a remote push, `2` pulling. Bytes 19–21
come from every BUSY, including the routine admission reply, so they describe
the peer as it is now; byte 18 keeps the last reason that was not admission.
All of them reset when a copy starts. The host reports these when a copy
stalls; no decision depends on them. Bytes 16–21 were reserved zero before
2026-09-24, so the older app refuses this page.

Peer phases are `0` idle, `1` binding the staged VIA copy, `2` beginning, `3`
sending, `4` the peer validating and writing its prepared copy, `5` the peer
prepared, `6` the peer committing, `7` aborting the peer's copy, `8` the split
reconciler stopped (the last status says why), and `9` waiting for the link
between steps. Split statuses are `0` ok, `1` invalid frame, `2`
incompatible, `3` stale, `4` conflict, `5` corrupt, `6` busy, `7` range
error, `8` digest mismatch, `9` storage error and `10` validation error. The
host treats a change of phase, offset or last status as progress when it
decides whether a commit has stalled.

Candidate states are `0` idle, `1` receiving, `2` complete, `3` validating,
`4` validated, `5` rejected, `6` committing, `7` activating, `8` preparing
the peer before local durability, `9` converging the peer after local
durability but before provider activation, and `10` authority-failed after a
durable local commit that was deliberately not activated.
Last-operation ids are `0` none, `1` begin, `2` chunk, `3` validate, `4`
abort, and `5` commit. Error ids are stable:

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
| 17 | durable commit succeeded but activation failed |
| 18 | final marker durability is unknown; reconcile status |
| 19 | inactive precommit candidate expired on device |
| 20 | compatible peer authority superseded the precommit candidate |
| 21 | simultaneous peer prepare won deterministic arbitration |
| 22 | local durability completed but peer authority was lost before activation |
| 23 | peer committed a conflicting identity during postcommit convergence |
| 24 | the copy to the peer stopped before any commit, after its storage retries; page 1 names the peer's last answer, and nothing was saved |

Errors which have no domain/table/row/tap/field location use the sentinels
above. The operation sequence lets a host distinguish a newly processed
result from an older polled status without inventing another transaction id.

The executable cross-language golden reads live in
`tests/fixtures/profile_wire_v1_reads.fixture` and are consumed by both the C
host codec suite and the Charybdis Ark JavaScript suite. Exact candidate
begin, chunk, validate, commit, abort, acknowledgement, and operation-status
reports live in `tests/fixtures/profile_candidate_v1.fixture` and are consumed
by the standalone firmware C codec/coordinator suite.

### Value Ids

Every value this firmware routes on custom channel `0x00`. A value missing
from a build (a diagnostic, bridge or read-only image) falls through to QMK's
unhandled reply.

| VIA command | Value id | Operation | Specified in |
| --- | ---: | --- | --- |
| custom get | `0x01` | capabilities page | this document |
| custom get | `0x02` | profile status page | this document |
| custom get | `0x03` | cadence recorder, diagnostic builds only | `runtime_diag.h` |
| custom get | `0x04` | committed payload read | Payload Readback, above |
| custom get | `0x05` | compiled payload read | Payload Readback, above |
| custom get | `0x06` | native combo readback | this document |
| custom get | `0x07` | effective settings readback | [Portable Profile V1](portable-profile-v1.md) |
| custom get | `0x08` | VIA storage status and editor pages | [Portable Profile V1](portable-profile-v1.md) |
| custom get | `0x09` | retired; unsupported | — |
| custom get/set | `0x0A` | bounded split transaction capture, diagnostic builds only | [Split activity sync](split-activity-sync.md) |
| custom get | `0x0B` | detected host OS | Host OS Readback, below |
| custom set | `0x10` | candidate begin | this document |
| custom set | `0x11` | candidate chunk | this document |
| custom set | `0x12` | candidate validate/prepare | this document |
| custom save | `0x13` | candidate commit | this document |
| custom set | `0x14` | candidate abort | this document |
| custom set | `0x15` | logical VIA staging begin | [Logical Profile Transaction V1](logical-profile-transaction-v1.md) |
| custom set | `0x16` | logical VIA staging chunk | [Logical Profile Transaction V1](logical-profile-transaction-v1.md) |
| custom set | `0x17` | logical VIA staging verify | [Logical Profile Transaction V1](logical-profile-transaction-v1.md) |
| custom get | `0x18` | candidate operation status | this document |
| custom get | `0x19` | logical VIA staging status | [Logical Profile Transaction V1](logical-profile-transaction-v1.md) |
| custom set | `0x1A` | logical VIA staging abort: always refused, unsupported | [Logical Profile Transaction V1](logical-profile-transaction-v1.md) |

Every mutating operation includes a nonzero transaction id. Candidate begin
declares schema, length, CRC, digest, and requested domain mask. Chunks include
transaction id, offset, explicit length, and at most the remaining report
payload. Firmware validates every frame length before reading any field.

Retries are idempotent when transaction id, offset, length, and bytes match.
Conflicting retransmission rejects the transaction. Commit acknowledgement loss
is resolved by reading status and comparing transaction, digest, commit state,
last operation, and operation sequence.

USB receive context performs exact frame decoding and one bounded mailbox copy
only. Semantic validation, EEPROM work, safe activation, and split convergence
run from scan context.

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
- final-marker durability unknown;
- post-commit activation failure;
- peer pending or divergent;
- busy or transport contention.

Structured validation details identify domain, table, row, tap index, field,
and error id without returning source strings from firmware.

A candidate the keyboard is asked to save is also held to where its actions
are placed, by the same rules compile-time validation applies to the authored
profile (`noah_action_supported_at`): a behaviour's key, tap, press-and-hold
and other holds, and a combo's output. A combo holds its output while it is
held, so a layer hold (`MO()`, `TT()`) is a key, a combo or a press-and-hold
branch; `OSL()` is a key, a tap or a combo; `LM()`, which holds a layer and its
modifiers, is a key or a combo; `LT()` is only a key, since its own tap/hold
decision on top of a combo's is untested; `TG()`, `TO()` and `LOCK_LAYER()` go
anywhere; `DF()` and `PDF()` go nowhere. A refused
behaviour action is an invalid cross-reference at its row, tap index and
field; a refused combo output rejects the combo domain at its row. Only a
candidate the host uploads is held to placement: a committed record being
adopted, the other half's committed record arriving as a peer candidate, and
the compiled defaults are not, so a profile saved before a rule existed still
loads and still syncs between the halves.

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

## Combo Domain `0x30`, Version 3

Capability `supported_domain_mask` bit 2 advertises combo overrides. Profiles
without this domain use compiled combos. Zero rows explicitly disable all
definitions. Firmware accepts only version 3 (D-F14): up to 128 combos of 2–16
inputs, the inputs counted so a smaller combo takes fewer bytes.

QMK keeps two values for every combo: `COMBO_TERM`, the window a combo without
its own uses, and `COMBO_HOLD_TERM`, the one hold/tap wait. The domain stores
both once, in an eight-byte header, so they exist with or without rows:

| Offset | Bytes | Field |
| ---: | ---: | --- |
| 0 | 1 | row count, 0..128 |
| 1 | 3 | reserved, zero |
| 4 | 2 | default combo window in ms, `1..65535`, little endian |
| 6 | 2 | combo hold threshold in ms, little endian |

Each row is `12 + 4 × inputs` bytes (20 to 76), in priority/index order:

| Offset | Bytes | Field |
| ---: | ---: | --- |
| 0 | 1 | input count, 2..16 |
| 1 | 1 | flags: bit 0 must hold, bit 1 tap only, bit 2 ordered, bit 3 disabled; others zero |
| 2 | 2 | combo window in ms, little endian; `0` follows the default window |
| 4 | 4 | allowed layers, `u32`, one bit per layer; bits at or above the bank (16) zero, an empty mask allowed |
| 8 | 4 | output semantic action |
| 12 | 4 × inputs | the input actions, in declared order |

The rows end exactly at the domain's end. A row that follows the default
changes with it; a row with its own window keeps it, even when it equals the
default. A zero default is rejected because every row following it would
never fire. Hold and tap-only flags are mutually exclusive. A disabled combo
keeps its inputs, output, window and layers; whether it and its layers let a
press join a chord is the [participation policy](participation-policy.md).
Input actions must be distinct, including after native translation;
no-action/transparent inputs and callback/no-action outputs are rejected.
Semantic references must exist in the compiled action ABI. Unknown versions,
unknown flags, a short or overlong row and trailing bytes are rejected.
Incremental validation reads the header in one step, a row's fixed part in
one, then its inputs four at a time (16 bytes), preserving the one-read /
20-byte scan bound. Translation from version 2 keeps each 28-byte row's count,
flags, window and output, drops its four reserved bytes and unused input slots,
and allows every layer (`ff ff 00 00`).

QMK runs these through its long-combo representation (`EXTRA_LONG_COMBOS`): a
16-bit member state, sixteen-member combos and a sixteen-key press buffer. Its
queue of completed combos (`COMBO_BUFFER_LENGTH`, four) is a separate limit.

The existing candidate/commit/split protocol carries this domain with the rest
of the profile. There is no new mutation on GET value `0x06`. The owner publishes
the native table at the strict idle boundary, resolving every window that
follows the default then; QMK, origin tracking and readback use that same table.
A cold publication walks the header and rows once (at most 128 rows of 76
bytes, two to five reads a row) into a native table of 128 rows of up to
seventeen keycodes with the terminator; key processing and combo readback use
RAM only. A failed copy makes combo readback
unavailable and exposes zero definitions rather than a partially decoded table.

## Portable Settings And Complete Readback

Domain `0x40` v6 and GET values `0x07`/`0x08` are specified in
[portable-profile-v1.md](portable-profile-v1.md). The canonical envelope now
permits five known domains; an unknown domain still rejects the candidate.
The settings validator uses the existing bounded reader and safe publication
boundary. Validator/provider state policies are 384/784 bytes respectively
(384 since D-F14: the 128-byte pointing records make the PD iterator the largest domain state).
The standard compiled payload contains all five enabled domains in registry
order: RGB, key behaviours, combos, settings and pointing. Its immutable
factory settings and authored names share the settings domain's encoder.
Compiled combo rows preserve authored order, default-window inheritance and
native actions through semantic encoding. No domain version changes.

Opening compiled defaults records five bounded offset/length pairs alongside
its metadata (a 40-byte handle policy). A domain's length is the byte count
its module's encoder emits; an encoder that emits nothing leaves the domain out
of that build's compiled profile. An arbitrary read emits the blob header,
intersecting domain headers and only intersecting domain payloads; it never
replays preceding domains. Reads may cross any header or domain seam. Effective
caches warm on cold paths, so key events and RGB frames never invoke the virtual
profile reader. The one compiled RGB writer also produces its immutable
memory-backed RGB view for factory fallback; stored and factory frames use the
same domain accessors.

The complete compiled bytes change its compiled-default digest, while its
action-ABI digest stays the same. Store admission still requires the current
compiled-default digest; earlier records therefore fall back to factory
configuration and require a client restore. Full export continues to read
current combo enable state and QMK-owned settings through device readback;
immutable factory values never substitute for those live values.

## Host OS readback

Feature bit 21 also advertises custom GET on channel 0, value `0x0B`, page 0.
The usual request ID, status and padding rules apply; only page 0 exists.
Success carries two bytes: version 1 and the detected OS (0 unknown, 1 macOS,
2 Windows, 3 Linux). Unknown includes unsupported iOS and unavailable detection.
This reports QMK’s current USB guess, not the selected override or confirmation
of host input configuration. Settings scalar 27 stores the override, Unicode
switch and (feature bit 22) host layout separately; clients derive the effective OS with manual selection first.
The result is volatile and is refreshed by Read keyboard; it is not a profile
value, backup field or part of the profile fingerprint. Detection may be wrong
or change after USB setup, particularly with switches/KVMs. Do not infer an OS
from the computer running the client. The retired `0x09` remains unsupported;
`0x0A` remains the diagnostic capture route.

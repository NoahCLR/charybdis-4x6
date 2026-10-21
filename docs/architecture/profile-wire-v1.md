# Profile Wire V1

> Current firmware accepts only the formats it writes (D-F10, D-F14, D-F28):
> Profile Wire 2.0, profile schema 4.0, a slot-sized profile payload, and
> logical store format 6. Every profile length and offset is 32 bits wide, in
> storage and on the wire. The profile is the only store: VIA is not
> supported (D-F31). Split framing is version 4. Older profile/store formats and the
> legacy GET 9 source page are rejected. A backup in an earlier format is
> refused, not translated.

Status: accepted wire contract; Profile Wire 2.0 opens the new base (D-F28).
The file keeps its `v1` name, as do the C modules that implement it.

All multi-byte integers use little-endian byte order. All reserved bytes and
reserved flag bits must be zero. Encoders emit one canonical representation;
decoders reject noncanonical duplicates, ordering, padding, and unused data.

## Canonical Profile Blob

The blob is independent of Raw HID framing and EEPROM slot metadata.

### Blob Header — 8 Bytes

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII magic `NLP1` |
| 4 | 1 | schema major, `4` |
| 5 | 1 | schema minor, initially `0` |
| 6 | 1 | domain count |
| 7 | 1 | flags; bit 0 means canonical encoding, all others reserved |

The transport candidate length or storage header supplies total blob length,
a 32-bit field everywhere it travels. Schema 4.0 is capped at 130,048 bytes,
one 128 KiB flash slot less its 1 KiB physical record (D-F29); nothing in the format
itself stops at 64 KiB. Schemas 1 to 3 are refused.

### Domain Envelope — 6 Bytes Plus Payload

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | domain id |
| 1 | 1 | domain version |
| 2 | 4 | payload length, uint32 |
| 6 | N | domain payload |

Schema 4.0 differs from 3.0 only here: the payload length was 16 bits, and
the envelope 4 bytes. Domain versions keep their numbers (D-F28); a domain's
payload bytes are the same in both.

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
| `0x40` | Settings, layer records, custom-key shortcuts, layer names, macro names, custom-key names and the keyboard name | 8 |
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
round-trip harness saves and reboots both the complete authored seven-domain
profile and a profile with populated imported combo/settings overrides,
checking every published domain and runtime view. Hardware eligibility is
injected; this host harness does not establish physical acceptance. The
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
| 6 | macro slot |
| 7 | custom key `0..127` (`CUSTOM_KEY_n`, native `0x7f00 + n` since D-F14; before it `0..63` at `0x7e40 + n`, now inert); under earlier action vocabularies this kind named the retired user macros, and a digest mismatch keeps the two apart. Valid as a key, a combo output and, since D-F26, a behaviour step's tap, press-and-hold or other hold, where it sends the custom key's shortcut only |

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
([participation policy](participation-policy.md)). Compiled default rows are
enabled and allowed on every bank layer.

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

## Raw HID Profile Channel

The firmware answers Raw HID itself (D-F31): QMK's Raw HID interface, usage
page `0xFF60`, with 32-byte reports in both directions. Every report reaches
`raw_hid_receive` (`users/noah/lib/compat/qmk_profile_channel.c`), goes to the
Profile Wire handlers, and is sent back exactly once as the reply. Byte 0 is
the command (`0x08` get, `0x07` set, `0x09` save), byte 1 the channel, `0x00`
for every Profile Wire value. A report no handler takes is answered with
command byte `0xFF` and the rest of the request unchanged; that is the value
VIA used for unhandled, so the protocol-version, keymap, macro and EEPROM
commands a VIA client sends all get it. The host always sends and receives
exactly 32 bytes. The keyboard identifies itself through the capability page
alone (its firmware version at page 0 bytes 17–20); there is no separate
protocol-version read.

### Native Combo Readback — GET Value `0x06`

This optional read uses the envelope below. It reports the connected half's
effective native combo definitions. It observes the active `0x30` override when
present, otherwise the compiled table. Its digest also covers transient global
enable state, so it is separate from canonical profile identity.
Firmware built without combos reports a
valid disabled empty table. No SET or SAVE operation exists for this value.

The request takes a 16-bit page, its low byte at request byte 4 and its high
byte at byte 5. Every successful page has exactly 25 payload
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
| A | 6 | 1 | flags: bit 0 must hold, 1 must tap, 2 press in order, 3 follows the default window, 4 disabled, 5 bypass output behaviour |
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
| 0 | 1 | command `0x08`, get |
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

Capabilities use three pages (response layout `3`, D-F28).

| Page 0 byte | Field |
| ---: | --- |
| 0 | response layout, `3` |
| 1 | capability page count, `3` |
| 2 | Profile Wire major, `2` |
| 3 | Profile Wire minor, `0` |
| 4 | schema major, `4` |
| 5 | schema minor, `0` |
| 6 | report size, `32` |
| 7 | candidate chunk capacity, `0` or `1..20` |
| 8 | status page count, `2` |
| 9–12 | feature word, below |
| 13–16 | action-ABI digest; zero until the live-profile owner reports |
| 17–20 | firmware version |
| 21–24 | compiled-default digest; zero until the owner reports |

Page 1 holds every ceiling that fits a byte:

| Page 1 byte | Field |
| ---: | --- |
| 0 | compiled layer count |
| 1 | maximum logical layers |
| 2 | maximum behaviour rows |
| 3 | supported tap depth (maximum tap steps per behaviour) |
| 4 | maximum combos |
| 5 | maximum inputs per combo |
| 6 | maximum reusable RGB groups |
| 7 | maximum RGB stage-group rows |
| 8 | physical LED count |
| 9 | LED bitmap bytes |
| 10 | custom-key slots |
| 11 | macro slots (the macro domain's, 128; named VIA macro slots before D-F31) |
| 12 | supported domain mask; zero until the owner reports |
| 13 | maximum encoded bytes per name |
| 14 | layer-mask width in bits, `32` |
| 15 | placement positions per layer (matrix rows × columns) |
| 16–24 | zero |

Page 2 holds the wider ones; every byte count is 32 bits:

| Page 2 byte | Field |
| ---: | --- |
| 0–1 | maximum populated behaviour steps |
| 2–5 | profile slot size in bytes |
| 6–9 | maximum profile payload |
| 10–13 | slot payload capacity |
| 14–17 | macro domain capacity in bytes (34,903; the VIA bank's before D-F31) |
| 18–24 | zero |

### The feature word

Everything the base does is implied by its Profile Wire and schema versions,
which a client requires exactly (Ark's D-L64). The feature word holds only what a
build genuinely varies by, never a step in the firmware's history (D-F28); a
later capability raises the minor version instead. No firmware behaviour
depends on a bit: the word only describes the build.

| Bit | Meaning |
| ---: | --- |
| 0 | split keyboard: this build is one half of a split pair; a save copies the profile to the other half, and status reports the peer |
| 1 | saves: candidate writes (with reuse and streaming chunks), durable commit and activation. Set once the live-profile owner reports, in a build that saves |
| 2 | pointing device: the build has one, so pointing keys and the pointing domain mean something on it |
| 3–31 | reserved, zero |

Supported-domain-mask bits 0–4 are RGB, key behaviors, combos, portable
settings and pointing. Candidate chunk capacity is zero exactly when bit 1 is
clear and otherwise `1..20`. Before the owner reports, the word holds bits 0
and 2 only, and the digests and domain mask are zero. The live client rejects
inconsistent combinations before it offers a live operation.

#### The Profile Wire 1 feature word (retired)

Profile Wire 2 retired every bit of the earlier word; this is their one
record. What each promised is now part of the base, except that bit 4 is
today's bit 0; bits 5, 6, 8, 9, 12, 24 and 25 are today's bit 1 (peer
reconciliation is bits 0 and 1 together); bits 2 and 3 are supported-domain-mask
bits 0 and 1; bits 10 and 11 are the nonzero digests on page 0 (status reports
"digests unavailable" until then); and bits 7 (RGB preview, never built), 13
and 20 have no successor. "Require this bit" in the old text means nothing
now: every Profile Wire 2 firmware does what the bit promised.

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
| 16 | custom keys and keycode blocks: userspace keycodes sit in fixed blocks (pointing holds `0x7e80`, pointing locks `0x7ea0`, layer locks `0x7ec0`, custom keys `0x7f00` since D-F14, each reserved beyond what is supported), action kind 7 is a custom key, and the settings domain names the 128 custom keys. It comes with its own action ABI digest; a host knowing that digest may offer custom keys as keys and combo outputs. With settings v8 (D-F26) a custom key may also be a behaviour step, sending its shortcut only; a host that writes settings v8 may offer that |
| 17 | physical gesture timing: handled physical keys measure holds from physical press and repeats from physical release to next press; eligible records buffered by combo/tapping retain their series. Authored `LT()` rows bypass native QMK tapping; plain `LT()` keys retain it. An authored `LT()` row's layer is its hold: without an authored first hold it holds `MO()` from the tap-hold term, never from the press (joined the bit on the same unreleased branch). Combo outputs retain their delivery/origin timing contract. This is a runtime capability, not a profile format change |
| 18 | runtime-owned tapping: every key handled by userspace bypasses native QMK tapping, including authored MT/OSM rows and intrinsic TT/OSL ownership. An authored MT/OSM row without a first hold keeps the key's modifier hold past the tap-hold term, and keys pressed while such a runtime-owned LT/MT/OSM key is undecided wait for its tap or hold (both joined the bit on the same unreleased branch). Unhandled LT/MT/OSM keep native QMK tapping. Extends bit 17 without changing its physical timestamp contract or profile bytes |
| 19 | wide pages: the payload, compiled, combo and settings readbacks (GET `0x04`, `0x05`, `0x06`, `0x07`) take a 16-bit page, request byte 4 its low byte and byte 5 its high byte, with bytes 6–31 reserved; their responses echo bytes 0–4 and the request id correlates them |
| 20 | reserved, never advertised: participation controls ([participation policy](participation-policy.md)) come with the current settings and combo domain versions, so no bit announces them |
| 21 | canonical UTF-8 macro text and settings scalar 27 Host settings and GET 0x0B OS readback; require this bit before writing either extension. See [runtime flow](runtime-flow.md) |
| 22 | host layouts: settings scalar 27 bits 16–23 name a layout from the [host layout catalogue](host-layouts-v1.md) and bit 24 marks a macOS ISO keyboard; macro text is typed through that layout. Require this bit before writing bits 16–24. See [runtime flow](runtime-flow.md) |
| 23 | macro input protection: automatic protection when the selected layout needs Unicode entry, with optional per-macro `01 05 01` (On) or `01 05 02` (Off) prefixes. Require this bit before writing a prefix. New physical presses are ignored until released; earlier releases remain admitted. See [runtime flow](runtime-flow.md) |
| 24 | candidate reuse: copy bounded ranges from an identified active profile into the inactive candidate (`SET 0x1B`, below) |
| 25 | candidate streaming chunks: admit sequential chunks and prove bounded batches through status (`SET 0x1C`, below) |
| 26 | protected macro keyboard-output isolation: suspend already-held ordinary keys, send only macro keyboard output, restore current held modifiers and suppress remaining ordinary usages until release; requires bit 23. See [runtime flow](runtime-flow.md) |
| 27 | streaming stored macros: no separate compiled-program ceiling; slot bytes and terminators must fit the advertised macro capacity (34,903 bytes in this layout). See [runtime flow](runtime-flow.md) |
| 28 | pointing-mode clicks: a direction or button-override tap in the PD domain may be a plain mouse button 1–8 (`0x00d1..0x00d8`). Require this bit before writing one; older firmware rejects it as `INVALID_ACTION`. See [PD-mode domain](pd-mode-domain-v1.md#taps-modifiers-and-buttons) |
| 29 | pointing-mode macros: direction, diagonal and button tap overrides may start slots 0–127 (`0x7700..0x777f`), inherit policy and zero mask only. Require this bit before writing one. See [PD-mode domain](pd-mode-domain-v1.md#taps-modifiers-and-buttons) |
| 30 | combo output behaviour permissions: settings header byte 5 bit 0 bypasses all combo output behaviours; combo v3 row flag bit 4 bypasses one output. Both zero means allowed. Native combo readback v3 row flag bit 5 reports the per-combo bypass. Require this bit before writing either extension. See [participation policy](participation-policy.md#combo-output-behaviour-permission) |
| 31 | unified tapping: every `LT()`, `MT()` and `OSM()` key is decided by the key runtime with or without a behaviour row, and a key without a row behaves exactly as with an empty one; behaviours switched off or bypassed ignore the row only. QMK's tapping engine classifies no record of a key the firmware supports, physical or combo output. A dual-role key (`LT`, `MT`, `OSM`, `TT`, `OSL`) holds at the dual-role setting (settings id 0) unless its row sets its own time; every other row holds at the tap/hold setting. Tapped and then held again at once, such a key starts a new hold rather than repeating its tap. A row without steps is no row for a key's taps and for whether the runtime handles it: a plain key with one stays native, `TT()` keeps its own taps, and its hold times apply only to a key the runtime handles anyway (both joined the bit on the same unreleased branch). Extends bits 17 and 18 (D-F27) without changing profile bytes |

Status also uses two pages. Page 0 reports state flags and source, compiled,
active, pending, and committed digests. Page 1 reports active, committed, and
peer generation identities plus transaction, conflict, validation, and error
state. Until the store and generated compiled digest land, firmware truthfully
reports `compiled-only` with `digests unavailable`; zero is not presented as a
real digest.

State flags (page 0, bytes 2–3). Bits 10–15 are reserved and must be zero.

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
| 9 | a slot was refused as an earlier store format or test layout |

Page 0 byte 24 is the active kind: compiled defaults `0`, committed `1`,
pending `3` (pointing caches not ready), or the working copy `4` (D-F32:
unsaved live edits). For the working copy, page 1's active generation and
origin are its saved base's (zero and `255` over the compiled defaults) and
page 0's active digest carries its revision, not a digest; GET `0x21` page 0
has its session too, for a REUSE naming it (source kind `2`).

Peer cleanup pending (added 2026-09-23) means a cancelled save's split ABORT
was never acknowledged, so the USB half released its own side after
`NOAH_PROFILE_SPLIT_PREPARED_ABORT_TIMEOUT_MS` (see the
[authority state table](authority-state-table.md#cancelled-prepare-peer)). No
marker exists on either half, and durable authority is unchanged. Until the
peer acknowledges the retried ABORT, the keyboard starts no new save or split
transfer; a peer that never does needs a restart (SET `0x28`, below, or a
replug). A decoder that predates the
bit rejects the status while it is set, as it rejects any unknown flag.

Bit 9 (D-F28) means a slot holds a profile written in an earlier store format
(format 5, `NT`) or a retired format-6 test layout (the compact 32-byte
header or the VIA-bound header), and nothing current is committed. The firmware never reads or
converts such a slot (Ark's D-L64): the keyboard runs its compiled defaults, and
bit 0 is set with it. The bit clears once a profile is saved, which writes the
other slot; the earlier one stays untouched until a later save reuses it.

GET `0x0c`, the sparse VIA macro read map (D-F21), is retired with VIA
(D-F31) and answered unhandled.

### Payload Readback — GET Values `0x04` And `0x05`

GET `0x04` reads the committed payload and `0x05` the compiled defaults the
firmware was built with, through the envelope above with a wide page (request
byte 4 its low byte, byte 5 its high byte): a 130,048-byte payload needs
5,203 pages, and 16 bits of page reach 1.6 MB. Page 0 is metadata; page
`n` from 1 carries payload bytes `(n - 1) × 25` onward, at most 25, with the
payload length byte giving the count. A page past the end answers status `2`;
status `3` means nothing is committed, or the compiled defaults cannot be
opened. Page 0's 25-byte payload, little-endian:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version `2` |
| 1 | 1 | page payload size `25` |
| 2 | 4 | payload length, uint32 |
| 6 | 4 | generation; `0` for the compiled defaults |
| 10 | 4 | FNV-1a payload digest |
| 14 | 4 | CRC32 of the payload |
| 18 | 1 | schema major |
| 19 | 1 | schema minor |
| 20 | 1 | domain mask |
| 21 | 1 | origin half; committed record only |
| 22 | 1 | record flags; committed record only |
| 23 | 2 | reserved, zero |

Layout 1 (Profile Wire 1) had a 16-bit length at offset 2 and every later
field two bytes earlier.

### Candidate Mutation Envelope

Candidate staging mutations use custom set command `0x07`; durable commit
uses custom save command `0x09`. Both use custom channel `0x00` and an exact
32-byte report:

| Value id | Operation |
| ---: | --- |
| `0x10` | begin candidate |
| `0x11` | write candidate chunk |
| `0x12` | validate candidate |
| `0x13` | durably commit and request safe activation |
| `0x14` | abort candidate |
| `0x1B` | reuse active candidate bytes |
| `0x1C` | write streaming candidate chunk |

Custom set values `0x15`, `0x16`, `0x17` and `0x1A`, and get `0x19`, were the
logical VIA staging channel, retired with VIA (D-F31) and answered unhandled;
the complete map is under Value Ids below. A firmware build must not route
candidate mutation frames or advertise candidate-write capability merely
because the standalone codec and coordinator are compiled.

All mutation requests use this correlation header (byte 0 is `0x07` except
for commit, which uses `0x09`):

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | command: set `0x07`, or save `0x09` for commit |
| 1 | 1 | custom channel `0x00` |
| 2 | 1 | operation value id |
| 3 | 2 | nonzero transaction id |

Begin candidate uses this complete layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 1 | schema major, `4` |
| 6 | 1 | schema minor, `0` |
| 7 | 1 | requested-domain mask; bits 0 RGB, 1 key behaviors, 2 combos, 3 settings, 4 pointing |
| 8 | 4 | canonical blob length, `8..130048`, uint32 |
| 12 | 4 | CRC32 of the exact canonical blob |
| 16 | 4 | FNV-1a digest of the exact canonical blob |
| 20 | 4 | action-ABI digest used to encode actions |
| 24 | 8 | reserved, zero (the bound VIA generation and digest until D-F31) |

Profile Wire 1's BEGIN also carried a flags byte (always zero) and the store
format (`5`); a 32-bit length leaves no room for either, and the protocol
version now implies the store format (6). A length whose low 16 bits are in
range but whose high bytes are not is refused like any other.

The requested-domain mask may be zero for the canonical empty profile and may
contain only the domains advertised by that firmware. Compatibility with the build's advertised
schema, domain mask, action ABI, and capacity is checked by the scan owner
before storage work begins.

A repeated BEGIN for the same transaction is idempotent only when every field
matches the original.
A changed field rejects and poisons the candidate with `CONFLICTING_RETRY`;
it cannot replace the original identity or restart storage work. Repeating the
original BEGIN afterward does not revive the poisoned candidate.

Candidate chunk uses this complete layout:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 4 | zero-based byte offset in the candidate blob, uint32 |
| 9 | 1 | chunk length, `1..20` |
| 10 | 20 | chunk bytes, followed by zero padding |
| 30 | 2 | reserved, all zero |

Chunks are sequential. A retry of an already staged `{transaction, offset,
length, bytes}` tuple is accepted after the scan owner reads and compares the
staged bytes. A retry with different bytes rejects and poisons that candidate.
Partial overlaps, gaps, writes beyond the declared length or selected schema bound,
and nonzero padding are rejected.

#### Differential candidate transfer

REUSE (`SET 0x1B`) and STREAM_CHUNK (`SET 0x1C`) come with candidate writes
(feature bit 1, saves). A client may still upload every byte with ordinary
chunks.

REUSE uses the same transaction header as CHUNK:

| Offset | Size | Field |
| ---: | ---: | --- |
| 5 | 4 | destination offset in the new candidate, uint32 |
| 9 | 4 | source offset in the active canonical blob, uint32 |
| 13 | 4 | copy length, `1..1024`, uint32 |
| 17 | 4 | source generation |
| 21 | 4 | source FNV-1a digest |
| 25 | 4 | source CRC32 |
| 29 | 1 | source active kind: compiled `0`, committed `1`, working copy `2` (D-F32: generation and origin its base's, digest its revision, CRC32 its session) |
| 30 | 1 | source origin: compiled `255`, committed `0..1` |
| 31 | 1 | reserved, zero |

Compiled sources require generation zero; committed sources require a nonzero
generation. Destination ranges must start at the exact next staged offset.
Ranges cannot exceed the source or declared target. Each read checks the active
source's kind, origin, generation and both checksums, plus the build's compiled
default and action ABI identities. The source stays active; only the inactive
candidate is written. A scan step copies up to one flash page (256 bytes) of the
range, each piece read and then written in at most 20 bytes, so a step programs
at most one page (D-F29, "Pacing"). Progress advances `next_offset` after each
write, and the mailbox remains pending until the range finishes. REUSE increments the operation
sequence once, at completion; its operation ID is `6`. Source/read/write failure
poisons the candidate. There is no implicit retry of a partially copied range.
The range ceiling bounds one occupied mailbox and the wait before an abort can
be admitted. Owner supersession can discard it earlier, part copied. The transaction remains within the existing 256-byte state ceiling and
holds only a 20-byte copy buffer, never a profile-sized RAM buffer.

STREAM_CHUNK has CHUNK's layout and status operation ID (`2`), but admission
checks transaction, receiving state, exact next offset and declared capacity.
Admission `4` (`REJECTED`) names the failed check in its error byte and gives a
frame offset or `255`. The existing `QUEUED`, `BUSY`, `MALFORMED` and `UNSUPPORTED`
admissions retain their meanings. The mailbox still admits one command; no queue
or work in the HID callback is added. Storage failure is sticky: later streaming
writes cannot erase it. A host can send a bounded batch of admitted chunks, then
prove the exact next offset, transaction, candidate identity and operation-sequence
advance before continuing. On BUSY it drains the admitted prefix and retries
only the definitely unadmitted suffix. A lost reply is ambiguous, never permission
to resend a batch blindly.

These operations only reconstruct the candidate. Full checksum and semantic
validation, peer preparation, marker-last durability, activation, cancellation
and recovery remain the same. The copy to the other half reuses too: the
firmware remembers the ranges REUSE staged and asks the other half to fill
them from its own active profile, which it checks against the same source
identity, and sends only the rest. Where the other half's active profile is
not that source, it sends the bytes. The other half still receives, validates
and prepares the complete candidate. This is internal to the firmware
([split payload reuse](profile-split-v1.md#payload-reuse)); the host sends
nothing different.

Validate and abort contain only the five-byte common header; bytes 5 through
31 are reserved and zero. Commit uses the same body-free shape but byte 0 is
custom save `0x09` rather than custom set `0x07`. Validate can start only after
every declared byte is staged. It performs digest and semantic validation
incrementally from scan context.

Commit is admitted only for the matching validated transaction. The scan owner
writes and reads back the non-marker header, streams one payload readback,
checks canonical top-level shape, writes the two-byte commit marker last, reads
that marker back, and only then requests provider activation. Each scan step
performs one storage operation of at most 20 bytes, except the payload
readback, which reads up to a flash page (256 bytes) a step in reads of at most
20 (D-F29, "Pacing"). Behavior activation
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
processed precommit work; see
[Profile Transaction V1](logical-profile-transaction-v1.md#cancellation-and-lease-ownership).
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
| 5 | 1 | admission: `0` queued, `1` malformed, `2` busy, `3` unsupported, `4` rejected |
| 6 | 1 | stable candidate error id |
| 7 | 1 | malformed-frame byte offset, or `0xFF` |
| 8 | 24 | reserved, all zero |

`queued` acknowledges only the bounded copy. It is not validation, durability,
commit, or activation success.

### Candidate Operation Status

Candidate operation status uses custom get value `0x18` and the normal read
request/response envelope. It exists only in builds that route and advertise
candidate writes. Page 0 is the candidate's progress; its successful 25-byte
payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `2` |
| 1 | 1 | candidate state |
| 2 | 1 | last operation |
| 3 | 1 | flags: bit 0 mailbox pending, bit 1 candidate poisoned |
| 4 | 2 | candidate or last affected transaction id |
| 6 | 4 | next sequential byte offset, uint32 |
| 10 | 4 | declared candidate length, uint32 |
| 14 | 4 | declared FNV-1a digest |
| 18 | 1 | stable candidate error id |
| 19 | 2 | operation sequence, incremented after each processed mailbox item |
| 21 | 2 | error sequence, incremented whenever the error id or its location changes |
| 23 | 2 | reserved, zero |

Page 3 (D-F28) is where the error is. Layout 1 of page 0 carried it, with a
16-bit byte offset; the 32-bit offsets leave it no room. Its payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `1` |
| 1 | 1 | stable candidate error id, as on page 0 |
| 2 | 1 | error domain id, or `0xFF` |
| 3 | 1 | error table id, or `0xFF` |
| 4 | 2 | error row index, or `0xFFFF` |
| 6 | 1 | error tap/step index, or `0xFF` |
| 7 | 1 | error field id, or `0xFF` |
| 8 | 4 | candidate byte offset, uint32, or `0xFFFFFFFF` |
| 12 | 2 | transaction id, as on page 0 |
| 14 | 2 | operation sequence, as on page 0 |
| 16 | 2 | error sequence, as on page 0 |
| 18 | 7 | reserved, zero |

A host reads page 3 after a page 0 that reports an error and keeps it only
when its transaction id, operation sequence and error sequence all match that
page 0; otherwise it reads page 0 again. The operation sequence alone is not
enough: validation, commit and peer events replace the error between mailbox
items (a CHUNK refused while validating, then the validator's own rejection,
share one operation sequence), and only the error sequence moves with each
replacement. Setting the same error again does not advance it.

Page 1 (added 2026-09-23) reports the save's copy to the other half, which
page 0 cannot show: during `PREPARING_PEER` page 0 stays identical until the
peer is ready. Builds with the live-profile owner answer it; others answer
`UNKNOWN_PAGE`, which the host treats as "not reported". Its 25-byte payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `2` |
| 1 | 1 | peer phase |
| 2 | 1 | the peer's last split status |
| 3 | 1 | flags: bit 0 peer cleanup pending, bit 1 this half is transport master, bit 2 waiting for a safe boundary |
| 4 | 4 | bytes of the profile transferred to the peer, including those it copied from its own profile, uint32 |
| 8 | 4 | bytes to transfer, uint32 |
| 12 | 4 | split retries since boot |
| 16 | 4 | split transport failures since boot |
| 20 | 2 | the peer's BUSY replies in a row (saturating) |
| 22 | 1 | why the peer last held off: its last BUSY reason other than routine admission |
| 23 | 1 | the peer's store state in its last BUSY |
| 24 | 1 | low nibble: the peer's transfer owner in its last BUSY; high nibble: who held the peer's storage admission then, `0` none, `1` host, `2` peer |

Page 2 is a read-only peer-copy diagnostic. Its 25-byte payload is:

| Payload offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout version, `2` |
| 1 | 1 | flags: bit 0 reuse became unavailable; other bits zero |
| 2 | 4 | bytes acknowledged as copied from the peer's own source, uint32 |
| 6 | 2 | PAYLOAD_REUSE RPC attempts, including repeated progress polls and transport failures |
| 8 | 2 | correlated SOURCE_UNAVAILABLE replies accepted by the sender |
| 10 | 4 | sender reconciler scan invocations during the prepared push |
| 14 | 4 | all RPC attempts during the prepared push, including failed attempts |
| 18 | 4 | current reconciler transfer offset, as on page 1, uint32 |
| 22 | 3 | reserved, zero |

Layout 1 of pages 1 and 2 had 16-bit offsets and lengths; page 2 also
repeated page 1's transfer length, which layout 2 leaves to page 1.

Counters saturate at their field widths, reset when a new prepared push is
accepted, and remain available after it ends. They include validation,
preparation, commit and retries, so take differences at the phase boundaries
to isolate sending. Reconciler invocations include visits during retry waits;
they are not all QMK matrix scans, since another durable-I/O subsystem may
own a scan. An RPC attempt is QMK's complete info/request/execute/reply
sequence, not one serial subtransaction. Reused bytes advance on a range's
acknowledgement or source-unavailable reply, not each in-flight copy step.
Read page 1 before and after page 2 and discard a sample crossing a phase or
transfer change; the two reports are individually coherent, not one atomic
capture. A later background repair can change the current offset and length
without resetting the last prepared push's counters.

Flag bit 2 (added 2026-09-25) means the commit decision is waiting for this
half to go idle: no held key, pending
tap, locked layer or pointer mode, one-shot, macro or combo. Input still works
in this state, so the user can release or unlock what holds it. If it does not
clear before the 60 s pre-decision no-progress timeout, the save is cancelled
with `TIMEOUT` and nothing changes; after the decision the save simply waits.

Busy reasons are `0` unspecified, `1` admitted (queued; the answer comes on a
retry), `2` mailbox full (an earlier request is still unprocessed, which, if it
persists, means the peer is not getting scan time), `3` the peer's store holds
a different copy, `4` the store is not receiving this copy, `5` the store is
validating, preparing or committing, `6` the peer is pulling a profile itself,
and `7` the peer only converges just now. Copying a split reuse range is reported
as `5` (store working), keeping the host vocabulary unchanged. Store states are `0` uninitialized,
`1` idle, `2` receiving, `3` validating, `4` preparing, `5` prepared, `6`
committing, `7` committed, `8` rejected and `9` reconcile required; transfer
owners are `0` none, `1` receiving a remote push, `2` pulling. Bytes 19–21
come from every BUSY, including the routine admission reply, so they describe
the peer as it is now; byte 18 keeps the last reason that was not admission.
All of them reset when a copy starts. The host reports these when a copy
stalls; no decision depends on them.

Peer phases are `0` idle, `1` retired (binding the staged VIA copy until
D-F31; never reported), `2` beginning, `3`
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
abort, `5` commit, and `6` reuse. Error ids are stable:

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
unhandled reply (command byte `0xFF`).

| Command | Value id | Operation | Specified in |
| --- | ---: | --- | --- |
| custom get | `0x01` | capabilities page | this document |
| custom get | `0x02` | profile status page | this document |
| custom get | `0x03` | cadence recorder, diagnostic builds only | `runtime_diag.h` |
| custom get | `0x04` | committed payload read | Payload Readback, above |
| custom get | `0x05` | compiled payload read | Payload Readback, above |
| custom get | `0x06` | native combo readback | this document |
| custom get | `0x07` | effective settings readback | [Portable Profile V1](portable-profile-v1.md) |
| custom get | `0x08` | editor pages; page 0, the VIA store's status until D-F31, is an unknown page | [Portable Profile V1](portable-profile-v1.md) |
| custom get | `0x09` | retired; unsupported | — |
| custom get/set | `0x0A` | bounded split transaction capture, diagnostic builds only | [Split activity sync](split-activity-sync.md) |
| custom get | `0x0B` | detected host OS | Host OS Readback, below |
| custom get | `0x0C` | retired with VIA (sparse macro read map); unhandled | D-F21, D-F31 |
| custom get | `0x0D` | flash pauses of each half | Flash Pauses, below |
| custom set | `0x10` | candidate begin | this document |
| custom set | `0x11` | candidate chunk | this document |
| custom set | `0x12` | candidate validate/prepare | this document |
| custom save | `0x13` | candidate commit | this document |
| custom set | `0x14` | candidate abort | this document |
| custom set | `0x15`–`0x17` | retired with VIA (logical staging); unhandled | D-F31 |
| custom get | `0x18` | candidate operation status | this document |
| custom get | `0x19` | retired with VIA (logical staging status); unhandled | D-F31 |
| custom set | `0x1A` | retired with VIA (logical staging abort); unhandled | D-F31 |
| custom set | `0x1B` | candidate reuse | Differential candidate transfer, above |
| custom set | `0x1C` | candidate streaming chunk | Differential candidate transfer, above |
| custom set | `0x20` | edit begin | Live Edits, below (D-F32) |
| custom get | `0x20` | edit status pages 0–2 | Live Edits, below |
| custom set | `0x21` | edit splice | Live Edits, below |
| custom get | `0x21` | the active profile (the working copy, or the saved profile), paged | Live Edits, below |
| custom set | `0x22` | edit data (inserted bytes) | Live Edits, below |
| custom get | `0x22` | runtime overrides and QMK's live values | Live Edits, below |
| custom set | `0x23` | edit commit | Live Edits, below |
| custom set | `0x24` | edit abort | Live Edits, below |
| custom set | `0x25` | save the working copy | Live Edits, below |
| custom set | `0x26` | revert the working copy | Live Edits, below |
| custom get/set | `0x27` | edit mode | Live Edits, below |
| custom set | `0x28` | restart both halves | Live Edits, below (D-F34) |

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
run from scan context. Restart (SET `0x28`) is the one exception: its handler
asks the other half over a split RPC, for at most 250 ms, before it answers,
since the keyboard restarts right after; it touches no profile state.

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
| 1 | 1 | flags: bit 0 must hold, bit 1 tap only, bit 2 ordered, bit 3 disabled, bit 4 bypass output behaviour; others zero |
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

The output bypass flag does not disable the chord or change its inputs,
output, timing or chord-layer mask. It is independent of the global output
behaviour switch in settings header byte 5. Both flags are zero by default;
existing v3 combo data keeps its behaviour and needs no format migration.
The completed chord snapshots output permission and origin for
queued delivery and release; see the [participation policy](participation-policy.md#combo-output-behaviour-permission).
Incremental validation reads the header in one step, a row's fixed part in
one, then its inputs four at a time (16 bytes), preserving the one-read /
20-byte scan bound.

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

Domain `0x40` v8 and GET values `0x07`/`0x08` are specified in
[portable-profile-v1.md](portable-profile-v1.md). The canonical envelope now
permits seven known domains; an unknown domain still rejects the candidate.
The settings validator uses the existing bounded reader and safe publication
boundary. Validator/provider state policies are 400/784 bytes respectively
(384 since D-F14: the 128-byte pointing records make the PD iterator the largest domain state;
400 since D-F28, whose 32-bit blob lengths and offsets add 20 bytes, 392 on Cortex-M0+).
The standard compiled payload contains all seven enabled domains in registry
order: RGB, key behaviours, combos, settings, pointing, keymap and macros. Its immutable
factory settings and authored names share the settings domain's encoder.
Compiled combo rows preserve authored order, default-window inheritance and
native actions through semantic encoding. No domain version changes.

Opening compiled defaults records seven bounded 32-bit offset/length pairs
alongside its metadata (a 76-byte handle). A domain's length is the byte count
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
the stored settings, not QMK's copies of them (D-F32); QMK's live values and
the toggles' overrides read back through GET `0x22` (Live Edits, above).

## Live Edits

An edit takes effect on both halves at once, without a save (D-F32). Each
half keeps a **working copy** of its active profile in RAM, one slot payload
in size (capability page 2's slot payload, 130,048 bytes). While nothing is
edited the keyboard runs its saved profile (a committed slot, or the compiled
defaults when nothing is saved) and the working copy is unused; the first
edit starts it from the saved profile, and from then until Save, Revert, an
Apply or a replug the keyboard runs the working copy: it has **unsaved
edits**. A replug always boots the saved profile, and so does a restart
(SET `0x28`, below).

### Identity: session and revision

The working copy is named by a **session**, a 32-bit value chosen afresh at
every boot, and a **revision**, which starts at zero each boot and advances
by one with every change to the working copy's bytes: each published edit,
each Revert, and each profile activated from outside it (an Apply, or a newer
profile the other half supplied). Save does not advance it: the saved bytes
are the working copy's. Every edit, Save and Revert names the session and
revision it was made against; anything else is refused STALE, and the reply
carries the current pair. The pair never repeats within a boot; a client that
sees a different session knows the keyboard restarted and its unsaved edits
are gone.

The keyboard's adjustment keys (DPI, RGB) queue settings edits and advance
the revision too. A client's next edit learns of them as STALE; while a
keyboard adjustment waits to publish, new client edits are BUSY, and so is a
new Apply: its candidate BEGIN (a transaction other than the one running) is
answered BUSY with error `MAILBOX_BUSY`, and edit status page 0 flag 13 says
why. Published later, the adjustment would overwrite what the Apply set.
Retry once the flag clears. An Apply already admitted carries on to its end;
an adjustment pressed during it stays queued, publishes after the Apply lands
on the profile it activated (the key press is the later act, so it wins), and
is then saved as any adjustment is.

### An edit

An edit is up to eight **splices**, applied together as one revision or not
at all. A splice names a domain id, an offset into that domain's payload, a
count of bytes removed there and a count of bytes inserted; the inserted
bytes come after it, in splice order, as one stream. Splices are in domain
order, then offset order, and do not overlap. A splice may instead
**create** an absent domain (offset and removal zero, the whole payload
inserted; the firmware writes the domain header in canonical order) or
**drop** one (offset zero, removal its whole length, nothing inserted; a
required domain cannot be dropped). The blob header and every domain header
are the firmware's to write: lengths follow from the splices.

The inserted bytes are staged at the top of the working copy, above both the
current and the resulting profile, so an edit fits when its inserted bytes
plus the larger of the two profiles fit the slot payload. Anything larger is
refused CAPACITY_EXCEEDED before any byte arrives. A **replacement** (an
import) has no splices: its stream is a whole new profile, written straight
into the working copy, so it needs the saved profile active (no unsaved
edits; NOT_CLEAN otherwise: Save or Revert first).

Each domain an edit touches is checked on its own, exactly as a whole-profile
validation checks it, action references and placements included, and held
to what a host's candidate is held to; no other domain is read, and the
whole profile is not checksummed. The resulting profile must keep every
required domain and fit the payload ceiling. A replacement is checked whole
(envelope and every domain), still without a checksum. An edit that fails
changes nothing.

A checked edit is published at a safe boundary, the same one activation
waits for (held keys, taps in progress, locks, one-shots, macros): until then
the edit status says WAITING_SAFE and typing goes on. Its bytes are rewritten
inside the publication, then the runtimes that read the changed domains are
rebuilt; a domain the edit did not touch keeps its tables, and its settings
are not projected into QMK again.

### SET reports

Command `0x07`, channel `0`, value at byte 2, transaction id (nonzero,
little-endian) at bytes 3–4; every byte after a value's fields must be zero.
Multi-byte fields are little-endian.

| Value | Bytes 5… |
| --- | --- |
| `0x20` begin | 5–8 session, 9–12 revision, 13 splice count (1–8, or 0 for a replacement), 14 flags (bit 0 replacement), 15–18 inserted bytes in total |
| `0x21` splice | 5 index (in order from 0), 6 domain id, 7 flags (bit 0 create, bit 1 drop), 8–11 offset, 12–15 removed, 16–19 inserted |
| `0x22` data | 5–8 offset in the inserted stream, 9 count (1–22), 10… the bytes |
| `0x23` commit | 5–8 CRC-32 of the whole inserted stream |
| `0x24` abort | nothing; the edit in progress with this transaction is dropped (OK when none is) |
| `0x25` save | 5–8 session, 9–12 revision |
| `0x26` revert | 5–8 session, 9–12 revision |
| `0x27` edit mode | 5 mode: `0` Apply, `1` Live, `2` Live + review |
| `0x28` restart | 5–8 the confirmation `52 45 53 54` ("REST"); nothing after it (D-F34) |

Splices and data arrive in order; a repeated report whose answer was lost is
accepted again when it matches what was taken (a data report's bytes are
compared), and refused OUT_OF_ORDER when it does not. Commit checks that every
splice and byte arrived and that the CRC matches, then answers QUEUED; the
outcome is the edit status's last result for that transaction.

Every SET is answered in place: bytes 0–4 echoed, then

| Byte | Field |
| ---: | --- |
| 5 | result |
| 6 | working copy phase |
| 7–10 | session |
| 11–14 | revision |
| 15–16 | edit status flags (page 0) |
| 17 | restart only: `1` the other half acknowledged and restarts too, `0` it did not answer |

Results: `0` OK, `1` QUEUED (the outcome comes later), `2` STALE, `3` BUSY
(another edit, a save, an Apply, a publication waiting, a keyboard adjustment
waiting to publish, or the other half not yet holding the same working copy), `4` INVALID_ARGUMENT (a malformed report
too), `5` INVALID_STATE, `6` WRONG_TRANSACTION, `7` OUT_OF_ORDER, `8`
CAPACITY_EXCEEDED, `9` CHECKSUM_MISMATCH, `10` UNSUPPORTED_DOMAIN, `11`
INVALID_SPLICE, `12` VALIDATION_REJECTED (page 2 says where), `13` NOT_CLEAN,
`14` READ_ERROR (also: this half has nowhere to store the edit mode), `15`
ABORTED (a save, an Apply or the other half's newer profile replaced the
profile the edit was made against, or the edit was left unfinished: a
Revert, or no accepted report for 15 s), `16` PUBLICATION_FAILED. Phases: `0` idle, `1`
receiving, `2` loading (the first edit copying the saved profile), `3`
validating, `4` waiting for a safe boundary, `5` reverting.

A new edit, Save and a mode change are BUSY while anything else holds the
profile and until the other half holds the same session and revision; an
edit that has begun carries on. Revert never waits for the other half, which
follows. An edit being received (phase `1`) holds a lease: with no accepted
begin, splice or data report for 15 s on the keyboard's clock (which, as for
an Apply, stops while storage waits for a quiet keyboard), the keyboard
aborts it, last result ABORTED. A client that lost track of its edit can
also Revert: Revert aborts an edit being received, then reverts. While an edit is being received or checked, or a save runs, a
candidate's begin, chunk, reuse, validate and commit are answered BUSY; abort
is not.

### Save

Save writes the working copy as a new generation through the ordinary
candidate path: a candidate made entirely of REUSE from the working copy, so
nothing is uploaded and the other half copies its own working copy too
(source kind `2`, below), with the same validation, durable preparation on
both halves, marker-last decision and convergence as any Apply, and the same
power-loss guarantees. Its transaction is the candidate's: candidate status
(GET `0x18`) follows it exactly as it follows an Apply. With no unsaved edits
Save answers OK at once and writes nothing. A working copy byte for byte the
saved profile it began from (edits undone, a value set back) is not written
either: when its checksum, length and domains are the saved profile's, Save
compares the bytes (save phase `7`), and if they are the same the keyboard
runs its saved profile again at the next safe boundary (phase `8`), with
nothing unsaved and the same revision, and the other half follows as after a
Revert. The keyboard's own save in Live does the same. When it lands, the saved profile
holds the working copy's bytes: no unsaved edits, the same revision. A save
the keyboard cannot finish is reported as an Apply's would be, and the
working copy is left as it was. A save waits for a quiet keyboard before each
flash erase (D-F29); that wait counts toward no timeout on either half.

**REUSE from the working copy.** Candidate REUSE (`0x1B`) and the split
PAYLOAD_REUSE name a source by kind, generation, origin, digest and CRC. Kind
`2` is the working copy: its generation and origin are its saved base's
(`0` and `255` over the compiled defaults), its digest field carries its
revision and its CRC field its session. A host may use it too, for an Apply
that keeps most of the working copy.

**Apply** still works and replaces the working copy: unsaved edits are gone
once it activates, and the revision advances.

### Revert

Revert discards the working copy at the next safe boundary on both halves
and runs the saved profile again; no flash is written. With no unsaved edits
it answers OK and changes nothing.

### GET reports

Command `0x08`, channel `0`, the usual nonzero request id at byte 3; a
malformed request answers status `1`, an unknown page status `2`, an owner
not ready status `3`.

**`0x20` edit status**, byte 4 the page (0–3), 25-byte payloads:

| Page 0 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | phase |
| 2 | 2 | flags: bit 0 unsaved edits, 1 the other half is required, 2 the other half holds this session and revision with nothing in progress, 3 the other half answers, 4 a new edit would be BUSY now, 5 the saved profile is the compiled defaults, 6 a save is running, 7 the halves' edit modes conflict, 8 waiting for a safe boundary, 9 a save here waits for a quiet keyboard, 10 the other half's save waits for a quiet keyboard, 11 the other half refused this working copy, edit and whole copy (apart until Revert or an Apply; Save is BUSY), 12 writing the edit mode failed and is being retried, 13 a keyboard adjustment (DPI or lighting key) awaits publication: a new Apply, edit, Save, Revert and mode change are BUSY until it publishes; 14 this half stopped: only a restart clears it, 15 the other half answers that it stopped (both D-F34, below) |
| 4 | 4 | session |
| 8 | 4 | revision |
| 12 | 4 | saved generation (`0`: compiled defaults) |
| 16 | 1 | saved origin half |
| 17 | 1 | last result |
| 18 | 2 | last finished transaction |
| 20 | 2 | transaction in progress |
| 22 | 1 | edit mode |
| 23 | 1 | splices received |
| 24 | 1 | reserved, `0` |

| Page 1 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | the other half's phase |
| 1 | 1 | its last result |
| 2 | 2 | its transaction (in progress, else last) |
| 4 | 4 | its session |
| 8 | 4 | its revision |
| 12 | 1 | its flags: bit 0 unsaved edits, 1 its saved profile is the compiled defaults |
| 13 | 1 | save phase (`0` idle; `7` and `8` a save that found nothing to write) |
| 14 | 1 | last save's result |
| 15 | 1 | last save's candidate error id (`0` none) |
| 16 | 2 | save transaction in progress |
| 18 | 2 | last save's transaction |
| 20 | 4 | inserted bytes received |
| 24 | 1 | the other half's edit mode |

| Page 2 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | last refused edit: validator result |
| 1 | 1 | domain id |
| 2 | 1 | table id |
| 3 | 2 | row |
| 5 | 1 | step |
| 6 | 1 | field |
| 7 | 4 | byte offset in the resulting profile |
| 11 | 1 | detail kind |
| 12 | 2 | detail code |
| 14 | 4 | safe-boundary reasons now |
| 18 | 4 | length of the profile now active |
| 22 | 3 | reserved, `0` |

Page 3 records automatic saves on this USB half since boot (unreleased 2.0
base, no version change). Counters identify an event when polling; a client
must compare the session too after a restart. The completed event remains
available after a subsequent explicit Save. Last save's result, error and
transaction remain on page 1; only success increments the keyboard counter.

| Page 3 offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | layout, `1` |
| 1 | 1 | flags: bit 0 automatic save in progress, bit 1 last finished save was automatic |
| 2 | 4 | completed automatic saves since boot |
| 6 | 4 | working revision of the last completed automatic save |
| 10 | 4 | saved generation of that save |
| 14 | 2 | quiet period in ms, `3000` |
| 16 | 9 | reserved, `0` |

**`0x21` the active profile**, a wide page at bytes 4–5, paged like the
saved profile (GET `0x04`): page 0 is metadata, pages 1… carry 25 bytes each.
Page 0: layout `1`, page size `25`, length (4), session (4), revision (4),
saved generation (4), saved origin (1), domain mask (1), flags (1: bit 0
unsaved edits). Reading page 0 again after the bytes and finding the same
session and revision proves they are one revision. GET `0x04` still reads the
saved profile and `0x05` the compiled defaults, so a client can compare the
working copy with what is saved.

**`0x22` runtime**, page 0, 24 bytes: combos master (stored), combos
override (QMK's flag), behaviours master (stored), behaviours override, then
QMK's live values: default DPI (2), sniping DPI (2), RGB mode word (4), RGB
colour word (4), default layers (4), keymap options (4), in the settings
scalars' encodings.

**`0x27` edit mode**, page 0, 11 bytes: this half's mode and counter (4),
the other half's mode and counter (4), flags (bit 0 the other half answers,
1 the two disagree at the same counter, 2 both hold the same record).

### Edit mode

How the client's edits reach this keyboard: `0` Apply, `1` Live, `2` Live +
review. A keyboard never set is in Live + review. It is stored outside every
profile, in QMK's EEPROM user datablock on each half, never in a profile, a
backup or a review: two 16-byte records (magic `EM`, layout `1`, mode, three
zero bytes, a counter, CRC-32 of the first twelve), the newer valid one
counting, a write going to the other and read back. A change takes a counter
above both halves'; the USB half sends it to the other half, and on
reconnect the higher counter wins. Equal counters that disagree are a
conflict the next change resolves. A change is refused NOT_CLEAN with unsaved
edits: the client asks Save or Revert first. A change is answered OK at
once and takes effect at once; each half writes it to its EEPROM when its
keyboard is quiet (no key held, the matrix unchanged for 50 ms), and a failed
write is retried with a growing wait and reported by status flag 12 until it
succeeds.

In Live the USB half saves the working copy itself after 3 seconds without
an edit and without a held key (the 50 ms quiet-matrix erase gate also
applies), with no client connection required. Every held-key scan or
published revision restarts the quiet interval. It starts only when the
halves agree and no other operation holds the profile, through the ordinary
Save transaction. There is at most one automatic attempt per revision; a
failed attempt leaves the working copy unsaved and is reported as any Save
failure. Explicit Save or a new edit can retry; no automatic failure loop.
Unplugging before completion loses unsaved edits (at least the most recent
quiet interval when it has not elapsed), with the ordinary old-or-new result
if a save was running. Live + review never saves automatically. Apply saves
only adjustment-only working copies, never a posted client edit mixed into
one. No host keepalive or disconnect message controls this scheduler.

### A stopped keyboard, and Restart

A half's live-profile owner **stops** in a state nothing but a restart
leaves, a fresh read of flash at boot (D-F34). Stopped, the half saves,
edits, reverts and follows the other half no more: Save, Revert, edits and
the edit mode answer INVALID_STATE, and an Apply is refused. Reads go on:
edit status, the working copy (GET `0x21`, so a client can keep unsaved
edits) and the profile status. Edit status page 0 flag 14 says this half
stopped; flag 15 says the other half answers that it did (it says so in every
working-copy sync reply, [split protocol](profile-split-v1.md#working-copy-sync-d-f32)).
A slow save is never either: a save under way is flag 6, a save waiting for a
quiet keyboard flags 9 and 10, a half catching up flags 2 and 3. A client that
sees flag 14 or 15 says the keyboard stopped saving and offers Restart.

| Owner state | Reached when | What a restart does |
| --- | --- | --- |
| `STORAGE_ERROR` | a slot read, write or readback failed: at boot, in a save, in a copy from the other half, or publishing a record | reads flash afresh: the last whole saved profile runs |
| `DURABILITY_UNKNOWN` | the commit marker's write or readback failed, so the decision may or may not be durable | boot reads the marker: the old or the new profile runs, and the halves reconcile |
| `POSTCOMMIT_AUTHORITY_LOST` | after the decision, the other half reported a newer profile | boot reconciliation compares the halves again |
| `CONCURRENT_COMMIT` | both halves committed one generation, or the other half prepared one that wins | boot reconciliation compares the halves again |
| `INTEGRATION_ERROR` | a runtime could not install, or the owner could not start | starts it again; a fault in the build returns at once |
| `GENERATION_CONFLICT` | at boot, two different saved profiles of one generation | nothing: the conflict is in flash and returns at once |
| `COMPILED_ERROR` | the compiled defaults did not validate | nothing: the fault is in the firmware |

A flag back at once after a restart means the fault is in flash or in the
firmware; a client says so rather than offering Restart again. An EEPROM
reset (bootmagic) forgets the saved profiles (D-F29) and so clears a conflict
in flash, at the cost of the saved profile. An owner that never started
answers edit status with status `3`, as one still starting does; Restart is
taken then too.

**Restart**, SET `0x28`: bytes 5–8 the confirmation `52 45 53 54` ("REST"),
every byte after them zero, a nonzero transaction. Anything else answers
INVALID_ARGUMENT and restarts nothing. It is taken in any owner state and
edit mode, mid-save or mid-edit, and reads nothing of the profile. It is
never automatic: the firmware does not restart itself, however it stopped.
The USB half sends the other half a restart frame on a split RPC of its own
([split protocol](profile-split-v1.md#keyboard-restart-d-f34)), asking again
every 5 ms until it acknowledges or 250 ms have passed, then answers: result
OK, byte 17 whether the other half acknowledged. 50 ms later, its reply sent,
it resets (QMK's `soft_reset_keyboard`); the other half resets 20 ms after it
acknowledged. A restart asked again before the reset answers as the first.
USB drops and comes back, and each half starts as from power: it reads its
saved profile, the halves reconcile, and the working copy has a new session.
Everything in RAM is gone, unsaved edits and runtime toggles included; the
edit mode is stored and stays. A client that wants unsaved edits back reads
the working copy first (or has them in its own draft) and sends them as one
replacement edit once the keyboard is back.

**Why it is safe.** A restart resets each half between two main-loop passes,
so it is exactly as safe as unplugging: every slot's commit marker is written
last and a record without it is no candidate at boot, so a power cut anywhere
in a save leaves the previous saved profile, or the new one once the USB
half's commit marker is durable, on both halves
([power-loss outcomes](logical-profile-transaction-v1.md#power-loss-outcomes),
[storage](storage-and-resource-baseline.md)). QMK handles Raw HID in its main
loop after the scan, and housekeeping, where each half resets, after both.
Every flash erase and program runs inside the scan's durable-I/O step with
interrupts off, so a reset never falls inside one (unplugging can, and the
store survives that too); the other half's split callback only notes the
request. Host tests restart both halves at sampled scans across a whole save
and find both on one whole profile, old or new.

**When the other half does not acknowledge** (byte 17 is `0`: the link is
down, or that half does not answer), the USB half restarts alone. Each
half's saved profile is still whole. But a half that held a durable prepared
copy of the save (between its prepared marker and the USB half's commit
marker) keeps it, since nothing tells it its sender restarted, and the halves
stay apart (edits and Save BUSY, flag 2 clear) until it restarts too: an open
issue of the split protocol (D-F34). A client whose restart was not
acknowledged, and whose halves do not agree once it is back, asks the user to
unplug the keyboard and plug it back in, which restarts both.

Restart and flags 14–15 join the unreleased 2.0 base; no version or feature
bit moves (D-F28). Firmware without them never sets the flags and answers
SET `0x28` unhandled (command byte `0xFF`), changing nothing.

### Toggles and the settings QMK holds

The profile is the only durable owner of the settings QMK holds (base RGB
mode and colour, default layers, keymap options, DPI, auto-mouse): they are
pushed into QMK at boot (the compiled defaults' once discovery finds nothing
saved), on activation and on a settings edit, always with QMK's setters that
do not write its EEPROM, and settings readback (GET `0x07`) reports the
stored values. A **toggle** is a runtime override beside its stored master,
never an edit: combos run only while the stored master and QMK's combo flag
(its override, flipped by QMK's combo keys) both allow; behaviours only while
the stored master and the firmware's behaviours override both allow. An
override starts allowed at boot, is never saved or reviewed, and reads back
through GET `0x22` beside its master. DPI up/down and RGB adjustment keys
use no-EEPROM setters for immediate
local feedback and queue the four adjustment scalars
([portable profile](portable-profile-v1.md#settings-domain-0x40-version-8)) as
a compound edit.
Repeated presses coalesce while another operation holds the working copy;
an adjustment arriving during Save becomes a subsequent revision. Pending
adjustments block new client edits, Save, Revert and mode changes until
published; review sees the ordinary stored working-copy settings afterwards.
Settings projection retains newer pending local values during an older
publication. The settings domain is optional (a profile without one inherits
the authored defaults), yet an adjustment replaces bytes inside one: when the
working profile has none, the adjustment's edit creates it from the authored
defaults with the adjusted scalars in place, so every other value is
unchanged, and it is saved like any other adjustment. A build without the
authored defaults cannot do this and leaves such a profile's adjustments as
local feedback only. RGB on/off is an adjustment to lighting, distinct from the
participation toggles. The profile remains their only durable owner.

## Flash pauses — GET value `0x0D`

Each half measures how long its profile slot flash work pauses the keyboard
(D-F29), so a keyboard session can read the real figures. Custom get on
channel 0, value `0x0D`, with the usual request id (nonzero), byte page and
reserved zero bytes 5–31; a malformed request answers status `1`, a page past
1 status `2`. Page 0 is the half answering USB; page 1 the other half, as the
USB half last pulled it over the split link, or status `3` until a pull has
succeeded. A valid page-1 request queues one refresh in housekeeping and
returns the cached figures immediately; the Raw HID handler never waits for
the split link. Requests coalesce, and pull attempts are at least one second
apart, including failures. A failed attempt preserves the previous cache and
does not retry without another page-1 request. With no requests there is no
periodic diagnostic traffic; page 0 and invalid requests queue nothing.
To refresh a previously cached value, request page 1, allow housekeeping and
the rate limit to run, then read it again. Cached figures carry no freshness
or connection guarantee. Each page's 25-byte payload, little-endian:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | flags: bit 0 figures present |
| 1 | 1 | physical half: `0` left, `1` right, `0xFF` not provisioned |
| 2 | 4 | longest 4 KiB sector erase, µs |
| 6 | 4 | longest 256-byte page program, µs |
| 10 | 4 | most flash time in one scan's storage step, µs |
| 14 | 4 | last save on this half, reset to final sync, µs |
| 18 | 4 | flash time inside that save, µs |
| 22 | 2 | saves since boot |
| 24 | 1 | reserved, `0` |

Every figure counts from boot and is lost on a power cycle; interrupts are
off during each erase and program, so a pause is the time the keyboard did
nothing else. A save's duration includes the host's transfer and any wait
for a quiet keyboard before an erase, so the flash time beside it says how
much of it was flash. Figures are diagnostics, not
part of the profile, backup or fingerprint; no client behaviour depends on
them.

## Host OS readback

Feature bit 21 also advertises custom GET on channel 0, value `0x0B`, page 0.
The usual request ID, status and padding rules apply; only page 0 exists.
Success carries two bytes: version 1 and the detected OS (0 unknown, 1 macOS,
2 Windows, 3 Linux). Unknown includes unsupported iOS and unavailable detection.
This reports QMK’s current USB guess, not the selected override or confirmation
of host input configuration. Settings scalar 27 stores the override, Unicode
switch and host layout separately; clients derive the effective OS with manual selection first.
The result is volatile and is refreshed by Read keyboard; it is not a profile
value, backup field or part of the profile fingerprint. Detection may be wrong
or change after USB setup, particularly with switches/KVMs. Do not infer an OS
from the computer running the client. The retired `0x09` remains unsupported;
`0x0A` remains the diagnostic capture route.

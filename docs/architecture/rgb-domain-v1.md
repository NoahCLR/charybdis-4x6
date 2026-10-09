# RGB Domain V1

This document freezes the canonical encoding for Profile Wire domain `0x10`,
version `4`: every RGB surface, as the keyboard stores it and the
live app decodes, edits and re-encodes it for the candidate mailbox.

All integers are unsigned. Multi-byte integers are little-endian. Every HSV is
three bytes in `h, s, v` order. Hue and saturation accept `0..255`; value is
also bounded by the connected firmware's compiled
`RGB_MATRIX_MAXIMUM_BRIGHTNESS`.

## Payload header

The payload starts with this fixed 16-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | payload format version, equal to the domain version: `4` in schema 3.0 |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask |
| 4 | 1 | reusable group count, `0..16` |
| 5 | 1 | layer-color count, `1..16`; when the layer stage is compiled, exactly the firmware's layer count (`16`) |
| 6 | 1 | layer-group row count |
| 7 | 1 | PD-color count: the firmware's slot count when its PD stage is compiled (`32` since version 3), else `0` |
| 8 | 1 | PD-group row count |
| 9 | 1 | combo-group row count |
| 10 | 1 | tap-branch color count: exactly the shared tap depth minus one (`4` at depth five; D-F14) |
| 11 | 1 | key-feedback group row count |
| 12 | 1 | physical LED count, exactly `58` |
| 13 | 1 | LED bitmap size, exactly `8` |
| 14 | 2 | reserved, zero |

The aggregate of the four group-row counts at offsets 6, 8, 9, and 11 is at
most 32. The maximum RGB payload is 65,492 bytes, so its envelope still fits
the 65,504-byte canonical profile blob with the blob header.

Stage-enable bits are:

| Bit | Stage |
| ---: | --- |
| 0 | layer |
| 1 | auto-mouse fade |
| 2 | PD-mode feedback |
| 3 | combo feedback |
| 4 | key-behavior feedback |

Bits 5 through 15 are reserved. A live bit can only enable a stage advertised
as compiled. Stage data may remain encoded while its live enable bit is off,
but a stage absent from the firmware must use its canonical empty/zero form.

## Fixed section order

Immediately after the header, records appear without padding in this order:

| Section | Record bytes | Record layout |
| --- | ---: | --- |
| reusable groups | 9 | `id:u8, bitmap:u8[8]` |
| layer colors | 5 | `layer_id:u8, hsv:u8[3], mode:u8` |
| layer group rows | 5 | `selector:u8, hsv:u8[3], group_id:u8` |
| auto-mouse fade | 4 fixed | `mode:u8, end_hsv:u8[3]` |
| PD colors | 5 | `pd_id:u8, hsv:u8[3], locality:u8` |
| PD group rows | 5 | `selector:u8, hsv:u8[3], group_id:u8` |
| combo feedback | 4 fixed | `hsv:u8[3], locality:u8` |
| combo group rows | 4 | `hsv:u8[3], group_id:u8` |
| tap-branch colors | 3 | `hsv:u8[3]` |
| key feedback | 11 fixed | committed, hold, and long-hold HSV; tap policy; locality |
| key-feedback group rows | 5 | `semantic:u8, hsv:u8[3], group_id:u8` |

The payload length is therefore
`35 + 9G + 5L + 5LG + 5P + 5PG + 4CG + 3T + 5KG` bytes.
Truncation, trailing bytes, padding, or a header count that does not exactly
describe the records rejects the complete domain.

## Stable ids and enums

Layer modes are `0` `ALL_KEYS` and `1`
`KEYS_MAPPED_ON_THIS_LAYER_ONLY`. Localities are `0` both halves, `1` left,
`2` right, `3` key half, and `4` keys only. Auto-mouse fade modes are `0`
follow the real destination, `1` use the end color only where the base effect
would show, and `2` use the end color on all keys. Tap-commit policy is `0`
off or `1` commit non-base taps.

PD ids address the device-owned slots: `0..5` in schema-1 version 1, `0..7` in
version 2, and `0..31` in version 3; their names and behaviors come from the
[PD-mode domain](pd-mode-domain-v1.md), not from the RGB id. A compiled PD
stage stores each firmware-supported id exactly once, in ascending order, so
version 3's header byte 7 is 32 and the PD colour row at index `n` is slot
`n`'s.

Version 3 (D-F09) is version 2 with 32 PD colour rows instead of eight; PD
group-row selectors are a slot below 32 or `0xff`. Every other byte is
version 2's. Schema-2 firmware with 32 slots accepts only version 3: an
importer turns a version-2 domain into version 3 by adding, after slot 7's
row, rows `n 00 00 00 02` (black, right half) for slots 8..31, as the 6 → 8
upgrade added rows for slots 6 and 7.

Version 4 (D-F14) is version 3 with up to sixteen layer colours: the version
byte (payload byte 0 and envelope) is 4, and layer-group selectors are a layer
below 16 or `0xff`. Every other byte is version 3's. Schema-3 firmware accepts
only version 4: an importer turns a version-3 domain into version 4 by setting
the version to 4, adding after the last layer-colour row the rows
`n 00 00 00 01` (black, keys mapped on this layer only) for each missing layer
`n` up to 15, and setting header byte 5 to 16. A domain without layer colours
keeps a count of zero. `tests/fixtures/rgb_domain_v4.json` holds the compiled
domain and rejection vectors (versions 3 and 2, a missing PD row, 33 rows, an
id of 32, out-of-order rows, a group selector of 32); version 3's vector file
is retired.

Layer and PD group selectors use `0xff` for all; other values are validated
layer or stable PD ids. Key-feedback group semantics are `0` tap branch
pending, `1` tap committed, `2` hold active, `3` long hold active, and `0xff`
all. Combo group rows have no selector.

In a group row only, HSV `0,0,0` retains the authored inherit-stage-color
meaning. In normal color tables it is literal black/off.

## Reusable LED groups

Each dictionary bitmap addresses global LEDs `0..57`; bits 58 through 63 of
the eighth byte are always zero. Empty and full 58-LED groups are valid.
Duplicate bitmaps are forbidden.

Canonical ids are assigned by unsigned lexicographic comparison of the eight
bitmap bytes, lowest first. They are consecutive from zero, and every group
row must reference one of them. This makes macro names, declaration order,
inline-versus-reusable source syntax, LED-list order, and duplicate LED indices
irrelevant to the bytes. Group-row order is kept as stored, because later
overlapping rows repaint earlier rows.

Layer colors are canonicalized by layer id and must cover the complete logical
layer set from zero. PD colors are canonicalized by stable PD id and must cover
the complete supported set. Tap-branch colors and every renderer group table
remain ordered data.

## Validation boundary

Ark's `core/schema/rgb-domain-v1.js` and the firmware `profile_rgb_v1.c` reader-backed decoder
perform matching strict validation for versions, reserved fields, compiled
feature inclusion, complete surfaces, capacities, stable ids, enums,
selectors, references, geometry, brightness, canonical ordering, and exact
length. `createRgbDomainV1()` produces the domain object accepted by the
generic `profile-blob-v1.js` encoder.

The JSON fixture contains the normalized semantic profile.
`tests/fixtures/rgb_domain_v1.fixture` is the single
cross-language source for codec limits, the exact payload and whole-blob
bytes, FNV-1a, and CRC32 vectors.

The firmware decoder retains only an injected bounded reader, the header
counts, and negotiated limits. Its view is statically capped at 40 bytes on a
32-bit target. Decode performs one 16-byte header read; every subsequent
validation or accessor read is at most the fixed 11-byte key-feedback record.
It exposes record-at-a-time accessors for every section and never materializes
the payload, dictionary, or renderer tables in RAM.

This document governs the byte format. Transport and activation are specified
by [Profile Wire](profile-wire-v1.md) and the
[logical transaction contract](logical-profile-transaction-v1.md).

## Compiled RGB and effective frames

`profile_rgb_compiled_v1.c` is the sole authored RGB encoder. Both the complete
compiled profile and factory RGB fallback use its canonical bytes. On a cold
path it fills one immutable encoded cache and validates a memory-backed view;
frame access uses the same decoded domain interface as stored RGB. Effective
accessors never reinterpret authored tables or replay the profile writer.

The cache bound is 591 bytes: 35 fixed bytes, sixteen 9-byte bitmap groups,
sixteen 5-byte layer colors, 32 5-byte PD colors, at most 32 5-byte stage rows in
total, and four 3-byte tap colors. Combo rows are smaller, so this covers every
current shape without reserving a profile-sized buffer. A copied view retains
publication/epoch checks for stored frames; factory bytes never change during
a firmware run. This is a representation bound, not a hardware RAM claim.

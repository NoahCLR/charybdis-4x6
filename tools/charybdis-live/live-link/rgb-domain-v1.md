# RGB Domain V1

This document freezes Profile Studio's canonical desktop encoding for Profile
Wire domain `0x10`, version `1`. It covers every Milestone A surface parsed
from `rgb_config.c`; it is not connected to preview, candidate transfer,
commit, source rewriting, or firmware activation yet.

All integers are unsigned. Multi-byte integers are little-endian. Every HSV is
three bytes in `h, s, v` order. Hue and saturation accept `0..255`; value is
also bounded by the connected firmware's compiled
`RGB_MATRIX_MAXIMUM_BRIGHTNESS`.

## Payload header

The payload starts with this fixed 16-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | payload format version, exactly `1` |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask |
| 4 | 1 | reusable group count, `0..16` |
| 5 | 1 | layer-color count, `1..8` when compiled |
| 6 | 1 | layer-group row count |
| 7 | 1 | PD-color count, `0..6` |
| 8 | 1 | PD-group row count |
| 9 | 1 | combo-group row count |
| 10 | 1 | tap-branch color count, exactly the compiled count and at most `4` |
| 11 | 1 | key-feedback group row count |
| 12 | 1 | physical LED count, exactly `58` |
| 13 | 1 | LED bitmap size, exactly `8` |
| 14 | 2 | reserved, zero |

The aggregate of the four group-row counts at offsets 6, 8, 9, and 11 is at
most 32. The maximum RGB payload is 4,052 bytes so its envelope can still fit
inside the 4,064-byte canonical profile blob.

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

PD ids come from the stable manifest registry: `0` drag-scroll, `1` volume,
`2` brightness, `3` zoom, `4` arrow, and `5` pinch. A compiled PD stage stores
each firmware-supported id exactly once.

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
irrelevant to the bytes. Profile Studio retains authored group-row order
because later overlapping rows repaint earlier rows.

Layer colors are canonicalized by layer id and must cover the complete logical
layer set from zero. PD colors are canonicalized by stable PD id and must cover
the complete supported set. Tap-branch colors and every renderer group table
remain ordered data.

## Validation boundary

`rgb-domain-v1.js` and the firmware `profile_rgb_v1.c` reader-backed decoder
perform matching strict validation for versions, reserved fields, compiled
feature inclusion, complete surfaces, capacities, stable ids, enums,
selectors, references, geometry, brightness, canonical ordering, and exact
length. `canonicalizeStudioRgbDomainV1()` is the adapter from Profile Studio's
parsed model; `createRgbDomainV1()` and `createStudioRgbDomainV1()` produce
domain objects accepted by the generic `profile-blob-v1.js` encoder.

The JSON fixture contains the normalized semantic profile and representative
parsed Studio model. `tests/fixtures/rgb_domain_v1.fixture` is the single
cross-language source for codec limits, the exact payload and whole-blob
bytes, FNV-1a, and CRC32 vectors.

The firmware decoder retains only an injected bounded reader, the header
counts, and negotiated limits. Its view is statically capped at 40 bytes on a
32-bit target. Decode performs one 16-byte header read; every subsequent
validation or accessor read is at most the fixed 11-byte key-feedback record.
It exposes record-at-a-time accessors for every section and never materializes
the payload, dictionary, or renderer tables in RAM.

## Intentional deferrals

- no extension command, webview, or UI changes;
- no HID candidate transport or device writes;
- no effective RGB provider or runtime activation;
- no preview/rollback, persistence, split reconciliation, or source pull;

# Lighting domain `0x10`

Domain `0x10`, version 4, holds the profile's lighting above the background
effect: which stages run, reusable LED groups, a colour per layer, the
auto-mouse fade, a colour per pointing slot, combo feedback and key feedback.
The domain id, envelope and byte order are
[blob and digests](profile-wire-v1/blob-and-digests.md)'s; the background
effect, its colour and the lighting idle timeout are
[settings](profile-wire-v1/settings-domain.md) scalars; the pointing slots
the colour rows name are the [pointing domain](pd-mode-domain-v1.md)'s; the
lighting ceilings a build reports are on
[capability page 1](profile-wire-v1/hid-channel.md#capability-pages--get-0x01).

## Conventions

All integers are unsigned; multibyte integers are little-endian. A colour is
three bytes, hue, saturation, value. Hue and saturation take `0..255`; value
is at most the build's `RGB_MATRIX_MAXIMUM_BRIGHTNESS`, which GET `0x08`
page 1 reports (D-F38, [ADR](../adr/0038-keyboard-reports-its-brightness-limit.md);
[editor metadata](profile-wire-v1/settings-domain.md#editor-metadata--get-0x08)).
Black (`0, 0, 0`) is literal black, except in a group row
([below](#group-rows)).

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_fields_features_and_surfaces`), `tests/fixtures/rgb_domain_v1.fixture`.

## Header

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | version, `4` |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask, below |
| 4 | 1 | reusable group count `G`, `0..16` |
| 5 | 1 | layer-colour count `L`, `0..16` |
| 6 | 1 | layer group-row count `LG` |
| 7 | 1 | pointing-colour count `P`, `0..32` |
| 8 | 1 | pointing group-row count `PG` |
| 9 | 1 | combo group-row count `CG` |
| 10 | 1 | tap-branch colour count `T`, `0..4` |
| 11 | 1 | key-feedback group-row count `KG` |
| 12 | 1 | physical LEDs, exactly `58` |
| 13 | 1 | LED bitmap bytes, exactly `8` |
| 14 | 2 | reserved, zero |

`LG + PG + CG + KG` is at most 32. The tap-branch colour count's ceiling is
the shared tap depth less one.

| Stage bit | Stage |
| ---: | --- |
| 0 | layer |
| 1 | auto-mouse fade |
| 2 | pointing mode |
| 3 | combo feedback |
| 4 | key feedback |
| 5–15 | reserved, zero |

A set bit runs its stage; a clear bit keeps the stage's data and draws
nothing from it. A bit may be set only for a stage the build compiles. A
build with RGB Matrix compiles the layer stage and, by its options, the
others; its compiled defaults set every compiled stage's bit.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_headers_lengths_and_dictionary`, `test_fields_features_and_surfaces`), `tests/fixtures/rgb_domain_v4.json`.

## Sections

Records follow the header with no padding, in this order:

| Section | Bytes each | Layout |
| --- | ---: | --- |
| reusable groups | 9 | `id`, `bitmap[8]` |
| layer colours | 5 | `layer`, `h s v`, `mode` |
| layer group rows | 5 | `selector`, `h s v`, `group` |
| auto-mouse fade | 4, once | `mode`, `h s v` (end colour) |
| pointing colours | 5 | `slot`, `h s v`, `locality` |
| pointing group rows | 5 | `selector`, `h s v`, `group` |
| combo feedback | 4, once | `h s v`, `locality` |
| combo group rows | 4 | `h s v`, `group` |
| tap-branch colours | 3 | `h s v` |
| key feedback | 11, once | below |
| key-feedback group rows | 5 | `semantic`, `h s v`, `group` |

The payload is exactly `35 + 9G + 5L + 5LG + 5P + 5PG + 4CG + 3T + 5KG`
bytes; the counts' ceilings make the largest 591 bytes. Shorter is
`TRUNCATED`, longer `TRAILING_BYTES`, both refusing the whole domain.

Key feedback:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 | tap committed colour |
| 3 | 3 | hold colour |
| 6 | 3 | long hold colour |
| 9 | 1 | tap-commit policy: off `0`, commit non-base taps `1` |
| 10 | 1 | locality |

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_shared_golden_and_accessors`, `test_maximum_dictionary_and_rows`), `tests/fixtures/rgb_domain_v1.fixture`.

## Values

| Field | Values |
| --- | --- |
| layer colour mode | `0` all keys; `1` only keys mapped on this layer (neither `KC_NO` nor `KC_TRNS`) |
| locality | `0` both halves; `1` left half; `2` right half; `3` the half of the key that caused it; `4` only the keys that caused it |
| auto-mouse fade mode | `0` fade toward the layers beneath, the background effect where none paints; `1` toward them, the end colour where none paints; `2` toward the end colour on every key |
| tap-commit policy | `0` off; `1` a committed tap of tap count 2 or more flashes, unless it is a layer or pointing action |
| layer and pointing selector | a layer below the layer-colour count, a slot below 32, or `0xff` all |
| key-feedback semantic | `0` tap branch pending, `1` tap committed, `2` hold, `3` long hold, `0xff` all |

Locality `3` paints both halves for a combo whose keys span them; a pointing
mode with no known owner key paints every key under localities `3` and `4`. The
key runtime reads the tap-commit policy, so it changes, like every profile
change, only at the safe boundary. Tap-branch colour `n` is the pending colour
for tap count `n + 2`; a deeper tap count takes the last one.

Held by: `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, the locality, mapped-only and auto-mouse tests; `rgb_key_effective_test.c`, `test_tap_commit_policy_fails_closed_on_stale_read`), `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`).

## Reusable groups

A group is a set of the 58 LEDs: LED `n` is bit `n % 8` of bitmap byte
`n / 8`, and bits 58–63 (byte 7, bits 2–7) are zero. Empty and full groups
are valid; two equal bitmaps are not. Groups are sorted by their eight bytes,
compared as unsigned bytes lowest first, and their ids are their positions
from zero. So declaration order, names, inline or reusable source syntax, LED
order and repeated LEDs never reach the bytes.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_headers_lengths_and_dictionary`, `test_maximum_dictionary_and_rows`).

## Group rows

A group row paints its group in its colour when its selector matches (the
active layer, the active slot, the key-feedback semantic, or `0xff`); combo
group rows have no selector. Rows keep their stored order, since a later row
repaints an earlier one where they overlap. In a group row, black means the
stage's own colour (the layer's, the slot's, the feedback colour). Every row
names a group below `G`.

Held by: `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, `test_preview_preserves_later_group_override_order`, the `*_led_groups_*` tests; `rgb_pd_effective_test.c`; `rgb_combo_effective_test.c`).

## Complete surfaces

- Layer colours cover layers `0..L−1` in order, `layer` equal to the row's
  index. With the layer stage compiled, `L` is the bank's layer count, `16`.
- Pointing colours name each slot at most once, in ascending order. With the
  pointing stage compiled, every one of the 32 slots has a row, so row `n` is
  slot `n`'s, whether or not the slot holds a mode. An empty slot's row is
  kept and paints nothing, since an empty slot never runs.
- With the key-feedback stage compiled, `T` is the tap depth less one, `4`.
- A stage the build does not compile has its empty form: no layer colours or
  layer rows; a zero auto-mouse record; no pointing colours or rows; a zero
  combo record and no combo rows; no tap-branch colours, a zero key-feedback
  record and no key-feedback rows.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_canonical_empty_forms`, `test_fields_features_and_surfaces`), `tests/host/run_profile_rgb_v4_tests.sh` (`profile_rgb_v4_test.c`), `tests/fixtures/rgb_domain_v4.json` (`missing-pd-row`, `pd-group-selector-slot-31`).

## Refusals

The domain is refused at its first fault: the payload length, then the
header's checks in the order listed, then each section's records in order:

| Refusal | Where |
| --- | --- |
| `TRUNCATED` | a payload under 16 bytes |
| `CAPACITY_EXCEEDED` | a payload over 65,535 bytes |
| `INVALID_VERSION` | header byte 0 not `4` |
| `RESERVED_BITS` | header bytes 1, 14 or 15, or stage bits 5–15, nonzero |
| `UNSUPPORTED_STAGE` | a stage bit for a stage the build does not compile |
| `INCOMPATIBLE_GEOMETRY` | header byte 12 not `58` or byte 13 not `8` |
| `CAPACITY_EXCEEDED` | at header byte 4: a count over its ceiling, or more than 32 group rows |
| `TRUNCATED`, `TRAILING_BYTES` | a payload shorter or longer than its counts describe |
| `RESERVED_BITS` | LED bits 58–63 set in a group |
| `NONCANONICAL_ORDER` | a group or layer id not its row index, bitmaps or slots out of order |
| `DUPLICATE_BITMAP` | two equal groups |
| `BRIGHTNESS_EXCEEDED` | a value above the brightness limit |
| `INVALID_ENUM` | a mode, locality or tap-commit policy out of range |
| `INVALID_ID` | a slot at or past 32 |
| `INVALID_SELECTOR` | a selector or semantic out of range |
| `INVALID_REFERENCE` | a group id at or past `G` |
| `INCOMPLETE_SURFACE` | a compiled stage's table not complete |
| `UNSUPPORTED_STAGE_DATA` | data for a stage the build does not compile |

The whole-profile validator reports a refusal as `INVALID_DOMAIN` (or
`CAPACITY_EXCEEDED`, `READ_ERROR`) with domain `0x10`, the row, the byte
offset, and these table and field ids
([where an error is](profile-wire-v1/errors.md#where-an-error-is)):

| Table id | Table | | Field id | Field |
| ---: | --- | --- | ---: | --- |
| 0 | header | | 0 | header |
| 1 | reusable groups | | 1 | version |
| 2 | layer colours | | 2 | reserved |
| 3 | layer group rows | | 3 | stage mask |
| 4 | auto-mouse fade | | 4 | count |
| 5 | pointing colours | | 5 | geometry |
| 6 | pointing group rows | | 6 | id |
| 7 | combo feedback | | 7 | bitmap |
| 8 | combo group rows | | 8 | colour |
| 9 | tap-branch colours | | 9 | mode |
| 10 | key feedback | | 10 | locality |
| 11 | key-feedback group rows | | 11 | selector |
| | | | 12 | group id |
| | | | 13 | semantic |

Validation reads the header in one 16-byte step and every record in one step
of at most 11 bytes, holding the counts and the previous bitmap, never the
payload.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_incremental_error_locations`, `test_reader_is_bounded`, `test_incremental_reader_budget_and_failures`), `tests/host/run_profile_rgb_v4_tests.sh` (`profile_rgb_v4_test.c`), `tests/fixtures/rgb_domain_v4.json` (`invalid`).

## Compiled lighting and the running view

`profile_rgb_compiled_v1.c` is the one encoder of the authored lighting
tables; the compiled defaults and the lighting fallback use its bytes. A
profile without this domain draws the compiled lighting. The renderer reads
records through a validated view of the active profile, one record at a time;
it never rebuilds tables from the authored sources. A frame that began before
a new generation or working copy activated is refused as stale and draws
nothing from the old view. A build without RGB Matrix has no lighting domain.

Held by: `tests/host/run_effective_rgb_runtime_tests.sh` (`effective_rgb_runtime_test.c`), `tests/host/run_rgb_layer_render_tests.sh` (`rgb_pd_effective_test.c`, `rgb_key_effective_test.c`, `rgb_combo_effective_test.c`, `test_compiled_and_live_*_rendering`), `tests/host/run_rgb_validation_tests.sh` (`rgb_validation_test.c`).

## Compatibility

The firmware reads only version 4 (D-F10,
[ADR](../adr/0010-firmware-accepts-only-current-formats.md)); version 3 and
earlier are `INVALID_VERSION` and are not translated. The 32 pointing-colour
rows are D-F09's ([ADR](../adr/0009-thirty-two-pointing-slots-stored-sparsely.md));
the sixteen layer colours D-F14's ([ADR](../adr/0014-bigger-profiles.md)).

Held by: `tests/host/run_profile_rgb_v4_tests.sh` (`profile_rgb_v4_test.c`), `tests/fixtures/rgb_domain_v4.json` (`version-3`, `version-2`).

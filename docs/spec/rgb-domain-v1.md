# Lighting domain `0x10`

Domain `0x10`, version 5, holds the profile's lighting above the background
effect: which stages run, reusable LED groups, a colour per layer, the
auto-mouse fade, a colour per pointing slot, combo feedback, behaviour
feedback and replacement feedback.
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
([below](#group-rows)), and in a layer colour and a replacement-feedback
colour, where saturation and value `0` at any hue paints nothing
([drawing](#drawing)).

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_fields_features_and_surfaces`), `tests/fixtures/rgb_domain_v1.fixture`.

## Header

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | version, `5` |
| 1 | 1 | reserved, zero |
| 2 | 2 | stage-enable mask, below |
| 4 | 1 | reusable group count `G`, `0..128` |
| 5 | 1 | layer-colour count `L`, `0..16` |
| 6 | 1 | layer group-row count `LG`, `0..255` |
| 7 | 1 | pointing-colour count `P`, `0..32` |
| 8 | 1 | pointing group-row count `PG`, `0..255` |
| 9 | 1 | combo group-row count `CG`, `0..255` |
| 10 | 1 | tap-branch colour count `T`, `0..4` |
| 11 | 1 | behaviour-feedback group-row count `KG`, `0..255` |
| 12 | 1 | physical LEDs, exactly `58` |
| 13 | 1 | LED bitmap bytes, exactly `8` |
| 14 | 1 | replacement-feedback group-row count `RG`, `0..255` |
| 15 | 1 | reserved, zero |

`LG + PG + CG + KG + RG` is at most 640 (D-F63,
[ADR](../adr/0063-more-led-sets-and-accents.md)). The tap-branch colour
count's ceiling is the shared tap depth less one.

| Stage bit | Stage |
| ---: | --- |
| 0 | layer |
| 1 | auto-mouse fade |
| 2 | pointing mode |
| 3 | combo feedback |
| 4 | behaviour feedback |
| 5 | replacement feedback |
| 6–15 | reserved, zero |

A set bit runs its stage; a clear bit keeps the stage's data and draws
nothing from it. A bit may be set only for a stage the build compiles. A
build with RGB Matrix compiles the layer stage and, by its options, the
others; its compiled defaults set every compiled stage's bit.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_headers_lengths_and_dictionary`, `test_fields_features_and_surfaces`, `test_total_rows_at_ceiling`), `tests/fixtures/rgb_domain_v5.json`.

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
| behaviour feedback | 11, once | below |
| behaviour-feedback group rows | 5 | `semantic`, `h s v`, `group` |
| replacement feedback | 10, once | below |
| replacement-feedback group rows | 5 | `semantic`, `h s v`, `group` |

The payload is exactly
`45 + 9G + 5L + 5LG + 5P + 5PG + 4CG + 3T + 5KG + 5RG` bytes; the counts'
ceilings make the largest 4,649 bytes. Shorter is
`TRUNCATED`, longer `TRAILING_BYTES`, both refusing the whole domain.

Behaviour feedback:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 | tap committed colour |
| 3 | 3 | hold colour |
| 6 | 3 | long hold colour |
| 9 | 1 | tap-commit policy: off `0`, commit non-base taps `1` |
| 10 | 1 | locality |

Replacement feedback (D-F70,
[ADR](../adr/0070-replacement-feedback-is-a-lighting-stage.md)):

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 3 | text replaced colour |
| 3 | 3 | macro replaced colour |
| 6 | 3 | undone colour |
| 9 | 1 | locality |

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_shared_golden_and_accessors`, `test_maximum_dictionary_and_rows`, `test_replacement_feedback_records_and_refusals`), `tests/fixtures/rgb_domain_v1.fixture`, `tests/fixtures/rgb_domain_v5.json` (`replacement-rows-every-semantic`).

## Values

| Field | Values |
| --- | --- |
| layer colour mode | `0` all keys; `1` only keys mapped on this layer (neither `KC_NO` nor `KC_TRNS`) |
| locality | `0` both halves; `1` left half; `2` right half; `3` the half of the key that caused it; `4` only the keys that caused it |
| auto-mouse fade mode | `0` fade toward the layers beneath, the background effect where none paints; `1` toward them, the end colour where none paints; `2` toward the end colour on every key |
| tap-commit policy | `0` off; `1` a committed tap of tap count 2 or more pulses, unless it is a layer or pointing action |
| layer and pointing selector | a layer below the layer-colour count, a slot below 32, or `0xff` all |
| behaviour-feedback semantic | `0` tap branch pending, `1` tap committed, `2` hold, `3` long hold, `0xff` all |
| replacement-feedback semantic | `0` text replaced, `1` macro replaced, `2` undone, `0xff` all |

Locality `3` paints both halves for a combo whose keys span them; a pointing
mode with no known owner key paints every key under localities `3` and `4`,
and so does a replacement flash whose key is not known. The
key runtime reads the tap-commit policy, so it changes, like every profile
change, only at the safe boundary. Tap-branch colour `n` is the pending colour
for tap count `n + 2`; a deeper tap count takes the last one.

Held by: `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, the locality, mapped-only and auto-mouse tests; `rgb_key_active_profile_test.c`, `test_tap_commit_policy_fails_closed_on_stale_read`; `rgb_autocorrect_active_profile_test.c`, `test_each_locality`), `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`).

## Reusable groups

A group is a set of the 58 LEDs: LED `n` is bit `n % 8` of bitmap byte
`n / 8`, and bits 58–63 (byte 7, bits 2–7) are zero. Empty and full groups
are valid; two equal bitmaps are not. Groups are sorted by their eight bytes,
compared as unsigned bytes lowest first, and their ids are their positions
from zero. So declaration order, names, inline or reusable source syntax, LED
order and repeated LEDs never reach the bytes.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_headers_lengths_and_dictionary`, `test_maximum_dictionary_and_rows`).

## Group rows

A group row paints its group in its colour when its selector matches (a layer
that is on, the running slot, a behaviour-feedback or replacement-feedback
semantic shown, or `0xff`); combo group rows have no selector. Rows paint
after their stage's own colour and keep their stored order, since a later row
repaints an earlier one where they overlap; behaviour-feedback and
replacement-feedback rows with semantic `0xff` all paint before the rows
naming one semantic. In a group row, black means the stage's own colour (the
layer's, the slot's, the feedback colour), so a black row paints nothing on a
layer whose colour is black, or for a replacement-feedback semantic whose
colour paints nothing; a coloured row still paints. Every row names a group
below `G`.

Held by: `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, `test_preview_preserves_later_group_override_order`, the `*_led_groups_*` tests; `rgb_pd_active_profile_test.c`; `rgb_combo_active_profile_test.c`; `rgb_autocorrect_active_profile_test.c`, `test_accents_follow_their_semantic`, `test_a_colour_that_is_off_paints_nothing`).

## Complete surfaces

- Layer colours cover layers `0..L−1` in order, `layer` equal to the row's
  index. With the layer stage compiled, `L` is the bank's layer count, `16`.
- Pointing colours name each slot at most once, in ascending order. With the
  pointing stage compiled, every one of the 32 slots has a row, so row `n` is
  slot `n`'s, whether or not the slot holds a mode. An empty slot's row is
  kept and paints nothing, since an empty slot never runs.
- With the behaviour-feedback stage compiled, `T` is the tap depth less one,
  `4`.
- A stage the build does not compile has its empty form: no layer colours or
  layer rows; a zero auto-mouse record; no pointing colours or rows; a zero
  combo record and no combo rows; no tap-branch colours, a zero
  behaviour-feedback record and no behaviour-feedback rows; a zero
  replacement-feedback record and no replacement-feedback rows.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_canonical_empty_forms`, `test_fields_features_and_surfaces`, `test_replacement_feedback_records_and_refusals`), `tests/host/run_profile_rgb_v5_tests.sh` (`profile_rgb_v5_test.c`), `tests/fixtures/rgb_domain_v5.json` (`missing-pd-row`, `pd-group-selector-slot-31`).

## Refusals

The domain is refused at its first fault: the payload length, then the
header's checks in the order listed, then each section's records in order:

| Refusal | Where |
| --- | --- |
| `TRUNCATED` | a payload under 16 bytes |
| `CAPACITY_EXCEEDED` | a payload over 65,535 bytes |
| `INVALID_VERSION` | header byte 0 not `5` |
| `RESERVED_BITS` | header byte 1 or 15, or stage bits 6–15, nonzero |
| `UNSUPPORTED_STAGE` | a stage bit for a stage the build does not compile |
| `INCOMPATIBLE_GEOMETRY` | header byte 12 not `58` or byte 13 not `8` |
| `CAPACITY_EXCEEDED` | at header byte 4: a count over its ceiling, or more than 640 group rows |
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
| 10 | behaviour feedback | | 10 | locality |
| 11 | behaviour-feedback group rows | | 11 | selector |
| 12 | replacement feedback | | 12 | group id |
| 13 | replacement-feedback group rows | | 13 | semantic |

Validation reads the header in one 16-byte step and every record in one step
of at most 11 bytes, holding the counts and the previous bitmap, never the
payload.

Held by: `tests/host/run_profile_rgb_v1_tests.sh` (`profile_rgb_v1_test.c`, `test_incremental_error_locations`, `test_reader_is_bounded`, `test_incremental_reader_budget_and_failures`, `test_replacement_feedback_records_and_refusals`), `tests/host/run_profile_rgb_v5_tests.sh` (`profile_rgb_v5_test.c`), `tests/fixtures/rgb_domain_v5.json` (`invalid`).

## Drawing

Each half draws its own LEDs every frame, over the background effect, in this
order. A later stage covers an earlier one where both paint; where a stage
paints nothing, what is beneath shows.

1. **Layer**: every layer that is on, the base layer always, lowest first, so
   a higher layer covers a lower one; a black layer colour paints nothing.
   Then the layer rows. While a layer of a running pointing mode's link is
   on and the mode has a colour to paint (the pointing-mode stage is on), the
   stage draws in two parts: the layers below the lowest such layer
   and their rows, then the mode's colour and rows as stage 5 would paint
   them, then that layer, the layers above and their rows (D-F62). The rows of the layers below no
   longer cover those above in that case, and the stages after this one
   (combo feedback beneath, the layer preview) paint over the mode. While the
   auto-mouse fade replaces this stage, the mode paints in stage 5 instead.
2. **Auto-mouse fade**, in place of the layer stage while the pointer layer
   is on and the fade has begun. It begins after the auto-mouse fade delay,
   blends from the layer stage toward its mode's end over the rest of the
   auto-mouse timeout, and holds the end until the pointer layer turns off.
   It does not run while a pointing mode is locked or auto-mouse is off.
3. **Combo feedback, beneath**: active combos whose output owns the layer
   preview or the running pointing mode, so those stages cover them.
4. **Layer preview**: while a key whose hold enters a layer (held until
   release) is down and could still become a tap, that layer's colour and
   rows as though it were on. It draws the layer stage's data, so it follows
   the layer bit.
5. **Pointing mode**: the running slot's colour in its locality, then its
   rows, unless stage 1 already painted it beneath a layer of its link
   ([pointing domain](pd-mode-domain-v1.md#lighting-of-a-running-mode)).
6. **Combo feedback, above**: every other active combo. A combo whose output
   holds a momentary layer paints in neither; the layer stage shows the hold.
7. **Behaviour feedback**, below.
8. **Replacement feedback**, below.

After the stages, in an image built with the write path
(`NOAH_PORTABLE_PROFILE_ENABLE`), a half turns every LED off once the last
input on either half is older than the lighting idle timeout. For five seconds after boot
every LED is white at value 150 and no stage draws.

Behaviour feedback names the state of a key that has a behaviour, painted on
every key that produced it (all of a combo's keys):

| Semantic | Shown while | Colour |
| --- | --- | --- |
| tap branch pending | a run of two or more taps sits on a branch that authors a step, until the run resolves | that tap count's tap-branch colour |
| tap committed | a tap-commit pulse; or the key is held on a branch with a tap and no hold, once the multi tap window has passed since the press | tap committed |
| hold | the hold tier's action is held or repeating (flashing), or reached and still to send (steady), or a hold pulse | hold |
| long hold | the same on the long hold tier | long hold |

- On one key the highest wins: tap committed, long hold, hold, tap branch
  pending.
- Behaviour feedback paints nothing for the base branch, a tap count with no
  authored step, the hold of a layer or pointing action (its own stage shows
  it), a hold that previews a layer, a hold the engine supplies where no
  behaviour authors one, or a pending tap branch on keys an active combo's
  feedback covers.
- Flashing shows in alternate flash half-periods from the moment the action
  registered. A pulse lasts one half-period; one arriving during another
  follows it.
- Locality `4` paints each key in its own state. Locality `3` paints each half
  in the state of the newest feedback touching that half, and `0`, `1` and `2`
  in the state of the newest feedback anywhere. Newest is the latest to begin;
  while it is in its flash-off phase nothing paints, rather than an older
  state. A group row follows the newest feedback of its semantic.

Replacement feedback is a flash for what text replacement
([`autocorrect-v1.md`](autocorrect-v1.md)) has just done (D-F70):

| Semantic | Shown from | Colour |
| --- | --- | --- |
| text replaced | the replacement of a typed form begins to be typed | text replaced |
| macro replaced | a typed form begins to be erased for its macro | macro replaced |
| undone | Backspace right after a replacement begins to put the word back | undone |

- A flash lasts the replacement-feedback flash length, settings scalar 31
  ([settings](profile-wire-v1/settings-domain.md#scalars)), whatever the
  replacement goes on to do; it is set apart from the behaviour-feedback
  flash half-period, scalar 15. One shows at a time: a newer one takes the place
  of the one showing.
- Its key is the key that ended the word, or the Backspace that undid the
  replacement; a combo's output is every key of the combo, as they are when
  the typing stage reads the key. Locality `3` paints that key's half and
  `4` that key alone. The key is not known when what ended the word reached
  the report by no key's press or action, or in a report read after one
  whose changes did not fit the queue of held-back changes; the flash then
  paints as [values](#values) say.
- The master half times the flash. The slave half shows it from the packet
  that carries it to the packet that ends it
  ([runtime state packets](split-activity-sync.md#runtime-state-packets)), so
  a failed send moves either edge there by the retry delay.
- A semantic whose colour has saturation and value `0` paints nothing, and
  so do black rows for it; the other semantics are unaffected.
- A word that ends and is left as typed shows nothing: a word not on the
  list, one that already reads as its replacement, one the layout cannot
  type, one passed over or suspended, and text replacement switched off.

Held by: `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, `test_render_order_*`, `test_combo_*_stays_*`, `test_runtime_boot_indicator_overrides_scene`, the `*automouse*` and `*timeout*` tests, `test_newest_global_feedback_owner_wins_over_older_higher_priority`, `test_key_half_feedback_follows_newest_owner_without_visible_fallback`, `test_render_order_replacement_feedback_covers_key_feedback`, `test_a_frame_reads_replacement_feedback_once`; `rgb_base_underlay_test.c`; `rgb_autocorrect_active_profile_test.c`), `tests/host/run_autocorrect_tests.sh` (`autocorrect_test.c`, the tests under "What lighting is told"), `tests/host/run_runtime_debug_tests.sh` (`runtime_debug_test.c`, the `test_key_feedback_*` and `test_display_preview_*` tests, `test_split_term_branch_names_its_tap_at_the_accept_window`, `test_long_hold_only_branch_keeps_branch_color_until_long_hold`), `tests/host/run_qmk_combo_origin_tests.sh` (`qmk_combo_origin_test.c`, `test_active_combo_partition_*`).

## Compiled lighting and the running view

`profile_rgb_compiled_v1.c` is the one encoder of the authored lighting
tables; the compiled defaults and the lighting fallback use its bytes, kept in
a buffer the keymap sizes to its own tables. The compiled lighting holds at
most 16 saved groups and 32 group rows (D-F63). A profile without this domain
draws the compiled lighting. The renderer reads records through a validated
view of the active profile, one record at a time; it never rebuilds tables
from the authored sources. Each publication's layer, pointing,
behaviour-feedback and replacement-feedback row selectors are read once and
kept, a byte per row, and a stage reads a row and its group only when its
selector matches: a layer on in the selection, the running slot, a semantic
shown (D-F63). Combo rows have no selector and are read whenever a combo
partition paints. A frame that began before a new generation or working copy
activated is refused as stale and draws nothing from the old view. A build
without RGB Matrix has no lighting domain.

Held by: `tests/host/run_active_profile_rgb_runtime_tests.sh` (`active_profile_rgb_runtime_test.c`), `tests/host/run_rgb_layer_render_tests.sh` (`rgb_layer_render_test.c`, `test_ceiling_profile_reads_only_lit_accents`; `rgb_pd_active_profile_test.c`, `rgb_key_active_profile_test.c`, `rgb_combo_active_profile_test.c`, `test_compiled_and_live_*_rendering`), `tests/host/run_rgb_validation_tests.sh` (`rgb_validation_test.c`).

## Compatibility

The firmware reads only version 5 (D-F10,
[ADR](../adr/0010-firmware-accepts-only-current-formats.md)); version 4 and
earlier are `INVALID_VERSION` and are not translated. Version 5 added
replacement feedback: stage bit 5, the row count in header byte 14 and the
two sections at the end (D-F70); how long its flash lasts is settings scalar
31, settings version 12, in the same profile schema 4.5. D-F63 raised the
group and row ceilings within version 4, and version 5 keeps them. The 32
pointing-colour rows are D-F09's ([ADR](../adr/0009-thirty-two-pointing-slots-stored-sparsely.md));
the sixteen layer colours D-F14's ([ADR](../adr/0014-bigger-profiles.md)).

Held by: `tests/host/run_profile_rgb_v5_tests.sh` (`profile_rgb_v5_test.c`), `tests/fixtures/rgb_domain_v5.json` (`groups-128`, `groups-129`, `rows-255-in-one-table`, `rows-640-in-all`, `rows-641-in-all`, `rows-640-with-replacement`, `rows-641-with-replacement`, `replacement-rows-255`, `version-4`, `version-3`, `version-2`).

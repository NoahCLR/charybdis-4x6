# Lighting Rules — RGB Domain `0x10` Version 5

This document governs the keyboard's lighting: the bytes of Profile Wire
domain `0x10` version `5`, the four open registries (facts, targets, paints,
timings) and their contract, how the keyboard renders a rule list, how facts
reach the other half, the conversion from version 4, the **Standard lighting**
rule set, and the walkthroughs that prove the design grows without a rewrite.
It supersedes [RGB domain v1](rgb-domain-v1.md) for everything the firmware
reads and writes; that document remains the definition of the version-4 bytes
the conversion starts from (D-F26).

The shared contract file `tests/fixtures/lighting_contract_v1.json` names every
id, parameter and meaning below in a form a program can read.
`tests/fixtures/lighting_vectors_v1.json` holds the cross-language vectors:
codec, conversion and per-entry render vectors. A client that edits lighting
implements the same contract and passes the same vectors.

All integers are unsigned and little-endian. Every colour is three bytes in
`h, s, v` order, converted to RGB with QMK's `hsv_to_rgb` (no CIE curve in this
build). LED indices are the global `0..57` of the compiled board: left half
`0..28`, right half `29..57`.

## The model

The keyboard keeps an ordered list of **rules**. A rule says: *when these
facts are true, paint these LEDs like this.* For each LED the topmost rule that
says something about it wins. A rule may say **clear** for an LED, and then
whatever is below shows through. Below the last rule is the QMK matrix effect:
an LED no rule paints is not written, so the effect shows exactly as it does
today.

A rule has:

- up to four **conditions**, each a fact, optionally negated;
- one **target**: which LEDs it paints;
- one **paint**: what each targeted LED shows;
- an optional **background**: a paint for the LEDs the rule does not paint;
- a **brightness**;
- optional **timings**: when a true rule actually paints;
- optional **extensions**: rule properties added after version 5.

The keyboard only walks the list. Placing rules sensibly is the editor's job.

### Subjects

The first condition, if it is not negated, is the rule's **subject**. A
condition can name one thing ("layer 3") or any thing of its kind ("any
layer"); it then yields one subject per matching thing, in that kind's
natural order (a higher layer before a lower one). The rule applies once per
subject, the first subject on top, as if it were that many rules in a row.
Every other condition is a **filter**: a positive filter holds when its fact
yields at least one subject, a negated one when it yields none. A rule whose
first condition is negated, or that has none, has one subject: the **board**,
which names no thing.

Retained and live batches preserve that existing natural priority as the primary
ordering. Only members at exactly equal natural priority use newer full-match
entry observation first. A state thing's entry is a monotonic observation-order
token: refreshing its whole matching batch does not renew it; full-match
re-entry does. This is separate from local lifetime age and synchronized paint
time. A saturated age or wrapping/resynchronized timestamp cannot serve as the
token. Event subjects keep their captured natural occurrence order unchanged.
For genuinely equal entry order, ascending thing kind then ascending thing id
break the tie; authored within-batch order is the final stable tie.

For example, row A at matrix key0 ends at100 with linger1000. Row B at that
same key enters at600. B paints above A at600; A remains underneath until1100.
Unequal matrix positions and higher/lower layer priorities still win before
entry recency. This is not a preference for live batches: if A continuously
matches at another key while its key0 member leaves and returns, a newer B
key0 tail can remain above A's older, still-live full match. Partial member
changes do not create new full-match entries or independent linger clocks.
The shared `retainedOrdering` traces pin pixel, complete member order and entry
recency through these cases, show-after maturation and timer boundaries. The
original owned-record/merge assertions remain reference checks. Production
counterparts in `lighting_working_ordering_test.c` use admitted profiles, actual
registry walkers, relative entry ranks and `scene.working` whole/half painting.
Broad behaviour cases use declared2×1-key geometry; the default10×6 rejects
those reservations and admits the layer control. The mixed-kind/same-id and
duplicate-owner comparator fixture is explicitly synthetic. Reachable same-kind
simultaneous/partial-refresh controls exercise production ties instead; the
different-per-thing show durations use a faithful uniform-rule counterpart.
These checks do not substitute for linked resource or hardware acceptance.

A subject carries what targets and paints may ask about it: its thing kind
and id (for its identity colour and place), the keys it concerns, and the
halves it concerns. Each fact kind below defines what its subjects carry.

### Identity and place

Every thing kind with a table (layers, pointing modes, combos, behaviours) has
a **thing table** in the domain. A thing's record holds its **place** (the
target its rules use when they say "the subject's place") and its identity
colour or colours. Recolouring a thing once recolours every rule that paints
"identity" for it. A record may inherit either field from the table's default
record.

## Payload

### Header — 8 bytes

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | format version, `5` |
| 1 | 1 | reserved, zero |
| 2 | 1 | physical LED count, exactly `58` |
| 3 | 1 | palette count `P`, `0..128` |
| 4 | 1 | thing-table count |
| 5 | 1 | key-set count `K`, `0..255` |
| 6 | 1 | map count `M`, `0..255` |
| 7 | 1 | rule count `R`, `0..255` |

The sections follow without padding in this order: palette, thing tables,
key sets, maps, rules, domain extensions. The payload is at most **8,192
bytes** (one shared lighting budget; there are no per-section caps beyond the
count fields). Truncation, trailing bytes or a count that does not exactly
describe the sections rejects the whole domain.

### Palette

`P` colours of three bytes each. Value is at most the firmware's compiled
`RGB_MATRIX_MAXIMUM_BRIGHTNESS`. Order is the user's; duplicates are allowed.
Everything else refers to colours by palette index.

A **colour reference** byte is a palette index below `P`, `0xff` *none* (no
colour: a paint that would show it says clear), or, in a thing record only,
`0xfe` *inherit*.

### Thing tables

Each table is:

| Size | Field |
| ---: | --- |
| 1 | thing kind |
| 1 | record count `N`, at most the kind's thing count |
| record size | default record |
| `N` × record size | records for things `0..N-1` |

Tables appear in strictly ascending kind order, each kind at most once, and
only for kinds the keyboard supports. A thing at or beyond `N`, or a record
field that is `0xfe`, uses the default record's field. The default record may
not inherit.

A record is one **place** byte followed by the kind's colour slots:

| Kind | Id | Things | Colour slots |
| --- | ---: | ---: | --- |
| layer | 1 | logical layers (16) | colour |
| pointing mode | 2 | 32 | colour |
| combo | 3 | 128 | colour |
| behaviour | 4 | 128 | hold, long hold, tap, then one per tap branch `2..depth` |
| macro | 5 | 128 | colour |
| modifier | 6 | 8 | colour |
| key | 7 | 60 matrix positions | colour |

The behaviour record therefore has `1 + 3 + (depth − 1)` bytes, `8` at the
supported tap depth of five (D-F14). Kind `0` is the board: it has no table.

A place byte is a **place-capable target** id (a target kind with no
parameters that is not "the subject's place" itself) or, in a record only,
`0xfe` inherit.

### Key sets

`K` saved LED sets of eight bytes each: bit `n` of byte `n / 8` is LED `n`.
Bits 58–63 are zero. Empty, full and duplicate sets are valid; order is the
user's. Version 4's reusable groups become key sets with the same ids.

### Maps

`M` painted maps. Each starts with an encoding byte:

| Encoding | Bytes | Layout |
| ---: | ---: | --- |
| `0` dense | 59 | encoding, 58 values in LED order |
| `1` sparse | `2 + 2n` | encoding, count `n`, then `n` × (`led`, `value`) |

A map value is a palette index `0x00..0x7f` (below `P`) or `0x80 + k`, which
paints with paint kind `k` and no parameters (`0x80` clear, `0x81` off, `0x83`
identity, `0x88` outcome). Only supported paint kinds without parameters may appear, and never
the map paint itself. In a sparse map the LEDs are strictly ascending and below
58, and no value is `0x80`; the LEDs it omits are clear.

The encoding is canonical: a map that paints (does not clear) at most 28 LEDs
is sparse, any other dense. A six-key look costs 14 bytes.

### Rules

Each rule is:

| Size | Field |
| ---: | --- |
| 1 | size: the bytes that follow, `6..255` |
| 1 | flags: bit 0 disabled; bits 1–7 zero |
| 1 | brightness, `0..255`; `255` shows the paint as it is |
| size − 2 | parts |

A disabled rule is kept, validated and never rendered.

A **part** is `slot << 5 | length`, then a kind id, then `length` parameter
bytes. `length` is `0..16` and must equal the kind's declared parameter
length.

| Slot | Part | Count |
| ---: | --- | --- |
| 0 | condition | together with slot 1: `0..4` |
| 1 | negated condition | |
| 2 | target | exactly 1 |
| 3 | paint | exactly 1 |
| 4 | background paint | `0..1` |
| 5 | timing | `0..4`, kinds strictly ascending |
| 6 | rule extension | kinds strictly ascending |
| 7 | reserved | never |

Parts appear in slot order, conditions (slots 0 and 1, in the user's order)
first. Every kind must be one the keyboard supports, and its parameters must
satisfy the kind's rules below.

### Domain extensions

| Size | Field |
| ---: | --- |
| 1 | extension count `E` |
| per extension | kind, length, `length` body bytes |

Kinds are strictly ascending and supported, and each body has the kind's
declared length. A domain extension is a board-wide setting added without a
version change.

| Kind | Length | Body |
| ---: | ---: | --- |
| 1 auto-mouse fade | 4 | flags (bit 0 on), mode (`0` real destination, `1` end colour where the effect would show, `2` end colour on all keys), end colour (palette index), base rule count (`0..R`) |
| 2 tap-commit flash | 1 | `1`: a committed non-base tap flashes its tap colour |

The tap-commit flash is behaviour-affecting: the key runtime decides whether
to raise the flash, so the setting takes effect at the strict activation
boundary like every other behaviour-affecting field. Without the extension no
tap-commit flash is raised.

## Registries

There is one engine and four open registries, plus two lists of
extensions and the thing kinds:

| Registry | An entry | Firmware module provides |
| --- | --- | --- |
| facts | a state the board knows | its frame slice, collection on the USB half, its subjects |
| targets | a way to pick LEDs | the LED mask for a subject |
| paints | what an LED shows | `colour = paint(below, params, led, time)` |
| timings | when a true rule paints | a gate amount and its next change |
| rule extensions | a rule property | (none in version 5) |
| domain extensions | a board-wide setting | its reader |
| thing kinds | things with identities | their record shape |

The contract every entry keeps:

- **A stable id, never reused.** Ids are per registry, `0..255`. A retired id
  stays reserved.
- **A fixed parameter layout.** Each kind declares its parameter length and
  the rule each byte must meet. The validator checks parameters from that
  declaration; a kind has no validation code of its own.
- **Its own module and compile flag.** An entry is one source file under
  `users/noah/lib/rgb/lighting/kinds/` plus one row in the registry
  (`users/noah/lib/profile/schema/profile_lighting_kinds_v1.h`), guarded by
  the module's flag, and one line in `users/noah/source_manifest.mk`. The
  engine dispatches through switches generated from the rows, so every call is
  a direct edge for the reviewed stack contexts (D-F13); it has no other code
  per kind.
- **Reported as a capability.** A build reports every row it compiled in the
  lighting capability bitmaps (below). It rejects any kind it does not have,
  as strictly as ever: it never renders something it does not understand.
- **One definition, shared vectors.** Each entry is described once in the
  contract file, and has render vectors in the vectors file. Adding an entry
  means adding its contract row and its vectors.

### Facts

A fact kind owns a slice of the **fact frame**: the bytes that describe it
for one render. Its module collects the slice on the USB half. A slice is
either **local** (each half collects it itself, because both halves already
know it) or **synced** (the USB half sends it; see
[Facts on the split link](#facts-on-the-split-link)). The module yields its
subjects from the slice, highest-priority first, and says whether any exist.

| Id | Fact | Thing | Parameters | Slice | Subjects |
| ---: | --- | --- | --- | --- | --- |
| 1 | layer active | layer | layer (`0..15`, `0xff` any) | local, 2 B | each active layer, highest first; layer 0, the base layer, is always active |
| 2 | layer previewing | layer | layer (`0..15`, `0xff` any) | synced, 1 B | the layer a pending momentary hold previews |
| 3 | pointing mode active | pointing mode | mode (`0..31` supported, `0xff` any) | local, 11 B | the mode shown; its keys and halves are those of the key or combo that started it |
| 4 | combos held | combo | role: `0` owning the shown preview or mode, `1` the others, `0xff` both | synced, 16 B | one subject for all such combos together: their keys, the halves those keys are on |
| 5 | behaviour state | behaviour | states, scope | synced, 61 B | see below |
| 7 | combo held | combo | combo (`0..127`, `0xff` any), role (`0`, `1`, `0xff`) | synced, 40 B | each identified held combo, newest completion first; its own input keys and halves |
| 8 | behaviour | behaviour | behaviour (`0..127`, `0xff` any), states | synced, 180 B | each key showing that authored row, in matrix order; its selected colour slot |
| 9 | layer status | layer | layer, status (`0` top, `1` held, `2` locked, `3` one-shot armed) | synced, 64 B | matching layers, highest first; actual momentary owner keys and their halves |
| 10 | mode status | pointing mode | mode, status (`0` held, `1` locked) | synced, 11 B | the actual held or locked mode with its real owner keys/halves |
| 11 | combo layer | combo | combo, layer | synced, 40 B | held outputs whose representative key actually holds the selected layer, newest first; combo inputs/halves |
| 12 | behaviour phase | behaviour | behaviour, phase (`0` selected branch, `1` tap pending, `2` hold pending, `3` active hold tier, `4` active long-hold tier), branch (`1..depth`, `255` any) | synced, 180 B | authored key instances in matrix order, independent of flash visibility; captured row, branch and selected colour slot |
| 13 | macro playing | macro | slot (`0..127`, `255` any) | synced, 1 B | actual named VIA playback slot through cleanup; anonymous direct payloads yield none |
| 14 | key pressed | key | matrix index (`0..59`, `255` any) | synced, 8 B | physical matrix keys down, in matrix order; each owns that key and half |
| 15 | modifier | modifier | modifier (`0` Ctrl, `1` Shift, `2` Alt, `3` GUI), side (`0` left, `1` right, `2` either), state (`0` active, `1` one-shot armed) | synced, 2 B | actual modifier mask; either yields left then right |
| 16 | host lock | board | LED (`0` Caps, `1` Num, `2` Scroll) | synced, 1 B | board subject when the USB host's corresponding lock LED is set |
| 17 | pointer | board | state (`0` auto-mouse target on, `1` fading, `2` sniping, `3` raw sensor x/y nonzero) | synced, 1 B | board subject for the actual selected state |
| 18 | idle | board | — | synced, 1 B | board subject for the existing RGB sleep predicate |


"Combos held" leaves out a combo whose output holds a momentary layer: the
layer's own lighting says what the hold does. Its subject stands for every
held combo of that role (thing `0xff`), so it takes the combo table's default
identity; fact 7 yields one subject per identified combo without changing this aggregate.

**Behaviour state.** The USB half keeps, for every key, the state its
behaviour shows (none, branch selected, tap, hold, long hold), the branch for
a selected branch, whether a flashing state is lit, and the newest gesture
that owns each surface: the board, each half, and each state. A flashing state
(a registered hold, a repeat) counts only while lit. The slice carries those
maps exactly as the key runtime produces them (the
[feedback model](../rgbflow.md)). The parameters are a states mask (bit 0
branch selected, bit 1 tap, bit 2 hold, bit 3 long hold; bits 4–7 zero, at
least one set) and a scope:

| Scope | Subjects |
| ---: | --- |
| 0 each key | every key whose state is in the mask; its keys are that key |
| 1 newest per half | the newest gesture on the left half, then on the right; each subject's halves are that half only |
| 2 newest | the newest gesture on the board |
| 3 newest per state | for each state in the mask, long hold first, the newest gesture that entered it, while its key still shows that state |

A subject's identity slot follows its state: hold, long hold, tap, or the
branch colour for branch `n` (`n ≤ 2` uses the first, deeper branches clamp to
the last). Its thing is `0xff` (the subject stands for the gesture, not a
behaviour row), so it uses the behaviour table's default palette.

A key that sits in a held combo's footprint never shows a selected branch: the
combo already lights that key. The fact applies this as it reads the frame,
from the frame's combos slice, so both halves drop the same branches from the
same facts. A build without the combos fact applies it to the slice as the USB
half collects it instead.

**Individual Painting subjects.** Fact 7 uses the same owning/other roles and
momentary-layer-owner exclusion as fact 4, but yields actual combo-domain row
ids (equal completions break ties by lower combo id). It carries the existing runtime's simultaneous output slots (four in
this build), not a new limit on authored combo definitions. Unidentified
fallback outputs yield no subject. Fact 4 continues to use thing `0xff` and
the table default. Fact 7 is the first condition for a rule customising a
single combo, or an Every combo rule that uses individual identities/places.

Fact 8 selects an authored behaviour-domain row id, captured when the runtime
resolves its branch, including compiled rows in authored table order. Intrinsic
and bypassed gestures have no authored row and yield no subject. `states`
has the same bit layout as fact 5; no new state families or scope parameter
are added. Each subject carries the current key (a combo-output behaviour is
projected onto its physical input keys), that key's half, and the selected
identity slot. Flashing holds are absent in their dark interval; release
pulses retain their captured row after slot settlement, including queued
pulses and deferred tap dispatch. Selected branches are suppressed under a
held combo, just as in fact 5. Layer/mode holds that Standard lighting leaves
to the layer/mode scene are also available on this individual surface,
including a pending layer preview, with steady feedback. Fact 5 retains its
aggregate/default identity and its four scopes exactly.

Use fact 8 first for an individual behaviour's identity/painted map; a later
condition is only a filter and cannot supply an identity. An identity paint
uses the chosen slot of this row, inheriting each unset slot separately from
the board-wide behaviour default. The row's place follows the same field-wise
inheritance. An outcome paint is chosen on a rule or map cell; selecting it on
a behaviour's hold/long-hold rule is the colour-by-outcome option.

**Actual state and observation boundaries.** Fact 9 reads momentary bindings,
locked mask and one-shot ownership directly; the keys of a layer subject are
its actual momentary owners. A combo output contributes its representative
owner key, never inferred owners at every input. Top means the highest active
layer, including the permanent base. Automatic activation provenance is not
advertised: QMK's public auto-mouse-active predicate also includes mouse keys,
manual layer anchors, toggle and one-shot retention, so it cannot establish
that sensor motion raised a layer. Facts 17's target-on and fading states
remain available without making that provenance claim.

Fact 12 distinguishes gesture phases from fact 8's displayed feedback. Its
phase bits can overlap: a selected branch can have a tap or hold pending.
An entered hold/long-hold tier remains a phase even when a feedback flash is
dark. Intrinsic/bypassed gestures with no authored row yield none.

Fact 15's active mask is QMK real plus weak plus locked-one-shot modifiers;
its armed mask is the unconsumed one-shot mask. It describes current modifier
state rather than claiming every modifier originates at a physical key.
Modifier ids are their report-bit positions (`0..3` left, `4..7` right).

Fact 17's target-on means auto-mouse is enabled and its configured target layer
is active. Fading uses the existing nonzero fade progress while that target
is on. Sniping includes automatic layer sniping and manually enabled sniping.
Sensor observation is literally the latest x/y passed to
`noah_pointing_device_task_user`, before idle-noise suppression and pointing-mode
transforms. It excludes wheel output and buttons, can include tiny sensor
noise, and makes no claim to distinguish physical ball movement from noise.
Every poll replaces the observation; there is no continuation window. Lighting
collection samples it, and the peer sees the latest committed split frame;
a nonzero sample replaced by a later zero before collection/publication need
not appear on either render. This is distinct from auto-mouse on/fading and
preserves their timing and Standard pixels.

Fact 18 is parameterless: effective RGB timeout is nonzero and
`last_input_activity_elapsed() > timeout`, exactly the existing sleep test.
The collector exports the boolean, so a threshold crossing changes fact bytes
without a new wake API. Timeout zero disables the predicate. There is no
independent idle interval or held-key/macro veto. The existing global sleep
override still forces every LED black; a matching idle rule, or its negation,
cannot override sleep. These new modules default on with the portable runtime
and their required subsystem; each has its own compile flag and capability.

### Targets

| Id | Target | Parameters | Place-capable | LEDs |
| ---: | --- | --- | --- | --- |
| 0 | whole board | — | yes | all 58 |
| 1 | left half | — | yes | `0..28` |
| 2 | right half | — | yes | `29..57` |
| 3 | key set | set (`< K`) | no | the set's LEDs |
| 4 | the subject's keys | — | yes | its keys' LEDs; the whole board when it has none |
| 5 | the subject's halves | — | yes | the halves it concerns; the whole board when none |
| 6 | the subject layer's mapped keys | — | yes | LEDs of keys that are neither `KC_NO` nor `KC_TRNS` on the subject's layer, read live from the keymap; none for a subject that is not a layer |
| 7 | the subject's place | — | no | the target named by the subject's place; none for a kind without a table |
| 8 | keys that | query `0..15`, selector `0..127` or `255` | no | keys matching the query on the shown layer |

**Key queries.** Target8 takes two bytes. Queries0/1/2 select mapped (neither
`KC_NO` nor `KC_TRNS`), transparent and `KC_NO`. Queries3..9 select modifier,
letter, number, symbol, navigation, media/system and mouse usages. Queries10/11/12
select direct layer/mode/macro actions;13 custom keys;14 authored behaviour
rows;15 combo inputs. Selector255 means any; a specific selector is an identity
for10/11/12/14/15 and is ignored otherwise. Out-of-domain or absent identities
match nothing. Behaviour means a row exists, independent of its participation
policy. Combo membership requires an enabled row whose allowed layers include
the shown layer, and exact native-input equality (at most16 inputs per row).
The shown layer is a layer subject's own layer, otherwise the highest active
layer, including the synthetic fade destination.

These are native usages/actions, never host-layout characters. Modifier-wrapped
literals use their base HID usage for categories and also match modifiers.
Modifier-tap, one-shot-modifier and layer-mod actions match modifiers; their
other possible emissions are not inferred. HID ranges are letters04..1d,
numbers1e..27/59..62, symbols2d..38/54..57/64/67/85,
navigation49..52/28/29/2a/2b/58, media/systema5..be and mousecd..df.
Direct layer actions include MO/TO/TG/OSL/TT/LM/LT and userspace layer locks;
mode holds/locks and named macros use their stable native action blocks.
Raw DF/PDF, authored branches and other unsupported actions have no inferred
identity.

The per-layer cache retains native u16 plus authored behaviour row+1 u8 per
matrix position, a canonical any-combo input bitmap per layer, and one geometry
key index per LED. This sufficient basis
composes exact queries without a bitmap per rule. RAM is
`3 * LAYER_COUNT * MATRIX_ROWS * MATRIX_COLS + LAYER_COUNT * KEY_ORIGIN_BITMAP_SIZE + LED_COUNT`, plus generation/readiness
metadata; no query/rule/identity cap is added. On the main context before a
frame's render walk, a changed VIA/keymap or effective-profile generation
rebuilds the basis. Behaviour rows are captured there because their lookup
is reader-backed. Before/after generation disagreement rejects the in-place
rebuild; reader-busy/read-error results also reject it rather than inventing
an absent behaviour. Existing pixels remain and no chunk may render the incomplete basis.
A later settled frame retries. Publication callbacks never walk storage to
rebuild it. Existing VIA local/split/default-restore invalidations and effective
profile generation changes cover the source generations. Rendering uses no
keymap/behaviour profile-reader I/O. Specific combo queries cost at most16 input
comparisons per key; any-combo reads its canonical bitmap. Rebuilding that bitmap costs at most
`LAYER_COUNT * matrix_keys * 128 * 16` input comparisons. These are operation bounds, not
physical timing or cadence measurements. The current query basis has a scoped
3072-byte static-policy increment documented in
[memory budgets](memory-budgets.md#cached-key-queries-and-direct-action-paint);
it does not alter the shared8192-byte lighting budget,255 accepted rules or
any physical/core-memory/stack limit, and does not budget future retained histories.

### Paints

A paint receives the colour **below** (what the LED would show without this
rule and the subjects above it), its parameters, the LED and the time, and
returns a colour or clear. It declares whether it **reads below** and whether
it **changes with time**.

| Id | Paint | Parameters | Reads below | Shows |
| ---: | --- | --- | --- | --- |
| 0 | clear | — | no | clear |
| 1 | off | — | no | black |
| 2 | colour | palette index | no | that colour |
| 3 | identity | — | no | the subject's identity colour; clear when it has none |
| 4 | map | map (`< M`) | as its value | the map's value for the LED, a palette colour or the param-less paint it names |
| 5 | dim | level | yes | `below × level / 255` |
| 6 | tint | palette index, amount | yes | `below` mixed toward the colour by `amount / 255` |
| 8 | outcome | — | no | a fact-8 hold/long-hold's resolved layer or pointing-mode identity, else clear |
| 9 | what it does | — | no | the key's direct layer/mode/macro target identity on the shown layer; unsupported/absent is clear |

**What it does.** Paint9 uses the same shown layer and cached native actions as
target8. It takes the target thing's colour slot zero, with ordinary default,
inherit and `none` handling. A missing table or unsupported action is clear;
black remains opaque. It is parameterless, including map value0x89. Non-key
LEDs are clear; when geometry maps several matrix positions to one LED, the
first matrix position supplies its action. The geometry lookup is cached so
this paint does not scan the matrix or read storage per LED.

**Outcome support.** Paint 8 follows the materialized hold or long-hold action
of the selected branch, including a resolved transparent tier, rather than the
key's current mapping. Supported layer outcomes are the userspace layer hold,
lock/toggle, goto, one-shot and layer-with-modifiers actions; supported pointing
outcomes are a single slot's momentary or lock action. The target table's colour
slot zero supplies the colour, with ordinary default/inherit handling. A missing
table or `none` identity is clear; a palette entry whose value is zero is opaque
black. Literal keys, modifiers, macros, raw unsupported QMK layer actions,
layer-tap action outputs and any multi-mode/unknown outcome are clear. Tap and
branch slots, board and aggregate subjects also say clear. This paint supplies
no fallback to the behaviour palette: use identity for that, or another rule
below. It takes no parameters and is legal in a painted map as `0x88`.

No version-5 paint changes with time. Arithmetic per channel, with integer
division:

- brightness `b`: `(c × b + 127) / 255`;
- dim: `(below × level + 127) / 255`;
- tint and timing blends: `(below × (255 − a) + colour × a + 127) / 255`.

### Timings

A timing gates a rule's subject: from the moment the subject appears it
returns an amount `0..255`, the time of its next change, and whether it is
animating. `0` hides the subject; `255` shows it as painted; a value between
blends the painted colour over below with the timing blend above. Gate state
is kept per (rule, subject thing) while the subject exists and dropped when it
disappears.

| Id | Timing | Parameters | Amount |
| ---: | --- | --- | --- |
| 1 | show after | milliseconds `1..65535` | `0` until the subject has existed that long, then `255` |

The C implementation retains every accepted timed rule and every subject
thing. Equivalent first conditions share presence clocks, normalizing a
specific thing selector to any; rule filters retain separate clocks. The
continuous matching age is the smaller of subject presence and filter age.
An absent subject or failed filter resets its matching lifetime, independently
of other rules. Millisecond ages saturate at 65535, after every possible
show-after delay has elapsed.

Presence storage uses a bitmap of possible signature/subject pairs and a dense
array of ages for the pairs currently present, in bitmap order. Forward removal
then backward insertion preserves every continuing age without a temporary
union of departed and arriving ids or a second age array. A rank query finds
an age; no age is evicted to make room. Only signatures selected by enabled
timed rules are indexed. Duplicate rules share presence but keep independent
filter lifetimes. All 255 state/show-after rules remain accepted within the
shared 8192-byte payload; the new flash/linger admission model below is a
deliberate additional constraint for those new constructions only.

The registry's joint live bound applies across **all** non-selector signatures
of a fact, not separately to each signature. This bounds the dense age array
for every accepted profile, including one containing every distinct signature:

| Fact | Joint live clocks, default four combo records | Reason |
| --- | ---: | --- |
| layer active / preview / mode active | 16 / 1 / 1 | layer bitmap; single preview and shown mode |
| aggregate combos / aggregate behaviour | 3 / 60 | three role signatures; 15 state masks times four scopes, thing 255 |
| combo held | 8 | each of four records matches its role and any |
| behaviour | 480 | at most 60 key records, each in one state; eight of 15 masks contain that state |
| layer status | 34 | 16 held plus 16 locked, one top and one one-shot |
| mode status | 2 | at most one held and one locked subject |
| combo layer | 8 | four records, each matches one layer and any |
| behaviour phase | 600 | at most five phase bits per key, each matching its branch and any; 60 keys |
| macro playing / key pressed | 1 / 60 | one current slot; one record per physical key |
| modifier | 32 | eight bits in each of two classes, each matching its side and either |
| host locks / pointer / idle | 3 / 4 / 1 | one boolean for each parameter signature |
| **Total** | **1314** | sum of the independent conservative upper bounds |

Repeated authored ids only reduce these counts: a pair is stored once even
when several keys yield it. High ids through 127 do not consume extra ages.
Increasing combo buffer length changes its two registry bounds mechanically;
tap depth changes possible signatures but a live phase record still matches
at most its branch and any. Module-off builds sum only their compiled rows.
The possible-pair bitmap covers the full subject domains (8963 pairs with the
default registry/tap depth); the dense ages need 2628 bytes instead of 17926.
The selected-signature index, filter ages and bitmap remain separate, linked
fixed storage, with no allocator escape from RAM accounting.

This live bound is specific to show-after. It cannot be reused as a bound for
linger or event retention: absent things can retain timing state and their
last owner footprint until expiry, and a repeated event must refresh its own
lifetime. New flash/linger rules therefore use the separate deterministic
reservation and admission bound below, including independently filtered rules;
a normal profile or the current live-state maximum does not establish it.

The event catalog provides the following additive, optional entries. Each has
its own source module and generated registry dispatch. Production RGB builds
enable the strong capture/consumer seam; a build without that consumer must
omit event capabilities and reject an explicit event-module configuration.

| Id | Event trigger | Parameters | Captured subject |
| --- | --- | --- | --- |
| 19 / 20 / 21 | layer locked / unlocked / one-shot used | layer or any | actual layer and momentary owner keys at occurrence |
| 22 / 23 | pointing mode started / ended | mode or any | actual mode and its captured owner keys |
| 24 | combo fired | combo row or any | actual row and input footprint, once per fired completion |
| 25 | tap fired | behaviour row or any, branch or any | actual row, physical key, captured branch colour slot and outcome |
| 26 / 27 | hold / long-hold fired | behaviour row or any | actual row, physical key, captured hold tier and outcome |
| 28 | named macro played | slot or any | successful completed slot; no invented key owner |

The corresponding optional timing rows are `2 linger` (state-tail role) and
`3 flash` (event-window role), each with a nonzero little-endian u16 duration.
Show-after row1 has the unrestricted role. Construction roles live on registry
rows and in the client contract; validators use those roles rather than event
or timing id switches. An event is an occurrence matcher, never a fact walker
with an implicit held/recent truth. The qualifier walks the published program
against its caller-supplied coherent occurrence frame, copies borrowed owners
synchronously and returns the qualified255-rule mask plus captured subject.
A false qualification does not authorize erasing a previous qualified marker.
It performs no profile-reader I/O and does not mutate the input fact frame.
The capture pipeline sequences caller-owned collection, qualification and
synchronous retention, and neither keeps producer pointers nor provides a
queue. Its collector/retainer must stay in main, without input reentry or
profile publication during the call. Actual runtime hooks/workspace ownership
and retained histories remain separate integration requirements. In the immediate combo-output path, origin/cache
publication and record normalization precede observation; the output action
itself executes afterwards. A scan-first fired completion can be observed
before delayed output has published an active output owner, so filters see
that actual existing state, not a manufactured layer or cache entry.

New event kinds are positive first-condition triggers and require explicit
flash timing. Mode and combo event capabilities also require their actual
pointing/combo producers; forcing a module without its producer is a compile
error outside the all-kinds schema build. They do not define a recent-event boolean or a negated truth
window. State kinds remain independently negatable filters. The filters
qualify against the coherent occurrence context: a layer appearing later
cannot resurrect an occurrence that failed its layer filter. No occurrence
at boot means no flash. A qualifying repeat of the same event thing replaces
its last ordered matching batch and restarts expiry; a nonqualifying repeat
preserves its older qualifying flash. Different thing ids coexist, retaining
their captured natural order.

Linger is per (rule, subject thing). While the full condition matches, its
last whole ordered matching batch is refreshed. When the full match ends,
that batch freezes until expiry. Departure of some keys while the same thing
still matches updates the live batch rather than creating independent tails.
The enabled production consumer implements these construction and retention
rules. Its compiled registry capabilities advertise flash/linger only with the
actual synchronous observer, admitted working owner and split adapter linked.

Producer occurrences use an explicit `NOAH_LIGHTING_EVENT_CAPTURE_ENABLE`
integration gate. Disabled builds have inline no-ops and no observer symbol,
queue or event capability. Enabled builds must link the actual synchronous
observer. Its owner bitmap is borrowed only for the call: the observer must
copy needed data before returning and must not reenter an input producer.
Standalone producer tests link recording observers; these do not provide
the production consumer. An event module cannot be advertised until its
consumer is enabled and linked on the production and host build surfaces.

Authored behaviour occurrences follow actual branch emission, independently
of Standard's visual feedback eligibility. A committed buffered base tap or
inherited tap still has its captured row and branch even when no visual pulse
is requested. A pending release hold is not a fired hold. Repeated action
emission from one committed tap branch is one branch occurrence, after its
final emission. Deferred output carries the captured row and branch through
the existing dispatch queue; queue-full immediate fallback observes the
actual immediate emission. It does not look up a later key binding. Behaviour
subjects own the same physical matrix key as the per-key phase facts, with
the captured colour slot and supported layer/mode outcome.

Fallback settlement uses a synchronous one-effect stream with caller-owned
context, rather than an unused buffered core plan. Its explicit planning
boundary ends before later transition execution. A sink may drain/project
while planning remains active: that route retains the stream's effect and
caller plan on stack and requires separate reviewed-path coverage. Queue
admission scratch ends before queue-full immediate dispatch. Neither lifetime
boundary changes queue capacity, output order or captured owner/commit data.

The compact action effects and persistent dispatch slots remain12 bytes in
the current compiler configuration. Deferred branch metadata uses the six
unused dispatch-flag bits and snapshot bitfields. Compiled tap depths above63
use an explicit byte per dispatch slot instead (two slots per matrix key),
and larger snapshots remain bounded by the existing64-byte drain workspace.
The default and8/64/255-depth host configurations test those representations;
these sizeof/serialization checks are not linked resource or stack proof.

Flash is valid only for an event trigger; linger only for state/board rules.
Combining event flash and linger is invalid. Show-after and event flash use
parallel clocks from the qualifying occurrence: observation100, flash500 and
show-after200 paints300..599 and expires600; show-after700 never paints.
Qualifying repeats restart both clocks; nonqualifying repeats change neither.
Subsequent filter changes cannot alter an already qualified event's flash.
For state show-after100/linger200, a match100..149 never paints or leaves a
tail; a match100..249 paints200..449 and expires450. Departure cannot mature
unmet show-after. Without show-after, an eligible state can linger immediately.

#### Historical storage experiments (reference tests only)

The following components describe superseded experiments, **not** the current
allocation, acceptance, wire protocol or capability implementation. They remain
explicit reference-test dependencies, outside the production source manifest:
`lighting_lifetime`, `lighting_batch`, `lighting_batch_arena`, `lighting_retained`,
`lighting_priority`, `lighting_history_owner/store/bank/observe`,
`lighting_retained_render`, `lighting_owned_batch`, `lighting_retained_group`,
`lighting_event_capture` and the legacy event transport adapter. Their unlimited
host fixtures and former source-expired delivery journals do not establish
admitted runtime capacity or current replay semantics. Original assertions
remain executable for those historical interfaces.

Current production uses `lighting_working`: deterministic admitted direct
current/snapshot slots, whole ordered members with owned key bitmaps, and the
single replacement workspace described below. Shared calendar vectors execute
through that owner as well as independent calendar references. Admitted shared
whole-batch/event pixel vectors execute both the current `scene.working`
resolver and half-chunk resolution; over-admission profiles are explicitly
reference-only. Actual optional-module natural-priority bindings drive the
production walk. No captured subject is collapsed to its eventual LEDs/pixels.

Each half times from the frame in which it first sees the subject, so the
halves may differ by the link's latency. Gate observation and wake deadlines
use local `timer_read32()`, independently of `sync_timer_read32()` passed to
paint context for animations. Split offset adjustments cannot mature, delay or
rewind a gate. The engine defensively ignores a backward observation sample
and retains its last forward sample; a forward 32-bit wrap advances normally.
As with QMK deadline ordering, wrap/backward discrimination assumes successive
observations within the timer's half-range (about24 days); active RGB frames
observe at the configured frame interval. Millisecond ages remain saturated
at65535 and existing forward/large-jump pixels are preserved.

Resolved facts are owned by the main render context through the synchronous
whole-local-frame composition. Collection stages one module slice, compares actual bytes and updates
that resolved frame; a failed coherent peer read preserves its previous synced
slices. During fade destination rendering, the engine temporarily removes the
auto-mouse layer from active layers, retains base0 and recomputes synthetic
top from that active set. It saves/restores only active/top bytes synchronously;
held, locked and one-shot ownership remain actual. Split callbacks publish to
separate remote storage and cannot mutate the pinned render frame.

Transport storage has disjoint role lifetimes: master built/sent frames share
a union with slave staging/committed frames. Main-context init closes callback
readiness, initializes the selected role arm, registers RPC handlers and only
then publishes readiness. A wrong-role or unready callback rejects before
accessing packet or role storage. Reinitialization is quiescent: the high
priority split worker can preempt main, while main cannot preempt a running
callback. Failed sends preserve the last committed sent frame and force a
whole latest-frame retry. Module-specific idle builders initialize/compare
sentinels using at most one slice of scratch, not a persistent idle frame.

### Thing kinds

Listed under [Thing tables](#thing-tables). A fact kind names the thing kind
of its subjects.

## Rendering

### Publication

When a profile is published the firmware copies the validated lighting payload
into RAM and indexes it: palette RGB, thing tables, key sets, map offsets and
rule part offsets. Rendering never reads the profile. Both halves decode their
own copy of the profile. The compiled Standard lighting is built once, at boot
depth, when the compiled profile is first opened or the program first loads;
the compiled RGB domain that profile transfers stream only copies it.

### The walk

The meaning of a rule list, for one LED, is this walk:

```text
walk(led, from):
  for each enabled rule from `from`, top first, and each of its subjects in order:
      skip the rule if a filter fails
      amount ← the subject's timing gate (255 without timings); skip if 0
      v ← paint(subject) if led is in target(subject), else clear
      if v is clear and the rule has a background: v ← background(subject)
      if v is clear: continue
      if v reads below, or amount < 255: below ← walk(led, the next subject)
      colour ← brightness(v(below))
      if amount < 255: colour ← blend(below, colour, amount)
      return colour
  return unpainted
```

Below the last rule, **below** is the matrix effect's colour at that LED as the
effect drew it this frame (black where the driver cannot report it). An LED
the walk leaves unpainted is not written. Any number of paints that read
below compose.

The firmware evaluates the same thing bottom up, as the old stages did: it
starts from an unpainted board and applies each rule from the last to the
first, and each rule's subjects from the last to the first, so every paint
sees exactly what is below it.

### When it re-resolves

The firmware composes once after QMK has drawn its **last local effect chunk**,
before QMK flushes that frame. Earlier advanced-indicator calls leave the
matrix-effect buffer untouched. At that boundary it captures one coherent fact,
program, keymap and retained-history generation, then resolves/writes every
local LED together. Event, expiry, publication and remap between effect chunks
are included in that final coherent cut. This requires no third history bank
and never treats the previous frame's composed pixels as the current effect.

It re-resolves when facts/program/keymap or observable history state changes,
a timing wakes, or a time-varying result/effect composition needs it. Advancing
an age without changing visibility or a matching batch does not dirty the
scene or wire. Dirty latches survive arbitrary intermediate updates; an old
ACK clears only its immutable cut, never newer current work. Publication-stamp
checks are RAM-only and never lazily load defaults.

### Outside the registries

- **Diagnostics win outright.** While the runtime diagnostics indicator is
  active every LED shows it and nothing else renders.
- **The auto-mouse fade** stays as it was. The fade extension names the
  bottom `base` rules as the fade's scene. While auto-mouse is fading (the
  auto-mouse layer is active, its timeout is past the dead time, no pointing
  mode is locked, and the extension is on), the scene is walked twice: once
  as it is, once with the auto-mouse layer inactive. The start and end colours
  combine with today's blend for the progress, the end follows the mode
  (real destination: the effect where the scene paints nothing; end colour
  where the effect would show; end colour on all keys), and the rules above the
  scene paint over the result as usual. It is re-expressed as a paint once
  time-based paints exist.
- **The RGB timeout** still turns every LED off after the idle timeout.

## Facts on the split link

Each half renders its own LEDs from its own profile and its own fact frame.
Without retained rules, local slices are collected on each half from state both halves already share
(the layer state QMK syncs; the pointing state the runtime base packet
carries). Synced slices are collected on the USB half and sent to the other
half as one **lighting facts frame**, which replaces the per-stage combo and
key-feedback packets and the preview byte of the base packet.

The synced frame is the synced slices in registry order (607 bytes with these modules enabled:
the original 298-byte Painting prefix plus 309 state bytes). It travels in chunks on one transaction, `PUT_SPLIT_LIGHTING_FACTS_SYNC`:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | frame sequence |
| 1 | 1 | chunk index (bits 0–6); bit 7 commit |
| 2 | 30 | frame bytes `index × 30 ..`, the last chunk zero-padded |

Each pass the USB half rebuilds the frame and sends the chunks that changed
(every chunk on a heartbeat, after a failure or when forced), all with a new
sequence; the last chunk it sends carries commit. The other half starts a
staging copy of its live frame (or of the frame at rest, before its first
commit) on the first chunk of a new sequence, applies
each chunk, and publishes staging as its live frame on commit, as one
generation, so a render never mixes two frames. A pass that fails part-way
publishes nothing and the next pass resends every chunk. Heartbeats and
transport back-off are the ones the runtime sync already uses.

When retained rules are active, this separate lighting-facts broadcast is
suppressed. The history snapshot protocol below carries the complete coherent
scene instead; a slave never combines independently fresh local facts with an
unrelated retained cut. Base pointing/layer synchronization remains separate.

### Admitted local histories and conservative split freshness

New flash/linger rules require structural validation **and** shared working-memory
admission before activation. Existing Standard, state and show-after-only rules
retain their acceptance. This deliberately replaces the earlier promise that
8192 payload bytes alone suffice for every new timing construction. It is a
conservative admission tradeoff, not an impossibility claim about other layouts.
Over-budget activation/import is rejected atomically; the active program,
histories and pixels remain intact. There is no eviction or runtime drop escape.

Retained natural priority is registry-owned: layer surfaces use descending
layer identity; matrix/key surfaces retain their stable walker cursor; event
and single-current surfaces use captured entry order after natural ties. Named
combo tails preserve the complete actual newest-first batch. With the current
8220 pool, a broad128-identity combo tail with M>=2 already exceeds Q (10304
bytes at M=2), independent of occupancy. With M=1, the actual origin cache holds
at most one live completion, so older departed completions precede newer entries
only by their captured recency; there is no concurrently older live completion
behind a newer departed one. Named selectors have one whole live-or-tail batch.
This derives from existing producer capacity and admission arithmetic, not a new
selector cap or a sharing assumption. Increasing the pool/admitting broader
multi-live combo tails would also require durable module occurrence-order
bindings before advertising that changed working model.

Working model1 reserves two complete banks and one replacement slot in an
8220-byte pool per half. For each enabled new-timing rule, registry metadata
supplies N selected identities (1 for a named selector) and M maximum complete
ordered members per identity. With B=ceil(matrix_keys/8), C=align4(12+M*(5+B)),
S=sum(12+N*C), W=max(C), and Q=2*S+W; Q=0 without new timing. Checked arithmetic
rejects overflow. Owners, slot, outcome, sides, natural priority and independent
entry order/lifetimes are retained. No sharing, sparse occupancy, short duration,
filter impossibility or input-rate discount affects admission. The second bank
is master immutable outgoing snapshot **or** slave validated staging; local
render consumes current synchronously, never a third pinned bank. The 8220-byte
pool is fixed independently of the scoped linked RAM policy in memory-budgets.md;
reviewed stack policy is unchanged.

Startup loads the compiled program and initializes its histories before input
producers run. Successful publication initializes the admitted owned program,
profile identity and split lifecycle before exposing the new lighting generation;
all fallible source reads/admission precede active replacement. The observer
uses only that prepared generation and synchronously collects, qualifies and
retains an occurrence; it never lazily loads/decodes a program or defers filters.
The pinned split role is fixed at split_pre_init. An explicit role reinitialization
runs at a shallow main boundary before producers, closes callback readiness and
rebuilds the selected role before publication. Synchronous basic-indicator composition at actual effect completion reads the latest complete generation;
partial split transfer keeps the last committed scene. Command-mutation and collection
scratch return before later capture/qualification phases where their lifetimes
are disjoint; outer planning storage remains live during synchronous drains.

Each half renders locally. Source-expired flashes are not deliberately replayed
or granted a fresh window by reconnect. A remote flash paints only when its
**entire** proved age interval satisfies lower>=show-after and upper<flash.
A departed linger preserves its whole eligible batch and departure age, with
no extended expiry. Unknown freshness suppresses transient paint; uncertainty
can shorten or hide a transient. State facts still resynchronize coherently.
The former late-arrival full-window journal below is superseded for the new
production snapshot discriminator; its historical codec/component tests are
not evidence for the revised production protocol.

V1 explicitly assumes nominal **equal-rate logical millisecond timers** between
valid causal evidence and use; independent offsets are arbitrary. This is a
product/software model, not a measured crystal tolerance, ppm bound or exact
physical-deadline guarantee. No-resurrection is conditional on that model.
Slow physical drift need not be detected. Hardware drift, cadence and high-water
acceptance remain unproved.

For snapshot capture C, source request samples M0 before and M1 after a successful
synchronous RPC, and receiver callback sample S inside that causal interval.
At receiver time R the elapsed-source-age interval since C is conservatively
[max(0,M0-C+(R-S)-2), M1-C+(R-S)+2]. Each subtraction of two floor-to-ms timer
readings differs from elapsed logical time by less than one millisecond; the two
subtractions justify outward two-millisecond rounding. No symmetric-delay or
zero-delay assumption is used. C precedes M0, so capture/assembly time is charged;
R-S charges transfer, main-consume delay and subsequent elapsed time. Snapshot
slot ages at C are added with saturation at65535, which is expired for every
u16 duration, never wrapped or restarted.

Every modular span, and its composed evidence-to-use span, must be below2^31ms.
Detected backward/discontinuous local time, ambiguous wrap, session/profile/role
reset or malformed causal evidence invalidates calibration; fresh causal evidence
is required. Full-wrap silence cannot be inferred from a wrapping counter alone:
normal observation/use requires the half-range continuity precondition. This
model is deliberately separate from stepped synchronized paint-animation time.

### Coherent history snapshot v4 (internal split protocol)

HELLO/CONFIRM/DATA/REPLY tags are0xa1/0xa2/0xa3/0xa4. Legacy tags1..4 and
snapshot-v2/v3 tags0x81..0x84/0x91..0x94 are rejected; saved profile v5/ids remain unchanged.
HELLO is27 bytes (profile identity22 and next u32 sequence); CONFIRM9 bytes.
DATA contains tag, epoch u32, sequence u32 and a precisely sized payload:
PING3 (one byte), BEGIN0 (24 bytes), CHUNK1 (4+length bytes, length1..51),
ORDER4 (4+count bytes, count1..51) and COMMIT2 (3 bytes). REPLY14 contains tag,status,epoch,sequence,callback timer.
Requests never exceed64 and replies stay within32; QMK allocation remains128/32.

BEGIN offsets1/5/9/13 carry capture/source-before/source-after/receiver-callback
u32 samples,17..18 logical length,19 flags,20..23 acknowledged baseline token.
Flags0 initializes complete history/facts,1 initializes complete history with
fact deltas,3 preserves acknowledged history with fact deltas,5 applies changed
history slots/orders and fact deltas on the exact baseline; other flags are
invalid. The sender uses5 for changed history after baseline acknowledgment.
A delta requires the exact prior committed baseline token; each successful
whole cut advances it modulo2^32. One synchronous immutable request/sequence
is in flight, so a token cannot be ambiguously reused across observed updates;
session/profile/role reset invalidates the baseline and forces a full cut.

CHUNK offsets1..2 name a monotonically nonoverlapping logical range,3 its
length,4.. its bytes. Full history staging zero-initializes all slot bytes and
copies only local profile-derived descriptors. Source streams occupied headers
and exact ordered members, coalescing adjacent ranges within51 bytes. It does not stream reserved empty slots or stale member capacity; bounded
coalescing may include canonical zero gaps within one51-byte range. Native
descriptors are never sent. Full facts initialize zero; changed facts initialize from the last
published coherent fact frame. A registry-sized bitset selects whole changed
slices, including bytes changed to zero. The source owns its existing fact cut
and freezes collection during transmission, so a slice mask needs no third621
byte copy. New runtime state is collected/coalesced after ACK; new captured
events still mutate current history while the outgoing bank stays immutable.

For history deltas, staging starts from the current bank. Only changed slots
send their complete header/ordered batch, including a zero header on expiry.
ORDER4 offsets1..2 name the first profile-derived slot ordinal,3 count1..51,
4.. the exact entry-rank bytes of consecutive slots. It never changes independent
clocks, phase, owners or payload. Order-only updates of already conservatively
expired receiver cells remain absent. Main copies the locally advanced unchanged
cells at commit (preserving any local gate maturation/expiry); changed calendars
alone receive source-age correction. All updates share the same immutable cut,
calibration and final validation/commit. No partial scene is rendered.

Four fixed dirty bitsets separate current/full-slot, current/order-only and their
immutable outgoing copies; the receiver reuses the snapshot masks for staged
changes. Their bound is derived from admission, not occupancy/sharing: every
retained identity reserves at least align4(12+5+ownerBytes) in both banks, hence
N<=floor(pool/(2*minimumSlot)). For8220/60keys this gives146 cells,19 bytes per
bitset,76 bytes total. Descriptor bytes and replacement workspace only tighten
that bound. Existing state/show-after-only profiles reserve no retained cells.
No accepted sequence can exceed these masks; no new rule/subject cap is imposed.
The bank and replacement workspace remain exactly2S+W. Descriptor-index lookup
is bounded by the admitted rule count; its CPU time is not a measured deadline.

COMMIT offsets1..2 give the exact logical end, allowing omitted trailing
canonical zeros/unchanged baseline. Main validates the entire new history,
advances source-aged transients with causal uncertainty, then swaps its bank
and publishes facts as one cut. Facts-only commit keeps its acknowledged
history and advances its local calendar; it never resets ages or changes
occurrence qualification. Its source capture precedes transfer, but no transient
age calibration is replaced by a facts-only update. Wrong range/profile/size,
baseline or slot metadata cannot publish mixed state. Failed consume exposes
REJECT, releases the failed mailbox and requires HELLO/full resync; it never
ACKs a failed commit or spins permanently PENDING. The last committed scene
survives validation failure. Defined fresh session clears transient history.

Source dirty latches trigger one immutable coalesced cut. Idle settled scenes
send **zero** RPCs. An ordinary fact change on an acknowledged unchanged-history
baseline sends only changed slices; it does not wait for an unnecessary full
history resend. When history also changes both are captured/committed together; unchanged
large payloads and calendars are not resent for a small update.
Retries are byte-identical; committed sequence duplicates never reapply/restart
clocks. Failure backoff uses the existing split policy50..1000ms (configurable),
only after failed transport; no scene/flash/input rate or duration restriction.
Normal source expiry is never restarted at arrival. The fixed8220 pool remains
2S+W; fact staging, endpoint control/dirty bits and failure backoff are accounted
separately by linked resource checks. There is no third history/fact bank.

The existing base heartbeat additionally requests **one response byte** only
when capture is enabled and the source actually uses a retained scene. Its
version1 exact magic is `d0` NOT_READY or `d1` READY; zero, missing, unknown and
mixed-peer bytes are never READY. Standard/non-retained/disabled paths retain
the original zero-response base RPC. No new polling interval or buffer exists.
READY means main committed a baseline for the current initialized endpoint;
it is not profile/session authorization. The callback reads only a published
scalar. Main publishes it in a short ChibiOS scheduling-critical section after
complete validation/commit; session, role, profile, close, detected clock reset
and rejection clear it before exposing the unready state. In-progress coherent
deltas do not erase a previously committed baseline.

A source with an acknowledged baseline invalidates it once on unavailable or
unknown readiness, supersedes any old immutable outgoing packet with a fresh
HELLO, and captures a full current/live scene after the session handshake.
It keeps current histories, never creates a new window for source-expired
flashes, and cannot repeatedly reset a pending resync: invalidation clears
its baseline flag immediately. The same heartbeat later observes recovery.
Missing/unknown replies and mixed snapshot discriminators/profile mismatch
use the existing failure backoff; an unavailable base reply emits split trace
SCENE_UNAVAILABLE with raw byte/transport result. No diagnostic claims an
unsupported peer is compatible. A READY byte never replaces full identity,
epoch/sequence validation or main commit. Quiet peer-only reboot recovery uses
this existing heartbeat; settled unchanged lighting RPC count remains zero.

A continuously observed remote MATCH retains this half's own show-after start.
If the new source episode's age upper bound at consumption is strictly below
the receiver's uninterrupted observed lower age, a source restart is proved:
start a new local gate and clear old eligibility. A continuous episode cannot
meet that inequality under the equal-rate model. Saturation/unknown intervals
do not prove a restart. Departure/reentry that is entirely unobserved and not
provable from these intervals remains observationally indistinguishable; no
claim of perfect restart detection is made.

#### Raw effect storage and physical output ownership

The pinned RP2040/ChibiOS vendor WS2812 target composes the complete local
scene from QMK's **basic** indicator callback, which runs after actual effect
completion and before the advanced callback. This covers chunked effects and
whole-frame Digital Rain; the advanced range is not a completion signal.
The immediate advanced call is suppressed before range checks, including a
right-half iter0 range outside that half. The public advanced entry retains
its independent legacy range behavior, but production uses basic completion.
There are no project keymap/basic/advanced overrides or configured indicator
modules that bypass this order or write over the composed output afterward.
Any new override must preserve the owned completion/restore lifecycle and
explicitly document its indicator priority.

Before any owned paint, diagnostics or sleep-black override, capture **all**
local raw driver pixels. Native Raindrops/Jellybean preserve untouched pixels
between frames, so the composed buffer cannot become their next raw input.
One owned cache uses max(leftCount,rightCount) native WS2812 pixels (87B for
29/29 RGB), plus a phase byte and basic/advanced suppression flag. It is not a
history bank, an RGB-equality inference or a working-admission discount.
Indexing uses QMK's actual global-to-driver projection; asymmetric halves are
covered, with compile-time geometry bounds. Published resolved facts and the
immutable outgoing scene remain separately owned.

The two-state restore phase is derived from this **specific pinned loop**:
keyboard_task calls rgb_matrix_task once on each role, then main calls
housekeeping once. Same-loop housekeeping only arms restoration. The following
RGB step normally FLUSHES: ws2812_flush first copies all driver pixels into its
separate WS2812_BUFFER, then starts DMA. Housekeeping can restore raw driver
storage while the physical transfer consumes the separate composed copy.
Mode/enable/toggle setters can replace FLUSHING by STARTING; STARTING performs
no effect writes, so restoration then precedes the next render and cannot erase
new pixels. It is not a generic scan-count or elapsed-time flush heuristic.

USB suspension stalls the ordinary pair. Its userspace power-down hook cancels
pending restoration before QMK synchronously renders/flushes NONE black, even
if resume precedes housekeeping. On the slave, transactions_slave applies at
most one synchronous suspend observation per matrix pass before RGB; observing
suspended state in housekeeping discards the superseded cache. RPC callbacks
stage state, not raw RGB. NONE does not call basic indicators; its black flush
and subsequent init never inherit a stale restore. Profile/remap/publication
changes leave pending raw ownership intact; fresh keyboard/driver initialization
is a separate quiescent boundary. The runtime's actual output writers are its
owned composition/diagnostics/timeout and QMK's documented effect/control paths.
Production drivers/loops outside the reviewed RP2040/ChibiOS vendor WS2812
with RGB sleep configuration fail compilation pending a new ownership proof;
host fixtures are explicitly test backends. Raw storage is not a physical RGB
or cadence measurement. The source-backed native fixture compiles the pinned
effect bodies and task/control code and checks raw storage separately from the
physical-transfer copy under normal and sanitized builds.

#### Wire cost and timing limits

Pinned RP2040 vendor PIO serial defaults to230400 baud,8N1. Each RPC executes
four QMK transactions: INFO4, REQUESTn, EXECUTE1 and RESPONSE14. Four id/handshake
pairs and four trailing CRC8 bytes add12; total is n+31 wire bytes (95 for a
64-byte request). Each send also enters the pinned PIO receive-state wait of
floor(11 bit times)=47us. A host schedule may charge all twelve sends/waits,
including conservative overlap of the last character, plus explicit nonzero
main scheduling/retry delays; this is a software projection, not a hardware
cadence upper bound. Main-before-ACK stop-and-wait needs a second RPC to observe
main's commit. Short requests omit unused payload padding. A fact-only update
skips recalibration; a changed-history cut obtains fresh causal evidence.

The scoped base heartbeat has the same four transactions but RESPONSE1:
`n+18` wire bytes, versus `n+17` for the original RESPONSE0, an extra byte
on an existing response transaction. With the configured mode-owner base
payload13 this is31 bytes. It retains the existing250ms active/1000ms idle
heartbeat and50..1000ms failure backoff; recovery latency includes those waits
plus complete resync. No additional idle lighting RPC is issued to poll readiness.

Standard's existing feedback pulse uses the configured200ms default and retains
its legacy logic. Named500ms and useful-mixture200/500ms transients are exercised
with real621 facts and every wire/request/reply/main/retry charged. Short windows
can still be shortened or suppressed under load; an all-occupied admitted cut
may take longer than a short pulse. Capacity and conservative expiry are not a
promise that every event is observable remotely. Physical pointing cadence,
clock drift and allocator/stack high-water remain unproved.

The quantitative hardware-pending ledger includes the useful8220/200ms host
scenario:48RPC and approximately137ms charged synchronous blocking across its
trace; an ordinary fact delta takes5RPC/approximately20ms. Quiet peer reboot
can wait for the existing1000ms heartbeat, followed by approximately946ms for
the cold full-scene stress resync model. These are software wire/scheduling
projections, not measured physical latency or pointing cadence; suppression
at the original expiry still applies, without a restarted visibility window.

### Qualified event groups (internal split protocol) — legacy component

The following paragraphs describe the superseded **test-only** group protocol
and its component guarantees. Its source-expired journal, numeric occurrence
identities and proposed owner wiring are historical; they are not the enabled
working-owner/snapshot v4 behavior specified above.

`qmk_lighting_event_transport` binds the portable sender/receiver to actual
`transaction_register_rpc` / `transaction_rpc_exec` with at most one bounded
RPC per master step. The callback validates/stages only; slave main invokes
the consumer. Its role-selected link is initialized and registration kept
closed before readiness publication; wrong-role and registration-time calls
cannot overwrite a packet arm. The master offer workspace ends before the
synchronous RPC call. Enabled builds add their transaction id through the
shared config; disabled capture adds no id, instance or callback. A two-instance
host seam exercises these actual calls with normal and sanitized builds.
The history owner must still supply the fair next/consume/reset/ACK operations,
exact effective-profile identity and main/init hooks before production capture
is enabled. This adapter alone does not advertise or implement that owner.


Event transport is separate from the coherent state-fact frame. The portable
codec/session component below is implemented; the occurrence qualifier,
retained-history consumer and QMK RPC adapter were required before event
modules can be advertised. It does not add a profile/domain version or enlarge
QMK's 128-byte request/32-byte reply allocation. All integers are little endian;
there is no native struct serialization.

| Message | Bytes | Layout |
| --- | ---: | --- |
| HELLO1 |27 | opcode1, agreed profile identity (22 bytes), exact first sequence u32 |
| CONFIRM2 |9 | opcode2, receiver epoch u32, exact first sequence u32 |
| DATA3 |63 | opcode3, epoch u32, sequence u32, qualified group (54 bytes) |
| REPLY4 |10 | opcode4, status u8, epoch u32, echoed sequence u32 |

Profile identity (22 bytes) is generation, payload CRC32, payload digest, compiled-default
digest and action ABI digest (five u32), followed by origin and kind (two u8),
using the effective provider's identity fields. HELLO must agree before any
rule mask is consumed; publication/reboot/role change invalidates the session.
The group is order u64, subject thing-kind/thing/slot/outcome/sides/has-keys
(six u8), owner bitmap8 and qualified-rule mask32. Bits 0..254 name accepted
rule ordinals; bit 255 is zero and the mask is nonempty. Sides 0..3 and
has-keys 0..1 retain their subject meanings. Capture has already qualified all
independent filters at the coherent occurrence; the receiver never substitutes
its current filters. The order distinguishes a newer qualifying repeat from
the immutable outgoing version. Source qualification and storage bounds are
not established by these packet sizes.

Statuses are 1 pending,2 committed ACK,3 need HELLO,4 reject,5 profile mismatch
and6 HELLO ready. Callback validates and stages only. Main owns the immutable
mailbox through synchronous consumption and publishes ACK only after the whole
group commits. A consumer that cannot commit changes nothing and leaves the
packet pending. Retries before main return pending; the last committed sequence
returns ACK without consuming/restarting its clock. Any other DATA sequence
rejects. The exact next-u32 sequence increments modulo2^32. The sender advances
the first sequence when an old pending request discovers a fresh receiver, so
that request is excluded even if the receiver's boot epoch repeats. HELLO and
CONFIRM are retried byte-identically until their respective replies; a lost
CONFIRM ACK retries CONFIRM, never starts another HELLO/reset.

Epoch is a receiver session counter, not a unique boot nonce. Pinned QMK runs
one synchronous CRC-protected info/request/execute/reply sequence at a time:
no older asynchronous RPC survives a later completed request. The sender owns
one outgoing packet until ACK; a boot loses it, and a role/profile reinit closes
callback readiness before touching union/control storage. Epoch/sequence wrap
therefore does not rely on comparing arbitrary packets retained across 2^32
completed sessions. Out-of-order/stale DATA within the bounded RPC lifetime
rejects; an already committed last request is the sole idempotent duplicate.
Synthetic packets retained outside that lifetime are not an extra transport
queue or a security/replay guarantee.

In the superseded legacy component, an ordinary failed request or lost ACK never
resets the session or discards an expired unsent marker. Marker 100/flash500 first committed remotely at 1000 starts
that half's local window 1000..1499 and expires 1500; a duplicate cannot restart
it. An explicit fresh session clears receiver histories and seeds the sender
only from currently live qualified markers, excluding expired past events.
Main gives a staged reset priority over an old pending/consuming group; an old
ACK cannot publish across it. A qualifying newer repeat remains dirty when ACK
cleanup compares the captured order/mask with current records; a nonqualifying
repeat leaves the older qualified record untouched. Round-robin dirty history
selection remains a consumer integration obligation, not a short dropping
transport queue.

One caller-owned link object bounds control plus the role-disjoint outgoing/
mailbox storage at 112 bytes (native layout is not the wire layout). No retained
history or event queue is allocated inside it. Initialization/readiness and
synchronous borrowed group lifetimes must hold on each half; no packet pointer
survives its ACK callback. Component host vectors cover codec boundaries,
pending/ACK loss, immutable retries, newer occurrences, sequence wrap, reset
while consuming and role readiness. They do not prove the unfinished retained
history bound, enabled ARM integration, physical cadence or hardware behavior.

## Capabilities

Feature bit 31 advertises version-5 lighting and the lighting capability
read: custom GET on channel 0, value `0x0D`, with the usual request id, status
and padding rules.

| Page | Payload |
| ---: | --- |
| 0 | layout version `1`, domain version `5`, payload budget (u16, `8192`), palette maximum (`128`), conditions per rule (`4`), part parameter maximum (`16`), tap depth, LED count (`58`), registry count (`7`) |
| `1 + 2r + h` | registry `r`, ids `128h..128h+127`: registry id, first id, then a 16-byte bitmap (bit `i` of byte `i / 8` is id `first + i`) |
| 15 | working model1, pool capacity u16, owner bitmap bytes, descriptor12, slot header12, member base5, alignment4 |
| `128 + fact-id` | compiled module id, subject domain, selector parameter (255 if none), maximum whole-batch members; unsupported pages return no payload |

Registries `r` are `0` facts, `1` targets, `2` paints, `3` timings, `4` rule
extensions, `5` domain extensions, `6` thing kinds. A client offers only what
the keyboard reports and keeps a rule it cannot edit byte for byte.

With all modules enabled, the low-id fact page is registry 0, first id 0,
bitmap prefix `be ff ff 1f` (ids 1–5, 7–28); builds without the
retention consumer omit ids19–28 (`be ff 07`); the paint page is registry 2,
bitmap prefix `7f 01` (ids 0–6, 8). Target prefix becomes `ff 01` (ids0–8); paint prefix `7f 03` adds paint9.
Thing kinds now include 5–7, giving the low-id thing bitmap `fe`.
The timing low-id page is page7 (registry3, first id0): payload prefix
`03 00 0e`, with bitmap `0e` for show-after1, linger2 and flash3. The remaining
15 bitmap bytes are zero. Each timing module contributes only its own bit:
disabling `NOAH_LIGHTING_TIMING_SHOW_AFTER`, `NOAH_LIGHTING_TIMING_LINGER` or
`NOAH_LIGHTING_TIMING_FLASH` removes respectively `02`, `04` or `08`.
Without the retained consumer (`NOAH_LIGHTING_EVENT_CAPTURE_ENABLE=0`), linger
and flash default off, leaving timing bitmap `02` when show-after is enabled;
explicitly enabling retained timing/event modules without that consumer fails
compilation. Schema-only all-kind test builds are not consumer capabilities.
The rest of each 16-byte bitmap is zero. Disabling
`NOAH_LIGHTING_FACT_COMBO_HELD`, `NOAH_LIGHTING_FACT_BEHAVIOUR` or
`NOAH_LIGHTING_PAINT_OUTCOME` removes only its kind/bit and rejects that kind
in rules/maps. The default fact flags follow combo and key-feedback support;
the paint defaults on with RGB. These additions use the open registries:
domain version 5, page layout version 1, existing ids and Standard lighting's
encoded bytes are unchanged. The illustrative future ids 6 (host OS) and
7 (breathing paint) in the walkthroughs remain unimplemented.

## Version 4 → version 5 conversion

Firmware accepts only version 5 (D-F10). A version-4 domain, from a profile
stored by earlier firmware or an older backup, becomes version 5 through this
conversion, which a client performs before Apply. The firmware performs the
same conversion on its compiled `rgb_config.c` tables, encoded as version 4,
to produce its compiled defaults and factory fallback, so a v4 profile and the
firmware's own lighting are the same function of the same bytes. The result is
**Standard lighting**: exactly the frames the version-4 renderer drew.

Inputs: a valid version-4 payload, the target keyboard's tap depth, and the
kinds it supports. A rule is emitted only if the keyboard supports every kind
it uses; a thing table or extension only if it supports that kind. A rule from
a stage whose version-4 enable bit is clear is emitted **disabled**. Every rule
has brightness `255` and no background, timings or extensions.

Notation: `pal(c)` is the palette index of colour `c` (below); a group row's
colour `0,0,0` means **inherit**, painted as `identity`, any other colour as
`colour pal(c)`. Localities map to places: both halves → whole board (0),
left → left half (1), right → right half (2), key half → the subject's halves
(5), keys only → the subject's keys (4).

**Rules**, top to bottom:

1. *Behaviour feedback* (only when the version-4 tap-branch count is not zero;
   stage bit 4):
   1. each key-feedback group row whose semantic is not "all", last stored
      first: `behaviour state(states = 1 << semantic, scope 3)`, `key set
      (group)`, inherit or colour;
   2. each "all" row, last stored first: the same with states `0x0f`;
   3. `behaviour state(0x0f, scope)`, `the subject's place`, `identity`, where
      scope is `0` for keys only, `1` for key half, `2` otherwise.
2. *Combo overlay* (stage bit 3): each combo group row, last first: `combos
   held(1)`, `key set`, inherit or colour; then `combos held(1)`, `the
   subject's place`, `identity`.
3. *Pointing modes* (only when the PD-colour count is not zero; bit 2): each
   PD group row, last first: `pointing mode active(selector)`, `key set`,
   inherit or colour; then `pointing mode active(any)`, `the subject's place`,
   `identity`.
4. *Preview* (only when the layer-colour count is not zero; bit 0): each layer
   group row, last first: `layer previewing(selector)`, `key set`, inherit or
   colour; then `layer previewing(any)`, `the subject's place`, `identity`.
5. *Combo underlay* (bit 3): as 2 with role `0`.
6. *Layers* (only when the layer-colour count is not zero; bit 0): each layer
   group row, last first: `layer active(selector)`, `key set`, inherit or
   colour; then **Every layer**: `layer active(any)`, `the subject's place`,
   `identity`. These rules are the fade's scene.

**Thing tables:**

- layer (when layer colours exist): one record per layer colour row; place
  whole board for "all keys", else the subject layer's mapped keys; colour
  `pal` of the row, or none when its saturation and value are both zero.
  Default record: mapped keys, none.
- pointing mode (when PD colours exist): one record per slot: place from its
  locality, colour `pal` of the row (black is a colour here). Default record:
  whole board, none.
- combo: no records; default record: place from the combo locality, colour
  `pal` of the combo colour.
- behaviour (when tap-branch colours exist; their count must be the target's
  depth − 1): no records; default record: place from the key-feedback
  locality, then `pal` of hold, long hold, tap committed and each tap-branch
  colour in order.

**Key sets:** the version-4 groups, in id order. **Maps:** none.

**Extensions:** auto-mouse fade with flags = stage bit 1, the version-4 mode,
`pal(end colour)` and base = the number of rules emitted in step 6;
tap-commit flash when stage bit 4 is set and the tap-commit policy is `1`.

**Palette:** each distinct colour gets the next index the first time the
encoded result references it, reading the payload in order (thing tables,
default record first, fields in record order; then rules top to bottom; then
extensions). The worst version-4 profile needs at most 89 colours (92 at depth
eight), under the 128-colour ceiling.

## Standard lighting

The compiled profile converts to these rules (`tests/fixtures/lighting_vectors_v1.json`
holds its exact bytes as `standard`):

| Rule | When | Where | Paint |
| --- | --- | --- | --- |
| Behaviour accents | behaviour state any, newest per state | thumbs key set | identity |
| Behaviour feedback | behaviour state any, newest per half | the subject's place (its half) | identity |
| Combo accents (overlay) | combos held, others | thumbs | identity |
| Combo overlay | combos held, others | the subject's place (its halves) | identity |
| Pointing accents | pointing mode active, any | thumbs | identity |
| Every mode | pointing mode active, any | the subject's place | identity |
| Preview | layer previewing, any | the subject's place | identity |
| Combo accents (underlay) | combos held, owning | thumbs | identity |
| Combo underlay | combos held, owning | the subject's place | identity |
| Every layer | layer active, any | the subject's place | identity |

with the auto-mouse fade over **Every layer** and the tap-commit flash on.
Layer 0 has no colour, so the matrix effect shows wherever no rule paints.

## Modularity walkthroughs

Each walkthrough adds one entry to the version-5 engine. Each touches one
firmware module (its file, its registry row and manifest line), the contract
file and its vectors, and no format version; a client adds one descriptor.

1. **Host OS fact.** New fact id `6`, *host OS is* (parameter: os `1` macOS,
   `2` Windows, `3` Linux; thing: board). Its module `fact_host_os.c` reads
   the effective OS (the settings override, else QMK's detection) into a
   synced one-byte slice and yields one board subject when it matches. The
   frame grows by one byte, which the slice table sizes; the chunked frame
   carries it with no protocol change. Capability: fact bit 6. Vectors: each
   OS against a rule `host OS is(macOS)`, `whole board`, `colour`.
2. **"Keys under the fingers" target.** New target id `8`, *keys held now*
   (no parameters; place-capable). Its module `target_held_keys.c` owns a
   synced eight-byte slice of the matrix keys currently down, collected on the
   USB half (which sees both halves' matrices), and returns their LEDs. A
   target owning a slice uses the same slice table as facts. Capability:
   target bit 8. Vectors: two held keys under a rule `always`, `keys held
   now`, `colour`.
3. **Breathing paint.** New paint id `7`, *breathe* (palette index, period
   ms u16, floor u8; changes with time). Its module `paint_breathe.c` computes
   `floor + (255 − floor) × wave(time / period)` of the colour. Because it
   declares that it changes with time, the engine re-resolves every frame
   while it shows; time is the split-synchronised millisecond clock, so both
   halves breathe in phase. Capability: paint bit 7. Vectors: the colour at
   four times.
4. **Fade-in timing.** New timing id `2`, *fade in* (milliseconds). Its module
   `timing_fade_in.c` returns `255 × elapsed / ms`, animating until it reaches
   `255`, with the engine's per-(rule, subject) gate state. The walk already
   blends partial amounts over below. Capability: timing bit 2. Vectors: a
   layer rule at 0, 50 % and 100 % of the time.

None of the four changes the header, a section, the part encoding, the walk
or the split frame's protocol, so the domain stays version 5. A keyboard
without the module rejects the new id; a client without the descriptor keeps
the rule read-only and byte for byte.

## Validation

The firmware validates a candidate before it can become active, in bounded
steps of at most 20 bytes read each, as every domain does; each step checks
every whole colour, record, key set, map value or rule part its read holds:
header and counts,
palette brightness, thing tables (kinds, counts, place and colour bytes),
key-set padding bits, map encodings, canonical sparse/dense choice and values,
every rule's size, flags, part order, counts and kinds, each part's declared
length and parameter rules, and the extensions. Reference bounds (`< P`, `< K`,
`< M`, base `≤ R`) are checked against the header counts. Errors carry the
payload-relative offset; the vectors file names each rejection.

This document governs the bytes and their meaning. Transport and activation
are specified by [Profile Wire](profile-wire-v1.md) and the
[logical transaction contract](logical-profile-transaction-v1.md); RAM policy
by [memory budgets](memory-budgets.md).

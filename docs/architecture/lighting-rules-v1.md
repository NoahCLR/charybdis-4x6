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
filter lifetimes. All 255 rules remain accepted within the shared 8192-byte
payload; no timed-rule or subject limit is added.

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
lifetime. The unfinished additions need a separate accepted-profile history
bound, including independently filtered rules; a normal profile or the current
live-state maximum does not establish it. No event/linger module is advertised
by this checkpoint.

New event kinds are positive first-condition triggers and require explicit
flash timing. They do not define a recent-event boolean or a negated truth
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
These construction and retention rules constrain the forthcoming modules;
this checkpoint does not advertise flash or linger support.

Producer occurrences use an explicit `NOAH_LIGHTING_EVENT_CAPTURE_ENABLE`
integration gate. Disabled builds have inline no-ops and no observer symbol,
queue or event capability. Enabled builds must link the actual synchronous
observer. Its owner bitmap is borrowed only for the call: the observer must
copy needed data before returning and must not reenter an input producer.
Standalone producer tests link recording observers; these do not provide
the production consumer. An event module cannot be advertised until its
consumer is enabled and linked on the production and host build surfaces.

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

Resolved facts are owned by the main render context through every effect
chunk. Collection stages one module slice, compares actual bytes and updates
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

The firmware resolves a whole frame at the first LED chunk of a frame that
falls on its own half (or the first chunk drawn at all), and only when
something changed: the fact frame, the published
program, the keymap (for mapped-key targets), a timing's next change, or every
frame while something time-based shows (an animating timing, a paint that
changes with time, the auto-mouse fade). Between resolutions each chunk copies
the resolved colours; LEDs whose result composes over the effect are finished
per chunk from the effect the chunk just drew.

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
Local slices are collected on each half from state both halves already share
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

## Capabilities

Feature bit 31 advertises version-5 lighting and the lighting capability
read: custom GET on channel 0, value `0x0D`, with the usual request id, status
and padding rules.

| Page | Payload |
| ---: | --- |
| 0 | layout version `1`, domain version `5`, payload budget (u16, `8192`), palette maximum (`128`), conditions per rule (`4`), part parameter maximum (`16`), tap depth, LED count (`58`), registry count (`7`) |
| `1 + 2r + h` | registry `r`, ids `128h..128h+127`: registry id, first id, then a 16-byte bitmap (bit `i` of byte `i / 8` is id `first + i`) |

Registries `r` are `0` facts, `1` targets, `2` paints, `3` timings, `4` rule
extensions, `5` domain extensions, `6` thing kinds. A client offers only what
the keyboard reports and keeps a rule it cannot edit byte for byte.

With all modules enabled, the low-id fact page is registry 0, first id 0,
bitmap prefix `be ff 07` (ids 1–5, 7–18); the paint page is registry 2,
bitmap prefix `7f 01` (ids 0–6, 8). Target prefix becomes `ff 01` (ids0–8); paint prefix `7f 03` adds paint9.
Thing kinds now include 5–7, giving the low-id thing bitmap `fe`.
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

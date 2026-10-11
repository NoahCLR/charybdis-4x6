# PD-mode domain v1

Implemented in schema-2 builds: domain codec, incremental validation, immutable
effective cache, shared engines, persistence/split writers, portable profiles
and live editor. Side-specific builds own committed profiles; the generic build
loads the 32 compiled factory slots without a profile owner. Domain version 2
(D-F09, 2026-10-06) stores 32 slots sparsely; version 1 below is its record
layout and the retired fixed eight-slot domain.
Physical upgrade and release acceptance remain pending; see
[Hardware acceptance](#hardware-acceptance).

## Scope

The keyboard owns 32 pointing-mode slots, stable IDs `0..31` shown as slots
1–32, and 32 matching PD RGB rows keyed by the same IDs (RGB domain v3). The app reads, edits
and saves them in the ordinary draft, review and logical Apply; reboot, backup
and restore preserve them without the repository.

- One mode is active at a time. Mode replacement, same-mode unlock and lock
  exclusivity keep their pre-slot semantics.
- Each slot has a momentary (hold) and a lock-toggle action. Tap, hold and
  multi-tap gestures stay in key behaviors.
- Normal cursor movement is the fallback outside the slot bank. Auto-sniping
  stays a layer/CPI policy and uses no slot.
- There are two engine families, **directional** (four or eight directions,
  single axis or dominant axis; see D-L24 and D-L28 in the
  [direction](../LIVE_EDIT_APP_DIRECTION.md)) and **scrolling**. Optional
  modifier and mouse-button policies cover Arrow and Pinch. No behavior depends
  on a slot's name.

Out of scope: scripts or C hooks, macro programs as motion outputs, layer or
mode changes emitted by motion, simultaneous modes, new gesture engines,
foreground-app switching and volatile device preview.

### Factory presets

The original factory records migrated the six pre-slot modes with two disabled
slots. The current authored profile also configures slot 6 as horizontal Undo /
Redo; slots 7–31 are disabled and unnamed, so the compiled domain stores seven
records. Slot contents are profile data, not part of the
keycode allocation.

| Slot | Name | Engine | Behavior preserved |
| --- | --- | --- | --- |
| 0 | Dragscroll | Scrolling | Local scroll algorithm, inversion, axis hysteresis, timing, CPI, lock-owned auto-mouse |
| 1 | Volume | Directional, vertical | Volume down/up, threshold 60, inherited DPI, pointer-layer anchoring |
| 2 | Brightness | Directional, vertical | Brightness down/up, threshold 60, inherited DPI, pointer-layer anchoring |
| 3 | Zoom | Directional, vertical | Cmd-minus/Cmd-equals, threshold 80, DPI 400 |
| 4 | Arrow | Directional, dominant axis | Arrows, X/Y thresholds 40/50, DPI 400, vertical Alt masking, BTN1 hold right Shift, BTN2 Cmd+C, BTN3 Cmd+V, typing-layer preference |
| 5 | Pinch | Scrolling | Dragscroll's tuning plus owned left Cmd with managed-only modifier masking |
| 6 | Undo / Redo | Directional, horizontal | Cmd-Z / Shift-Cmd-Z, threshold 40, DPI 100 |
| 7–31 | Empty | Disabled | Inert actions; not stored; retained, editable RGB row |

Dragscroll and Pinch run this repository's
[`pd_mode_dragscroll.c`](../../users/noah/lib/pointing/modes/pd_mode_dragscroll.c),
not the fork's native `DRAGSCROLL_MODE`; never activate both engines.

### Slot operations and RGB identity

- Slot order is fixed; renaming never changes an ID.
- Create configures an empty slot. Duplicate copies into a chosen empty slot,
  with its RGB row and slot-specific group assignments but not its activation
  bindings.
- Clearing a slot that keys still reach is allowed: the firmware refuses to
  activate an empty record, so those keys are inert until the slot is
  configured again, and the app says so.
- A disabled slot keeps its RGB row but paints nothing. An "all modes" group
  applies to any active configured slot.
- Names, mode data and RGB share undo/redo, review, conflict detection, backup
  and Apply. Colour lives only in the RGB domain.

## Runtime and publication

Engines and policies are compiled capabilities; profile data selects and
configures them. Slot configuration is materialized once, immutably, when a
generation is published. Transient accumulator, gesture and output-lease state
stays separate from configuration and out of backups. A late release resolves
against the activation that acquired its output, even after another slot
became active.

Every build links the configured directional engine and shared dragscroll
algorithm. The mode registry retains the 32 stable native hold/lock
identities but no per-preset motion, key, reset, DPI or lifecycle callbacks.
The old per-preset handlers and readback-bridge build options are retired.
Factory-only builds encode and validate their compiled slot records at startup;
owner builds warm the cache from their published generation and fail closed if
that read fails.

Pointing polls never decode EEPROM or allocate. A cache-copy failure exposes an
unavailable state, never a partial table or a mix of generations. Publication
uses the strict activation boundary: Apply waits for every active or locked
mode, held button override and owned modifier to clear, with a reason the app
shows, and never force-releases a user-owned key. Changing modes clears old
motion debt and releases only the old mode's outputs. A host disconnect is not
a mode exit; committed modes keep working without the app.

Directional output ceilings (four taps per poll, 32 queued whole steps per
accumulator) are firmware safety policy, not per-slot settings.

## Domain and record encoding

Domain ID `0x50`, version `3` (D-F14). All multibyte integers are unsigned,
little-endian. The payload is the eight-byte header `03 20 80 nn 00 00 00 00`
(version 3, slot capacity 32, record size 128, record count `nn` = 0..32, four
zero bytes), then exactly `nn` 128-byte records in strictly ascending slot ID,
each ID below 32: `8 + 128 × nn` bytes, at most 4,104. A slot's record is
present exactly when the slot is configured (kind ≠ 0) or disabled with a
non-empty name; an omitted slot is disabled with an empty name. A present
record that is disabled with an empty name is noncanonical and rejected
(`NONCANONICAL`), as are a count above 32 or a wrong version, capacity or
record size (`INVALID_HEADER`), nonzero header bytes 4..7 (`RESERVED`), a
length other than `8 + 96 × nn` (`INVALID_LENGTH`), and a repeated,
out-of-order or out-of-range ID (`INVALID_ID`). Version 3 is version 2 with
the name moved to the shared counted rule: bytes 0..7 and 32..95 keep their
meaning, so the pointing engine reads the same 96 bytes it always has.
Translation from version 2 moves the 24-byte name field's text (up to its
terminator) to bytes 96..127, writes its length to byte 8 and zeroes bytes
9..31. The surrounding profile domain envelope adds four
bytes. RGB remains in its own domain; RGB v4 holds 32 ID/HSV/locality rows,
one per slot whether or not the slot is stored here.

Retired version 1 was fixed: 776 bytes, the header `01 08 60 00 00 00 00 00`
then eight records in ID order `0..7`, disabled slots included. No schema-2
firmware with 32 slots accepts it. An importer converts it by dropping each
disabled record without a name and writing the version-2 header; record bytes
are unchanged.

| Record offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 1 | Slot ID (version 1: equal to record index) |
| 1 | 1 | Kind: disabled `0`, directional `1`, scrolling `2` |
| 2 | 1 | Pointer-layer policy: keep available `0`, prefer typing `1` |
| 3 | 1 | Directional axis: vertical `0`, horizontal `1`, dominant `2`, eight directions `3`. Scrolling axes: both `0`, horizontal only `1`, vertical only `2` |
| 4 | 2 | DPI: zero inherits; otherwise explicit value |
| 6 | 1 | Owned scrolling modifiers; directional modes use zero |
| 7 | 1 | Reserved, zero |
| 8 | 1 | Name length in bytes, 0..32 |
| 9 | 23 | Reserved, zero (version 2's name field) |
| 32, 34 | 2 each | Directional X/Y thresholds |
| 36, 40, 44, 48 | 4 each | Left/right/up/down tap records |
| 52, 58, 64 | 6 each | Mouse button 1/2/3 override records |
| 70, 72 | 2 each | Scroll H/V activation thresholds |
| 74, 76 | 2 each | Scroll H/V divisors |
| 78, 80, 82 | 2 each | Scroll output interval, buffer expiry, axis timeout in ms |
| 84, 85 | 1 each | Scroll start ratio numerator/denominator |
| 86, 87 | 1 each | Scroll sustain ratio numerator/denominator |
| 88 | 1 | Cross-axis decay divisor |
| 89 | 1 | Invert flags: bit 0 horizontal, bit 1 vertical |
| 90 | 6 | Reserved, zero |
| 96 | 32 | UTF-8 name, `length` bytes, the rest zero |

Eight-direction records (kind `1`, axis `3`, added 2026-09-23) reuse bytes
70..90, which every other directional record leaves zero:

| Record offset | Bytes | Meaning |
| --- | --- | --- |
| 70, 74, 78, 82 | 4 each | Up-left/up-right/down-left/down-right tap records |
| 86 | 1 | Empty diagonal: nearest straight direction `0`, both neighbours `1`, nothing `2` |
| 87 | 1 | Output: once per step `0`, once per movement `1` |
| 88 | 2 | Reserved, zero |

Firmware and apps that predate axis `3` reject it as an unknown axis policy,
so the domain version stays `1`: an older reader refuses such a profile rather
than misreading it. Byte 87 (added 2026-10-05) follows the same rule: older
readers reject a nonzero value as reserved.

Names follow the one rule every domain shares (D-F14): at most 32 bytes of
well-formed UTF-8 (no overlong or surrogate encodings, nothing past U+10FFFF),
no ASCII C0 controls or DEL, the length not cutting a sequence, and zero after
it. Configured modes require a
nonempty name. Disabled modes may retain their name; everything except ID and
name is zero. Disabled RGB rows are separately retained by the RGB domain.

Directional modes require positive thresholds for used axes and zero thresholds
and outputs for unused axes. All scrolling parameters are zero; eight-direction
modes instead carry their diagonals there, read both axes and so need both
thresholds.

Every directional mode runs one engine (2026-09-24); the axis policy says
which directions exist: vertical and horizontal two, dominant axis the four
straight ones, eight directions all eight. Motion toward a direction that does
not exist goes to the nearest one that does, which for a single axis is exactly
counting that axis. Motion is counted in units of threshold X × threshold Y,
so each axis moves in steps of its own threshold and a straight direction
accumulates sensor counts exactly; the backlog keeps at most the capped whole
steps and the part of one, and a still report drains it at the per-report
budget, both as the single-axis accumulator did. A smoothed heading (half of
each report) picks the nearest available direction and holds it until another
is nearer by 15°, which moves each boundary 7.5° past the held side. A move
against the held direction releases it at once, and after a 150 ms pause the
next move chooses afresh, keeping its progress only if it continues the same
way. Only progress along the held direction counts and sideways drift is
dropped; a diagonal step is one threshold step along the diagonal, and entering
a diagonal takes half a step more before its first tap, so a turn from one axis
to the other passes through it without firing.

Byte 87 is how often every directional mode sends: `0` once per threshold
step, as above, and `1` once per movement, so an imprecise movement does not
send a burst of the same shortcut. A movement starts with the first motion
after the mode starts or after a 150 ms pause. Its first step that sends
anything spends it in the direction that sent. From then on only a direction
pointing back against that one, more than 90° from it, may send, a whole
threshold of its own, and its sending spends the movement in turn; so a turn
sends nothing more, and a back-and-forth sends once per leg. Any other motion
is dropped rather than banked. A stray report against the held direction
releases the hold as usual but sends only if it alone carries a whole step
back, so a long move with one wobble still sends once. One step is one output under the
empty-direction policy below: a shortcut, or both neighbours as a pair; a step that sends nothing (a dead zone,
or both neighbours empty) leaves the movement unspent. The per-report budget
still applies, and a still report drains nothing once the movement is spent.

Byte 86 is what every directional mode does with motion toward a direction
that exists but has no shortcut: `0` its neighbours take its share, `1` both
compass neighbours 45° either side are tapped (a diagonal's two straight
directions, a straight direction's two diagonals; only eight directions has
them, so in the other modes `1` acts as `0`; the one the movement leans
toward is tapped first, and an empty neighbour sends nothing),
and `2` the motion is consumed. Records with axis 0–2 carry bytes 86 and 87
too; bytes 70..85 stay zero outside eight directions, and 88..89 are always
zero. A configured axis may have no output in either direction. Scrolling modes zero the directional
fields; their byte 3 says which axes they scroll (added 2026-10-05; older
readers reject a nonzero value). The engine runs as for both axes, choosing
and holding an axis per gesture with every threshold and ratio, but a gesture
held on an axis the mode does not scroll sends nothing: its steps are consumed
and still decay the other axis, so a sideways swipe in a vertical-only mode
neither scrolls nor leaks its drift into vertical scrolling, and nothing it
banked can scroll later. Their thresholds, divisors and axis timeout are positive;
expiry is at least the timeout. Interval zero means no output throttling.
Ratio numerators and denominators are positive, start and sustain are at least
1:1, and sustain cannot exceed start. Decay is at least two. Only two inversion
bits exist. Integer bounds follow their encoded widths.

These are format bounds, not proof that every value is supported by the sensor
or safe in the old handlers' arithmetic. Runtime integration must advertise
hardware DPI limits and use bounded arithmetic across the accepted range.
Preserve zero/inherit behavior deliberately: source-driver clamping of old
scroll DPI must be resolved during migration, not inferred from the fixture.

## Taps, modifiers and buttons

A tap is `keycode:u16, modifierPolicy:u8, mask:u8`. No output is four zero bytes.
Policies are inherit `0`, mask selected ambient modifiers `1`, and exact
shortcut `2`. Mask is nonzero only for policy 1. Modifier masks use the normal
eight left/right HID modifier bits. Exact emission temporarily isolates the
shortcut, then restores other owners' modifiers.

The allowlist is tied to the audited QMK 0.0.8 native ABI:

- Plain `0x0004..0x00c2`: keyboard `0x04..0xa4`, system `0xa5..0xa7`,
  consumer `0xa8..0xc2`.
- Modified keyboard taps: high byte `0x01..0x1f` with at least one of its low
  four modifier bits set, low byte `0x04..0xa4`.

Standalone modifiers, mouse actions, layer/PD controls, macro triggers and
custom/control keycodes are rejected. Modified consumer/system codes are not
part of this vocabulary. This prevents recursive activation and unbalanced
holds through motion outputs. The engine must still dispatch through existing
owned-output helpers and apply the existing four-tap/32-step work ceilings.

A button record is `kind:u8, modifiers:u8, tap:4 bytes`. Pass-through `0` and
consume `1` require five zero payload bytes. Tap `2` requires modifiers zero
and a nonempty valid tap. Hold-modifiers `3` requires a nonzero modifier mask
and zero tap bytes. Both engine families can use these button records.
A button press a mode consumes never reaches the button's own behavior, and
its release goes to the mode that took the press, even after another mode
replaced it; the release of a press the mode did not take stays with the
behavior. The key runtime owns that routing; see
[Runtime Flow](runtime-flow.md#key-press-flow).

## Validation and evidence

Firmware implements whole-payload and single-record cold validators in
`users/noah/lib/profile/schema/profile_pd_v1.c`; no cache, heap or pointing-path
decode is introduced. The app codec lives in
`core/schema/pd-mode-domain-v1.js` in the independent Ark repository. It rejects unknown
object fields, coercion, explicit null values, truncation, trailing bytes,
reserved data, incorrect ID order and noncanonical encodings.

Validation errors are `INVALID_ARGUMENT`, `INVALID_LENGTH`, `INVALID_HEADER`,
`INVALID_ID`, `RESERVED`, `INVALID_NAME`, `INVALID_POLICY`, `INVALID_ACTION`,
and `INVALID_PARAMETER`, with a byte offset. Record-validator offsets are
record-relative; whole-domain offsets are payload-relative. Object-encoding
errors without a corresponding byte position use zero. C success is `OK`.

Validation errors add `NONCANONICAL` for version 2's disabled record without a
name; the incremental validator and the cache read the header, then one record
at a time.

`tests/fixtures/pd_mode_domain_v1.json` records six repository presets and two
disabled slots in version 1, with independently packed golden bytes.
`tests/fixtures/pd_mode_domain_v3.json` holds version-3 vectors: an empty
domain, the presets (the firmware's compiled domain, checked against the
golden compiled profile), a gap with a named disabled slot 31, all 32 slots
(4,104 bytes), a 32-byte multibyte name, and rejections with their codes and
payload offsets (noncanonical record, unordered, repeated and out-of-range IDs,
count/length mismatches, header and reserved bytes including version 2's name
field, a version-2 payload, and over-long, malformed, control-character,
cut-sequence and unpadded names). Both are test
evidence, **never a fallback migration source**. The frozen C/JS differential
corpus (8,635 version-1 cases, including byte mutations, all truncated lengths
and Unicode) still checks every record rule by extracting the frozen records,
moving each one's 24-byte name field verbatim into a version-3 record (its
length the first zero, or past the limit when there is none) and using the
current record validator, with every verdict unchanged; the legacy envelope validator is removed;
the C runners also use ASan/UBSan. Tests freeze existing native action values
and prove that current readers reject legacy envelopes.

## Storage geometry

Current firmware uses one geometry:

| Range | Bytes | Owner |
| --- | ---: | --- |
| `0x00000..0x08fff` | 36,864 | QMK/VIA allocation |
| `0x09000..0x15fff` | 53,248 | Profile slot A, 32-byte header + 53,216-byte payload |
| `0x16000..0x22fff` | 53,248 | Profile slot B, same layout |

Logical EEPROM is 143,360 bytes; RP2040 wear-level backing is 286,720 bytes
(D-F14). Older layouts are not interpreted in place. Keep a complete backup and
its matching old firmware pair before an upgrade; restore a client-translated
current profile through logical Apply after both halves run compatible firmware.
Physical geometry migration acceptance remains outstanding. Resource accounting
belongs to [memory budgets](memory-budgets.md).

## Integration and identity gates

Schema 3.0 includes domain `0x50`, advertised by domain-mask bit 4. Format 5
(`NT`) stores five domain bits, origin in bit 5, flags in bit 6 and reserved
bit 7. It binds schema 3.0 and nonzero VIA identity. `NP`, `NQ`, `NR` and `NS`
headers are rejected. Header CRC, marker-last publication and bounded I/O remain unchanged.

Synchronous validation, bounded boot scanning and commit shape validation
share the current version rules in `profile_versions.h`: RGB v4, key behaviors
v2, combos v3, settings v6 and sparse PD v3. Blob magic remains `NLP1`; schema
bytes must be 3.0. Domain bodies also require semantic validation. Store tests
cover all mask/origin/flag combinations, both boot paths, old-header rejection,
reserved bits with repaired CRCs, incompatible versions, one-byte commit steps,
durable prepare/abort and interrupted writes. These do not prove physical flash
acceptance.

The current action-ABI digest is the one capability page 0 reports; it covers
the layer count, so the sixteen-layer bank changed it (D-F14). Older action
vocabularies require client translation before a current-format Apply.
Candidate metadata, owner, peer storage and background stale-peer repair all
use format 5 and require a
correlated VIA bind before PREPARE_BEGIN. Retry preserves the bind for that exact
generation/digest. Incompatible peers cannot Apply.

GET subcommand 9 and feature bit 13 are retired. Firmware neither advertises nor
serves legacy source definitions, and has no version-1 PD envelope validator.
Current sparse PD and its record rules remain the readback/validation contract.

The effective PD cache reads only on initialization/publication, in chunks of
at most 20 bytes: the header, then each stored record into its slot's row,
validating each; omitted slots are materialized disabled with an empty name,
so the cache always holds 32 rows (3,072 bytes) before becoming ready. A
failed read or validation leaves it unavailable; the owner fails closed and
status does not advertise an active compiled fallback. Motion performs no
profile storage reads. Activation waits for held/locked modes and intercepted
button releases, using the existing persistent-intent reason bit.

The compiled reader must service PD-only reads directly from the final PD
payload, whose compiled size the authored slots fix, using the same encoder and validators as full serialization. A
20-byte read limit alone does not bound CPU work: replaying the preceding RGB
and cubic behavior canonicalization for all 39 cache reads caused 1,443 row
sorts in one startup scan on the authored profile. The compiled-cache integration
test requires zero behavior-row sorts during warming and checks every PD byte
boundary against the full golden serialization. The fix does not rely on the
watchdog timeout.

Native IDs are fixed by executable assertions:

The canonical keycode vocabulary is `PD_SLOT_n` for a momentary hold and
`PD_SLOT_n_LOCK` for its persistent toggle, for `n = 0..31`. Each userspace
family owns one fixed, aligned block (`users/noah/noah_keymap_ids.h`), reserved
beyond what is supported, so adding pointing slots or layers renumbers nothing:
slot `n` holds at `0x7e80 + n` and toggles at `0x7ea0 + n`. A slot keycode
selects its numbered slot; the mode's name and behavior come from the active
profile. Former names such as `DRAGSCROLL` and `VOLUME_MODE` identify factory
presets only and remain compatibility aliases for older portable profile
expressions. They are not firmware keycode symbols.

| Native action | Block | Supported |
| --- | --- | --- |
| Retired custom keys (before D-F14) | `0x7e40..0x7e7f` | — (inert) |
| PD hold | `0x7e80..0x7e9f` | 32, the whole block |
| PD lock | `0x7ea0..0x7ebf` | 32, the whole block |
| Layer lock | `0x7ec0..0x7edf` | 16 (`0x7ec0..0x7ecf`) |
| Unassigned | `0x7ee0..0x7eff` | — |
| Custom key 0–127 | `0x7f00..0x7f7f` | 128 (D-F14) |
| Unassigned | `0x7f80..0x7fff` | — |

Profiles store pointing and layer-lock actions by slot (action kinds 2–5), so
the blocks change only raw keycodes: VIA layouts and custom keys. The blocks
came with a new action ABI digest and VIA sync metadata schema 3, so firmware
never adopts an older stored profile or layout bank, and the app translates an
older backup key by key on import (portable-profile-v1.md). Before the blocks,
holds 0–5 were `0x7e50 + n`, locks 0–5 `0x7e56 + n`, slots 6 and 7
`0x7ef0..0x7ef3` interleaved, layer locks `0x7e5c + n`, the keymap's own keys
followed from `0x7e64`, and the retired user macros held `0x7e40..0x7e4f`;
historical five-layer profiles used a custom-action base of `0x7e61`.

Old backups lack compiled thresholds and modifier/scroll policies. Action ABI
alone cannot recover that tuning. The retired bridge once supplied the source
domain; this repository no longer builds it. Only backups that already carry
verified source PD data can migrate through the current portable importer.
Hardware acceptance remains distinct from codec and runtime parity tests.

## Compatibility

| Combination | Required behavior |
| --- | --- |
| New app, old firmware | Existing supported editing continues; slot editing is visibly unavailable |
| Old app, new firmware or profile | Incompatibility handling refuses destructive writes |
| New firmware, old stored record | Recognized version or identity migration, or explicit recovery; never silent default replacement |
| New app, old export | Migrates only with verified source PD data; otherwise keeps the file and names what is missing |
| New app and firmware, new export | Exact 32-slot round trip including named disabled slots, RGB and references |
| 32-slot firmware, eight-slot backup (`0x1d3fcacc`) | Imported through the key-by-key action translation plus PD v1 → v2 and RGB v2 → v3 ([portable profiles](portable-profile-v1.md)) |
| Mismatched halves or unknown ABI | Apply refused before mutation; draft and recovery file kept |

Never restore a 32-slot document into eight-slot firmware by dropping slots;
downgrade restores the old backup.

## Hardware acceptance

Still to record on the physical keyboard, with firmware identity, app version,
profile digest and both-half state for each case:

1. The six migrated presets keep their movement feel, buttons, modifiers,
   activation gestures, pointer-layer behavior and RGB locality.
2. Empty slots can be configured, bound, locked, unlocked and coloured;
   slot 31 proves the upper-bound identity path (the mask's top bit and the
   last keycode of each block).
3. Complete backup and restore survive reboot, including restore onto firmware
   with empty authored behaviors and combos.
4. Held or locked modes make Apply wait with a useful reason; no modifier or
   mouse button stays stuck.
5. Power or link interruption on either side of the logical decision recovers
   the old or new complete generation, with no mode, RGB or VIA mixture.
6. Mixed-version halves, stale drafts and old clients refuse mutation without
   losing the prior configuration.
7. Pointing cadence, save time and per-half memory and stack evidence.

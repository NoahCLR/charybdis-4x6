# PD-mode domain v1

Implemented in schema-2 builds: domain codec, incremental validation, immutable
effective cache, shared engines, persistence/split writers, portable profiles
and live editor. Side-specific builds own committed profiles; the generic build
loads the eight compiled factory slots without a profile owner.
Physical upgrade and release acceptance remain pending; see
[Hardware acceptance](#hardware-acceptance).

## Scope

The keyboard owns eight pointing-mode slots, stable IDs `0..7` shown as slots
1–8, and eight matching PD RGB rows keyed by the same IDs. The app reads, edits
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
Redo; slot 7 remains disabled. Slot contents are profile data, not part of the
keycode allocation.

| Slot | Name | Engine | Behavior preserved |
| --- | --- | --- | --- |
| 0 | Dragscroll | Scrolling | Local scroll algorithm, inversion, axis hysteresis, timing, CPI, lock-owned auto-mouse |
| 1 | Volume | Directional, vertical | Volume down/up, threshold 60, inherited DPI, pointer-layer anchoring |
| 2 | Brightness | Directional, vertical | Brightness down/up, threshold 60, inherited DPI, pointer-layer anchoring |
| 3 | Zoom | Directional, vertical | Cmd-minus/Cmd-equals, threshold 80, DPI 400 |
| 4 | Arrow | Directional, dominant axis | Arrows, X/Y thresholds 40/50, DPI 400, vertical Alt masking, BTN1 hold right Shift, BTN2 Cmd+C, BTN3 Cmd+V, typing-layer preference |
| 5 | Pinch | Scrolling | Dragscroll's tuning plus owned left Cmd with managed-only modifier masking |
| 6, 7 | Empty | Disabled | Inert actions; retained, editable RGB row |

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
algorithm. The mode registry retains the eight stable native hold/lock
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

Domain ID `0x50`, version `1`. All multibyte integers are unsigned,
little-endian. The payload is exactly 776 bytes: the eight-byte header
`01 08 60 00 00 00 00 00`, then eight 96-byte records in ID order `0..7`.
The surrounding profile domain envelope adds four bytes. RGB remains in its
own domain; schema 2 contains eight corresponding ID/HSV/locality rows.

| Record offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 1 | Slot ID, equal to record index |
| 1 | 1 | Kind: disabled `0`, directional `1`, scrolling `2` |
| 2 | 1 | Pointer-layer policy: keep available `0`, prefer typing `1` |
| 3 | 1 | Directional axis: vertical `0`, horizontal `1`, dominant `2`, eight directions `3`; scrolling uses zero |
| 4 | 2 | DPI: zero inherits; otherwise explicit value |
| 6 | 1 | Owned scrolling modifiers; directional modes use zero |
| 7 | 1 | Reserved, zero |
| 8 | 24 | UTF-8 name, NUL terminated and zero padded |
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

Eight-direction records (kind `1`, axis `3`, added 2026-09-23) reuse bytes
70..90, which every other directional record leaves zero:

| Record offset | Bytes | Meaning |
| --- | --- | --- |
| 70, 74, 78, 82 | 4 each | Up-left/up-right/down-left/down-right tap records |
| 86 | 1 | Empty diagonal: nearest straight direction `0`, both neighbours `1`, nothing `2` |
| 87 | 3 | Reserved, zero |

Firmware and apps that predate axis `3` reject it as an unknown axis policy,
so the domain version stays `1`: an older reader refuses such a profile rather
than misreading it.

Names contain at most 23 UTF-8 bytes, no embedded NUL, ASCII C0 controls or DEL,
and no malformed, overlong or surrogate encodings. Configured modes require a
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

Byte 86 is what every directional mode does with motion toward a direction
that exists but has no shortcut: `0` its neighbours take its share, `1` both
compass neighbours 45° either side are tapped (a diagonal's two straight
directions, a straight direction's two diagonals; only eight directions has
them, so in the other modes `1` acts as `0`; the one the movement leans
toward is tapped first, and an empty neighbour sends nothing),
and `2` the motion is consumed. Records with axis 0–2 carry byte 86 too; bytes
70..85 stay zero outside eight directions, and 87..89 are always zero. A configured
axis may have no output in either direction. Scrolling modes zero the axis and
directional fields. Their thresholds, divisors and axis timeout are positive;
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

`tests/fixtures/pd_mode_domain_v1.json` records six repository presets and two
disabled slots, with independently packed golden bytes. It is test evidence,
**never a fallback migration source**. C/JS differential validation covers
7,765 cases, including byte mutations, all truncated lengths and Unicode;
the C runner also uses ASan/UBSan. Tests freeze existing native action values
and prove that legacy schema-1 readers/writers reject domain `0x50`.

## Capacity gate

`measurePdProfileUpgrade` validates a complete current portable profile and
explicitly supplied slot definitions before measuring this candidate upgrade.
It neither supplies missing legacy settings nor produces a writable migrated
document. With unchanged settings length, growth is 776 payload + 4 envelope
+ 10 for the two additional RGB rows = **790 bytes**.

| Complete fixture | Old bytes | Candidate bytes | Fits 4,064? |
| --- | ---: | ---: | --- |
| Existing portable test profile | 1,488 | 2,278 | Yes |
| 32 combos, 918 bytes of IR macro payload | 3,274 | 4,064 | Exactly |
| Same, 919 bytes of IR macro payload | 3,275 | 4,065 | No |
| Same, full 1,024-byte IR macro payload | 3,380 | 4,170 | No, 106 bytes over |

These are valid complete profiles, not sums of independent theoretical maxima.
Thus unchanged geometry plus this encoding **cannot migrate every valid old
profile**. Compacting only this new domain cannot guarantee room in an already
full old payload. The selected path is a deliberate storage migration, preserving
the whole VIA bank and providing two 5 KiB profile slots:

| Schema-2 range | Bytes | Owner |
| --- | ---: | --- |
| `0x0000..0x1fff` | 8,192 | Existing QMK/VIA allocation, unchanged |
| `0x2000..0x33ff` | 5,120 | Profile slot A, 32-byte header + 5,088-byte payload |
| `0x3400..0x47ff` | 5,120 | Profile slot B, same layout |

Logical EEPROM becomes 18,432 bytes; RP2040 wear-level backing becomes 36,864
bytes. The fork requires backing at least twice logical, an integral logical
multiple, and 4 KiB flash erase sectors: these sizes meet all three constraints.
All addresses still fit uint16 and the existing 25-byte readback pages fit the
one-byte page index. No sibling source change is required for those limits.

The maximum old payload, 4,064 bytes, plus 790 is 4,854 bytes, leaving 234 bytes
in the schema-2 slot. This proves aggregate capacity for every valid old payload
under the specified unchanged-length migration, without truncating macros,
combos, names or slots. Further migration growth must re-run this proof.
`measurePdProfileUpgrade` reports both current capacity and `plannedStorage`,
leaving its input intact. Negotiated schema-1 capacity stays 4,064 bytes;
schema-2 devices accept up to 5,088. `upgradePdSnapshot` materializes the
validated migrated document from device-reported source definitions.

PD-enabled firmware selects the expanded geometry. The larger wear-level cache
adds 2,048 bytes in SRAM0–3 per half; fresh linked accounting and the deliberate
feature-specific policy are in [memory budgets](memory-budgets.md). Physical
high-water acceptance remains outstanding. The backing region's physical base
and write-log layout change, so interpreting old flash in place is unsafe.
Require a verified complete old backup, source PD readback, saved old firmware
pair, and materialized migrated document **before flashing either half**. Restore
through the new logical Apply after both halves have compatible firmware.
Downgrade likewise restores the old backup; never reinterpret the new bank.

## Integration and identity gates

Schema 2 adds domain `0x50`, advertised by supported-domain bit 4.
The legacy logical storage header format 2 (`NQ`) cannot encode that mask:
its identity byte packs domain bits 0–3, origin in bit 4 and flags in bit 5.
Naively extending the mask corrupts origin. PD-enabled owners select
format 3 (`NR`) with the same offsets, five domain bits, origin in bit 5,
flags in bit 6, and reserved bit 7. It binds schema **2.0**; `NQ` binds schema
**1.0**. Neither takes its schema identity from the reader's expectation.
Header CRC, VIA binding, prepared/committed markers and bounded I/O are unchanged.
Admission rejects incompatible schema/format pairs before invalidating a slot.

Synchronous validation, bounded boot scanning and commit shape validation all
use the same domain-version rules: `NR` accepts RGB v2, key behaviors v1,
combos v1, settings v2 and PD v1; `NP`/`NQ` retain the old four-domain v1 rules.
The blob framing magic remains `NLP1`; its explicit schema bytes distinguish
schema 2.0. Domain bodies still require semantic validation upstream; store
shape checks alone do not make a candidate publishable.

Store tests cover all 128 mask/origin/flag combinations, both validation paths,
reserved bits with repaired CRCs, domain-mask disagreement, wrong domain
versions, rejection by the schema-1 reader, one-byte commit steps, durable
prepare/abort, and every write cut with all partial lengths 0..32. Reboot must
select one whole generation with its matching origin, flags and VIA binding.
This tests the header protocol in the existing fake EEPROM geometry; it does
not prove physical flash-geometry migration.

The integrated contract is blob schema 2.0, RGB v2 with eight rows, settings v2
with retired DPI scalars 10–14 encoded as zero, portable document v2 and PD v1.
The action ABI is `0x61072732`, generated from the compiled vocabulary. The
legacy eight-layer ABI remains `0xeb80829c`; the five-layer bridge remains
`0xdcb00959`. The HID envelope stays version 1. Candidate metadata uses format 3
with VIA generation/digest binding; owner, peer store and split admission all
select that same format. Incompatible peers cannot Apply.

The settings body's first byte must use the same selected settings version as
its enclosing domain header. Complete app-generated schema-2 profiles are tested
through the compiled firmware compatibility and incremental validator, including
the settings domain; compiled factory defaults alone do not exercise that domain.
Settings-v2 tests reject legacy body headers and nonzero retired DPI scalars.

Legacy GET subcommand 9, advertised by feature bit 13, returns a versioned,
CRC/FNV-checked 776-byte source domain in 25-byte pages. These are source
compiled definitions, independent of current DPI edits. Legacy live DPI comes
from settings during upgrade (dragscroll zero normalizes to 100). Complete
schema-1 documents may carry this evidence as `pdModeSource`; upgrade refuses
missing or incompatible evidence. The app saves both original and migrated
files and verifies their contents before recommending the geometry upgrade.

The effective PD cache reads only on initialization/publication, in chunks of
at most 20 bytes, then validates all eight records before becoming ready. A
failed read or validation leaves it unavailable; the owner fails closed and
status does not advertise an active compiled fallback. Motion performs no
profile storage reads. Activation waits for held/locked modes and intercepted
button releases, using the existing persistent-intent reason bit.

The compiled reader must service PD-only reads directly from the final fixed-size
PD payload, using the same encoder and validators as full serialization. A
20-byte read limit alone does not bound CPU work: replaying the preceding RGB
and cubic behavior canonicalization for all 39 cache reads caused 1,443 row
sorts in one startup scan on the authored profile. The compiled-cache integration
test requires zero behavior-row sorts during warming and checks every PD byte
boundary against the full golden serialization. No watchdog timeout is relaxed.

Native IDs are fixed by executable assertions:

The canonical keycode vocabulary is `PD_SLOT_n` for a momentary hold and
`PD_SLOT_n_LOCK` for its persistent toggle, for `n = 0..7`. Each userspace
family owns one fixed, aligned block (`users/noah/noah_keymap_ids.h`), reserved
beyond what is supported, so adding pointing slots or layers renumbers nothing:
slot `n` holds at `0x7e80 + n` and toggles at `0x7ea0 + n`. A slot keycode
selects its numbered slot; the mode's name and behavior come from the active
profile. Former names such as `DRAGSCROLL` and `VOLUME_MODE` identify factory
presets only and remain compatibility aliases for older portable profile
expressions. They are not firmware keycode symbols.

| Native action | Block | Supported |
| --- | --- | --- |
| Custom key 0–63 | `0x7e40..0x7e7f` | 64 |
| PD hold | `0x7e80..0x7e9f` | 8 (`0x7e80..0x7e87`) |
| PD lock | `0x7ea0..0x7ebf` | 8 (`0x7ea0..0x7ea7`) |
| Layer lock | `0x7ec0..0x7edf` | 8 (`0x7ec0..0x7ec7`) |
| Unassigned | `0x7ee0..0x7fff` | — |

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
| New app and firmware, new export | Exact eight-slot round trip including disabled slots, RGB and references |
| Mismatched halves or unknown ABI | Apply refused before mutation; draft and recovery file kept |

Never restore an eight-slot document into old firmware by dropping slots;
downgrade restores the old backup.

## Hardware acceptance

Still to record on the physical keyboard, with firmware identity, app version,
profile digest and both-half state for each case:

1. The six migrated presets keep their movement feel, buttons, modifiers,
   activation gestures, pointer-layer behavior and RGB locality.
2. Both empty slots can be configured, bound, locked, unlocked and coloured;
   slot 7 proves the upper-bound identity path.
3. Complete backup and restore survive reboot, including restore onto firmware
   with empty authored behaviors and combos.
4. Held or locked modes make Apply wait with a useful reason; no modifier or
   mouse button stays stuck.
5. Power or link interruption on either side of the logical decision recovers
   the old or new complete generation, with no mode, RGB or VIA mixture.
6. Mixed-version halves, stale drafts and old clients refuse mutation without
   losing the prior configuration.
7. Pointing cadence, save time and per-half memory and stack evidence.

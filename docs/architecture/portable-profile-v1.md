# Portable keyboard profile v1

> Current firmware accepts only the formats it writes (D-F10, D-F14): profile
> schema 4.0; RGB v4, key behaviors v2, combos v3, settings v8, sparse PD v3, keymap v1 and macros v1;
> a slot-sized payload (see the storage contract); and logical store format 6 (D-F28). The profile
> is the only store (D-F31). Profile Wire is 2.0 and split
> framing version 4, with 32-bit profile lengths and offsets throughout.
> Older profile/store formats and the legacy GET 9 source page are rejected.
> A backup in an earlier format is refused, not translated.

A profile export is the effective configuration read from the keyboard. Flashed
and committed data have the same representation. An absent live domain is
materialized from device readback before export; it never means “use the
destination firmware's defaults”. Transient presses, running macros, pointer
motion and temporary layer activation are not configuration.

## Keymap domain `0x60`, version 1

The payload is exactly 1,920 bytes: sixteen layers of sixty matrix positions,
row first, then column, each a little-endian uint16 native QMK keycode. The
four unwired matrix positions are included. There is no inner header.
A candidate uses the firmware's ordinary key placement rules: empty and
transparent positions are valid; unowned layer actions, unassigned userspace
codes and layer targets outside 0–15 (including `TT(16)` and `OSL(16)`) fail.
Validation reads at most twenty bytes per step and reports the rejected layer
and matrix position. Existing domain versions are unchanged.

`keymap_key_to_keycode` is the runtime authority for QMK, combo-origin
reconstruction, handled-key transparency and RGB layer coverage. A saved
domain borrows the active generation's reader and reads two bytes per lookup;
compiled or omitted domains read the authored const matrix directly. Hot
lookups never serialize the compiled profile. A publication invalidates the
RGB coverage map before its next render.

## Macro domain `0x70`, version 1

The payload retains the existing bank encoding: 128 consecutive zero-terminated
macro streams, at most 34,903 bytes in total, with optional unused capacity after
the last macro and a final zero validity byte. Unused capacity may retain stale
bytes after shortening macros, just as in the existing bank. Empty slots contain only their terminator. Text, UTF-8,
tap/down/up, decimal delays and protection prefixes retain the encoding read
by `macro_payload_decode_qmk.c`. The incremental validator checks framing,
canonical UTF-8 and the protection prefix; playback retains its existing
whole-stream preflight, balanced ownership and bounded IR windows. Invalid
playback never emits a partial stream.

The active saved bank is read through the profile reader. Compiled fallback
compiles the authored payloads in `keymap.c` with the same compiler. A publication invalidates the slot metadata and lookup cache;
the profile's safe boundary excludes a running macro, so a refill cannot span
generations. Neither a key lookup nor a refill materializes the whole bank.

VIA is not supported (D-F31): the firmware keeps no other copy of the keymap
or macros. The portable JSON envelope below describes the client before it
moved to the new base; clients of the new base carry the two domains in the
canonical NLP1 profile.

## Portable document

The UTF-8 JSON file has exactly these fields. Unknown fields and unsupported
versions fail validation. Files are limited to 100,000 bytes.

| Field | Meaning |
| --- | --- |
| `format` | `charybdis-profile` |
| `version` | `1` |
| `keyboard` | `charybdis-4x6` |
| `actionAbiDigest` | Nonzero uint32 engine vocabulary identity |
| `layers` | Sixteen arrays of 60 uint16 native keycodes, in matrix row/column order, including unused physical matrix positions |
| `profile` | Canonical base64 NLP1 payload, at most 4,064 bytes; domains `0x10`, `0x20`, `0x30`, `0x40`, in that order |
| `macros` | Exactly 128 canonical base64 macro streams, excluding their zero terminators |

A document is restorable only onto firmware with the same action-ABI digest,
profile schema and domain versions (the summary above). A document exported
from earlier firmware is refused, never translated: neither the firmware nor
its client keeps a path from an older vocabulary or format.

RGB and behaviour domains come from the effective committed domain or the
keyboard's compiled readback. Combo rows come from effective native readback;
the exported override is explicit even when empty. Custom trigger/release/
repress hooks, strict/disabled timing and fixed combo-reference builds are not
portable in v1 and prevent export. This standard board has no encoder or
user-selectable layout options; EEPROM padding and validity metadata are
not profile content. Executable hooks, hardware geometry, engine inclusion and
safety ceilings remain firmware capabilities.

Macro streams support canonical UTF-8 text, tap/down/up instructions and decimal delay
instructions. Held keys must balance. The bank is reconstructed with zero
padding and a final zero validity byte. Both macro banks are independent:
64 VIA slots and 16 user macro slots retain their existing key identities.

See [Unicode macro playback](runtime-flow.md) for encoding, compiled size, host setup and cancellation.

The reconstructed bank is the document's, not necessarily the keyboard's. A
valid bank may hold nonzero bytes after its 64th terminator, for instance after
a writer shortened the macros without clearing the rest; the document cannot
carry them and export stays canonical. Differential transfer therefore never
compares against a bank rebuilt from a document. A capture keeps the exact
layout and macro bytes it read, beside the storage identity it verified, and an
Apply compares staging and local roll-forward against those. Optional
macro read maps (GET `0x0c`, retired with VIA in D-F31) let a capture
reconstruct firmware-confirmed zero blocks while reading every nonzero block
and verifying the same full-store identity. Its result keeps
the target bank, which it has just proved both halves hold. A reviewed snapshot
without those bytes is not reused: Apply reads the keyboard again. The
interrupted-capture path already carries the raw bank it read.

## Settings domain `0x40`, version 8

Capability domain-mask bit 3 advertises this domain. An ordinary profile may
omit it; a complete portable file includes it. Firmware accepts only version
8, with the envelope version equal to the payload's first byte, and refuses
earlier versions.

All multibyte fields are little-endian:

| Offset | Bytes | Contents |
| ---: | ---: | --- |
| 0 | 8 | `[8, 16, 31, 128, 128, flags, 128, 0]`: version, layer count, scalar count, macro-name count, custom-key-name count, flags, custom-key-shortcut count, a reserved zero |
| 8 | 124 | 31 uint32 scalar values |
| 132 | 272 | 16 layer records of 17 bytes, layer 0 first: byte 0 the layer's combo reference layer (below 16); bytes 1–8 its behaviour-bypass placement bitmap; bytes 9–16 its combo-exclusion placement bitmap |
| 404 | 256 | 128 custom-key shortcuts of 2 bytes, custom key 0 first (D-F26): byte 0 the modifier mask in HID report order (bit 0 Left Ctrl, 1 Left Shift, 2 Left Alt, 3 Left GUI, 4 Right Ctrl, 5 Right Shift, 6 Right Alt, 7 Right GUI; any combination); byte 1 the key, `0x00` for none, a basic key `0x04..0xa4` or a mouse button `0xd1..0xd8`. Both zero means no shortcut |
| 660 | variable | 273 counted names: 16 layer names, then 128 macro names, then 128 custom-key names, then the keyboard name; each a uint8 length `0..32`, then that many bytes of UTF-8 |

The minimum is 933 bytes (every name empty) and the maximum 9,669 bytes (every
name 32 bytes). The complete profile shares the slot payload ceiling and
rejects excess without trimming. Names are profile data for the client. Macro
content belongs to profile domain `0x70`; a custom key's shortcut is its own
action and its behaviour row works on top of it (D-F26). Firmware does not
check that a step naming a custom key has a shortcut; one without sends
nothing.
Validation reads at most 20 settings bytes per step, in one read. Retired per-mode DPI
scalars 10–14 must be zero; mode tuning is in domain `0x50`.

The keyboard name is the keyboard's own name (D-F25). The firmware serves it
as the USB product string, so hosts, browser device choosers and clients list
the keyboard under it. Empty, the compiled default, means QMK's built-in
`keyboard_name`. [Runtime flow](runtime-flow.md#usb-product-name) defines when
a new name reaches the host.

Settings header byte 5 bit 0 bypasses shared behaviours for every combo output
while physical-key behaviours remain eligible. Other header flag bits are
zero. Zero means **Combo
outputs use behaviours** is on, preserving existing profiles and compiled
defaults. The corresponding per-combo switch is combo v3 row flag bit 4;
[participation policy](participation-policy.md#combo-output-behaviour-permission)
defines their vetoes, origin context and release pairing. An export preserves them. Neither extension changes domain versions, offsets or the action ABI.

Every name in this domain follows one rule: at most 32 encoded bytes of
well-formed UTF-8 (no overlong forms, no surrogates, nothing past U+10FFFF), no
C0 control character (`0x00`–`0x1f`) and no DEL (`0x7f`), and no multibyte
sequence cut by the length. Limits count bytes, not characters. Names are
counted, not NUL-terminated. Pointing-slot names in domain `0x50` keep their
own rule.

A placement bitmap's bit `n` (byte `n / 8`, bit `n % 8`) is matrix position
`n = row × MATRIX_COLS + column`. A bitmap holds up to 64 positions; this
keyboard has 60 (10 × 6), and bits at or past the advertised position count
(capability page 2, byte 9) must be zero. A set bypass bit sends the key at
that layer and position to its normal action instead of its shared behaviour;
a set exclusion bit keeps it out of combos. The
[participation policy](participation-policy.md) defines both.

| Scalar ID | Meaning |
| ---: | --- |
| 0–3 | Tapping, tap/hold, long-hold and multi-tap timing (ms) |
| 4–7 | Auto-mouse enabled, layer, timeout (ms), debounce (ms) |
| 8–9 | Auto-sniping enabled and layer |
| 10–14 | Retired per-mode DPI fields, must be zero |
| 15–16 | Key-feedback flash half-period and auto-mouse fade dead time (ms) |
| 17 | RGB idle timeout (ms); zero disables it |
| 18–19 | Actual normal and sniping DPI |
| 20 | Combos enabled |
| 21 | Four bytes: RGB enabled, effect, speed, flags |
| 22 | Three bytes: RGB hue, saturation, actual brightness; high byte zero |
| 23 | Persistent default-layer bitmask |
| 24 | QMK keymap options |
| 25–26 | Auto-mouse activation delay (ms) and movement threshold |
| 27 | Host settings: bits 0..1 select Auto (0), macOS (1), Windows (2), Linux (3); bit 8 enables Unicode playback; bits 16..23 name the [host layout](host-layouts-v1.md) (0 US) and bit 24 marks a macOS ISO keyboard; other bits zero. Previously reserved zero after version 5 combo references moved to layer records |
| 28 | Behaviour master enable |
| 29 | Layer behaviours mask, one bit per layer |
| 30 | Layer combos mask, one bit per layer |

Boolean values are 0/1. The auto-mouse layer (5) and auto-sniping layer (9)
are below 16. Timing/DPI policy scalars are uint16 except the RGB idle timeout
(maximum one day). Debounce is uint8. Flash half-period must be nonzero and
fade dead time less than auto-mouse timeout. Normal DPI is 400–3,400 in steps
of 200; sniping DPI is 100–400 in steps of 100. The default-layer mask (23) is
nonzero with bits only in 0–15. The layer masks (29, 30) reject bits 16 and
above; an empty mask is allowed.

Compiled defaults set scalar 28 to 1 and scalars 29 and 30 to `0xffff`; every
layer refers combos to itself, every bitmap is zero, layers 8–15 are
unnamed and so is the keyboard. So a profile that never sets the participation controls behaves as
version 5 did.

At publication the settings invalidator copies the bounded domain into a cold
runtime cache using reads of at most 20 bytes. Key, RGB, pointer and macro
execution use the cache, without EEPROM reads. The settings module owns immutable factory scalars and authored names, also
included in the complete compiled profile. Compat injects the QMK apply hook;
the settings runtime has no reverse call into compat. Explicit stored settings
apply at the safe boundary. A missing settings domain warms the compiled
fallback cache without applying factory values to native owners.
Native RGB/DPI/default-layer/keymap settings retain their EEPROM ownership:
boot preserves newer native values, and export reads their current values.
Ordinary domain edits refresh these values before writing an existing settings
domain, and bind that read before and after acquiring the candidate lease.

## Readback commands

These use the existing Profile Wire custom GET envelope and request ID.

GET value `0x07` reads the effective settings domain with its scalars overlaid
by their live QMK owners. With no profile settings live it returns the current
version named by the keymap: `layer_names[]`, the name in each `VIA_MACROS`
row and in each `CUSTOM_KEYS` row. The request takes a 16-bit page, its low
byte at request byte 4 and its high byte at byte 5. Page 0's
12-byte payload is version `1`, chunk size `25`, uint16 length, CRC32, and
FNV-1a digest of the bytes as they are at that moment. Pages 1 onward return
successive 25-byte chunks with an exact short final chunk, streamed from the
live domain rather than a snapshot; the largest domain needs 376 chunk pages.
The host reads page 0 again after the chunks and requires identity and both
checksums to match, so a change during the read fails it. The complete export also
rechecks settings after reading the rest of the profile, so changing RGB or
DPI during a backup invalidates the read.

GET value `0x08`, page 0 reported the VIA store's sync state until D-F31; it
is now an unknown page (status `2`). The profile owner's status is the only
storage status.

GET value `0x08`, page 1 is optional editor metadata: exactly two bytes,
version `1` and the device's actual RGB maximum brightness (uint8, including
zero). The firmware reports `RGB_MATRIX_MAXIMUM_BRIGHTNESS`; it does not
change LED state. A failed reply is an error. With the reported limit, Defaults and full-profile restore reject an excessive
brightness before writing. This metadata is not configuration, is not exported,
and does not alter the portable profile fingerprint or payload schema.

GET value `0x08`, page 2 adds optional native editor capabilities. Its nine-byte
metadata payload is:

| Offset | Value |
| --- | --- |
| 0 | Version `1` |
| 1 | Chunk size `25` |
| 2–3 | Body length, uint16 little-endian |
| 4 | Effect count; effect IDs are 1 through this count |
| 5 | Semantic key-option count, exactly `13` |
| 6–7 | Supported semantic key-option bitmask, uint16 little-endian |
| 8 | OR of this keyboard's LED classification flags |

Pages 3 onward return successive body chunks, with an exact short final chunk.
The body is 13 native uint16 little-endian key-option masks followed by one
64-byte, NUL-terminated, zero-padded ASCII effect token per effect. Names follow
`[A-Z][A-Z0-9_]*`; names, masks and support flags come from the running firmware's
QMK build. The masks are distinct, nonzero single bits, derived from the native
keymap union rather than assuming its bitfield layout. Semantic option IDs are:

| ID | Key option |
| --- | --- |
| 0 | Swap Control and Caps Lock |
| 1 | Caps Lock becomes Control |
| 2 | Swap left Alt and GUI |
| 3 | Swap right Alt and GUI |
| 4 | Disable GUI keys |
| 5 | Swap Grave and Escape |
| 6 | Swap Backslash and Backspace |
| 7 | NKRO |
| 8 | Swap left Control and GUI |
| 9 | Swap right Control and GUI |
| 10 | Enable one-shot keys |
| 11 | Swap Escape and Caps Lock |
| 12 | Autocorrect |

The host bounds total body length to pages 3–255 and rechecks page 2 after
reading. Only a canonical correlated status `2` with empty payload on page 2
means unavailable metadata. Unsupported bits, malformed names/masks, invalid
lengths, failed chunks and changed metadata are errors. Supported LED classes
are modifier (1), underglow (2), keylight (4) and indicator (8). The UI offers
their reported combinations, plus all (255) and none (0), and preserves a
current custom selection. Missing metadata makes effects, LED selection and
native key options read-only. Supported semantic bits gate editable key options.
Restore rejects an effect absent from the advertised inventory before staging.
This metadata is not exported and does not remap native effect IDs between
different firmware builds.

## Layer order and restoration

The standard image reserves sixteen layers, IDs 0–15. `LT()` and `LM()`
encode their layer in four bits, so every value they can express names a bank
layer; the five-bit layer actions (`MO`, `TG`, `TT`, `OSL`, `TO` and layer
locks) refuse layers 16 and above. Base stays at index zero. The app
shows highest priority first and rewrites references when overlays move:
matrix LT/LM/MO/TO/TG/DF/PDF/OSL/TT and userspace lock actions, behaviour targets
and branches, combo inputs/outputs/reference mapping, RGB rows/groups,
auto-mouse/auto-sniping targets, the persistent default-layer bitmask and the
layer behaviour and combo masks. Names and layer records (combo reference and
placement bitmaps) move with their layers. Save/discard other pending edits before reordering or
importing; stale layout drafts must not be applied to a new layer order.

Restore validates the file, captures a coherent current state, shows a review,
and saves a local recovery file before staging. After acquiring the profile
lease it compares the profile status and settings digest, then uploads the
candidate, reusing ranges of the active profile. Firmware validates and
durably prepares the record on both halves; the USB-side marker is the
decision. Firmware commits the other half's record and activates once that
half is seen holding it (D-F31).

A timeout after the decision is reported as incomplete and retains the recovery
path. Before the decision, an interrupted prepared record remains inactive and
the previous profile stays authority on both halves. The detailed ordering is specified in
[`logical-profile-transaction-v1.md`](logical-profile-transaction-v1.md).
Physical power-loss acceptance at each durable boundary remains required.

## Upgrade and acceptance

Current firmware has a sixteen-layer keymap domain (1,920 bytes) and a
34,903-byte macro domain (D-F30). Older geometry, action vocabulary and
storage are refused: firmware does not interpret old storage in place, and a
backup from earlier firmware cannot be restored.

Host tests validate the app-generated populated payload using the compatibility
rules of firmware compiled with no behaviours or combos. Tests also cover
empty overrides, migration, reference rewrites, macro invalidation/retry,
review conflicts, recovery-file failures and readback mismatches. Browser
checks exercise naming, moving, saving and reviewing an import. The current side-specific pair is the supported build. The user reports that the new workflow appears to work
on their keyboard. Physical bridge/export/upgrade/import, restoration onto
firmware without authored behaviours or combos, reboot, power loss and USB-role
changes still require a recorded acceptance matrix. Ark's macro
builder now reads and edits both device banks through this complete-profile
restore path. Defaults controls use that path too. Physical acceptance remains
separate product work.

## Defaults editor

Ark's section controls receive effective scalar values from the
complete snapshot. Section saves preserve both macro banks, layer names and
all unedited profile bytes, use the complete-profile recovery/restore path, and
acknowledge only verified readback. Drafts are keyed by device and section and
retain their original complete-profile fingerprint. Failed saves and re-reads
keep drafts; external changes block stale saves until discard. Verified macro
and settings edits can advance unrelated drafts from their exact common base.

Dedicated controls cover all 28 settings, subject to advertised capabilities:
timing and pointer policies, all four bytes of lighting setting 21, HSV (22),
startup layers (23), supported native key options (24), and per-layer combo
references (27). Unedited bytes and unknown native bits remain intact. Startup
layers require a nonzero final mask; combo references use the reported names.

The firmware applies native lighting while temporarily enabled because QMK
ignores mode/HSV setters when disabled. At the existing safe activation boundary
it synchronously applies all lighting values, then restores and saves the final
enabled state. The renderer cannot run between these operations. This permits
editing saved colours/effects while lighting stays off and restoring a profile
whose lighting state differs from the current state.

## Shared editor draft

For a complete profile snapshot, the session owns a base fingerprint and a
validated target document. All editor messages stage through the existing pure
editors; layer moves and imports stage whole documents. Undo/redo retains up to
100 transitions. Messages carry both a unique draft instance ID and revision;
Apply additionally requires review of that exact revision and the original device.

The semantic review compares layout, names, actions, macros, RGB and settings.
Re-reading preserves a dirty target. If the saved fingerprint differs, editing
and Apply are blocked until the user explicitly reviews against the new read.
This rebases the complete target without merging conflicting external changes.
A failed/interrupted restore retains the target; an incomplete recovery read
uses its diagnostic fingerprint and clearly states that a full comparison is
unavailable.

Apply invokes one existing complete-profile restore, with recovery storage before
writes and target fingerprint verification before resetting history. Current
firmware publishes the profile as one generation (D-F31). Export
still captures saved state. Drafts are window-local; unkept forms and kept
changes are not crash-persistent backups.

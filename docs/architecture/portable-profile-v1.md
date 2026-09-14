# Portable keyboard profile v1

A profile export is the effective configuration read from the keyboard. Flashed
and committed data have the same representation. An absent live domain is
materialized from device readback before export; it never means “use the
destination firmware's defaults”. Transient presses, running macros, pointer
motion and temporary layer activation are not configuration.

## Portable document

The UTF-8 JSON file has exactly these fields. Unknown fields and unsupported
versions fail validation. Files are limited to 100,000 bytes.

| Field | Meaning |
| --- | --- |
| `format` | `charybdis-profile` |
| `version` | `1` |
| `keyboard` | `charybdis-4x6` |
| `actionAbiDigest` | Nonzero uint32 engine vocabulary identity |
| `layers` | Eight arrays of 60 uint16 native keycodes, in matrix row/column order, including unused physical matrix positions |
| `profile` | Canonical base64 NLP1 payload, at most 4,064 bytes; domains `0x10`, `0x20`, `0x30`, `0x40`, in that order |
| `macros` | Exactly 64 canonical base64 VIA macro streams, excluding their zero terminators |

The temporary bridge also exports five-layer documents. Import into the
standard image recognizes the deployed `0xdcb00959` action vocabulary, adds
three transparent layers, and translates native user triggers from `0x7e61`
upward by three positions. The eight-layer vocabulary is `0xeb80829c`. Other
legacy vocabularies are rejected. Large legacy macro banks must fit the new
7,191-byte capacity, including 64 terminators and the final validity byte.

RGB and behaviour domains come from the effective committed domain or the
keyboard's compiled readback. Combo rows come from effective native readback;
the exported override is explicit even when empty. Custom trigger/release/
repress hooks, strict/disabled timing and fixed combo-reference builds are not
portable in v1 and prevent export. This standard board has no encoder or
user-selectable VIA layout options; EEPROM padding and validity metadata are
not profile content. Executable hooks, hardware geometry, engine inclusion and
safety ceilings remain firmware capabilities.

VIA streams support ASCII text, tap/down/up instructions and decimal delay
instructions. Held keys must balance. The bank is reconstructed with zero
padding and a final zero validity byte. Both macro banks are independent:
64 VIA slots and 16 user macro slots retain their existing key identities.

## Settings domain `0x40`, version 1

Capability domain-mask bit 3 advertises this domain and the full readback
commands below. The portable image includes the pointing, combo and RGB engines.
An ordinary profile may omit the domain; a complete portable file must include
it. An explicit empty macro slot overrides its flashed default.

All multibyte fields below are little-endian:

| Offset | Bytes | Contents |
| ---: | ---: | --- |
| 0 | 8 | `[1, 8, 28, 16, 0, 0, 0, 0]`: version, name count, scalar count, macro count, reserved zeros |
| 8 | 112 | 28 uint32 scalar values |
| 120 | 192 | Eight UTF-8 names, each 24 bytes: at most 23 bytes of text, then a zero and zero padding |
| 312 | variable | Sixteen records: uint16 instruction length followed by instruction bytes |

The minimum domain size is 344 bytes; the maximum is 1,368. Each user macro
has at most 512 instruction bytes and their combined content at most 1,024.
Instructions are `1` + uint8 text length + ASCII bytes, `2` + uint16 delay,
`3` + uint8 key down, `4` + uint8 key up, or `5` + uint8 key count + simultaneous
tap keys. Key lists and concurrently held sets are bounded to 16 and must not
duplicate keys or leave them pressed. Controls and malformed UTF-8 are rejected
in names. Firmware validation reads at most one byte per settings step.

| Scalar ID | Meaning |
| ---: | --- |
| 0–3 | Tapping, tap/hold, long-hold and multi-tap timing (ms) |
| 4–7 | Auto-mouse enabled, layer, timeout (ms), debounce (ms) |
| 8–9 | Auto-sniping enabled and layer |
| 10–14 | Dragscroll, volume, brightness, zoom and arrow-mode DPI |
| 15–16 | Key-feedback flash half-period and auto-mouse fade dead time (ms) |
| 17 | RGB idle timeout (ms); zero disables it |
| 18–19 | Actual normal and sniping DPI |
| 20 | Combos enabled |
| 21 | Four bytes: RGB enabled, effect, speed, flags |
| 22 | Three bytes: RGB hue, saturation, actual brightness; high byte zero |
| 23 | Persistent default-layer bitmask |
| 24 | QMK keymap options |
| 25–26 | Auto-mouse activation delay (ms) and movement threshold |
| 27 | Eight four-bit combo reference-layer IDs, lowest layer first |

Boolean values are 0/1; layer IDs must fit the bank. Timing/DPI policy scalars
are uint16 except the RGB idle timeout (maximum one day). Debounce is uint8.
Flash half-period must be nonzero and fade dead time less than auto-mouse
timeout. Normal DPI is 400–3,400 in steps of 200; sniping DPI is 100–400 in steps
of 100. The default-layer mask is nonzero and contains no out-of-bank bits.

At publication the settings invalidator copies the bounded domain into a cold
runtime cache using reads of at most 20 bytes. Key, RGB, pointer and macro
execution use the cache, without EEPROM reads. The invalidator refreshes macro
validation metadata and applies QMK-owned settings at the safe boundary.
Native RGB/DPI/default-layer/keymap settings retain their EEPROM ownership:
boot preserves newer native values, and export reads their current values.
Ordinary domain edits refresh these values before writing an existing settings
domain, and bind that read before and after acquiring the candidate lease.

## Readback commands

These use the existing Profile Wire custom GET envelope and request ID.

GET value `0x07`, page 0 captures effective scalars, names and user macros into
a cold snapshot. Its 12-byte payload is version `1`, chunk size `25`, uint16
length, CRC32, and FNV-1a digest. Pages 1 onward return successive 25-byte chunks
with an exact short final chunk. Reading page 0 again recaptures metadata; the
host requires identity and both checksums to match. The complete export also
rechecks settings after reading the VIA banks, so changing RGB or DPI during a
backup invalidates the read.

GET value `0x08`, page 0 returns 25 bytes: version `1`; flags; local generation
and digest; peer generation and digest; last acknowledged generation; last
error; uint16 conflict count. Flags are local dirty (bit 0), recovery required
(1), digest valid (2), replication pending (3), and receiver active (4).
Readiness requires only the digest-valid flag, no reported error/conflict, and
matching local/peer generation and digest. These are VIA storage identities;
the custom-profile owner retains its separate status and generation.

GET value `0x08`, page 1 is optional editor metadata: exactly two bytes,
version `1` and the device's actual RGB maximum brightness (uint8, including
zero). The firmware reports `RGB_MATRIX_MAXIMUM_BRIGHTNESS`; it does not
change LED state. Earlier firmware rejects this page with status `2`. The app
treats only that canonical, correlated rejection as absent metadata; malformed
or other failed replies remain errors. Without the limit, the Defaults brightness
field is read-only. Other settings and existing export/import stay available.
With a reported limit, Defaults and full-profile restore reject an excessive
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

The standard image reserves eight layers. Base stays at index zero. The app
shows highest priority first and rewrites references when overlays move:
matrix LT/LM/MO/TO/TG/DF/PDF/OSL/TT and userspace lock actions, behaviour targets
and branches, combo inputs/outputs/reference mapping, RGB rows/groups,
auto-mouse/auto-sniping targets and the persistent default-layer bitmask. Names
move with their layers. Save/discard other pending edits before reordering or
importing; stale layout drafts must not be applied to a new layer order.

Restore validates the file, captures a coherent current state, shows a review,
and saves a local recovery file before staging. After acquiring the profile
lease it compares the custom status, VIA generation/digest and settings digest
without transferring the complete matrix and macro bank again. The firmware
validates the candidate and commits the custom profile to both halves first.
Only changed 28-byte VIA blocks follow. Macro execution is invalidated by
writing the final bank byte to one before content changes and enabled by writing
it to zero only after all content acknowledgements. Success requires exact
changed-block readback, stable unchanged bytes from the coherent base, complete
effective-profile identity, and convergence of both persistence owners.

This is a recoverable sequence across two owners, not an atomic whole-keyboard
transaction. A timeout after commit is reported as incomplete and retains the
recovery path. A subsequent import can replace an interrupted macro bank; the
incomplete bytes are saved as `charybdis-recovery-capture` diagnostic JSON,
never presented as an importable full profile. Keep the original complete
backup. Storage conflicts or a pending custom-profile transaction must finish
or recover before a new restore.

The replacement atomic ordering is specified in
[`logical-profile-transaction-v1.md`](logical-profile-transaction-v1.md). Until
that firmware contract lands, the recovery file remains the cross-owner safety
boundary.

## Upgrade and acceptance

The bridge retains five-layer VIA geometry and schema-1 reconciliation metadata,
and the exact deployed compiled-profile digest/action identity. Recognized VIA
storage survives the firmware build-date change; dirty metadata remains dirty
for recovery. The eight-layer image uses metadata schema 2 and deliberately
resets incompatible five-layer geometry. Export on the bridge **before** flashing
the eight-layer pair, then import the resulting file. The app does not flash.

Host tests validate the app-generated populated payload using the compatibility
rules of firmware compiled with no behaviours or combos. Tests also cover
empty overrides, migration, reference rewrites, macro invalidation/retry,
review conflicts, recovery-file failures and readback mismatches. Browser
checks exercise naming, moving, saving and reviewing an import. Both standard
and bridge pairs build. The user reports that the new workflow appears to work
on their keyboard. Physical bridge/export/upgrade/import, restoration onto
firmware without authored behaviours or combos, reboot, power loss and USB-role
changes still require a recorded acceptance matrix. The existing Studio macro
builder now reads and edits both device banks through this complete-profile
restore path. Defaults controls use that path too. The inherited pointing-cadence
regression and physical acceptance remain separate product work.


## Defaults editor

Studio's existing section controls receive effective scalar values from the
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

For a complete eight-layer snapshot, the session owns a base fingerprint and a
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
writes and target fingerprint verification before resetting history. No firmware
atomicity is added. Export still captures saved state. Drafts are window-local;
unkept forms and kept changes are not crash-persistent backups.

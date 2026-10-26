# Settings domain `0x40` and its readback

Domain `0x40`, version 10, holds the keyboard-wide settings: 31 scalar
settings, a record per layer, each custom key's shortcut, and the names of the
layers, macros, custom keys and the keyboard. GET `0x07` reads the active
settings back and GET `0x08` reports the lighting and key-option metadata a
client needs to edit them. The domain id, envelope, byte order and checksums
are [blob and digests](blob-and-digests.md)'s; the read envelope is
[the HID channel](hid-channel.md)'s; how a live edit, a keyboard adjustment or
a toggle changes these values, and how they reach QMK, is
[live edits](live-edits.md)'. Participation controls are
[the participation policy](../participation-policy.md)'s to define.

## Layout

| Offset | Size | Contents |
| ---: | ---: | --- |
| 0 | 8 | header |
| 8 | 124 | 31 scalars, `u32` each, scalar 0 first |
| 132 | 272 | 16 layer records of 17 bytes, layer 0 first |
| 404 | 640 | 128 custom-key shortcuts of 5 bytes, custom key 0 first |
| 1,044 | 273 to 9,009 | 273 counted names |

The domain is 1,317 bytes with every name empty and 10,053 with every name 32
bytes; any other length is refused. The fixed part comes first so every field
before the names sits at a known offset. Validation consumes one byte at a
time from reads of at most 20 bytes; any refusal is `INVALID_DOMAIN`.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Header

| Offset | Value |
| ---: | --- |
| 0 | version, `10`; equal to the envelope's domain version |
| 1 | layer count, `16` |
| 2 | scalar count, `31` |
| 3 | macro-name count, `128` |
| 4 | custom-key-name count, `128` |
| 5 | flags: bit 0 bypasses every combo output's behaviour ([encoding](combo-domain.md#combo-output-behaviour-permission-encoding)); other bits zero |
| 6 | custom-key-shortcut count, `128` |
| 7 | reserved, zero |

Every byte but 5 must be exactly its value. Firmware accepts only version 10
and refuses every earlier one (D-F10,
[ADR](../../adr/0010-firmware-accepts-only-current-formats.md)).

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Scalars

Scalar `id` is four little-endian bytes at offset `8 + 4 × id`. Booleans are
`0` or `1`. Layer numbers are below 16 and layer masks name layers 0–15 only.
Unless the table says otherwise a scalar is at most 65,535.

| Id | Setting | Rule | Compiled default |
| ---: | --- | --- | --- |
| 0 | tapping term: dual-role keys and QMK's own tapping (D-F47, [ADR](../../adr/0047-dual-role-setting-is-the-tapping-term.md)), ms | | `TAPPING_TERM` |
| 1 | tap / hold threshold, ms | | `CUSTOM_TAP_HOLD_TERM` |
| 2 | long hold threshold, ms | | `CUSTOM_LONGER_HOLD_TERM` |
| 3 | multi tap window, ms | | `CUSTOM_MULTI_TAP_TERM` |
| 4 | auto-mouse on | boolean | 1 |
| 5 | pointer layer for auto-mouse | layer | `AUTO_MOUSE_DEFAULT_LAYER` |
| 6 | auto-mouse timeout, ms | nonzero | `AUTO_MOUSE_TIME` |
| 7 | auto-mouse debounce, ms | at most 255 | `AUTO_MOUSE_DEBOUNCE` |
| 8 | auto-sniping on: the master switch for scalar 9's layers | boolean | 1 |
| 9 | auto-sniping layers: sniping runs while any of them is on (D-F62) | layer mask, may be empty | `1 << CHARYBDIS_AUTO_SNIPING_LAYER` |
| 10–14 | retired | zero | 0 |
| 15 | key-feedback flash half-period, ms | nonzero | `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS` |
| 16 | auto-mouse fade delay: how long the colour holds before it fades, ms | below scalar 6 | `AUTOMOUSE_RGB_DEAD_TIME` |
| 17 | lighting idle timeout, ms; `0` never sleeps | at most 86,400,000 (one day) | `NOAH_RGB_IDLE_TIMEOUT_MS`, else 900,000 |
| 18 | normal DPI | 400–3,400 in steps of 200 | `CHARYBDIS_MINIMUM_DEFAULT_DPI` |
| 19 | sniping DPI | 100–400 in steps of 100 | `CHARYBDIS_MINIMUM_SNIPING_DPI` |
| 20 | combos on (the stored master) | boolean | 1 |
| 21 | lighting: byte 0 on, byte 1 background effect, byte 2 speed, byte 3 LED flags | byte 0 boolean; bytes 1–3 any | QMK's `RGB_MATRIX_DEFAULT_ON`, `_MODE`, `_SPD`, `_FLAGS` |
| 22 | colour: byte 0 hue, byte 1 saturation, byte 2 brightness | byte 3 zero | QMK's `RGB_MATRIX_DEFAULT_HUE`, `_SAT`, `_VAL` |
| 23 | default layers, one bit per layer | nonzero layer mask | 1 (the base layer) |
| 24 | QMK key options (`keymap_config.raw`) | | QMK's factory options |
| 25 | auto-mouse activation delay, ms (QMK's `AUTO_MOUSE_DELAY`) | | 200 |
| 26 | auto-mouse movement threshold (QMK's `AUTO_MOUSE_THRESHOLD`) | | 10 |
| 27 | host settings, below | | `NOAH_DEFAULT_HOST_*`, else 0 |
| 28 | behaviour master | boolean | 1 |
| 29 | layers whose behaviours run | layer mask, may be empty | `0xffff` |
| 30 | layers whose combos run | layer mask, may be empty | `0xffff` |

The fade delay is checked against the timeout in the same domain, so both
halves refuse a profile whose fade delay is not shorter than its timeout.
Scalar 24's bits are QMK's; GET `0x08` pages 2 onward name the ones a client
may edit. The domain accepts any brightness; QMK clamps it to the reported
maximum when it applies it ([GET `0x08` page 1](#editor-metadata--get-0x08)).

Scalar 27, the host settings word (D-F16,
[ADR](../../adr/0016-macro-text-typed-through-host-layout.md)):

| Bits | Meaning |
| --- | --- |
| 0–1 | host: 0 Auto, 1 macOS, 2 Windows, 3 Linux |
| 8 | Unicode entry for macro text |
| 16–23 | host layout id, below `NOAH_HOST_LAYOUT_LIMIT` (15); the catalogue is [host layouts](../host-layouts-v1.md)' |
| 24 | macOS classified the keyboard as ISO |
| others | zero |

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Adjustment scalars

Scalars 18, 19, 21 and 22 are the adjustment scalars, the ones the keyboard's
DPI and lighting keys set (D-F32,
[ADR](../../adr/0032-live-edits-working-copy-and-save.md)); adjustment index 0
to 3 is that order, which is scalar order, so an edit of any of them has its
splices in ascending offset order. An adjustment's bytes are the adjusted
scalars over the domain they land in, or over the compiled defaults' domain
when the profile has none; every other byte is unchanged. Toggles, combos on
and the behaviour master are not adjustment scalars. The queue that turns
adjustments into edits is [live edits](live-edits.md)'.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Layer records

Each layer's 17-byte record:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | the layer's combo reference layer, below 16 |
| 1 | 8 | behaviour-bypass placement bitmap |
| 9 | 8 | combo-exclusion placement bitmap |

Bit `n` of a bitmap (byte `n / 8`, bit `n % 8`) is matrix position
`n = row × columns + column`. A bitmap covers up to 64 positions; bits at or
past the build's position count (60 on this keyboard, 10 × 6, as the
capability pages report) are zero. A set bypass bit sends that placement's key
to its normal action instead of its behaviour; a set exclusion bit keeps it
out of combos ([participation policy](../participation-policy.md)). QMK's combo
engine reads each layer's reference layer from here
(`combo_ref_from_layer`), and GET `0x06` page 1 reports it.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Custom-key shortcuts

Each custom key's five-byte shortcut (D-F26,
[ADR](../../adr/0026-custom-key-carries-a-shortcut.md)):

| Offset | Field |
| ---: | --- |
| 0 | modifiers, HID report order: bit 0 Left Ctrl, 1 Left Shift, 2 Left Alt, 3 Left GUI, 4 Right Ctrl, 5 Right Shift, 6 Right Alt, 7 Right GUI; any combination |
| 1 | key: `0x00` none, a basic key `0x04..0xa4` (`KC_A`–`KC_EXSEL`), or a mouse button `0xd1..0xd8` (`MS_BTN1`–`MS_BTN8`) |
| 2–3 | layers, `u16`: bit `n` names layer `n`; never bit 0 or a bit of scalar 23 |
| 4 | layer kind, the layer key the layers act as, all together: `0` Hold (`MO()`), `1` Lock (`TG()`), `2` Move to (`TO()`), `3` Tap-toggle (`TT()`), `4` One-shot (`OSL()`); `0` when no layer is named |

All zero means no shortcut; any part alone (modifiers, a key, layers) is a
shortcut. Modifier keys belong in the mask; media, system, mouse-movement and
every non-basic keycode are refused. The base layer and the default layers are
always on, so a shortcut naming one is refused; the check reads scalar 23 from the
same domain, so a profile whose default layers include a layer a shortcut
names is refused too. A layer kind above `4`, or any kind but Hold without a
layer, is refused, so a shortcut has one record. The firmware does not check that a behaviour step
naming a custom key has a shortcut; one without sends nothing. What a shortcut
does is [key press](../../architecture/key-press.md#from-an-action-to-the-report)'s.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Names

273 names follow, in order: 16 layer names, 128 macro names (D-F43,
[ADR](../../adr/0043-macros-have-names-user-macros-retired.md)), 128 custom-key
names, then the keyboard's own name. Each is a length byte `0..32` and that
many bytes; names are counted, not terminated, and the last one ends the
domain exactly.

Every name follows one rule: at most 32 bytes of well-formed UTF-8 (no
overlong form, no surrogate, nothing past U+10FFFF), no C0 control
(`0x00`–`0x1f`) and no DEL (`0x7f`), and no multibyte sequence cut by the
length. Limits count bytes, not characters. The pointing domain's slot names
use the same rule in their own fixed field ([pointing domain](../pd-mode-domain-v1.md)).
Names are the profile's data for clients; macro content is the
[macro domain](macro-domain.md)'s.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Keyboard name

The last name is the keyboard's name (D-F25,
[ADR](../../adr/0025-keyboard-name-is-usb-product-string.md)). A build with
saves serves it as the USB product string through the BK fork's runtime
product string hook (`USB_PRODUCT_STRING_USER`, `usb_product_string_user()`);
empty means QMK's built-in product string. The manufacturer and serial
number never change.

Hosts read the string only when they enumerate the keyboard:

- Before USB starts (`keyboard_pre_init`), both halves read the name of the
  saved profile boot selection chooses, which checks the record's checksum
  and compatibility but not the other half; with nothing saved, the compiled
  defaults' name (`NOAH_KEYBOARD_NAME`).
- When the active profile's name differs from the one USB enumerated with (an
  Apply or Save renamed the keyboard, or the halves reconciled to a profile
  other than the one read at boot), the master half restarts its USB driver
  (QMK's `restart_usb_driver`, 50 ms off the bus) once all of these hold:
  its owner is ready with no candidate, save, edit, activation or cleanup in
  flight, both halves hold the same saved profile, and the
  [safe boundary](../authority-state-table.md#safe-boundary) reports no
  reason; no Raw HID report has arrived for 1 s, so a client finishes the
  reads that follow its Apply; USB is enumerated and not suspended.
- The new string is built in a second buffer and served by switching to it
  just before the restart, so a host never reads a half-written string.
- One change of the name is one restart, and an unchanged name never
  restarts. A name that cannot be encoded serves the built-in string and
  settles the same way. The empty name and a stored name equal to the
  built-in text are different names, so a change between them restarts too.
- The slave half never restarts USB: its USB stays off as QMK left it.

Held by: `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`), `tests/host/run_qmk_usb_product_name_tests.sh`, `tests/host/run_qmk_usb_product_name_owner_tests.sh`.

## Compiled defaults

A profile may omit this domain; the compiled defaults' settings then run. They
are the header with flags zero, the scalars in the table above, every layer
referring combos to itself with both bitmaps zero, the shortcuts of the
keymap's `CUSTOM_KEYS` rows, and the keymap's names: `layer_names[]`, each
`MACROS` row's name, each `CUSTOM_KEYS` row's name, and
`NOAH_KEYBOARD_NAME` (empty unless the keymap's `config.h` sets it). The
keymap's `config.h` may also author `NOAH_RGB_IDLE_TIMEOUT_MS` and the host
settings `NOAH_DEFAULT_HOST_OS`, `NOAH_DEFAULT_HOST_LAYOUT`,
`NOAH_DEFAULT_UNICODE_PLAYBACK` and `NOAH_DEFAULT_MACOS_ISO`.

When a profile becomes active the firmware copies its settings domain, in
reads of at most 20 bytes, into one RAM copy; key, lighting, pointing and
macro code read that copy and never storage. An edit that leaves this domain
unchanged keeps it.

Held by: `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Settings readback — GET `0x07`

GET `0x07` reads the active profile's settings domain exactly as stored, or
the compiled defaults' when no settings are active. It reports stored values,
never QMK's copies (D-F32); QMK's live values and the toggles' runtime
overrides are GET `0x22`'s ([live edits](live-edits.md)).

The request takes a 16-bit page, low byte at request byte 4 and high byte at
byte 5; bytes 6 onward are zero. Page 0's 12-byte payload:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 1 | version, `1` |
| 1 | 1 | chunk size, `25` |
| 2 | 2 | domain length, `u16` |
| 4 | 4 | CRC32 of the domain |
| 8 | 4 | FNV-1a 32-bit digest of the domain |

Page `p ≥ 1` returns domain bytes `25 × (p − 1)` onward, 25 at a time with an
exact short final chunk; the largest domain needs 403 chunk pages. Page 0
computes its checksums over the bytes as they are at that moment, and chunks
stream from the active copy rather than a snapshot, so a consistent read is
page 0, every chunk, then page 0 again with equal bytes and checksums matching
the chunks. A page past the end answers status 2; a zero request id or a
nonzero byte from 6 on answers status 1.

Held by: `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Editor metadata — GET `0x08`

GET `0x08` takes a one-byte page at request byte 4; bytes 5 onward are zero
(status 1 otherwise, or for a zero request id). Page 0 and any page past the
metadata answer status 2. None of it is configuration: it describes the
running build and is not part of any profile.

Page 1, the brightness limit (D-F38,
[ADR](../../adr/0038-keyboard-reports-its-brightness-limit.md)), two bytes:
version `1`, then `RGB_MATRIX_MAXIMUM_BRIGHTNESS` as `u8`. Reading it changes
no LED state.

Page 2, the lighting and key-option inventory (D-F39,
[ADR](../../adr/0039-native-settings-use-device-reported-capabilities.md)),
nine bytes:

| Offset | Value |
| ---: | --- |
| 0 | version, `1` |
| 1 | chunk size, `25` |
| 2 | body length, `u16` |
| 4 | effect count; effect ids are 1 through this count |
| 5 | key-option count, `13` |
| 6 | supported key options, `u16`, one bit per option id |
| 8 | OR of every LED's QMK class flags: modifier 1, underglow 2, keylight 4, indicator 8 |

Pages 3 onward carry the body in 25-byte chunks with an exact short final
chunk; the body always fits pages 3–255. The body is 13 `u16` native masks,
option id 0 first, each the QMK `keymap_config` value with only that option
set, then one 64-byte, NUL-terminated, zero-padded name per effect in id
order. A name is the effect's QMK identifier, prefixed `COMMUNITY_` for a
community module's effect and `CUSTOM_` for the keyboard's or keymap's own.

| Id | Key option | Supported when |
| ---: | --- | --- |
| 0 | swap Control and Caps Lock | `MAGIC_ENABLE` |
| 1 | Caps Lock becomes Control | `MAGIC_ENABLE` |
| 2 | swap left Alt and GUI | `MAGIC_ENABLE` |
| 3 | swap right Alt and GUI | `MAGIC_ENABLE` |
| 4 | disable GUI keys | `MAGIC_ENABLE` |
| 5 | swap Grave and Escape | `MAGIC_ENABLE` |
| 6 | swap Backslash and Backspace | `MAGIC_ENABLE` |
| 7 | NKRO | `NKRO_ENABLE` |
| 8 | swap left Control and GUI | `MAGIC_ENABLE` |
| 9 | swap right Control and GUI | `MAGIC_ENABLE` |
| 10 | one-shot keys | unless `NO_ACTION_ONESHOT` |
| 11 | swap Escape and Caps Lock | `MAGIC_ENABLE` |
| 12 | autocorrect | `AUTOCORRECT_ENABLE` |

Effect ids are the running build's; they are not remapped between builds.

Held by: `tests/host/run_qmk_portable_editor_tests.sh` (`qmk_portable_editor_test.c`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

## Lighting applied at the safe boundary

QMK ignores effect and colour setters while its lighting is off. So the
firmware applies scalars 21 and 22 in one synchronous step at the safe
boundary: it turns QMK's lighting on, sets the effect, speed, LED flags and
colour, then turns lighting on or off as scalar 21 byte 0 says. Every setter is
QMK's no-EEPROM one, and the renderer cannot run between them. Saved colours
and effects therefore apply while lighting stays off, and an Apply whose
lighting state differs from the running one lands whole. When the firmware
applies settings is [live edits](live-edits.md)'.

Held by: `tests/host/run_qmk_portable_editor_tests.sh` (`qmk_portable_editor_test.c`), `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`).

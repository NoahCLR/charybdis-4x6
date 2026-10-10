# Host Layouts v1

The keyboard sends key positions; the computer's keyboard layout turns them
into characters. A **host layout** records, for one computer layout, what each
key types and which keys type each character. Macro playback uses it to type
text on that layout, and clients use the same data to label keys and to judge
which characters a macro can type.

This document defines the catalogue, the fixture that carries it, the stroke
encoding and how the data is derived. Selecting a layout and typing through it
are separate contracts.

## Catalogue

A layout is named by a stable one-byte ID. An ID is never reused or renumbered.

| ID | Slug | Name | OS | Source |
| --- | --- | --- | --- | --- |
| 0 | `us` | US | any | US, ASCII only: what QMK's send_string assumes |
| 1 | `macos-abc` | ABC | macOS | `com.apple.keylayout.ABC` |
| 2 | `macos-dutch` | Dutch | macOS | `com.apple.keylayout.Dutch` |
| 3 | `macos-unicode-hex-input` | Unicode Hex Input | macOS | `com.apple.keylayout.UnicodeHexInput` |
| 4 | `macos-british` | British | macOS | `com.apple.keylayout.British` |
| 5 | `macos-german` | German | macOS | `com.apple.keylayout.German` |
| 6 | `macos-french` | French | macOS | `com.apple.keylayout.French` |
| 7 | `windows-us-international` | US International | Windows | QMK `keycodes_us_international` |
| 8 | `windows-uk` | United Kingdom | Windows | QMK `keycodes_uk` |
| 9 | `windows-german` | German | Windows | QMK `keycodes_german` |
| 10 | `windows-french` | French | Windows | QMK `keycodes_french` |
| 11 | `linux-us-international` | US International | Linux | XKB `us(intl)` |
| 12 | `linux-uk` | United Kingdom | Linux | XKB `gb` |
| 13 | `linux-german` | German | Linux | XKB `de` |
| 14 | `linux-french` | French | Linux | XKB `fr` |

Layout 0 is the default and types exactly what playback typed before host
layouts existed. On layout 3, Option enters hexadecimal Unicode, so the layout
types no Option characters and carries the `HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT`
flag.

## Strokes

A **stroke** is one basic keycode tapped with optional Shift and AltGr. AltGr is
Right Alt; on macOS, Right Option is Option. The fixture writes a stroke in QMK
notation, in exactly one form: `KC_E`, `S(KC_E)`, `ALGR(KC_E)` or
`S(ALGR(KC_E))`. The C tables encode it as 16 bits: the keycode in bits 0–7,
Shift in bit 8 and AltGr in bit 9. Zero means no stroke.

The keycodes are the alphanumeric and punctuation keys `KC_A`–`KC_0`,
`KC_SPC`, `KC_MINS`, `KC_EQL`, `KC_LBRC`, `KC_RBRC`, `KC_BSLS`, `KC_NUHS`,
`KC_SCLN`, `KC_QUOT`, `KC_GRV`, `KC_COMM`, `KC_DOT`, `KC_SLSH` and `KC_NUBS`,
plus `KC_TAB` and `KC_ENT` for tab and newline. Every host OS here reads
`KC_BSLS` and `KC_NUHS` as the same key.

A character takes one stroke, or two when the first is a dead key: the dead
key then Space types the dead key's own character, and the dead key then a
letter types the composed character.

## Fixture

`tests/fixtures/host_layouts_v1.json` is the contract. It is generated; never
edit it by hand. Its top level is `{"format": 1, "generator", "spec",
"layouts": [...]}`, with layouts in ID order. Each layout has:

| Field | Meaning |
| --- | --- |
| `id`, `slug`, `name`, `os` | The catalogue entry. `os` is `macos`, `windows`, `linux` or `any`. |
| `unicodeHexInput` | Present and `true` only on layout 3. |
| `source` | Where the data was read, including the OS or data version. |
| `keys` | For every keycode above except Tab and Enter: what it types plain, with Shift, with AltGr and with Shift+AltGr. Each cell is one character, `null`, or `{"dead": stroke}` for a dead key. |
| `dead` | For each dead-key stroke: `space`, the character the dead key then Space types (or `null`), and `compose`, mapping a stroke to the character it composes. |
| `chars` | Every character the layout can type, in code point order, mapped to its canonical sequence of one or two strokes. |

`keys` and `dead` describe the layout, for labelling keys. `chars` is what
typing uses. A client must read `chars` and must not re-derive it.

## Derivation

`tools/host_layouts/build.py` derives `chars` from `keys` and `dead`:

1. Every printable single character a stroke types directly, every dead key's
   `space` character (dead key, then `KC_SPC`), and every composition (dead
   key, then the base stroke) is a candidate. Tab is `KC_TAB` and newline is
   `KC_ENT` on every layout. C0 and C1 control characters are never typed.
2. Of a character's candidates, the canonical one has the fewest strokes, then
   the fewest modifiers, then the lowest keycodes, then plain before Shift
   before AltGr.

The raw sources under `tools/host_layouts/sources/` are read from the systems
themselves where possible:

- **macOS:** `extract_macos.swift` reads each input source's layout data with
  `UCKeyTranslate`, including every dead-key composition.
- **Linux:** `extract_xkb.sh` compiles each layout with libxkbcommon from
  xkeyboard-config (`evdev` rules, `pc105` model, Right Alt as AltGr). It
  composes dead keys with the `en_US.UTF-8` Compose table, which GTK and IBus
  follow. Note that dead acute then Space types an apostrophe there.
- **Windows:** no system data is read. `build.py --import-qmk` takes each key's
  plain, Shift, AltGr and Shift+AltGr characters from QMK's `keycodes_<layout>`
  data. QMK names letters by their capital; the layout types the small letter,
  and the capital with Shift. A dead key then Space types the dead key's
  character, except that US International's apostrophe and quote dead keys type
  `'` and `"`. A dead key composes only with letters, and only into Latin-1
  letters (U+00C0–U+00FF), which these Windows layouts compose. Rarer
  compositions are deliberately left out.
- **US (layout 0):** written in `build.py`.

The host test checks every printable ASCII character of layout 0 against QMK's
US send_string tables, and of the four Windows layouts against QMK's
`sendstring_<layout>.h`. `KC_BSLS` and `KC_NUHS` count as the same key.

## macOS keyboard type

macOS classifies a keyboard it does not recognise as ANSI or ISO. On ISO it
exchanges the keys QMK calls `KC_GRV` and `KC_NUBS`, and nothing else; the
layouts' own data does not differ between the two types. The macOS layouts are
read for ANSI. For an ISO-classified keyboard, the same tables apply with
`KC_GRV` and `KC_NUBS` exchanged in every stroke and legend. QMK's
`*_mac_iso` layout data shows that exchanged form.

## C tables

`build.py --write` generates `users/noah/lib/macro/host_layout_tables.c` from
the fixture: per layout, an array of `host_layout_char_t` (code point and two
strokes, 8 bytes each) sorted by code point, and the `host_layouts[]`
catalogue. `host_layout.h` and `host_layout.c` give `host_layout_get(id)` and
a binary-search `host_layout_lookup()`. The first set is about 4,000
characters, roughly 32 KB of flash once linked. The host test keeps it under
40 KB as a tripwire. This is a regression check, not a resource budget.

Version 1 defines the data only. No firmware image links these tables until
playback selects a layout.

## Changing the data

- To refresh a source, run its extractor (see each tool's header). Then run
  `python3 tools/host_layouts/build.py --write` and review the fixture diff.
- To add a layout, add its source and a new catalogue ID in `build.py`, write
  the fixture and tables, and extend the host test.
- `sh tests/host/run_host_layout_tests.sh` runs `build.py --check`, then the
  table tests. The full host suite includes it.

Changing a published layout's `chars` changes what macros type for its users.
Treat it as a contract change and check the layout on its OS.

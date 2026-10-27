# Host layouts v1

The keyboard sends key positions; the host's keyboard layout turns them into
characters. A host layout records, for one host keyboard layout, what each key
types and which keys type each character. Macro text is typed through it
(D-F16, [ADR](../adr/0016-macro-text-typed-through-host-layout.md)), and
clients read the same data to label keys and to tell which characters a macro
can type. This file covers the catalogue, strokes, the fixture that carries
the data, how it is derived, the C tables and how to change them. Settings
scalar 27 selects a layout
([settings domain](profile-wire-v1/settings-domain.md#scalars)); typing through
it is [macro playback](../architecture/macro-playback.md#typing-through-the-host-layout)'s.

## Catalogue

A layout is named by a one-byte id, stored in settings scalar 27 bits 16–23.
An id is never reused or renumbered.

| Id | Slug | Name | Host | Source |
| ---: | --- | --- | --- | --- |
| 0 | `us` | US | any | US, ASCII only, as QMK's `send_string` assumes |
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

Layout 0 is the default and types printable ASCII, tab and newline exactly as
QMK's US `send_string` does, and nothing else. On layout 3, Option enters
hexadecimal Unicode, so the layout types no Option characters; it carries
`HOST_LAYOUT_FLAG_UNICODE_HEX_INPUT`. Layouts 1–6 carry
`HOST_LAYOUT_FLAG_MACOS`.

The settings domain accepts an id below `NOAH_HOST_LAYOUT_LIMIT`
(`users/noah/lib/compat/qmk_host_setting.h`), which equals the number of
layouts in the tables. Playback latches the layout when a macro starts; an id
the tables do not have plays as layout 0.

Held by: `tests/host/run_host_layout_tests.sh` (`host_layout_test.c`, `test_catalogue_is_complete_and_well_formed`, `test_us_matches_qmk_send_string`), `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## Strokes

A stroke is one basic keycode tapped with optional Shift and AltGr. AltGr is
Right Alt; on macOS, Right Option, which is Option. The fixture writes a stroke
in QMK notation, in exactly one form: `KC_E`, `S(KC_E)`, `ALGR(KC_E)` or
`S(ALGR(KC_E))`. The C tables encode it in 16 bits:

| Bits | Meaning |
| --- | --- |
| 0–7 | the keycode (`HOST_LAYOUT_STROKE_KEYCODE`) |
| 8 | Shift (`HOST_LAYOUT_STROKE_SHIFT`, `0x0100`) |
| 9 | AltGr (`HOST_LAYOUT_STROKE_ALTGR`, `0x0200`) |
| 10–15 | zero |

Zero means no stroke. The keycodes are the alphanumeric and punctuation keys
`KC_A`–`KC_0`, `KC_SPC`, `KC_MINS`, `KC_EQL`, `KC_LBRC`, `KC_RBRC`, `KC_BSLS`,
`KC_NUHS`, `KC_SCLN`, `KC_QUOT`, `KC_GRV`, `KC_COMM`, `KC_DOT`, `KC_SLSH` and
`KC_NUBS`, plus `KC_TAB` and `KC_ENT` for tab and newline. Every host here
reads `KC_BSLS` and `KC_NUHS` as the same key.

A character takes one stroke, or two when the first is a dead key: the dead
key then Space types the dead key's own character, and the dead key then a
letter types the composed character.

Every layout types every printable ASCII character, tab (`KC_TAB` alone) and
newline (`KC_ENT` alone), and types `0`–`9`, `a`–`f` and `u` in one stroke
each, which Unicode entry needs. No layout types a control character or DEL.
Layout 3 has no AltGr stroke.

Held by: `tests/host/run_host_layout_tests.sh` (`host_layout_test.c`, `test_catalogue_is_complete_and_well_formed`, `test_layouts_type_their_own_characters`).

## Fixture

`tests/fixtures/host_layouts_v1.json` is the contract. It is generated; never
edit it by hand. Its top level is `{"format": 1, "generator", "spec",
"layouts": [...]}`: `generator` is `tools/host_layouts/build.py`, `spec` this
file's path, and the layouts are in id order. Each layout has:

| Field | Meaning |
| --- | --- |
| `id`, `slug`, `name`, `os` | The catalogue entry; `os` is `macos`, `windows`, `linux` or `any` |
| `unicodeHexInput` | Present, and `true`, only on layout 3 |
| `source` | Where the data was read, with the system or data version |
| `keys` | For every keycode above except `KC_TAB` and `KC_ENT`: what it types plain, with Shift, with AltGr and with Shift+AltGr. Each cell is one character, `null`, or `{"dead": stroke}` for a dead key |
| `dead` | For each dead-key stroke, in keycode order: `space`, the character the dead key then Space types (or `null`), and `compose`, mapping a stroke to the character it composes |
| `chars` | Every character the layout can type, in code point order, mapped to its canonical sequence of one or two strokes |

`keys` and `dead` describe the layout, for labelling keys. `chars` is what
typing uses: a client reads `chars` and does not derive it again.

Held by: `tests/host/run_host_layout_tests.sh` (`build.py --check`), `tests/fixtures/host_layouts_v1.json`.

## Derivation

`tools/host_layouts/build.py` derives `chars` from `keys` and `dead`:

1. Every printable single character a stroke types directly, every dead key's
   `space` character (the dead key, then `KC_SPC`), and every composition (the
   dead key, then the base stroke) is a candidate. Tab is `KC_TAB` and newline
   `KC_ENT` on every layout. C0 and C1 control characters and DEL are never
   typed.
2. Of a character's candidates, the canonical one has the fewest strokes, then
   the fewest modifiers, then the lowest keycodes, stroke by stroke, then plain
   before Shift before AltGr.

Held by: `tests/host/run_host_layout_tests.sh` (`build.py --check`).

## Sources

The raw sources are `tools/host_layouts/sources/{macos,linux,qmk}.json`, read
from the systems themselves where possible:

- **macOS**: `extract_macos.swift` reads each input source's layout data with
  `UCKeyTranslate` for an ANSI keyboard, including every dead-key composition;
  for Unicode Hex Input it reads no Option layer.
- **Linux**: `extract_xkb.sh` builds `extract_xkb.c`, which compiles each
  layout with libxkbcommon from xkeyboard-config (`evdev` rules, `pc105`
  model, Right Alt as AltGr) and composes dead keys with the `en_US.UTF-8`
  Compose table, which GTK and IBus follow. Dead acute then Space types an
  apostrophe there, so layout 13 cannot type `´`.
- **Windows**: no system data is read. `build.py --import-qmk` takes each
  key's plain, Shift, AltGr and Shift+AltGr characters from QMK's
  `data/constants/keycodes/extras/keycodes_<layout>_0.0.1.hjson`. QMK names
  letters by their capital; the layout types the small letter, and the capital
  with Shift. A key QMK leaves empty of `KC_BSLS` and `KC_NUHS` takes the
  other's cells. A dead key then Space types the dead key's character, except
  that US International's apostrophe and quote dead keys type `'` and `"`. A
  dead key composes only with letters, and only into Latin-1 letters
  (U+00C0–U+00FF); rarer compositions are left out.
- **US (layout 0)**: written in `build.py`, and written to `qmk.json` by
  `--import-qmk`.

The host test checks every printable ASCII character of layout 0 against
QMK's US `send_string` tables, and of the four Windows layouts against QMK's
`sendstring_<layout>.h`, both read from the QMK checkout; `KC_BSLS` and
`KC_NUHS` count as the same key.

Held by: `tests/host/run_host_layout_tests.sh` (`host_layout_test.c`, `test_us_matches_qmk_send_string`, `test_windows_ascii_matches_qmk_send_string`).

## macOS keyboard type

macOS classifies a keyboard it does not recognise as ANSI or ISO. On ISO it
exchanges the keys QMK calls `KC_GRV` and `KC_NUBS`, and nothing else; the
layouts' own data does not differ between the two types. The macOS layouts are
read for ANSI. For an ISO-classified keyboard the same tables apply with
`KC_GRV` and `KC_NUBS` exchanged in every stroke and legend; QMK's
`*_mac_iso` layout data shows that exchanged form. Settings scalar 27 bit 24
selects the exchange, and playback applies it only on a layout carrying
`HOST_LAYOUT_FLAG_MACOS`.

Held by: `tests/host/run_macro_payload_engine_tests.sh` (`macro_payload_engine_test.c`, `test_text_types_through_the_host_layout`), `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`).

## C tables

`build.py --write` generates `users/noah/lib/macro/host_layout_tables.c` from
the fixture: per layout, an array of `host_layout_char_t` (a 32-bit code point
and two strokes, 8 bytes) sorted by code point, then the `host_layouts[]`
catalogue (id, flags, count, characters) and `host_layout_table_count`.
`host_layout.h` and `host_layout.c` give `host_layout_get(id)`, which answers
`NULL` for an id not in the tables, and `host_layout_lookup()`, a binary
search that answers the strokes for a code point or false. The host test fails
if the tables pass 40 KiB, a regression tripwire, not a resource budget.

Held by: `tests/host/run_host_layout_tests.sh` (`build.py --check`, `host_layout_test.c`).

## Changing the data

Changing a published layout's `chars` changes what macros type for its users.
Treat it as a contract change and check the layout on its host.

- **Refresh macOS** (on a Mac):
  `swift tools/host_layouts/extract_macos.swift > tools/host_layouts/sources/macos.json`
- **Refresh Linux** (needs `brew install libxkbcommon xkeyboard-config libx11`):
  `sh tools/host_layouts/extract_xkb.sh > tools/host_layouts/sources/linux.json`
- **Refresh US and Windows** from the pinned BK checkout:
  `python3 tools/host_layouts/build.py --import-qmk <QMK checkout>`
- **Regenerate** the fixture and the C tables from the sources:
  `python3 tools/host_layouts/build.py --write`, then review the fixture diff.
- **Add a layout**: add it to its extractor's list (`layouts` in
  `extract_macos.swift` or `extract_xkb.c`, or `QMK_WINDOWS` in `build.py`)
  and refresh that source; add a catalogue entry with the next id to
  `CATALOGUE` in `build.py`; regenerate; raise `NOAH_HOST_LAYOUT_LIMIT` in
  `users/noah/lib/compat/qmk_host_setting.h` and `LAYOUT_COUNT` in
  `tests/host/host_layout_test.c` to the new count; and extend the host test.
- **Check**: `sh tests/host/run_host_layout_tests.sh` runs
  `build.py --check`, which fails when the fixture or the tables are stale,
  then the table tests against the QMK checkout. The full host suite includes
  it.

Held by: `tests/host/run_host_layout_tests.sh`.

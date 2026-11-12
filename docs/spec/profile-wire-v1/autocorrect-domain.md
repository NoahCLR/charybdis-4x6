# Autocorrect list domain `0x80`

Domain `0x80`, version 2, holds the profile's autocorrect list: what the
keyboard replaces as it is typed, and with what (D-F68,
[ADR](../../adr/0068-the-autocorrect-list-is-a-readable-profile-domain.md)),
and what the leader sends (D-F71,
[ADR](../../adr/0071-the-leader-is-part-of-the-typing-stage.md)). It holds the
entries as they are written, not a compiled tree. An **entry** is one
**replacement**, stored once, with how it **fires**, as a word ends, after
the leader or both, and every **typed form** that produces it; each typed
form says **where it matches** a word. A replacement is text the keyboard
types, or a macro slot it plays. The domain id, envelope and byte
order are [blob and digests](blob-and-digests.md)'s; how domains make a
profile is [the device-resident profile](../device-resident-profile.md)'s.
Refusal codes are named in [errors](errors.md).

The bytes, the code and the fixtures keep the names the domain was built
with: a replacement is a **correction** there and a typed form a **typo**.
This file uses those names wherever it names a field, a row, a table or a
refusal, and the glossary's words elsewhere.

It is the registry's eighth row, so its mask bit, `0x80`, is the last of the
one-byte domain mask. A ninth domain needs a wider mask everywhere one
travels: a candidate's BEGIN, a slot's record header, capability page 1 byte
12 and the split descriptors.

## Layout

Integers are little-endian. A header, two tables of fixed rows, two indexes,
then the text:

| Part | Size | Holds |
| --- | ---: | --- |
| Header | 10 | corrections `C` u16, typos `N` u16, part-of-word typos `P` u16, leader typos `D` u16, text bytes `T` u16 |
| Correction rows | 6 × `C` | text offset u16, length u8, kind and trigger u8, first typo u16 |
| Typo rows | 6 × `N` | text offset u16, length u8, match u8, correction u16 |
| Part index | 2 × `P` | a typo row's index u16 |
| Leader index | 2 × `D` | a typo row's index u16 |
| Text | `T` | every text correction's bytes, then every typo's |

The domain is exactly `10 + 6C + 6N + 2P + 2D + T` bytes. A text offset counts from
the start of the text. A correction's third byte holds its *kind* in bit 0,
what the replacement is, and its *trigger* in bits 1–2, how its typed forms
fire; bits 3–7 are zero:

| Kind | The replacement | The row's first two fields |
| ---: | --- | --- |
| 0 | text | its text offset and length |
| 1 | a macro | the [macro slot](macro-domain.md) it plays, `0`–`127`, and a length of `0` |

| Trigger | Its typed forms fire | As a client may write it |
| ---: | --- | --- |
| 0 | as a word ends | As you type |
| 1 | after the Leader key alone ([the leader](../autocorrect-v1.md#the-leader)) | After Leader |
| 2 | both ways | Both |

So the byte is `kind + 2 × trigger`, `0` to `5` (D-F71). A replacement's
typed forms are one list whatever its trigger: a client switches how a
replacement fires without touching them.

A typo's *correction* is the index of the correction row it names; its
*match* is where it matches:

| Match | Where the typed form matches a word | As a client may write it |
| ---: | --- | --- |
| 0 | the whole word | `teh` |
| 1 | the start of the word | `recieve*` |
| 2 | the end of the word | `*mnet` |
| 3 | anywhere in the word | `*tihng*` |

The last column is a notation, not bytes: a client may show and take a typed
form with `*` where other letters may go, and stores the match it stands for.
The domain never holds a `*`. A macro's typed forms match the whole word.
After the leader a typed form is its whole text, whatever its match: a
replacement that fires both ways replaces `recieved` as a word ends and is
sent by Leader then `recieve`.

One list has exactly one encoding:

- Text corrections come first and ascend strictly by their UTF-8 bytes,
  compared as unsigned bytes, a text before any longer text it begins. Macro
  corrections follow and ascend strictly by slot. Typos ascend by their
  bytes as text corrections do. So no replacement and no typed form is
  stored twice, a macro slot is one entry, and a typed form names one
  replacement, however that replacement fires.
- The text holds the text corrections in row order, then the typos in row
  order, back to back: each row's text begins where the row before it ended,
  the first at `0`, and the last typo's ends at `T`. A macro correction has
  no text.
- A correction's *first typo* is the lowest typo row that names it. Every
  correction therefore has a typo; the field is what lets the list be checked
  without remembering which corrections were named.
- The part index lists the typo rows whose match is `1` to `3` and whose
  correction fires as a word ends (trigger `0` or `2`), ascending, and nothing
  else; the leader index lists those whose correction fires after the leader
  (trigger `1` or `2`), ascending, and nothing else. A part-of-word typo of a
  correction that fires both ways is in both. Both are in the typos' byte
  order, so a binary search over the leader index finds the typed forms that
  fire after the leader and begin with a text.

Held by: `tests/host/run_profile_autocorrect_v1_tests.sh` (`profile_autocorrect_v1_test.c`, `test_long_and_macro_corrections`, `test_leader`; the encoder in `tools/autocorrect/build.py` against the fixture's `valid` vectors, `long`, `macros` and `leader` among them), `tests/fixtures/autocorrect_domain_v2.json`.

## Corrections and typos

A text replacement is 1–128 bytes of canonical UTF-8, as written, capitals
and all: no overlong form, no surrogate, nothing past U+10FFFF, and no
control character (below U+0020, or U+007F–U+009F), so no line break and no
tab. It may hold spaces, digits and punctuation: a phone number, an e-mail
address, a sign-off. Text of more than one line is a macro's.

A typed form is 1–32 bytes of the same UTF-8, of **word characters** only,
already **case-folded**, however its replacement fires:

- A word character is a letter or digit a [host layout](../host-layouts-v1.md)
  types: every character of any layout's `chars` whose Unicode general
  category is `Lu`, `Ll`, `Lo` or `Nd`, and the character each folds to.
- A character folds to its Unicode simple lowercase, when that is one
  character, and otherwise to itself. A typo holds only characters that fold
  to themselves (`teh`, never `Teh`), so a typed word is folded with the same
  table before it is looked up, and typos match whatever their case.
- An apostrophe, U+0027, may stand between two other characters of a typo
  (`dosen't`), never first or last.

The table is generated from `tests/fixtures/host_layouts_v1.json` by
`tools/autocorrect/build.py`, into the firmware
(`users/noah/lib/profile/schema/profile_autocorrect_words_v1.c`) and into
`tests/fixtures/autocorrect_domain_v2.json` as `wordCharacters`, each
`[character, folded]`: 491 characters today. It is part of this domain
version. A host layout that types a new letter extends it, which admits typos
the firmware before it refuses, so that change raises the schema minor. The
firmware's file also holds the table read the other way, each small letter
with the one character that folds to it, which the case rule looks up for
every character of a replacement.

A typed form is never equal, byte for byte, to the text replacement it names.
One that differs from it only by case is allowed (`iphone` for `iPhone`).

Held by: `tests/host/run_profile_autocorrect_v1_tests.sh` (`build.py --check`; `profile_autocorrect_v1_test.c`, `test_fold`, `test_capital` and the `invalid` vectors).

## Ceilings

Counted, and all held at once (D-F14,
[ADR](../../adr/0014-bigger-profiles.md)):

| Ceiling | Value |
| --- | ---: |
| Corrections | 1,024 |
| Typos, however they fire | 1,024 |
| Bytes in one typo | 32 |
| Bytes in one text correction | 128 |
| Macro slots a correction may name | 128 |
| Text bytes in all | 20,480 |
| The domain | 36,874 |

A replacement for every typed form fits, and the text holds 2,048 words of
ten bytes each, or 1,024 typed forms of eight with 96 replacements at full
length. The domain's ceiling is every counted one reached together:
`10 + 6 × 1,024 + 6 × 1,024 + 2 × 1,024 + 2 × 1,024 + 20,480`, every typo
being in both indexes at most. The keyboard reports the
counts, the text, the domain and both lengths on
[capability page 3](hid-channel.md#capability-pages--get-0x01).
[The maximum profile](../device-resident-profile.md#ceilings-and-the-maximum-profile)
holds a list at every one, a correction of 128 bytes among them.

Held by: `tests/host/run_profile_autocorrect_v1_tests.sh` (the `maximum` list and the over-ceiling vectors), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/maximum_profile_v4.fixture`), `tests/host/run_profile_wire_v1_tests.sh` (capability page 3).

## Validation

Validation walks the corrections, then the typos, reading one row or at most
20 bytes of text a step, and never more steps than the header's counts
allow. It holds one text of 128 bytes: the one being read, compared with the
row before it as it replaces it. For each typo it reads the correction row
the typo names, which says how the typo fires and so which indexes list it,
and that correction's text only when it is text of the typo's length.

A refusal is `INVALID_DOMAIN`. Its location names the table and row, and its
field id is the reason:

| Table id | Table |
| ---: | --- |
| `0` | corrections |
| `1` | typos |
| `2` | part index |
| `3` | leader index |
| `0xFF` | none: the header |

| Field id | Reason | Refused when |
| ---: | --- | --- |
| 3 | `INVALID_LENGTH` | the domain is shorter than its header, or not the length its counts make |
| 4 | `CAPACITY_EXCEEDED` | a count or the text is over its ceiling, or the part or the leader typos are more than the typos |
| 5 | `INVALID_OFFSET` | a text does not begin where the one before it ended, or runs past the text |
| 6 | `INVALID_TEXT_LENGTH` | a text correction is empty or over 128 bytes, a typo is empty or over 32, or a macro correction has a length |
| 7 | `INVALID_KIND` | a correction's kind and trigger byte is above `5`: no trigger `3`, and bits 3–7 zero |
| 8 | `INVALID_TEXT` | a text is not canonical UTF-8, or holds a control character |
| 9 | `ORDER` | a row is not above the one before it: out of order, stored twice, or a text correction after a macro one |
| 10 | `INVALID_MATCH` | a match above `3`, or a match of `1` to `3` on a typo that names a macro correction |
| 11 | `INVALID_REFERENCE` | a typo names a correction past the corrections, a first typo is past the typos, or there are typos and no corrections |
| 12 | `FIRST_TYPO` | a correction's first typo does not name it (which a correction no typo names cannot avoid), or a lower typo does |
| 13 | `NOT_A_WORD` | a typo holds a character that is not a word character, or an apostrophe first or last |
| 14 | `NOT_FOLDED` | a typo holds a word character that does not fold to itself |
| 15 | `TYPO_IS_CORRECTION` | a typo equals the correction it names |
| 16 | `PART_INDEX` | the part index leaves out a part-of-word typo whose correction fires as a word ends, lists another one, or names another row |
| 17 | `INVALID_MACRO` | a macro correction names a slot above `127` |
| 18 | `LEADER_INDEX` | the leader index leaves out a typo whose correction fires after the leader, lists another one, or names another row |

Validation checks that a macro slot exists, not that it holds a macro: the
macros are another domain, edited by themselves. A replacement whose slot is
empty is valid, and the keyboard passes it over
([autocorrect](../autocorrect-v1.md#a-macro-as-the-replacement)).

A row or text the reader cannot return is `READ_ERROR`, as for every domain.
`tests/fixtures/autocorrect_domain_v2.json` has a vector for each reason
under `invalid`, with the reason's name, table and row.

Held by: `tests/host/run_profile_autocorrect_v1_tests.sh` (`profile_autocorrect_v1_test.c`: every `invalid` vector, the one-read-of-twenty-bytes step bound, the step ceiling), `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`, `test_autocorrect_domain`), `tests/host/run_profile_working_copy_tests.sh` (`test_every_domain_is_checked_alone`), `tests/host/run_profile_owner_tests.sh` (`test_the_autocorrect_list_follows_the_active_profile`, a refused live edit).

## Runtime reads

The keyboard reads the list where it lies, in the active profile: a saved
slot in flash or the working copy in RAM (D-F54,
[ADR](../../adr/0054-profile-flash-and-validation-leave-the-raw-hid-callback.md)).
It keeps the list's range and its five counts, and no index or copy of the
list in RAM. A newly active profile, a live edit included, replaces the range.

- **A typo by its bytes**: binary search over the typo rows, at most eleven
  rows read, each with its text. It finds a typo whatever its match.
- **The part-of-word typos**: the part index, one typo row a position.
- **What the leader holds**: binary search over the leader index for the
  first typed form that fires after the leader not below the held text, at
  most eleven positions read, each with its typo row and text, then two more: whether
  one is exactly the held text, and whether a longer one begins with it.
- **A typo's correction**: the correction row, which says whether it is text
  or a macro slot and how it fires, and a text correction's bytes. A word end
  passes over a correction that fires only after the leader.
- **Whether any correction fires as a word ends**: the correction rows in
  order until one does, read once whenever the list is published; a list
  with none leaves typing alone until Leader.

Every lookup bounds what it reads by the header's counts and the text length,
so a stored offset, length or index never reaches outside the domain, and the
search ends in as many passes whatever the rows hold. A list whose header
does not count the domain's bytes opens as an empty list.

Held by: `tests/host/run_profile_autocorrect_v1_tests.sh` (`test_lookups`, `test_leader`, `test_malformed_lists_stay_bounded`), `tests/host/run_profile_owner_tests.sh` (`test_the_autocorrect_list_follows_the_active_profile`: the compiled defaults, the working copy on both halves, the saved slot after a replug), `tests/host/run_profile_domain_owner_round_trip_tests.sh` (`tests/fixtures/populated_profile_v4.fixture`, `tests/fixtures/maximum_profile_v4.fixture`).

## Absent domain and compiled defaults

A profile without this domain corrects from the authored list, as the
compiled defaults do. The authored list is the keymap folder's
`autocorrect.txt`, one typo a line in QMK's form (`typo -> correction`, a
colon before the typo for the start of a word, after it for the end, both for
the whole word, neither for anywhere; `:typo: => N` plays macro slot `N`;
a line that begins `leader ` fires only after the Leader key, its typo typed
whole and so without colons, and one that begins `both ` fires both ways; the
lines naming one replacement fire one way;
[authoring](../../guide/authoring.md#the-autocorrect-list)).
`tools/autocorrect/build.py --write-authored` encodes it into
`authored_autocorrect_list.h`, which the compiled defaults copy;
real-profile validation refuses a header the list no longer encodes to. An
empty list is its ten-byte header, so every build's compiled defaults hold
the domain and admit it. The list ships empty.

Held by: `tests/host/run_real_profile_validation_tests.sh` (`build.py --check-authored`), `tests/host/run_profile_autocorrect_v1_tests.sh` (a stale header fails; a list the domain refuses fails), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`tests/fixtures/compiled_profile_pd_v2.fixture`, `tests/fixtures/folded_keyboard_profile.json`), `tests/host/run_profile_domain_owner_round_trip_tests.sh`.

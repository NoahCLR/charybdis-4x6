# Autocorrect v1

The keyboard corrects the typos on the profile's
[autocorrect list](profile-wire-v1/autocorrect-domain.md) as they are typed,
on any computer, with nothing installed there (D-F69,
[ADR](../adr/0069-autocorrect-is-the-firmwares-own-and-corrects-at-the-word-end.md)).
This file is what it does: how it reads what is typed, when and what it
corrects, the undo, when it forgets the word, how a correction is typed and
what happens to keys pressed meanwhile. The list's bytes are the
[autocorrect domain](profile-wire-v1/autocorrect-domain.md)'s; the layouts
are [host layouts](host-layouts-v1.md)'s; how it sits in the report path is
[macro playback](../architecture/macro-playback.md#corrections)'s.

`tests/fixtures/autocorrect_v1.json` holds a case for every rule here: a
list, a host layout, the computer's Caps Lock, the keys typed and the text the
computer ends with. The firmware types every case through its report path. A
client that shows what the list will do runs its own matcher over the cases
that have a `screen` form, so the two cannot disagree.

Held by: `tests/host/run_autocorrect_tests.sh` (`autocorrect_test.c`, every case of `tests/fixtures/autocorrect_v1.json`).

## What counts as typing

The keyboard reads the keyboard reports it sends: each key going down, with
the modifiers the computer sees with it, whatever sent it (a plain key, a
behaviour's tap, a combo, a custom key). From the chosen host layout
(settings scalar 27) it works out the character that stroke shows:

- A key with Shift, AltGr or both types what the layout says. On a macOS
  layout either Option is AltGr; elsewhere only Right Alt is. So Shift+Space
  is a space and Shift+Enter is Enter.
- A dead key types nothing by itself. The next key then types what the layout
  composes from the two; Space after a dead key types the dead key's own
  character and is that character, not a space. A second key the layout
  composes nothing with [suspends](#forgetting-the-word) the word.
- Caps Lock, as the computer's LED report says, changes letters only. On
  macOS a letter shows as its capital whatever Shift does; everywhere else
  the two cases are exchanged. Windows layouts whose Caps Lock also changes
  the digit row are read as if it did not.
- An ISO Mac (scalar 27 bit 24) exchanges `KC_GRV` and `KC_NUBS`, as it does
  for macro text.

Every other key press is one of:

| Key | What it is |
| --- | --- |
| Backspace, alone or with Shift | Backspace |
| Enter, keypad Enter, Tab | a word end that prints nothing to take back |
| any key with Ctrl, GUI or, off macOS, Left Alt | a shortcut |
| a key the layout types nothing with: arrows, Home, End, Page Up and Down, Delete, Escape, the function keys, Backspace with Option or AltGr | a key that moves the cursor or deletes |
| the keypad's digits and signs | characters the keyboard does not follow |
| Caps Lock, Num Lock, Scroll Lock | nothing |

Modifiers alone, layers, pointer movement and consumer keys are not typing
and change nothing. A macro's text is never typing.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 1 cases: `shift-space`, `shift-enter`, `dutch-dead-key-read`, `us-international-dead-apostrophe`; `caps-lock`, `caps-lock-on-macos`), `tests/host/run_host_layout_tests.sh` (`test_strokes_read_back_as_their_characters`, `test_keys_and_dead_keys_are_recognized`).

## Words

A **word character** is a letter or digit of the
[word table](profile-wire-v1/autocorrect-domain.md#corrections-and-typos),
or an apostrophe (U+0027). A **word** is the word characters typed since the
last word end. Apostrophes before its first letter or digit and after its
last stay on screen and are not part of what a typo matches: `'teh'` is
matched as `teh`.

The word **ends** at the next character that is not a word character (a
space, punctuation, a hyphen) or at Enter or Tab.

Backspace inside a word removes its last character. The keyboard follows 32
characters of a word; a longer word is left alone.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 2 cases: `apostrophe-after`, `apostrophes-around`, `apostrophe-inside`, `digits-are-word-characters`, `word-end-punctuation`; `backspace-inside-a-word`).

## Which typo

When a word ends, it is folded with the word table and looked up:

1. A typo that matches the **whole word** and is the word wins.
2. Otherwise, of the part-of-word typos that fit (at the **start**, at the
   **end**, or **anywhere**, where the first place it is found counts), the
   one with the most characters wins, and of equal ones the first in the
   list, which is the lower in the domain's byte order.

A typo whose correction this host cannot type is passed over as if it were
not on the list. A word found nowhere, or still being typed, is never
touched.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 3 cases: `two-typos-one-correction`, `anywhere-in-a-word`, `end-of-a-word`, `start-of-a-word`, `longest-typo-wins`, `whole-word-beats-part`, `first-in-the-list-wins-a-tie`; `inside-a-longer-word`, `unfinished-word`).

## Case

Typos match whatever their case. The correction takes the case of the part
of the word the typo matched, as the computer shows it. A letter is a
capital when the word table folds it to another character, and small when
one character folds to it, which is then its capital; any other character
has no case.

| The matched part | The correction |
| --- | --- |
| two or more letters, all capitals (`TEH`) | every small letter as its capital (`THE`) |
| one capital, its first character, and the correction is written without capitals (`Teh`) | its first character as its capital (`The`) |
| anything else (`teh`, `tEh`), or a correction written with capitals (`Iphone` for `iPhone`) | as written |

When the layout cannot type that form, the correction is typed as written;
when it cannot type that either, the typo is passed over. A word that
already reads as the correction would leave it is left as it is.

The correction is typed so that it shows in that case under the computer's
Caps Lock: a letter goes out as the stroke of its other case, except on
macOS, where a capital is typed as itself and a small letter cannot be typed
while Caps Lock is on.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 4 cases).

## The correction

The corrected word is what stood before the typo, the correction, and what
followed the typo, apostrophes included. Only what differs is typed again:
the keyboard erases back to the first character that changes and types the
rest. `becuase` to `because` erases four characters and types four.

The key that ended the word is held back until the correction is typed. It
then goes to the computer as the real key, still held if the person still
holds it: Enter in a chat box sends the corrected text, and a held Space
repeats.

A correction is never typed through Unicode entry, and never takes more than
48 key strokes, erasing included; a longer one is passed over.

Held by: `tests/host/run_autocorrect_tests.sh` (`word-end-space`, `differing-suffix`, `word-end-enter`; `character-the-layout-cannot-type`; `test_held_keys_around_a_correction`), `tests/host/run_macro_slot_provider_tests.sh` (`test_a_correction_borrows_the_shared_window`).

## Undo

When a correction was ended by a key that prints (a space, punctuation) and
the very next key press is Backspace, that Backspace never reaches the
computer. Instead the keyboard erases the ending character and what the
correction typed, and types back the word exactly as it was typed. The word
is then left alone: ending it again corrects nothing.

After Enter or Tab there is no undo, since the text may be sent or the focus
moved: Backspace is Backspace. Any other key press between the correction
and Backspace ends the chance, and so does holding the ending key until it
repeats. When the layout can no longer type the word back (Caps Lock went on
on macOS), Backspace is Backspace.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 5 cases).

## Forgetting the word

The keyboard forgets the word it was following in one of two ways.

**Fresh start**: the next character starts a new word, which is corrected
normally. After:

- a shortcut;
- a key that moves the cursor or deletes forward;
- a mouse button going down;
- a macro, whatever it types.

**Suspended**: nothing is corrected until the next word end, because the
keyboard no longer knows where the word on screen starts. After:

- Backspace with no character of the word left;
- a key held 200 ms, which is taken to repeat on the computer (after a word
  end this only ends the chance to undo);
- a dead key the layout composes nothing from;
- a keypad character;
- a word past 32 characters;
- autocorrect coming on after reports went out unread, or its override being
  switched while a word is being typed.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 6 cases; `test_switching_off_forgets_the_word`).

## Typing a correction and holding keys back

A correction plays through the macro engine with its output isolated, as an
uninterruptible macro's is (D-F17): a neutral report and a 10 ms settle,
then each key held 5 ms and followed by 5 ms, Backspace first, then the
characters through the host layout. Modifiers the person holds never reach
it, and are back afterwards. A key that was down when the correction began
is up for the computer from then on and is not pressed again.

Unlike a macro's, the presses that arrive meanwhile are not ignored. Every
key and modifier change since the ending key waits in a queue of 64 changes,
the ending key first, and comes out afterwards in the order it happened, 2 ms
apart, and every scan while the queue is over half full. What comes out is
read as typing in its turn, so a typo typed while a correction plays is
corrected when its own word end comes out. A macro key pressed meanwhile
waits in the same queue and starts when its turn comes; the keys behind it
wait until it has played.

The queue is never allowed to cost a key:

- A correction does not start while more than 16 changes are waiting. The
  typo is left as typed.
- If the queue fills while a correction is typed, the correction stops where
  it is, the word is left as far as it got, and the queue empties. A change
  that does not fit stays in the keyboard's report and is taken up at the
  next scan, late and in order, never dropped.

The lookup itself runs from the scan, never from the report: the whole word
at once, then 32 part-of-word typos a scan. The ending key waits for it, at
most 33 scans for a full list and one for a list without part-of-word typos.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 7 cases; `test_no_correction_starts_behind_a_long_queue`, `test_a_full_queue_loses_no_key`, `test_a_correction_keeps_its_pace`, `test_a_one_shot_modifier_is_spent_once`), `tests/host/run_macro_payload_engine_tests.sh` (`test_erase_taps_backspace_at_the_text_pace`, `test_a_correction_is_isolated_without_ignoring_presses`).

## On and off

Autocorrect runs while the stored switch is on (key option 12 of settings
scalar 24,
[settings](profile-wire-v1/settings-domain.md#editor-metadata--get-0x08)), the
autocorrect override allows it and the list holds a typo. The override is
the switch keys' (`AC_ON`, `AC_OFF`, `AC_TOGG`, D-F66): allowed at boot,
never saved, read back on
[GET `0x22` page 1](profile-wire-v1/live-edits.md#runtime-get-0x22). No key
turns on what the profile has off.

With the override off the keyboard still follows words, so switching back
on finds the next word; switched in the middle of a word, that word is left
alone. A correction already being typed finishes. With the stored switch
off or the list empty, keyboard reports are not read at all.

The keyboard cannot see a password field or a terminal: `AC_OFF` or
`AC_TOGG` is how to keep it out of one.

Held by: `tests/host/run_autocorrect_tests.sh` (`switched-off`; `test_the_stage_is_idle_without_a_list`, `test_the_override_and_the_stored_switch_both_allow`), `tests/host/run_key_runtime_layer_lock_integration_tests.sh` (`test_plain_autocorrect_keys_switch_the_override`).

## The fixture

`tests/fixtures/autocorrect_v1.json`, version 1. Each case has:

| Field | Meaning |
| --- | --- |
| `name`, `rule`, `description` | The case, and the rule of the nine in the ADR it holds |
| `layout` | A host layout's slug |
| `capsLock` | The computer's Caps Lock when the case starts |
| `list` | The entries, as in `autocorrect_domain_v1.json` |
| `on` | `false` when the stored switch is off; absent otherwise |
| `events` | The keys, below |
| `result` | The text the computer ends with |
| `resultEndsWith` | In place of `result`, only its end, where the rest depends on timing |
| `screen` | The same input as the characters the computer would show with autocorrect off, `\b` for Backspace: what a text matcher is run over, with the case's list, layout and Caps Lock, to produce `result` |
| `keyboardOnly` | In place of `screen`, why the case has no text form |
| `strokes` | For one case, how many characters the correction erased and typed |

| Event | Meaning |
| --- | --- |
| `{"type": text}` | the keys that show those characters on the layout under the case's Caps Lock, one after another; `\n` is Enter and `\t` Tab |
| `{"fast": text}` | the same with no pause, so later keys arrive while a correction is typed |
| `{"key": name, "mods": [...]}` | a named key tapped: `Backspace`, `Enter`, `Left`, `Right`, or a letter with `Ctrl` |
| `{"stroke": stroke}` | a layout stroke tapped, in the host layouts' notation |
| `{"tap": key}` | a key tapped by its QMK name, with whatever modifiers are held |
| `{"press": mod}`, `{"release": mod}` | a modifier held and let go |
| `{"click": true}` | a mouse button |
| `{"hold": char, "ms": n, "repeats": n}` | a key held that long while the computer repeats it that many times |
| `{"macro": text, "fast": bool}` | a macro key pressed, whose macro types the text |
| `{"mash": char, "count": n}` | a key tapped that many times with no pause |

A small letter's capital is the one character of `wordCharacters` in
`autocorrect_domain_v1.json` that folds to it.

Held by: `tests/host/run_autocorrect_tests.sh` (the runner refuses a case without `screen` or `keyboardOnly`, and a rule without a case).

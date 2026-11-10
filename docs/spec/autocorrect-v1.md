# Autocorrect v1

The keyboard replaces what is on the profile's
[autocorrect list](profile-wire-v1/autocorrect-domain.md) as it is typed, on
any computer, with nothing installed there (D-F69,
[ADR](../adr/0069-autocorrect-is-the-firmwares-own-and-corrects-at-the-word-end.md)):
a typo with the right word, an abbreviation with a phone number or a
sign-off, a typed form with a macro. People know the feature as **text
replacement**; the code, the domain and the keys call it autocorrect. A list
entry is a **replacement** with the **typed forms** that produce it, which
the bytes and fixtures call a correction and its typos
([glossary](../../GLOSSARY.md#autocorrect)).

This file is what it does: how it reads what is typed, when and what it
replaces, the undo, when it forgets the word, how a replacement is typed or
played and what happens to keys pressed meanwhile. The list's bytes are the
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

The keyboard reads the keyboard reports it is about to send: each key going
down, with the modifiers the computer sees with it, whatever sent it (a plain
key, a behaviour's tap, a combo, a custom key). While autocorrect runs, every
key and modifier change is queued as it happens and shown to the computer
from the matrix scan, one key change a scan, so a key reaches the computer at
the scan after it was pressed. From the chosen host layout
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
last stay on screen and are not part of what a typed form matches: `'teh'`
is matched as `teh`.

The word **ends** at the next character that is not a word character (a
space, punctuation, a hyphen) or at Enter or Tab.

A word whose ending character was composed through a dead key is passed
over: it is not looked up, and the next word starts fresh. On
US-International the quote is such a character, a dead key and then Space.
The dead key reached the computer before the keyboard could know the word
had ended, so the computer is in the middle of composing, and what a
replacement erased and typed would land inside that. A text matcher cannot
tell how a character was typed, so this is one place where the keyboard
replaces less than a client's preview shows.

Backspace inside a word removes its last character. The keyboard follows 32
characters of a word; a longer word is left alone.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 2 cases: `apostrophe-after`, `apostrophes-around`, `apostrophe-inside`, `digits-are-word-characters`, `word-end-punctuation`, `word-ended-through-a-dead-key`; `backspace-inside-a-word`).

## Which typed form

When a word ends, it is folded with the word table and looked up:

1. A typed form that matches the **whole word** and is the word wins.
2. Otherwise, of the part-of-word typed forms that fit (at the **start**, at
   the **end**, or **anywhere**, where the first place it is found counts),
   the one with the most characters wins, and of equal ones the first in the
   list, which is the lower in the domain's byte order.

A client may write the three part-of-word matches with `*` where other
letters may go (`recieve*`, `*mnet`, `*tihng*`); the list stores the match,
never the `*`
([the domain](profile-wire-v1/autocorrect-domain.md#layout)).

A typed form whose replacement this host cannot type, or whose macro cannot
play, is passed over as if it were not on the list. A word found nowhere, or
still being typed, is never touched.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 3 cases: `two-typos-one-correction`, `anywhere-in-a-word`, `end-of-a-word`, `start-of-a-word`, `longest-typo-wins`, `whole-word-beats-part`, `first-in-the-list-wins-a-tie`, `more-part-of-word-typos-than-one-step`; `inside-a-longer-word`, `unfinished-word`).

## Case

Typed forms match whatever their case. A text replacement takes the case of
the part of the word the typed form matched, as the computer shows it. A letter is a
capital when the word table folds it to another character, and small when
one character folds to it, which is then its capital; any other character
has no case.

| The matched part | The replacement |
| --- | --- |
| two or more letters, all capitals (`TEH`) | every small letter as its capital (`THE`) |
| one capital, its first character, and the replacement is written without capitals (`Teh`) | its first character as its capital (`The`) |
| anything else (`teh`, `tEh`), or a replacement written with capitals (`Iphone` for `iPhone`) | as written |

Digits, spaces and punctuation have no case, so `+31 6 1234 5678` is typed
as written whatever the case of `pnumber`. When the layout cannot type that
form, the replacement is typed as written; when it cannot type that either,
the typed form is passed over. A word that already reads as the replacement
would leave it is left as it is. A macro has no case: it plays as stored.

The replacement is typed so that it shows in that case under the computer's
Caps Lock: a letter goes out as the stroke of its other case, except on
macOS, where a capital is typed as itself and a small letter cannot be typed
while Caps Lock is on.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 4 cases).

## The replacement

A text replacement is up to 128 bytes: a word, or text that is no word, with
spaces, digits and punctuation the layout types. The word after it is what
stood before the typed form, the replacement, and what followed the typed
form, apostrophes included. Only what differs is typed again: the keyboard
erases back to the first character that changes and types the rest.
`becuase` to `because` erases four characters and types four.

The key that ended the word is held back until the replacement is typed. It
then goes to the computer as the real key, still held if the person still
holds it: Enter in a chat box sends the replaced text, and a held Space
repeats.

A replacement is never typed through Unicode entry, and never takes more
than 288 key strokes, erasing included: a word of 32 characters erased and
128 typed, each of two strokes. One that would take more is passed over.

Held by: `tests/host/run_autocorrect_tests.sh` (`word-end-space`, `differing-suffix`, `word-end-enter`, `replacement-with-digits-and-spaces`, `longest-replacement`, `replacement-keeps-its-case`; `character-the-layout-cannot-type`; `test_held_keys_around_a_correction`), `tests/host/run_macro_slot_provider_tests.sh` (`test_a_correction_borrows_the_shared_window`).

## A macro as the replacement

A replacement may be a macro slot in place of text, for text of more than one
line or more than 128 bytes, or anything else a
[macro](profile-wire-v1/macro-domain.md) does. When a word that is one of its
typed forms ends:

1. the keyboard erases the typed form, as it erases before typing text;
2. the macro plays through the macro engine at its own pace, with its
   output isolated as a text replacement's is, whatever protection the macro
   is stored with: modifiers the person holds never reach it, and keys
   pressed meanwhile are held back, never ignored;
3. the key that ended the word goes to the computer, and the keys pressed
   meanwhile after it, in order.

A macro replaces the whole word only: its typed forms have no part-of-word
match. Apostrophes before the typed form stay on screen. With an apostrophe
after it the macro does not play and the word is left as typed, since the
erase would take the apostrophe along.

There is no case rule and no undo for a macro. It plays as stored whatever
the case of the typed form, and Backspace after it is only Backspace: what a
macro does (keys, shortcuts, waits) cannot be taken back by erasing.

A replacement whose macro slot is empty, or whose macro cannot play on this
host (its text needs Unicode entry the host has not set up, say), is passed
over, and nothing is erased. The check is made before the erase.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 10 cases: `macro-replacement`, `macro-replacement-has-no-case`, `macro-replacement-has-no-undo`, `macro-replacement-ended-by-enter`, `macro-replacement-ended-by-a-shifted-key`, `macro-replacement-ended-by-shift-enter`, `keys-during-a-macro-replacement`, `macro-replacement-empty-slot`, `macro-replacement-apostrophes`, `macro-replacement-whole-word-only`; `held-shift-stays-out-of-a-macro-replacement`; `test_typing_through_a_protected_macro_correction`, `test_a_full_queue_stops_a_macro_correction`), `tests/host/run_macro_payload_engine_tests.sh` (`test_a_corrections_macro_is_isolated_whatever_its_protection`), `tests/host/run_pointing_mode_key_runtime_integration_tests.sh` (`test_a_correction_drops_no_press`), `tests/host/run_macro_slot_provider_tests.sh` (`test_can_start_checks_without_playing`), `tests/host/run_profile_autocorrect_v1_tests.sh` (the `macros` vector and the macro refusals).

## Undo

When a text replacement was ended by a key that prints (a space,
punctuation) and the very next key press is Backspace, that Backspace never
reaches the computer. Instead the keyboard erases the ending character and
what the replacement typed, and types back the word exactly as it was typed.
The word is then left alone: ending it again replaces nothing.

After Enter or Tab there is no undo, since the text may be sent or the focus
moved: Backspace is Backspace. After a macro there is none either. Any other
key press between the replacement and Backspace ends the chance, and so does
holding the ending key until it repeats. So does anything that makes the
keyboard [forget the word](#forgetting-the-word) from the moment the
replacement starts, a mouse click or a switch key while it is still being
typed included: the cursor may be elsewhere by then. When the layout can no
longer type the word back (Caps Lock went on on macOS), Backspace is
Backspace.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 5 cases, `undo-a-long-replacement` and `no-undo-after-a-click-meanwhile` among them; `test_a_switch_key_during_a_replacement_ends_the_undo`).

## Forgetting the word

The keyboard forgets the word it was following in one of two ways.

**Fresh start**: the next character starts a new word, which is replaced
normally. After:

- a shortcut;
- a key that moves the cursor or deletes forward;
- a mouse button going down;
- a macro, whatever it types;
- the override being switched by key anywhere but in the middle of a word.

**Suspended**: nothing is replaced until the next word end, because the
keyboard no longer knows where the word on screen starts. After:

- Backspace with no character of the word left;
- a key held 200 ms, which is taken to repeat on the computer (after a word
  end this only ends the chance to undo, and changes nothing else);
- a dead key the layout composes nothing from;
- a keypad character;
- a word past 32 characters;
- autocorrect coming on after reports went out unread, or its override being
  switched while a word is being typed.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 6 cases, `dead-key-right-after-a-replacement` among them; `test_switching_off_forgets_the_word`, `test_a_held_ending_key_keeps_a_suspension`).

## Typing a replacement and holding keys back

A text replacement plays through the macro engine with its output isolated,
as an uninterruptible macro's is (D-F17): a neutral report and a 10 ms
settle, then each key held 5 ms and followed by 5 ms, Backspace first, then
the characters through the host layout. Modifiers the person holds never
reach it, and are back afterwards. A key that was down when the replacement
began is up for the computer from then on and is not pressed again.

Unlike a macro's, the presses that arrive meanwhile are not ignored. Every
key and modifier change since the ending key waits in the queue, the ending
key first, and comes out afterwards in the order it happened, one key change
a scan. What comes out is read as typing in its turn, so a typed form typed
while a replacement plays is replaced when its own word end comes out. A
macro key pressed meanwhile waits in the same queue and starts when its turn
comes; the keys behind it wait until it has played. The same holds while a
replacement's macro plays.

### The queue

The queue holds 240 changes, and a replacement stops once 192 wait. That is
sized from the pace above for the longest replacement the keyboard types:

| Replacement | Key strokes | Lasts | Changes a second that reach 192 before it ends |
| --- | ---: | ---: | ---: |
| 128 characters of one stroke each, for a typed form of 7 | 135 | 1.36 s | 141 |
| 128 characters of two strokes each (dead keys), for one of 32 | 288 | 2.89 s | 66 |

Typing makes two changes a character, and a few more for Shift: about 25 a
second at 120 words a minute and 45 at 200. So typing on through the longest
replacement never fills the queue; only keys arriving several times faster
than anyone types do, or a macro replacement that plays for many seconds.

The queue is never allowed to cost a key:

- A replacement does not start while more than 16 changes are waiting. The
  typed form is left as typed.
- Once 192 changes wait while a replacement is typed, or while its macro
  plays, the replacement stops where it is and the queue empties, a key
  change a scan. The word is left as far as the replacement got, which is the
  one thing given up; nothing typed is lost or reordered. The 48 places left
  take the changes that arrive while it stops.
- A change that finds the queue full all the same stays in the keyboard's
  report and is taken up at the next scan, late. That needs 48 changes within
  the few milliseconds a replacement takes to stop, or more than 240 during a
  macro a key started, which is never stopped (D-F17). Only then can a key
  pressed and released again before there is room go unseen.

A key pressed while a replacement, or a macro, holds that same key down
changes no report: the computer already has the key down. The keyboard reads
that press and its release where its own ledger of held keys records them,
and queues them like any other, so even a tap that begins and ends inside
those 5 ms comes out after the replacement. A behaviour's tap, a combo's
output and a custom key's tap are all that short: pressed and released in
one step.

A modifier is not read that way. While a replacement, or a macro, holds a
modifier (Shift for a capital, 5 ms), the person's own press or release of
that modifier is read when the replacement lets go of it. A key they press
in between is read without the change, and a modifier pressed and released
again inside that hold is not seen.

The lookup runs from the scan too: the whole word at once, then 32
part-of-word typed forms a scan. The ending key waits for it, at most 33
scans for a full list and one for a list without part-of-word typed forms.
A list published while a lookup is spread over scans (an Apply or a live
edit) starts that lookup over on the new list.

Held by: `tests/host/run_autocorrect_tests.sh` (rule 7 cases, `typing-over-the-longest-replacement` and `full-queue` among them; `test_no_correction_starts_behind_a_long_queue`, `test_a_full_queue_loses_no_key`, `test_typing_through_the_longest_correction`, `test_a_full_queue_stops_a_macro_correction`, `test_a_correction_keeps_its_pace`, `test_a_one_shot_modifier_is_spent_once`, `test_a_tap_of_a_key_the_correction_holds`, `test_a_list_published_during_a_lookup`; rule 3's `more-part-of-word-typos-than-one-step`), `tests/host/run_profile_owner_tests.sh` (the list's publication count), `tests/host/run_macro_payload_engine_tests.sh` (`test_erase_taps_backspace_at_the_text_pace`, `test_a_correction_is_isolated_without_ignoring_presses`).

## On and off

Text replacement runs while the stored switch is on (key option 12 of settings
scalar 24,
[settings](profile-wire-v1/settings-domain.md#editor-metadata--get-0x08)), the
autocorrect override allows it and the list holds a typed form. The override is
the switch keys' (`AC_ON`, `AC_OFF`, `AC_TOGG`, D-F66): allowed at boot,
never saved, read back on
[GET `0x22` page 1](profile-wire-v1/live-edits.md#runtime-get-0x22). No key
turns on what the profile has off.

With the override off the keyboard still follows words, so switching back
on finds the next word; switched in the middle of a word, that word is left
alone. A replacement already being typed finishes. With the stored switch
off or the list empty, keyboard reports are not read at all.

When reports start being read with keys held (the switch or a list has just
been applied), the computer sees nothing change: those keys stay down for
it and are not pressed again. They are read as typing from there, so a held
key that ends a word still ends the [suspension](#forgetting-the-word).

The keyboard cannot see a password field or a terminal: `AC_OFF` or
`AC_TOGG` is how to keep it out of one.

Held by: `tests/host/run_autocorrect_tests.sh` (`switched-off`; `test_the_stage_is_idle_without_a_list`, `test_the_override_and_the_stored_switch_both_allow`, `test_the_stage_starts_with_a_key_held`), `tests/host/run_key_runtime_layer_lock_integration_tests.sh` (`test_plain_autocorrect_keys_switch_the_override`).

## The fixture

`tests/fixtures/autocorrect_v1.json`, version 1. Each case has:

| Field | Meaning |
| --- | --- |
| `name`, `rule`, `description` | The case, and the rule of the ten in the ADR it holds |
| `layout` | A host layout's slug |
| `capsLock` | The computer's Caps Lock when the case starts |
| `list` | The entries, as in `autocorrect_domain_v1.json`: `correction` and `typos`, or `macro` (a slot) and `typos` |
| `macros` | For a case with a macro replacement, the text each stored macro types, by slot; a slot left out is empty |
| `on` | `false` when the stored switch is off; absent otherwise |
| `events` | The keys, below |
| `result` | The text the computer ends with |
| `resultEndsWith` | In place of `result`, only its end, where the rest depends on timing |
| `screen` | The same input as the characters the computer would show with autocorrect off, `\b` for Backspace: what a text matcher is run over, with the case's list, layout, Caps Lock and macros, to produce `result` |
| `keyboardOnly` | In place of `screen`, why the case has no text form |
| `strokes` | For one case, how many characters the replacement erased and typed |

| Event | Meaning |
| --- | --- |
| `{"type": text}` | the keys that show those characters on the layout under the case's Caps Lock, one after another; `\n` is Enter and `\t` Tab |
| `{"fast": text}` | the same with no pause, so later keys arrive while a replacement is typed |
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

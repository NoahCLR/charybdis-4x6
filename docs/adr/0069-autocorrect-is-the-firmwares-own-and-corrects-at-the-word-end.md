# Autocorrect is the firmware's own and corrects at the word end

Cited as D-F69.

The keyboard replaces what is on the profile's autocorrect list (D-F68) with
an engine of its own in `users/noah/lib/autocorrect/`. QMK's autocorrect is
not built and none of it is used: its list is compiled into the firmware, it
types corrections as US key positions, and it fires in the middle of a word.
Ours behaves the way phone and desktop text replacement does, for a typo and
for an abbreviation alike. The contract is
[`autocorrect-v1.md`](../spec/autocorrect-v1.md); these are its ten rules. A
list entry is a replacement with its typed forms (D-F68).

1. **Characters are what the layout produces.** Each key going down is read
   through the chosen host layout with the modifiers the computer sees with
   it, dead keys composing as the layout composes them and Caps Lock as the
   computer's LED reports it. A key is typing whenever that produces a
   character, so Shift+Space and Shift+Enter are.
2. **Whole words, at the end of the word.** A word is letters and digits
   with apostrophes between them. It is looked up only when a character that
   is none of those ends it, and the replacement is typed before that ending
   key reaches the computer, erasing only what differs.
3. **One replacement, many typed forms, matched whole or in part.** The
   longest typed form wins, and a whole-word one beats a part-of-word one.
4. **Case follows what is on screen**: a capitalized typed form gives a
   capitalized replacement, capitals give capitals, and a replacement
   written with capitals keeps them.
5. **Backspace right after a replacement undoes it**, when a printing
   character ended the word. Never after Enter or Tab.
6. **Only text you type.** A shortcut, a cursor key, a click or a macro
   starts the next word fresh; a Backspace past the word, a key held until it
   repeats or an unresolved dead key suspends correcting until the next word
   end.
7. **Replacements never lose or scramble typing.** Held modifiers stay out
   of a replacement, and keys pressed while it is typed come out afterwards
   in order.
8. **On and off**: the stored switch, and a runtime override the switch keys
   set (D-F66).
9. **What the layout cannot type is skipped**, never typed through Unicode
   entry.
10. **A replacement may play a macro** in place of text: the typed form is
    erased, the macro plays, then the ending key. Whole words only, no case
    rule and no undo; an empty slot is skipped.

## Where it reads and where it holds back

The engine is the **typing stage**: it sits in BK's report-copy hooks
(`QMK_REPORT_KEYS_FILTER`, `QMK_REPORT_MODS_OVERRIDE`), which the macro
engine already owns, between live key ownership and the report. That is the
one place a key's output is known whatever produced it: a plain key, a
behaviour's tap decided on release, a combo, a custom key's shortcut. It
reads each change of the outgoing report as a key or modifier going down or
up.

The hook itself only records. It runs at the bottom of the deepest reviewed
stack paths, inside key processing, so it compares the report with the one
before, queues what changed and answers with what the computer has been
shown so far; it looks nothing up. The scan reads the queue as typing and
shows it, one key change a scan. So with autocorrect on, a key reaches the
computer at the scan after it was pressed, and a press and its release never
share a report. The filters' working copies of the report became static for
the same reason, which keeps those paths inside their budget without a
larger stack.

Holding the ending key back at the same place keeps the key runtime out of
it. Presses keep being processed live, with their real timing, so tap and
hold decisions, layers and locks are not disturbed; only what the computer is
shown is delayed. The stage queues the changes it has not shown yet and shows
them later in order, one a step. Deferring the physical events instead would
have replayed presses with compressed timing through QMK's tapping and the
key runtime's reducer, and changed what a held key decides.

The lookup and every start (a correction, an undo, a queued macro) run from
the matrix scan too, in bounded steps (D-F54's rule for callbacks).

## Typing a correction: an amendment to D-F17

A correction plays through the macro engine, from the one shared window
(D-F59), as a third source beside a stored macro and a direct program. Its
output is isolated exactly as an uninterruptible macro's: only its own keys
and modifiers reach the computer. Its input is not. D-F17 ignores new presses
during uninterruptible playback and never replays them, which is right for a
macro the person started and wrong for a correction they did not ask for in
the middle of a sentence. So for this source the press guard stays open
(`macro_payload_engine_ignores_presses`), and the typing stage holds the
presses back and replays them.

A macro key pressed during a correction waits in the stage's queue rather
than finding the engine busy. The engine gained one step for this source,
*erase*: Backspace a counted number of times at the text pace. It is played
only; no stored macro holds it.

The pace is fixed here because the build defines no `TAP_CODE_DELAY`: 5 ms
held and 5 ms after each key of a correction. Held-back changes come out one
key change a scan.

## A macro as the replacement

A replacement of more than a line, or more than 128 bytes, is a macro the
profile already holds, so the list names its slot instead of growing a
second kind of long text. The stage erases the typed form with the same
erase step, and when that has played starts the slot's macro through the
slot provider exactly as its key would, protection and pace included; the
ending key and everything pressed since stay in the queue behind it, as they
do behind a macro key pressed during a correction.

Two things a text replacement has are left out on purpose. There is no case
rule, because a macro is steps, not letters. There is no undo, because
Backspace cannot take back what a macro did: it may have pressed Enter,
switched windows or waited. The stage asks the provider whether the macro
would start before it erases anything
(`macro_slot_provider_can_start`), so an empty slot, or a macro this host
cannot play, leaves the typed form alone.

## The queue is bounded and never costs a key

The queue holds 240 changes: 480 bytes, sized for the longest replacement.
At the pace above 128 characters for a short typed form take 1.4 s, and the
worst case, a 32-character word erased and 128 characters of two strokes
each, 2.9 s. Typing makes some 25 changes a second at 120 words a minute and
45 at 200, so typing through the worst case leaves the queue under 130.
Three rules keep it from dropping or reordering anything:

- no replacement starts with more than 16 changes waiting, and none is
  longer than 288 key strokes;
- once 192 changes wait while a replacement is typed, or while its macro
  plays, it is cancelled where it stands, so the queue can empty; the word is
  left partly replaced, which is the one thing given up. Stopping at 192
  leaves 48 places for what arrives while it stops;
- a change that finds the queue full all the same is not consumed: it stays
  in the report and is read again at the next scan, while the queue empties
  a key change a scan.

The last rule is where a key can still be missed: a key pressed and released
again while the queue has no room, which takes 48 changes in the few
milliseconds a replacement needs to stop, or more than 240 during a macro a
key started (D-F17 never stops one). The first design stopped a replacement
only when the queue was already full, in the report hook; a tap shorter than
the stop then went unseen, and the stop ran inside the hook that the deepest
stack paths end in. Stopping early, from the scan, removes both.

A key pressed while a replacement holds that same key down is in no report:
the computer already sees the key down, and the ledger sends nothing for a
usage's second owner. The stage first read such a key only once the
replacement let go of it, which lost every tap shorter than the 5 ms hold;
and a behaviour's tap, a combo's output and a custom key's tap are pressed
and released in one step. So the ledger tells the stage whenever the owners
of a usage change (`noah_typing_stage_note_owners`), and for a usage the
playing macro holds the stage takes the person's press and release from
there. Modifiers are not read that way: the person's press or release of a
modifier the replacement is holding is read when the replacement lets go, a
key pressed in between is read without it, and a modifier tapped inside
that hold is unseen. Reading those needs the modifier owners counted apart
from the macro's inside the report hook, which the deepest stack paths end
in.

The alternative, dropping presses as D-F17 does, loses typing; a larger queue
only moves the limit.

## Cost

With the stored switch off or an empty list, which is what ships, the stage
is not in the report path. The layouts gained reverse tables (what each key
types, what each dead key composes) beside the forward ones; the forward
entries shrank from 8 to 6 bytes, so every table together stays inside the
40 KiB tripwire. The engine's state is under 656 bytes on the master half,
480 of them the queue, with 704 more of static workspace for the one
replacement being worked on, which keeps a 128-byte replacement off the
scan's stack. Its replacement uses the macro window, so no second window is
held.

## What it does not do

It cannot see which application or field has focus, so it cannot stay out of
a password field or a terminal by itself: the switch keys are for that. A
replacement's text is stored in the profile as written, readable by anyone
who reads the keyboard, so it is no place for a password. It
does not follow the keypad, or text a macro types. It counts nothing and
writes no flash.

Contract: [`autocorrect-v1.md`](../spec/autocorrect-v1.md),
[`macro-playback.md`](../architecture/macro-playback.md#corrections).

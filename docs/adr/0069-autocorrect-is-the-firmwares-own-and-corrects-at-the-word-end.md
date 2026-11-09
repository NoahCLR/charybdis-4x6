# Autocorrect is the firmware's own and corrects at the word end

Cited as D-F69.

The keyboard corrects the typos on the profile's autocorrect list (D-F68)
with an engine of its own in `users/noah/lib/autocorrect/`. QMK's autocorrect
is not built and none of it is used: its list is compiled into the firmware,
it types corrections as US key positions, and it fires in the middle of a
word. Ours behaves the way phone and desktop autocorrect do. The contract is
[`autocorrect-v1.md`](../spec/autocorrect-v1.md); these are its nine rules.

1. **Characters are what the layout produces.** Each key going down is read
   through the chosen host layout with the modifiers the computer sees with
   it, dead keys composing as the layout composes them and Caps Lock as the
   computer's LED reports it. A key is typing whenever that produces a
   character, so Shift+Space and Shift+Enter are.
2. **Whole words, at the end of the word.** A word is letters and digits
   with apostrophes between them. It is looked up only when a character that
   is none of those ends it, and the correction is typed before that ending
   key reaches the computer, erasing only what differs.
3. **One correction, many typos, matched whole or in part.** The longest
   typo wins, and a whole-word typo beats a part-of-word one.
4. **Case follows what is on screen**: a capitalized typo gives a
   capitalized correction, capitals give capitals, and a correction written
   with capitals keeps them.
5. **Backspace right after a correction undoes it**, when a printing
   character ended the word. Never after Enter or Tab.
6. **Only text you type.** A shortcut, a cursor key, a click or a macro
   starts the next word fresh; a Backspace past the word, a key held until it
   repeats or an unresolved dead key suspends correcting until the next word
   end.
7. **Corrections never lose or scramble typing.** Held modifiers stay out of
   a correction, and keys pressed while it is typed come out afterwards in
   order.
8. **On and off**: the stored switch, and a runtime override the switch keys
   set (D-F66).
9. **What the layout cannot type is skipped**, never typed through Unicode
   entry.

## Where it reads and where it holds back

The engine is the **typing stage**: it sits in BK's report-copy hooks
(`QMK_REPORT_KEYS_FILTER`, `QMK_REPORT_MODS_OVERRIDE`), which the macro
engine already owns, between live key ownership and the report. That is the
one place a key's output is known whatever produced it: a plain key, a
behaviour's tap decided on release, a combo, a custom key's shortcut. It
reads each change of the outgoing report as a key or modifier going down or
up.

Holding the ending key back at the same place keeps the key runtime out of
it. Presses keep being processed live, with their real timing, so tap and
hold decisions, layers and locks are not disturbed; only what the computer is
shown is delayed. The stage queues the changes it has not shown yet and shows
them later in order, one a step. Deferring the physical events instead would
have replayed presses with compressed timing through QMK's tapping and the
key runtime's reducer, and changed what a held key decides.

The lookup and every start (a correction, an undo, a queued macro) run from
the matrix scan in bounded steps, never from the report hook, which is
reached from deep inside key processing (D-F54's rule for callbacks).

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
held and 5 ms after each key of a correction, and 2 ms between held-back
changes as they come out.

## The queue is bounded and never costs a key

The queue holds 64 changes. Three rules keep a full queue from dropping or
reordering anything:

- no correction starts with more than 16 changes waiting, and none is longer
  than 48 key strokes, so one lasts under half a second;
- if the queue still fills while a correction is typed, the correction is
  cancelled where it stands, so the queue can empty; the word is left partly
  corrected, which is the one thing given up;
- a change that does not fit is not consumed: it stays in the report and is
  read again at the next scan, while the queue empties one change a scan.

The alternative, dropping presses as D-F17 does, loses typing; a larger queue
only moves the limit.

## Cost

With the stored switch off or an empty list, which is what ships, the stage
is not in the report path. The layouts gained reverse tables (what each key
types, what each dead key composes) beside the forward ones; the forward
entries shrank from 8 to 6 bytes, so every table together stays inside the
40 KiB tripwire. The engine's state is under 304 bytes on the master half
and its correction uses the macro window, so no second window is held.

## What it does not do

It cannot see which application or field has focus, so it cannot stay out of
a password field or a terminal by itself: the switch keys are for that. It
does not follow the keypad, or text a macro types. It counts nothing and
writes no flash.

Contract: [`autocorrect-v1.md`](../spec/autocorrect-v1.md),
[`macro-playback.md`](../architecture/macro-playback.md#corrections).

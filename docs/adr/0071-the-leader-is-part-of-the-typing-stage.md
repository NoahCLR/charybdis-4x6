# The leader is part of the typing stage, and drops what does not match

Cited as D-F71.

Text replacement has a Leader key (Noah, 2026-11-10). After it, what is
typed is held on the keyboard and never reaches the computer; when it is a
typed form of a replacement that fires after the leader, that replacement is
sent, text or a macro, and anything else is dropped. It is built in the typing stage
(D-F69), not with QMK's Leader. QMK's collects at most five keycodes, key
positions rather than characters, ends on a 300 ms timeout and compares the
sequence in keymap C. The typing stage already reads every outgoing report
as characters through the host layout and holds key changes back in order,
so the leader is that stage holding characters instead of showing them: it
needs no place in the key runtime, the reducer (D-F55) or combos, and it works
on every host layout. Contract:
[`autocorrect-v1.md`](../spec/autocorrect-v1.md#the-leader).

**The key.** QMK's `QK_LEADER`, at QMK's value, is an action kind of the
firmware's own, placed wherever a switch key goes (a key, a behaviour's tap
or hold, a combo's output), acting at its tap or press and putting nothing in
a report. It is a capability, so Profile Wire's minor version goes to 2.2
(D-F28). With no replacement that fires after the leader on the list it does
nothing.

**What is stored.** How a replacement fires is the replacement's: its
*trigger*, in bits 1–2 of its correction row's kind byte, as a word ends
(`0`), after the leader alone (`1`) or both (`2`), in domain `0x80` version 2.
Its typed forms are one list, words as every typed form is (word characters
folded, an apostrophe only between two), so a client switches how a
replacement fires without touching them; after the leader a typed form is its
whole text, whatever its match. A **leader index** after the part index lists
the typed forms of replacements that fire after the leader, in the typos'
byte order, so whether what is held is one, and whether a longer one begins
with it, is a binary search over it; the part index lists only those of
replacements that fire as a word ends, so a part-of-word typed form of one
that fires both ways is in both, and the largest list grows by 2,048 bytes to
36,874. A word end passes over a replacement that fires after the leader
alone, and a list of only those keeps the stage out of the report path as one
without typed forms does.

At first a typed form after the leader was a fifth match on the typo row, any
characters but control characters, Space and punctuation included (`k r`).
One text could then be only one typed form, so a replacement could not be
typed `adn` both as a word ends and after Leader, and a client had to keep
two lists of inputs. Noah, trying the app, asked for a replacement to be made
leader-only or both cleanly, with its inputs shared, and chose words-only
typed forms over keeping spaces for leader-only replacements (2026-11-11):
the switch is then always clean, and a code like `kr` serves as `k r` did.

**In the report path.** The leader ignores both switches, the stored one and
the override (Noah). With text replacement off, or no replacement that fires
as a word ends, the stage is out of the report path as before, and Leader
brings it in:
its action sends the report the stage has missed and the stage reads it then,
so keys already down are read before Leader and only what follows is held. A
behaviour's tap is often decided at the next key's press, but the key runtime
types a pending tap before it handles the press that ended it, so that key
still comes after Leader. Once leading is over and nothing waits, the stage
leaves the path again, so with text replacement off a key costs nothing more
except while the leader holds. Keeping the stage in the path whenever the
list held a replacement that fires after the leader was built first and left: it delayed
every key by a scan, with the switch off, for nothing the read at Leader does
not already give.

**In queue order.** Leader is a change in the stage's queue, not an action
taken where it is pressed: keys pressed before it come out first, through any
replacement still being typed, and only what follows it is held.

**How it ends.** A character that makes what is held a typed form that fires
after the leader, and that no longer one begins with, sends it at once; Enter, or the leader
timeout from the last key held or taken back, sends one a longer one begins
with (`abc` beside `abcd`). A character that leaves what is held the start of
none ends leading at once. Escape ends it; Backspace takes back a character,
and with none held ends it; Leader again starts over; any other key that types
no character ends it and acts as itself. No ending that sends nothing lets
what was held out: early exit, Escape, a miss on Enter or the timeout alike.
These are Noah's rules and the grilling's defaults (2026-11-10).

**What is sent.** Text is typed exactly as written, through the host layout
and under its Caps Lock, as a correction is (the same playback, the press
guard open), with no case rule, since what was typed after Leader says
nothing of the case wanted, and nothing to erase; a macro plays as a
replacement's does. There is no undo: the computer never saw what was typed,
so there is nothing to put back. The next word starts fresh.

**The timeout** is settings scalar 32, 1 to 10 seconds, 2 compiled
(`NOAH_LEADER_TIMEOUT_MS`), appended after the last scalar: settings version
13.

**Its state.** The held characters are kept where the word is: leading has
no word, since Leader starts the next one fresh, and the stage's state had
twelve bytes left under its regression line. The leader adds a flag and the
time of its last key. Leading is engine state, which lighting shows as a
case of replacement feedback held until it ends (D-F72), and the other half
learns as it learns the rest (D-F58). What the leader sends flashes as the
replacement it is, text replaced or macro replaced, on the key that ended
leading, the character that made the match or Enter, and on the Leader key
that began leading when the timeout sent it (D-F70, D-F72): the leader shares text replacement's confirmation
(Noah, 2026-11-11). A leader that sends nothing flashes nothing.

**The format.** Profile schema 4.6, a step of its own after 4.5 (D-F70's
precedent): settings version 13 and autocorrect version 2 are its shares. The
largest list is 36,874 bytes and the maximum profile 116,771. A schema 4.5
profile, a settings version 12 or an autocorrect version 1 domain is refused,
not translated (D-F10), and like every format change this returns saved
profiles to the compiled defaults.

**Cost per half**, linked, from the instrumented pair
(`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh`, QMK
`1168001cfd`, both halves alike): SRAM0–3 `.data + .bss` is 205,128 bytes,
24 more than before, 636 below the 205,764-byte regression tripwire, which
does not move (8 of them hold, for each published list, whether a
replacement fires as a word ends); the stage's state among them is 648 bytes of its 656-byte
line, and the list handle 28 bytes, its line raised from 24 for the fifth
count. The list's validation state stays at 156 bytes, its text offset held
in 16 bits and a macro slot in a byte. The worst reviewed stack path is
unchanged, 1,904 of 1,920 bytes; the Leader key's press, which can send a
report into the typing stage, is a reviewed path of 1,208. The instrumented
image is 306,944 bytes on each half, 1,792 more on the right and 2,048 on the
left. These are link-time figures and reviewed-path estimates; nothing
measures the leader's time, or a stack high-water, on the keyboard.

**What it does not do.** It holds characters only: a sequence of keys that
type nothing is a macro's. It sees no application or field, as text
replacement does not. QMK's Leader and `LEADER_ENABLE` stay out of the build.

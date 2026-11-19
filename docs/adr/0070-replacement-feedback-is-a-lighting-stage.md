# Replacement feedback is a lighting stage, flashed on the key that ended the word

Cited as D-F70.

The computer shows a replacement only as changed text, which is easy to miss
and says nothing when it should have happened and did not. So the keyboard
flashes when text replacement (D-F69) changes what was typed (Noah,
2026-11-10). It is a lighting **stage** of its own, replacement feedback,
with everything a stage has: a bit in the stage-enable mask, colours, a
locality (one per case since D-F73) and group rows. Chosen over a fourth behaviour-feedback colour,
because behaviour feedback is the state of a key that has a behaviour, read
from the key runtime (D-F57), and a replacement is neither; folding it in
would have put a rule about typing into `feedback.c`. Contract:
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#drawing).

**What it looks like.** One flash, of a length of its own: settings scalar
31, 200 ms compiled (`RGB_AUTOCORRECT_FEEDBACK_FLASH_MS`). It first lasted
one flash half-period, the setting a behaviour-feedback pulse lasts, so that
a tap that commits and a word that is replaced read as the same kind of
event; at Noah's direction (2026-11-10) the two were set apart so each can be
tuned alone, and the compiled 200 ms keeps what he saw. It begins when the
replacement begins to be typed, not when it is found or finished. That
moment is a transition the typing stage makes, and the flash means the same
for a two-letter replacement and a 128-byte one; lighting for as long as the
replacement types would have made the common case, a few characters in some
tens of milliseconds, too short to see. One flash shows at a time and a
newer one replaces it. The stage paints last, over behaviour feedback: it is
the newest and shortest thing on the board.

**The cases.** Three semantics, each with its own colour: a typed form
replaced with text, a typed form replaced by its macro, and a replacement
undone by Backspace. A macro replacement lights like a text one, at the
moment its typed form begins to be erased, in a colour of its own so "fixed
a typo" and "ran my macro" can be told apart. The undo lights because it is
the one time Backspace does something other than delete a character, and in
its own colour because it means the opposite of the other two. A colour of
saturation and value 0 paints nothing for its semantic, as a layer colour
does, so each case can be left off by itself; a literal black flash was not
worth a policy byte. Group rows name one semantic or all three, the
all-semantics rows painting first, as behaviour feedback's do. A word that
ends and is left as typed shows nothing: the stage reports what the keyboard
did. Leading joined them later as a fourth case, held rather than flashed
(D-F72).

**Localities.** Both halves, the left half and the right half are as
everywhere. "The half of the key that caused it" and "only the keys that
caused it" mean the key that **ended the word**: the Space, Enter or
punctuation key whose press the replacement answered, every key of a combo
whose output it was, and for an undo the Backspace. The keys of the typed
word were considered and left out. The typing stage follows a word as
characters, through Backspace, dead keys and the layout; a second record of
which keys produced them would have to follow every one of those edits, for
a flash that lands under fingers that have already moved on. The ending key
is one key, pressed at the moment the flash answers.

Naming that key needed one new fact, since the typing stage reads usages in
keyboard reports and no key positions. Whatever is about to put a key in a
report notes its matrix position for the stage first
(`noah_typing_stage_noted_key`): a key QMK registers as the hook hands its
press on, a tap where it is dispatched, a hold where it is registered. The stage keeps the
noted key with each usage it queues going down, in the values the queued
change's kind byte had spare, so the queue of 240 changes grew by nothing,
and takes the ending key from the queue when a word end or an undo is
decided. Chosen over reading "the newest key down" when the replacement
starts, which names the wrong key whenever the next key is already down, the
ordinary case in fast typing. The note is a position stored where it is
already in hand, not a call: the notes sit on the deepest reviewed stack
paths, sixteen bytes from their budget, and a call or a computed index cost
those paths a frame slot each. A combo's footprint is taken when the stage
reads the ending key, the scan after its press, so the combo may be up
before the lookup ends; only an ending key that waited in the queue behind
an earlier replacement can have lost it by then. A usage that goes down from no key's press or
action, or is read from a report after one whose changes did not fit the
queue, has no known key; its flash
paints both halves under the half locality and every key under the keys
locality, as a pointing mode with no owner key does.

**The other half.** Only the master half runs the typing stage, so it sends
what shows (the semantic and the key footprint, nine bytes) in the base
runtime-state packet, on change and while active, and the slave half draws
it with the same stage (D-F58). The master half ends the flash; the slave
half never times one, so on a link that drops the packet that ends it the
flash outlasts its time there by the retry delay, as any mirrored state
does.

**Defaults.** The compiled defaults switch the stage on, as they do every
compiled stage, with the half of the key as the locality, the one my
behaviour and combo feedback use, green for text, magenta for a macro and red
for an undo, and the thumb LEDs as an accent for all three. The compiled list
ships empty (D-F68), so nothing flashes until a list exists.

**The format.** The lighting domain becomes version 5: stage bit 5, the
replacement group-row count in header byte 14 (byte 15 stays reserved), and
two sections at the end, a ten-byte record of three colours and a locality
and its five-byte rows. The rows count toward the 640 of D-F63, so the
largest lighting domain grows by the record alone, to 4,649 bytes. The
flash's length is settings scalar 31, appended after the last scalar as new
scalars always are, the retired 10 to 14 staying retired: settings version
12, 32 scalars, every offset after them four bytes later and the largest
settings domain 10,697 bytes. The maximum profile is 114,717 bytes. A version
4 lighting domain and a version 11 settings domain are refused, not
translated (D-F10), and the profile schema becomes 4.5 (D-F28): a step of
its own rather than a share of 4.4, so a firmware released between the two
is told from this one by its number. Both domain steps are shares of 4.5,
which no release has carried. Like every format change it returns saved
profiles to the compiled defaults.

**Cost per half**, linked, from the instrumented pair
(`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh`, QMK
`f57872b4f8`, both halves alike): SRAM0–3 `.data + .bss` is 205,104 bytes,
152 more than before, 660 below the 205,764-byte regression tripwire, which
does not move. That is what shows and when it began, the ending key's
footprint, the slave half's copy and the last packet sent, the frame's copy,
the noted key, and four bytes on each lighting view held. The lighting view and its validation state are 44
and 76 bytes (were 40 and 72). The worst reviewed stack path is unchanged,
1,904 of 1,920 bytes. The instrumented image is 305,152 bytes of flash on
the right half and 304,896 on the left, 2,560 and 2,304 more (a UF2 counts
in 256-byte blocks), under the 1 MiB the image may take. These are link-time figures
and reviewed-path estimates; nothing measures the stage's time, or a stack
high-water, on the keyboard.

# Leading is a case of replacement feedback, held until it ends

Cited as D-F72.

What is typed after the Leader key never reaches the computer (D-F71), so
nothing on the screen says the keyboard is leading, and what was mistyped
vanishes without a trace. The lighting shows it (Noah, 2026-11-11: "rgb has
to show that leader input is active and send confirmation"). **Leading** is a
fourth case of replacement feedback (D-F70): semantic `3`, a colour of its
own, under that stage's switch, locality and accents. It is the typing
stage's, as the other three are, so the same module notes it and the same
packet carries it. A stage of its own was weighed and left: it would take
header byte 15, the lighting header's last reserved byte, for its row count;
it would be a second reader of the typing stage and a second field in the
runtime-state packet; and it would be a ninth tab on Ark's Lighting screen,
whose stage strip has no room for one at 1440 px. Contract:
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#drawing).

**Held, not a flash.** Leading shows, steady, from the moment the stage reads
Leader until leading ends by any of its endings, however long the leader
waits. Leader again, which starts over, and a Backspace that leaves
characters held keep it on. Lighting never times it: the typing stage says
when it begins and when it ends (`noah_autocorrect_feedback_lead_begin` and
`_end`), into the record of what shows, as behaviour feedback reads the key
runtime's state rather than a clock (D-F57). `noah_autocorrect_leading`,
which D-F71 kept for lighting to read, is gone: lighting reads what shows,
and nothing else asked.

**The confirmation is shared** (Noah, 2026-11-11: "they should share the
confirmation"). What the leader sends flashes text replaced or macro
replaced, in that case's colour, on the key that ended leading, as D-F71
notes it; there is no case for a leader send. One colour fewer to set, and
the confirmation reads as what it is, a replacement typed for you. The send's
flash takes leading's place with nothing between them: leading ends and the
flash begins in the same step of the stage. An ending that sends nothing
shows no flash, and leading's colour goes at once: early exit, Escape, Enter
or the timeout on no exact input, Backspace with nothing held, a key that
types no character, and a typed form after the leader passed over because the
layout cannot type it or its macro cannot play. That is D-F70's rule for a
word left as typed: the stage shows what the keyboard did, and leading that
ends with no flash says nothing was sent. A flash for a leader that sends
nothing would be a case of its own; it was offered to Noah and not asked for.

**Where it paints.** Leading's key is the Leader key that began it, a
combo's every key when it was a combo's output, as the other cases' keys
are: locality `3` paints its half and `4` it alone, `0` both halves and `1`
and `2` one half. A Leader no key names (an action outside the matrix)
paints as a flash whose key is not known does: every key of both halves
under `0`, `3` and `4`. At first leading named no key, so the half of the
key, the default, lit both halves while leading, as Noah chose (2026-11-10);
trying it, he found the two key localities did nothing for leading, and
chose the Leader key as its key (2026-11-12). The stage keeps one locality,
so the half of the key, the default, follows the key that caused each case,
the Leader key for leading and the key that ended it for a leader's send. A
send at the timeout has no key that ended it, so it flashes on the Leader key
too (Noah, 2026-11-12), with the footprint leading showed. Accents for semantic `3` light the LED sets they
name, and the `0xff` rows cover all four. A colour of saturation and value 0
paints nothing for leading, as for the others, but a coloured accent for it
still paints, so leading can show on a few LEDs alone.

**One at a time.** Leading takes the place of a flash still showing when it
begins. No word ends while the leader holds what is typed, so nothing else
can begin meanwhile.

**Switches.** With stage bit 5 clear nothing paints. The text replacement
switches, the stored one and `AC_OFF`, leave the leader alone (D-F71), and
its colour with it.

**The other half.** Leading is a kind of what shows, in the nine bytes the
base runtime-state packet already carries (D-F70): sent at once when it
begins and when it ends, and with the 250 ms heartbeat while it holds, since
it is active state. The slave half shows it from the packet that carries it
to the packet that ends it and times nothing (D-F58). It has no packet of its
own.

**The idle timeout** applies as it does to every stage, counting from the
last input. The leader timeout, at most 10 seconds, ends leading long before
the compiled 15 minutes; an idle timeout set shorter than the leader's can
turn leading dark, and the spec says so.

**The format.** The lighting domain becomes version 6: the replacement
feedback record gains the leading colour after the undone colour and before
the locality, 13 bytes, and semantic `3` is a valid row selector. Putting the
colour in a record of its own after the rows was weighed and left: a
validation step reads a record into a buffer the 16-byte header sizes, so 13
bytes still take one step and no larger buffer, and one record keeps a colour
per semantic in semantic order for the codec, the running view, the compiled
encoder and Ark. The largest lighting domain is 4,652 bytes and the maximum
profile 116,774. The profile schema becomes 4.7, a step of its own after the
leader's 4.6 (D-F70's precedent): the two are separate changes, this one
stacked on the leader's, and should they land together they could share 4.6.
A version 5 lighting domain and a schema 4.6 profile are refused, not
translated (D-F10), and like every format change this returns saved profiles
to the compiled defaults.

**Defaults.** The compiled defaults light leading in blue, `HSV(170, 255,
max)`, apart from the green, magenta and red of the other three, and the
thumb accent for every case covers it. The compiled list ships empty, so
nothing leads until a list has a typed form after the leader.

**Cost per half**, linked, from the instrumented pair
(`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh`, QMK
`1168001cfd`, both halves alike): SRAM0–3 `.data + .bss` is 205,128 bytes,
unchanged, 636 below the 205,764-byte regression tripwire, which does not
move. What shows was already nine bytes and still is. Every reviewed stack
path is as it was on the leader's branch, the worst 1,904 of 1,920 bytes; the
lighting render, whose read of the record grew by three bytes, is not a
reviewed path. The instrumented image is 307,200 bytes on each half,
256 more on each (a UF2 counts in 256-byte blocks). These are link-time figures and reviewed-path estimates; nothing
measures the stage's time, or a stack high-water, on the keyboard.

**Ark follow-up:** lighting version 6 and schema 4.7 in its codec, the
leading case on its Replacement feedback tab, and the renamed vectors.

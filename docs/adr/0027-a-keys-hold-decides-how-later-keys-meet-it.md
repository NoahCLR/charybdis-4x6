# What a key's hold does decides how a later key meets it

Cited as D-F27.

Extends D-F02 and D-F03. When another key is pressed while a handled key is
down and undecided, the undecided key is judged by what its hold would do,
not by its keycode family:

1. A hold that changes other keys (a layer, modifiers alone, a pointing mode;
   every `LT`, `MT` and `OSM`; a layer held until release) makes later keys
   wait.
2. A fallback hold that is its own modifier registers.
3. Anything else is settled as a tap first.

Before, class 3 sent the later key at once and its own tap only on release,
so `, ` typed as ` ,`. Making class 3 wait would delay every key typed after
a symbol; settling every class at the next press was rejected by D-F03.

**Releases that wait** and **releases that settle**: a modifier or layer key
let go while such a key is undecided still applies to it, as QMK's tapping
buffer keeps such a release, so Cmd let go before F still types Cmd+F.

**Every dual-role key**: every `LT`, `MT` and `OSM` key is decided by the
runtime with or without a behaviour row, and a key without a row behaves as
with an empty one, so QMK's tapping engine classifies no record (Noah,
2026-10-18). Chosen over keeping QMK's engine for keys without a row, which
kept two engines that felt different; the cost is QMK's tap-hold options
(permissive hold, hold on other key press, Flow Tap, Chordal Hold) and its
quick-tap repeat.

**Rows without steps**: a row without steps is no row, so a plain key with
one stays QMK's, and adding or dropping an empty row never changes a key.

Contract: [`INTERACTION_MODEL.md`](../INTERACTION_MODEL.md), "Keys pressed
while a tap/hold key is undecided".

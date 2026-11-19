# A colour of saturation and value 0 is passthrough, wherever it is owned

Cited as D-F74.

A colour of saturation and value `0` meant three things. A layer with that
colour painted nothing, and the layers below and the background showed
through. A replacement-feedback case with it painted nothing while the
other cases still lit (D-F70, D-F72). A pointing slot, the combo colour and
a behaviour-feedback case painted it as black, and the area went dark while
it showed. Only the stage switch made those three paint nothing, and then
for every slot or case at once. The compiled pointing slots past the named
ones are `0, 0, 0`, described as starting uncoloured, so a mode in one of
them blacked out its half. One rule now covers them all (Noah, 2026-11-12:
"shouldnt it just all be passthrough?").

**The rule.** Every colour a layer, a pointing slot, the combo feedback or a
feedback case owns is **passthrough** when its saturation and value are `0`,
whatever its hue. A passthrough colour paints nothing, and what is beneath
shows. A group row whose colour is black takes the colour of its layer, slot
or case, so it paints nothing for a passthrough one. A coloured row still
paints, so an owner can show on its accents alone. The other slots and cases
still light. **Black**, value `0` with saturation above `0`, turns the LEDs
off: the area goes dark, covering what is beneath. Contract: [`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#conventions).

**Who decides is unchanged.** A passthrough behaviour-feedback case still
decides the area the newest key decides, as it does through its flash's dark
phase. So when the newest key holds a passthrough case, what is beneath shows
there, not an older key's case. Replacement feedback shows one case at a
time, so a newer passthrough case takes the place of the one showing. Where
cases share an area by case order, a passthrough case paints nothing and the
earlier ones show.

**Two colours keep their meaning.** Exact `0, 0, 0` in a group row still
means the owner's colour (it paints the owner's colour on a group outside the
owner's locality, which passthrough cannot do). The auto-mouse fade's end
colour is still literal: the fade has its own mode that ends on what is
beneath, so a passthrough end colour would only repeat it.

**Weighed and left.** Keeping black for pointing slots, combos and behaviour
feedback, and fixing only the editor's words, would have left one stored
value meaning two things by table.

**Words.** In what a person reads, passthrough is **passthrough** and the
dark colour is **off**, because its LEDs are off (Noah, 2026-11-12: "off is
better than black, the leds are off"). Off never stands for passthrough, so
the two cannot be confused. Black stays the colour's name in HSV and in code.

**Format.** No new bytes. The meaning arrives with lighting version 7 and
profile schema 4.8 (D-F73). That change already returns saved profiles to the
compiled defaults, so no profile saved under the old meaning is read under
the new one. The compiled pointing slots 8 to 31 now paint nothing, as their
comment said.

**Ark follow-up:** one off test, saturation and value `0` as the firmware's,
in place of value alone. The words **Make passthrough** and **Passthrough**
for every layer, slot, combo and case colour, and an **Off** choice that
stores value `0` with saturation. The Behaviour feedback list says
passthrough or off, each for what it is. Accents keep "same as the case's
colour" and the fade end stays literal.

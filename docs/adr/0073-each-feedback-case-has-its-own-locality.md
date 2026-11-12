# Each feedback case has its own locality, and each tap branch is a case

Cited as D-F73.

A pointing slot has its own colour, locality and accents, and a layer its own
colour, mode and accents. Behaviour feedback and replacement feedback had one
locality for every case, and behaviour feedback's four tap branches shared
one accent semantic, so an accent for the double tap also lit for the
triple tap (Noah, 2026-11-12: "I would like the behaviors and text
replacement to work the same way that PD modes and layers work"). Now each
**case** of the two stages has its own colour and its own locality, and an
accent can name any one case or all of them. Contract:
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md).

**The cases.** Behaviour feedback has seven: the tap branch pending for tap
counts 2, 3, 4 and 5 (a deeper count takes the last branch's colour,
locality and accents, as it took its colour), tap committed, hold and long
hold. Replacement feedback keeps its four (D-F70, D-F72): text replaced,
macro replaced, undone and leading. What stays one setting for a stage says
when, not where: behaviour feedback's tap-commit policy and flash interval
(settings scalar 15), replacement feedback's flash length (scalar 31).
Combo feedback has one colour, so one case, and is unchanged.

**Accent semantics.** Behaviour feedback's semantics keep their numbers and
gain three: `0` the tap branch pending for tap count 2, `1` tap committed,
`2` hold, `3` long hold, and `4`, `5`, `6` the tap branch pending for tap
counts 3, 4 and 5 or more; `0xff` all. Semantic `0` meant any tap branch and
now means the first; keeping the tiers' numbers keeps every stored row of
theirs the same bytes. Renumbering all seven in case order was weighed and
left: no profile is translated (D-F10), but the tiers' rows, fixtures and
Ark's words would all have moved for an order no one reads. A tap branch's
accent paints while any key shows that branch pending, whichever key is
newest, so the stage finds it by reading every key's state, not the newest
owner of "a tap branch pending". A `0xff` row is the exception: it takes
the tap branches as one semantic, as before this decision, shown while the
newest key with a tap branch pending shows it and, when black, in that key's
branch colour, then tap committed, hold and long hold, each while the newest
key of that tier shows it. So the compiled black accent for every case
paints the same with keys at different tap branches. Replacement feedback's
semantics are unchanged.

**Where a behaviour case paints.** Several keys can show behaviour feedback
at once, in different cases, so each case's locality now has an area of its
own and the areas stack:

- Every case that shows paints its area in its colour, widest area first: both
  halves (`0`), then one half (`1`, `2`), then the half of the key (`3`),
  then the keys alone (`4`). A narrower area paints over a wider one, so a
  key's flash under the keys locality stays seen over a hold that lights
  both halves.
- Where two cases share an area, the newest key's case decides it, as the one
  locality did: the newest key with feedback for areas `0`, `1` and `2`, the
  newest key on that half for `3`. It decides the whole area through its
  flash's dark phase too, as before, so a flashing hold still flashes its
  half. When the newest key's case is not one of them, the later case in case
  order paints over the earlier, each only while visible.
- When the newest key shows no case at all, which an owner whose footprint
  the semantic map lacks or a slave frame between two packets can give, the
  areas it decides stay dark, as with one locality. The case order applies
  only when the newest key's case has another locality, or no key is newest.
- Under `3`, a case paints each half on which a key shows it; under `4`, each
  key showing it paints its own key.

So when every case has the same locality, which the compiled defaults do,
the stage paints what it painted with one locality. One moment differs, on
the slave half alone: its semantic and newest-key maps arrive in separate
packets, and for the frame between them a key's case can show with no newest
key named, which now paints by case order where the one locality left the
area dark, so a case can light a frame early or linger a frame. The newest-key
owners the key runtime already keeps and sends to the other half
(`KEY_FEEDBACK_BROAD_OWNER_*`) say which key is newest; which cases show on
which half the stage reads from the semantic and tap-branch maps the other
half already has. Nothing new crosses the split. A fixed case order alone
was weighed and left: it would make the newest key lose its area to an
older one, which the one locality never did.

**Where a replacement case paints.** One replacement case shows at a time
(D-F70, D-F72), so it paints by its own locality, its key as before.

**The format.** The lighting domain becomes version 7:

- A tap-branch colour is 4 bytes, `h s v`, `locality`.
- The behaviour feedback record is 13 bytes: the three tier colours and the
  tap-commit policy where they were (bytes 0–9), then the tap committed,
  hold and long hold localities (10–12). Byte 10, which held the one
  locality, holds tap committed's.
- The replacement feedback record is 16 bytes: the four colours where they
  were (0–11), then a locality per case in semantic order (12–15).
- Behaviour-feedback rows take semantics `0..6` or `0xff`.

Each record is still read in one validation step, the largest 16 bytes as
the header is. A locality beside each colour in the replacement record, as a
pointing colour row has, was weighed and left: keeping the colours in place
keeps the record's first twelve bytes and the running view's colour reads as
they were. The profile schema becomes 4.8. A version 6 lighting domain and a
schema 4.7 profile are refused, not translated (D-F10), and like every
format change this returns saved profiles to the compiled defaults.

**Defaults.** Every case's compiled locality is the stage's old one, the
half of the key, so nothing a person sees changes until they choose.

**Cost per half**, linked, from the instrumented pair
(`NOAH_RESOURCE_CHECKS=yes sh tools/build-firmware-pair.sh`, QMK
`1168001cfd`, both halves alike): SRAM0–3 `.data + .bss` is 205,136 bytes,
8 more than the 205,128 before (the compiled lighting buffer holds nine more
bytes), 628 below the 205,764-byte regression tripwire, which does not move.
The lighting view and its validation state are unchanged, 44 and 76 bytes.
Every reviewed stack path is as it was, the worst 1,904 of 1,920 bytes. The
lighting render, whose frame now holds each case's colour and locality and a
case mask for each half, is reviewed now too (`tools/firmware_stack_budget.json`,
at head `0110fe18`): the behaviour stage painting an area by the newest key
632 bytes, reading an accent row 648, painting an accent 528, and the
lighting view's validation step, which reads the larger records, 992. The instrumented image is
307,712 bytes on each half, 512 more (a UF2 counts in 256-byte blocks). These
are link-time figures and reviewed-path estimates; nothing measures the
stage's time, or a stack high-water, on the keyboard.

**Ark follow-up:** lighting version 7 and schema 4.8 in its codec, a
locality for each case on the Behaviour feedback and Replacement feedback
tabs, each tap branch its own accent owner, and the renamed vectors
(`rgb_domain_v7.json`).

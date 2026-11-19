# An accent can light for every tap branch

Cited as D-F75.

D-F73 made each tap branch a case of its own, so behaviour-feedback
semantic `0`, which had meant any tap branch, now names the double tap
alone. An accent could then light for one tap branch or, with `0xff`, for
every case, the tiers included, but not for every tap branch and nothing
else. Lighting the same LEDs for all four took four rows. Noah asked for one,
as a pointing mode's accents can light for every mode, keeping the choice
every accent has between the colour of the case showing and a colour of its
own (2026-11-12). Contract: [`rgb-domain-v1.md`](../spec/rgb-domain-v1.md).

**The semantic.** Behaviour-feedback semantic `7` is **any tap branch
pending**, `KEY_FEEDBACK_GROUP_TAP_BRANCH_ANY` in authored data; `0xff` still
means every case. A row with semantic `7` takes the tap branches as one
semantic, the newest key showing any of them, exactly as the tap-branch part
of a `0xff` row does (D-F73), and paints for no tier. A black row takes the
colour of that key's tap branch, and paints nothing when it is passthrough
(D-F74); a coloured row paints its own colour while a tap branch shows.

**Paint order.** Rows paint widest first, so a narrower one repaints a wider
one where they share an LED: the `0xff` rows, then the rows with semantic
`7`, then the rows naming one case, each pass in stored order. Painting `7`
among the rows naming one case was weighed and left: a row for one tap branch
would then lose to a row for all of them stored after it, which no other
accent table does.

**Format.** No new bytes: semantic `7` was refused and is now read, within
lighting version 7 and profile schema 4.8 (D-F73), which nothing has
released, so no stored profile changes meaning. Semantic `8` and above stay
`INVALID_SELECTOR`. The golden vectors gain `key-row-tap-branch-any`, and
`key-row-semantic-7` becomes `key-row-semantic-8`.

**Cost.** The stage keeps one more case mask a frame and makes a third pass
over the selectors, reading only the rows whose semantic shows (D-F63). No
row is read twice.

**Ark follow-up:** a third choice under **Lights for** on a tap branch,
**Every tap branch**, posting semantic `7`, with the colour choice every
accent has.

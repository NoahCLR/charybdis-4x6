---
status: superseded by D-F64
---

# A run of taps past the deepest tap count types the first tap

Cited as D-F33.

A tap count with no tap of its own repeats the first tap. A quick run of taps
past the deepest authored tap count is typing: it fires no branch, and each
press sends the first tap, so a Delete whose only other tap count is a Double
tap hold deletes five times for five taps. This replaced cycling back to the
first tap count, which sent one action for a run of any length, dropped taps
on rows whose later counts hold or repeat, and sent one Escape for three.
Like cycling, an overshooting run never fires the branch it went past. There
is no setting; a first tap that toggles a lock toggles once per press.
Contract: [What keys can do](../guide/keys.md#tap-counts).

Amended (Noah, 2026-11-07): a bare modifier key (Left or Right Control,
Shift, Alt or GUI) is no exception. Its behaviour's empty single tap sends the
modifier's own tap once the multi tap window closes, and each empty tap count
inside its depth repeats it, as any plain key's does. It had sent nothing at
an empty tap count whenever its behaviour went past the single tap, a rule
left from when a pending modifier tap was held under the next key pressed
inside the window; that went long before, and the rule only swallowed the
tap, while some programs act on a lone modifier tap, or on two tapped
together. Held, the modifier still holds itself from the tap / hold threshold
until release, and a key pressed inside the window settles its pending tap as
any other key's (D-F56).

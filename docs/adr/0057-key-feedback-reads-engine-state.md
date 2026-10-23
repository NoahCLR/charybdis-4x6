# Key feedback reads the key runtime's state, never a clock of its own

Cited as D-F57.

A pending tap branch shows from the tap that counts into it until the tap
series resolves; both edges are transitions the key runtime already makes
(`key_feedback_tap_series_shows_tap_branch` in `key/runtime/feedback.c`), and
a branch acts where it is decided, with no window between deciding and
doing. Chosen over a display clock for the tap phase, with a second colour
for a settled count and an action-delay window that held the action back so
the colour could be seen: no anchor for that clock worked. Anchored on the
press it called the count settled while the engine still accepted a tap;
anchored on the last tap it kept the settled colour after the window had
closed on depth, because the engine's settle condition is a predicate over
the timer and the authored depth, not a moment. The light now cannot
describe a moment the action did not. Contract:
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#drawing).

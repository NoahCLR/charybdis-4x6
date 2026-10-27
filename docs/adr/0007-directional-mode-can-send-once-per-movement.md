# A directional mode can send once per movement

Cited as D-F07.

A directional mode sends either once per threshold step or once per movement,
so an imprecise movement does not send a burst of the same shortcut. A
movement ends after a 150 ms pause or when the mode ends; within it, only a
whole threshold of motion back against the direction that sent sends again, so
a back-and-forth sends once per leg and a stray report sends nothing. Existing
profiles keep once per step. Layout: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md).

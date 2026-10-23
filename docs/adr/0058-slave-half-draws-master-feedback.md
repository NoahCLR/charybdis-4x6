# The slave half draws the master half's computed lighting state

Cited as D-F58.

Only the master half runs the key runtime, combo origins and the pointing
runtime, so it computes what lighting shows and sends the result: the
per-key semantic map, tap branches, newest owners and flash visibility, the
combo bitmaps, the pointing identity and its owner keys, the preview layer
and the fade progress, in four split RPCs (`split/runtime_sync.c`). The
slave half runs the same stages over them and evaluates no feedback rule, so
the halves can differ in freshness but never in what a state means; flashes
keep the master half's phase. Chosen over sending engine state for the slave
half to derive from, which would put a second copy of the rules on the other
side of the link and let a lagging packet change an answer rather than delay
it. Each packet fits one QMK RPC and lands as one publication, under its own
publication counter, that a reader copies whole. Contract:
[`split-activity-sync.md`](../spec/split-activity-sync.md#runtime-state-packets).

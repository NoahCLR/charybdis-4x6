# Pointing modes click mouse buttons

Cited as D-F22.

A direction, straight or diagonal, or a mouse-button override's tap may click
a plain mouse button 1–8, so moving the ball can send buttons 3 and 4 without
a macro. The click goes through the owned-keycode ledger, not QMK's key
processing, so it cannot reach a mode's button overrides or the button's own
behaviour and never releases a button another owner holds; a click from motion
drops that poll's motion first. Rules:
[`pd-mode-domain-v1.md`](../architecture/pd-mode-domain-v1.md), "Taps, modifiers and buttons".

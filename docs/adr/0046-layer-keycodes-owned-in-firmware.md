# Layer keycodes are owned in the firmware

Cited as D-F46. Was D-L34.

QMK ran `TG()` and `TO()` behind the userspace's layer ownership, so a toggled
layer turned off when an `MO()` of it was released, and `TO()` left stale
locks. The firmware owns them instead: `TG(n)` is `LOCK_LAYER(n)`, `TO(n)`
locks only n, `TT(n)` is `MO(n)` that locks on its `TAPPING_TOGGLE`-th tap,
`OSL(n)` holds like `MO(n)` and its tap arms a one-shot, and `LM()` holds
through layer and modifier ownership. Each acts the same on a key, in a
behaviour and as a combo output, and both halves refuse a profile that places
them where keymap validation forbids. `DF()` and `PDF()` stay refused: layer 0
is the base, and `TO()` covers the need. Contract:
[What keys can do](../guide/keys.md#layer-keys).

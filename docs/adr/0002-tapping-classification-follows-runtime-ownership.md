# Tapping classification follows runtime ownership

Cited as D-F02.

A key the runtime owns (every `LT`, `MT` and `OSM` since D-F27; `TT` and `OSL`
through layer ownership) reaches the runtime without first waiting for QMK's
tapping classification, so one engine decides it. Bypassing QMK must not move
the key's first hold: `LT` holds its layer, and `MT`/`OSM` their modifiers,
only once held past the tap-hold term, as QMK decides. So an `LT` key is not a
momentary layer: its layer is not on from the press, and an interrupting key
does not cancel its tap. A runtime-owned `MT`'s one-shot is consumed by the
owner's release decision.

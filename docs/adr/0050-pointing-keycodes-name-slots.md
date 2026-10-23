# Pointing keycodes name slots, not factory presets

Cited as D-F50. Was D-L41.

Pointing keycodes are `PD_SLOT_n` (hold) and `PD_SLOT_n_LOCK` (toggle). The
factory preset names are no longer keycodes: a slot is whatever the profile
puts in it, and a client reads each slot's name and behaviour from the
keyboard. Values: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md).

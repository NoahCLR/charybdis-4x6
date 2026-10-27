# Pointing keycodes run a slot's mode, not a factory preset

Cited as D-F50. Was D-L41.

Pointing keycodes are `PD_MODE_n` (hold) and `PD_MODE_n_LOCK` (lock): they run
the pointing mode in slot `n`, whatever the profile puts there. The factory
preset names are no longer keycodes, and a client reads each mode's name and
behaviour from the keyboard. The keycodes were `PD_SLOT_n` and
`PD_SLOT_n_LOCK` until they were renamed so the name says what the key does
(it runs a mode), a change of names only, never of values or stored bytes.
Values: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md).

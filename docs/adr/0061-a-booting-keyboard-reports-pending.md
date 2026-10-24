# A booting keyboard reports pending until it activates

Cited as D-F61.

A half runs its compiled defaults from reset until it activates the profile
its boot settles on: it validates the compiled defaults, selects and validates
its saved profile, waits until both halves hold the same one, and activates
it. Profile status reported most of that window as the compiled defaults with
no saved profile, for up to a second with a large profile, so a client could
not tell a booting keyboard from one with nothing saved; and its first moments,
reported pending, carried error 17 as if activation had failed. Now profile
status reports active kind 3, pending, through every one of those owner
states, with no error, while still saying what the boot has found; a boot
that finds nothing usable saved reports the compiled defaults once it has
decided, and pending with error 17 means only a half that stopped with its
pointing slots unpublished. What runs does not change. A client waits for one
signal, kind 3 clearing; a half whose slave half never answers stays pending,
so that wait needs its own limit.

Considered: pending only until the saved profile is selected (a client would
still read the compiled defaults while the halves reconcile, just before the
saved profile replaces them), and running the saved profile earlier (it is not
validated, or agreed between the halves, until then). Contracts:
[`profile-wire-v1/hid-channel.md`](../spec/profile-wire-v1/hid-channel.md#active-kind),
[`device-resident-profile.md`](../spec/device-resident-profile.md#authority).

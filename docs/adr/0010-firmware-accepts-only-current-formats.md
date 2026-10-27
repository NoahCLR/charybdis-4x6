# Firmware accepts only what it writes today

Cited as D-F10.

The firmware accepts one profile and storage format, the current one, and has
no migration path: a stored profile or a backup in an earlier format is
refused, not translated. Ark keeps no translation either, so an old backup
restores only onto the firmware that made it. One format means one validator
and no legacy paths to keep correct; the cost is that a format change returns
saved profiles to the compiled defaults. Frozen old profiles in the host tests
are rejection evidence.

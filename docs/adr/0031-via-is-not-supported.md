# VIA is not supported: one store, and the firmware answers Raw HID itself

Cited as D-F31.

The firmware no longer builds VIA, and losing the VIA app is accepted. In
return Apply publishes one profile generation with no second store to bind,
stage, fence input for or roll forward (an unchanged Apply had taken about
6.6 s on the keyboard), and QMK's EEPROM holds only its eeconfig, saving
32 KiB of RAM per half. The firmware answers Raw HID itself: a report no
handler takes comes back with command byte `0xFF`, VIA's own "unhandled", and
the keyboard identifies itself by its capability page alone. With VIA gone the
authored macro aliases are just macros: `MACRO_n` (with `MACRO_SLOT_COUNT`,
`MACROS()` and the profile action kind `MACRO`), renamed from `VIA_MACRO_n`
as a change of names only, never of behaviour or stored bytes. Contracts:
[`profile-wire-v1/hid-channel.md`](../spec/profile-wire-v1/hid-channel.md),
[`logical-profile-transaction-v1.md`](../spec/logical-profile-transaction-v1.md).

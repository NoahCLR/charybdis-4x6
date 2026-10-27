# Bigger profiles in one version transition

Cited as D-F14.

The profile grew in one transition, so a user upgrades once: sixteen layers,
128 behaviour rows with 640 steps, 128 combos of 2–16 inputs, 128 custom keys,
128 macros sharing one 34,903-byte bank, and 32-byte UTF-8 names everywhere.
Formats are counted, so a later firmware can raise a limit, and one supported
tap depth (five) bounds behaviour steps. Behaviours and combos gained
participation controls at four scopes
([`participation-policy.md`](../spec/participation-policy.md)); BK gained the hooks
they need (`is_tap_record_user()`, a press-context byte through its queues, a
combo member-record gate). D-F29 replaced its EEPROM geometry and D-F31 its VIA
region. Ceilings and the maximum profile:
[`device-resident-profile.md`](../spec/device-resident-profile.md#ceilings-and-the-maximum-profile).

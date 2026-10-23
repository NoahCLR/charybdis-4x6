# Keymap and macros belong to the profile

Cited as D-F30.

The keymap (domain `0x60`) and the macros (domain `0x70`) are profile domains,
so typing and playback read the active generation like every other domain,
and a save, a backup and the other half carry them with the rest of the
profile. The compiled defaults encode the authored keymap and macros; a
profile without either domain uses the authored defaults without serializing
them on the key or playback path. This left VIA's store with nothing to hold
(D-F31). Layout: [`device-resident-profile.md`](../spec/device-resident-profile.md).

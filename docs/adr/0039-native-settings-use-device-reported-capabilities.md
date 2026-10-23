# Native settings use device-reported capabilities

Cited as D-F39. Was D-L18.

The keyboard reports QMK's enabled lighting effects, the key options it
supports with their bit masks, and the LED classes present (GET `0x08`
pages 2 onward), and a client takes them from there, never from source;
packed bytes and unknown bits survive unrelated edits. QMK ignores mode and
colour changes while lighting is off, so the firmware turns lighting on
without saving, applies the settings at the safe boundary and then restores
the requested on/off state: lighting edits apply while lighting is off. Pages:
[`profile-wire-v1/settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#editor-metadata--get-0x08).

# A stored macro plays from the profile in windows, all checked first

Cited as D-F59.

A macro is never copied whole into RAM. Playback keeps one 512-byte window of
decoded steps for the one macro that plays, and refills it from the active
profile's bank as it goes; a macro can be as long as the 34,903-byte bank
(D-F14) while macro storage stays within its 8,192-byte RAM policy
([`resource-policy.md`](../spec/resource-policy.md)). So that a long macro
still never types part of itself, every window is decoded and checked (the
host layout, Unicode entry, held keys balanced across windows) before the
first key, then the first window is decoded again to play. The cost is
decoding a macro twice and walking the bank to its slot at each start; the
safe boundary waits for playback, so the bank cannot change under a refill.
Chosen over a per-slot compiled cache or decoding the whole macro into one
buffer, which would size RAM to the largest macro.
Path: [`macro-playback.md`](../architecture/macro-playback.md#finding-the-macro).

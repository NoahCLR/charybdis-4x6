---
status: superseded by D-F31
---

# Fresh reads omit only storage proven zero

Cited as D-F21.

A read map (GET `0x0c`), bound to the storage generation and digest, let a
client skip VIA macro blocks proven zero and still verify the whole store.
D-F31 removed VIA: macros are read with the profile, and GET `0x0c` is
unhandled.

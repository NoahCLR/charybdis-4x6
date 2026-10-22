# A differential upload still builds a complete validated candidate

Cited as D-F18.

A host may build a candidate from bounded copies of the identified active
profile (REUSE) and streamed chunks, so an Apply sends only what changed. The
inactive slot still holds the complete target, which passes the ordinary full
validation and the both-half transaction before publication; the active
profile is untouched until then, and an interrupted upload is only an
uncommitted candidate. Frames: [`profile-wire-v1.md`](../architecture/profile-wire-v1.md).

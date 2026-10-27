# A new base: 32-bit profile lengths, and a feature word of build differences

Cited as D-F28.

Profile Wire 2.0, profile schema 4.0 and store format 6 opened a new major of
every format a profile travels in, so that every length and offset of a
profile is 32 bits on the wire and in storage and a profile can outgrow
64 KiB. Where a frame had no room, layouts changed rather than fields
narrowing. Chosen over a 24-bit length, which would have kept frames but set
a new ceiling.

The capability feature word holds only what a build genuinely varies by
(split, saves, pointing), never a step in the firmware's history (Noah,
2026-10-19): every Profile Wire 1 bit is retired, and a later capability
raises the minor version instead. A slot in an earlier format is never read
or converted: the keyboard runs its compiled defaults and its status says so.
Contracts: [`profile-wire-v1/`](../spec/profile-wire-v1/README.md),
[`profile-split-v1.md`](../spec/profile-split-v1.md).

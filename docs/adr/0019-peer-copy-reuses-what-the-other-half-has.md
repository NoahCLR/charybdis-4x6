# The copy to the other half reuses what that half already has

Cited as D-F19.

Apply used to send the other half every byte of the candidate. The firmware
now records the ranges the host built with REUSE, and the copy asks the other
half to fill those from its own active profile, checked against the same
source identity, sending only the rest; where that half's profile is not the
source, it says so and gets bytes. The other half still validates the whole
candidate before its prepared marker, so durability, the commit barrier and
recovery are unchanged. Copy chunks use QMK's whole 128-byte split RPC buffer
rather than 32-byte frames. Frames: [`profile-split-v1.md`](../architecture/profile-split-v1.md).

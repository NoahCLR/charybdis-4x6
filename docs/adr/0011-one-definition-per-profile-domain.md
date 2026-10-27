# One definition and traversal per profile domain

Cited as D-F11.

The profile-domain registry defines each domain's ID, mask bit, current
version and order once, and the blob, candidate, validator, compiled-defaults,
store and provider modules all read it. One envelope walker serves readback,
whole-profile validation and stepped storage checks without owning their I/O
scheduling; domain modules keep their record rules. It replaced per-module
lists that drifted: the store's own list of settings versions once refused
every saved copy (D-F44). See [`profile-wire-v1/blob-and-digests.md`](../spec/profile-wire-v1/blob-and-digests.md),
"How the firmware owns a domain".

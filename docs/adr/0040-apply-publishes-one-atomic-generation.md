# Apply publishes one atomic generation

Cited as D-F40. Was D-L21.

Apply publishes a profile as one generation on both halves. The USB half
validates the candidate and copies it to the other half, which validates it
and makes it durable as a prepared copy; once both are prepared, the USB
half's commit marker is the decision. The other half then commits, and the
new profile activates at the next safe boundary. A prepared marker without
the decision is ignored at boot, so the previous generation stays authority,
and a decided target is recovered from the other half's prepared copy. Once
decided, the keyboard refuses a cancel, even if the host never saw the
decision (D-F48). Since D-F31 the profile is the only store. Contract:
[`logical-profile-transaction-v1.md`](../architecture/logical-profile-transaction-v1.md).

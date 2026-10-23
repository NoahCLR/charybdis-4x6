# Only the keyboard ends a candidate

Cited as D-F48. Was D-L39.

A host cancelling through two channels once discarded the other half's
prepared copy when it was the only complete target, and an expired candidate
could keep the next Apply refused until a restart. Now only the keyboard ends
a candidate: every cancel before the decision (host abort, lease expiry,
supersession, a failed copy) releases it and aborts the other half's prepared
copy, without waiting for an absent peer, whose ABORT completes when the link
returns. After the decision nothing cancels, whatever the host saw. Contract:
[`logical-profile-transaction-v1.md`](../spec/logical-profile-transaction-v1.md),
"Cancellation".

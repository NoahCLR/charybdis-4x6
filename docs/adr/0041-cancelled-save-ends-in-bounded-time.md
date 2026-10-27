# A cancelled save ends in bounded time, and says why it ended

Cited as D-F41. Was D-L22.

A slave half that stopped acknowledging and then answered the cancel's ABORT
with BUSY once left an Apply stuck until a replug, because that ABORT was
retried without limit. Now the master half waits at most 15 s, then releases its
own side, reports *peer cleanup pending* in its status, keeps retrying the ABORT
in idle slots, and starts no new save or split transfer until the other half
acknowledges. Contracts:
[`authority-state-table.md`](../spec/authority-state-table.md),
[`profile-wire-v1/candidate-transfer.md`](../spec/profile-wire-v1/candidate-transfer.md).

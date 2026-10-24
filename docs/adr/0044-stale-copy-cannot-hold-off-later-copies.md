# A stale copy on the other half can no longer hold off every later one

Cited as D-F44. Was D-L27.

Apply stalled with the other half answering busy until a power cycle. The
causes, each fixed: the receiver's lease was kept alive by any frame on the
link rather than requests for its own copy; a sender retried a busy chunk
forever rather than restarting at `PREPARE_BEGIN`; a rejected copy did not
return the other half's storage admission; and the trigger, the store kept its
own list of accepted settings versions, which drifted from the validators'
(one definition since D-F11 and D-F13). A failed store on the other half is
retried from the start up to twice, safe because nothing is durable before the
commit, and then the Apply fails as `PEER_TRANSFER_FAILED`; busy replies say
why. Contract: [`profile-split-v1.md`](../spec/profile-split-v1.md).

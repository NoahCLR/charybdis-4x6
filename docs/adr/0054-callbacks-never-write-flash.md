# Callbacks never write flash; the scan does the durable work

Cited as D-F54.

No Raw HID or split callback runs a validation step, writes flash, publishes
or sends to the other half. A candidate frame or a split RPC request that
changes the profile is only decoded and copied into its protocol's one-frame
mailbox, and answered from the last published reply: admitted, busy, or a
cached answer to a repeat of the same request. Live edits are the exception:
an edit, Save, Revert or edit-mode request acts on the working copy and its
owner in the callback, bounded RAM work (copying an edit's bytes and updating their CRC,
planning its splices, setting up validation), and is answered with the working
copy's result (OK, QUEUED, OUT_OF_ORDER…). Validation, flash writes,
publication and every send to the other half run later, one bounded step per
durable-I/O grant in the matrix scan, so the grant stays the one path to flash
and its pacing (D-F29) holds. The slave half's split callbacks run on QMK's
split slave thread, which preempts the main loop with 1,024 usable stack
bytes, and a flash erase or program stops the keyboard with interrupts off;
work done there would race the scan's own store state and stall the link. The
cost is a round trip: a mutating request is first answered admitted, busy or
queued, and the sender asks again until the reply it waits for is published.
Chosen over doing the work in the callback and replying with its result.
Contracts: [`profile-split-v1.md`](../spec/profile-split-v1.md#split-rpcs),
[`profile-wire-v1/candidate-transfer.md`](../spec/profile-wire-v1/candidate-transfer.md#who-routes-candidate-frames).

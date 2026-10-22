# Keys wait for an undecided tap/hold key

Cited as D-F03.

Keys pressed while a tap/hold key is down and undecided wait for its decision,
then replay in order: its tap first on a release, its layer or modifiers first
on a hold. This is QMK's own rule, so rolls keep their order and a key tapped
under a held layer lands on it. Chosen over sending the next key at once
(rolls come out swapped) and over settling the tap at the next press (breaks
quick chords under a held layer); the cost is a wait of at most the key's hold
time.

Records wait in the fork's `process_record_admit_user()` hook, after combos and
before every QMK feature, and replay through `process_record()`. When the queue
is full, its oldest record replays early instead of any record being dropped.
QMK's optional permissive hold and hold-on-other-key-press are not reproduced.
D-F27 decides which keys make others wait.

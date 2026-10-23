# Physical gesture timing survives QMK buffering

Cited as D-F01.

Handled keys judge taps and holds by their physical press and release times,
not by when QMK's combo and record-admission queues deliver them, so buffering
cannot use up a repeated-tap window or stretch a hold that was already
released. Userspace asks QMK's existing queues whether a continuation is
waiting rather than keeping a second buffer, and one engine, the key runtime,
decides every handled key. The timing model is in
[What keys can do](../guide/keys.md#timing-follows-your-fingers).

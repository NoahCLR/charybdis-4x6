# A pending tap series belongs to its key

Cited as D-F56.

A released key's tap series, waiting out its multi tap window, is kept by
physical position: another key with a behaviour pressed meanwhile starts its
own series and leaves this one to its window or its own next press, so both
thumbs can count taps at once. Chosen over flushing every pending series at
any other press, the rule before, which made concurrent tap counts on two keys
impossible. The cost is typing order, so three presses still settle a pending
series first: a key without a behaviour settles all of them; any press settles
a tap/hold key's (D-F27); and a key without a behaviour settles a pending tap
that changes layers before its own keycode is resolved, so it types on the
layer the tap turns on.

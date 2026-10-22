# Userspace keycodes sit in fixed blocks, and custom keys are named

Cited as D-F51. Was D-L42.

Userspace keycodes were numbered by enum order, so adding a pointing slot or a
layer moved every keycode after it. Each family now owns an aligned block
reserved beyond what is supported (custom keys moved to their own in D-F14),
so growth changes storage and masks, never another keycode. The keymap's
hand-numbered keys became **custom keys**: named keys that do what their
behaviour row says, and since D-F26 their shortcut. The renumbering refused
older saved profiles by action ABI digest rather than reinterpreting them.
Blocks: [`pd-mode-domain-v1.md`](../architecture/pd-mode-domain-v1.md).

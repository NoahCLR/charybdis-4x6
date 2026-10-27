# The keyboard's name is its USB product string

Cited as D-F25.

The keyboard's name, stored in the settings domain, is served as its USB
product string through a BK hook, so the browser's HID chooser, the system's
device list and Ark show the name its owner chose; an empty name keeps QMK's
`keyboard_name`. Hosts read the string only when they enumerate, so
`keyboard_pre_init` reads the saved name before USB starts, and after a rename
the half on USB restarts USB once, when both halves are settled, nothing is
held or playing and the host has been quiet for a second. The other half never
restarts USB. Rules: [`profile-wire-v1/settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#keyboard-name), "Keyboard name".

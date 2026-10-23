# Profile slots in their own flash region

Cited as D-F29.

The two profile slots left QMK's emulated EEPROM, which mirrors every byte in
RAM, for a 256 KiB region of flash read in place: two 128 KiB slots, so a
profile can reach 130,048 bytes and the RAM came back. The flash part promises
nothing for a page programmed twice between erases, so each slot's header and
marker cells take whole pages (the first kilobyte of each slot is the price),
and the marker is still written last: power lost anywhere in a save leaves
the previous profile.

A sector erase stops the keyboard for tens of milliseconds with interrupts
off, so erases wait for a quiet keyboard (no key held, matrix unchanged for
50 ms) and a durable-I/O grant does at most one erase or page program.

**Pacing**: the first keyboard session showed a save waited on the fixed cost
of each storage step, not on flash, so a grant repeats storage steps while
work is in progress (up to 1 ms and 32 steps) and a step works through up to a
page. A 37 KB save went from about 30 s to under 5 s.

An EEPROM reset still returns the keyboard to its compiled defaults:
`keyboard_pre_init` erases both slot headers when QMK's EEPROM magic is not
valid. Chosen over a bigger emulated EEPROM (all of it in RAM) and over
erasing a whole slot up front (one long pause). Geometry and rules:
[`device-resident-profile.md`](../spec/device-resident-profile.md#where-the-profile-is-stored).

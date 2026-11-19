# More LED sets and accents, in the same lighting format

Cited as D-F63.

The lighting domain holds 128 reusable groups (LED sets, in Ark's words)
instead of 16, and 640 group rows (accents) in its four tables together
instead of 32, at most 255 in any one table (Noah, 2026-11-06). The
counts were always bytes and group ids positions, so only the ceilings moved:
the domain stays version 4 and the profile schema 4.3, every saved profile
and backup stays valid, and an older firmware refuses a profile past its
ceilings as `CAPACITY_EXCEEDED` instead of misreading it. The largest
lighting domain grows from 591 to 4,639 bytes and the maximum profile from
75,825 to 79,873 bytes, both far inside a 130,048-byte slot. A table's row
count is still its header byte, so 640 in all needs at least three tables. A
single table of more than 255 rows would need wider counts, a new domain
version and, under D-F10, every saved profile back to the compiled defaults,
for a need no profile has shown.

The ceilings are reported as behaviours' are: capability page 1 byte 6 is the
groups (128), byte 7 changes from the row total to the rows of one table
(255), and page 2 bytes 18–19, zero until now, carry the total (640) as a
16-bit field. So a client tells this firmware from the one before by its
capability page alone, which is what D-F28's minor-version rule ("a later
capability raises the minor version") exists for; a minor version would
also have refused every 4.3 profile and backup, as D-F26's raise to 4.3 did, for nothing
the bytes need. The firmware's validator, compiled writer and capability page
take the ceilings from one place and a static assertion holds them equal.

More rows must not cost every frame. Before, each 12-LED chunk read every
row of a stage and its group through the validated view, lit or not. Now
each lighting publication reads every layer, pointing and key-feedback row's
selector once into RAM, one byte per row (656 bytes of module state on each
half), keyed like the layer colours by the frame's epoch, publication count
and liveness. A stage compares the selector there and reads a row and its
group only when it would paint: a layer that is on in the selection, the
running slot, a key-feedback semantic shown. An accent that is not lit costs
no profile read, and a lit one what it did before, at most once a frame for
key feedback. Paint order, the black-takes-the-stage's-colour rule and the
D-F62 split stack are unchanged, and a frame begun before a publication is
still refused at its close. Combo rows have no selector and are still read in
both combo partitions of a chunk while a combo is active; a profile that
spends its rows there pays for them only then. A row that does not paint is
no longer read, so the renderer no longer notices a corrupt one; the
validator refuses such a profile before it is ever published, and a selector
that cannot be read paints nothing of the frame. Host tests count these reads
at the ceilings; nothing measures their time on the keyboard.

The compiled defaults' encoded lighting is kept in a buffer sized by the
keymap's own tables (`EXPORT_RGB_COMPILED_CACHE`), not the domain's
maximum, which would now be 4,639 bytes of RAM on each half. Together these
add 400 bytes of static RAM on each half (203,444 to 203,844 linked), so the
static-RAM regression tripwire rises by that much, to 204,176 bytes; it is a
policy line, not free memory.

The compiled lighting keeps D-F14's ceilings, 16 saved LED sets and 32 group
rows in all, each a compile error past it. Its writer numbers each row's LED
set by comparing it with every authored row, a cost that grows with the cube
of the rows: harmless at 32, a stalled boot at hundreds. Sorting the sets once
would need a kilobyte of RAM for a keymap that holds three rows, and a profile
from a client, which holds the rest, never passes through the writer.

Contracts: [`rgb-domain-v1.md`](../spec/rgb-domain-v1.md),
[capability pages](../spec/profile-wire-v1/hid-channel.md#capability-pages--get-0x01),
[the maximum profile](../spec/device-resident-profile.md#ceilings-and-the-maximum-profile).

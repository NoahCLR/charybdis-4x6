# The autocorrect list is a readable profile domain

Cited as D-F68.

The typos the keyboard corrects are part of the profile: domain `0x80`, the
autocorrect list. So a client edits the list, both halves save it, and a
backup holds it, with no reflash. QMK's autocorrect keeps its list as a trie
compiled into the firmware; nothing of it is used.

The domain holds the entries as they are written, not a compiled tree. Each
correction is stored once, as it is to be typed. Each typo is stored
case-folded with where it matches (the whole word, the start, the end,
anywhere) and the correction it names. A tree would need a compiler on the
client and a second one in the firmware's tests, and would read back as
bytes nobody can check by eye. Entries need one validator, read back exactly
as written, and one list has one encoding: corrections and typos each ascend
by their bytes, so nothing is stored twice and a typo cannot name two
corrections. A whole word is found by binary search over the typos; the
part-of-word typos have their own index, which is scanned. Lookups read the
active profile in place, flash or the working copy, with no index in RAM and
no flash write (D-F54).

A typo holds word characters only: the letters and digits the host layouts
type, folded to lower case by one table generated from the layouts, and
apostrophes between them. The firmware and a client fold with the same
table, which travels in the domain's fixture, so they cannot disagree about
what a typo matches. A layout that types a new letter extends the table and
raises the schema minor.

The ceilings are counted (D-F14): 1,024 corrections, 1,024 typos, 32 bytes
each, 20,480 bytes of text, 34,824 bytes for the domain, reported on
capability page 3. The maximum profile grows from 79,873 to 114,703 bytes
and still leaves the 12,288-byte reserve below the 130,048-byte payload
ceiling. The validator proves every correction is named without a bitmap of
them: each correction row holds its first typo, checked from both sides, so
its state stays inside the whole-profile validator's 400 bytes.

The domain takes the eighth and last bit of the one-byte domain mask. Every
mask bit now names a domain; a ninth domain needs a wider mask in a
candidate's BEGIN, the slot record, the capability page and the split
descriptors, which is a new store format and Profile Wire major.

The authored list is a text file in the keymap folder, `autocorrect.txt`, in
the `typo -> correction` form QMK's lists use, encoded into a generated
header that real-profile validation refuses when stale. It ships empty.

The new domain is profile schema 4.4 (D-F28). As with every format change
(D-F10), firmware with it refuses a slot or backup saved under schema 4.3:
after the update the keyboard runs its compiled defaults until a profile is
applied again, and an older backup is not restorable. The release that
carries this says so in its notes.

Contract: [`autocorrect-domain.md`](../spec/profile-wire-v1/autocorrect-domain.md).

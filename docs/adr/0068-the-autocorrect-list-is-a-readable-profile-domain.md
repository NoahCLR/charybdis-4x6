# The autocorrect list is a readable profile domain

Cited as D-F68.

What the keyboard replaces as it is typed is part of the profile: domain
`0x80`, the autocorrect list. So a client edits the list, both halves save
it, and a backup holds it, with no reflash. QMK's autocorrect keeps its list
as a trie compiled into the firmware; nothing of it is used.

The list is for more than typos. An entry is a **replacement** with the
**typed forms** that produce it: the right word for a misspelling, a phone
number for `pnumber`, a sign-off for `sig`. People know the feature as text
replacement. The domain, the code, the fixtures and the keys were named for
autocorrect and keep those names: a replacement is a `correction` there and
a typed form a `typo`.

The domain holds the entries as they are written, not a compiled tree. Each
replacement is stored once, as it is to be typed. Each typed form is stored
case-folded with where it matches (the whole word, the start, the end,
anywhere) and the replacement it names. A tree would need a compiler on the
client and a second one in the firmware's tests, and would read back as
bytes nobody can check by eye. Entries need one validator, read back exactly
as written, and one list has one encoding: replacements and typed forms each
ascend by their bytes, so nothing is stored twice and a typed form cannot
name two replacements. A whole word is found by binary search over the typed
forms; the part-of-word ones have their own index, which is scanned. Lookups
read the active profile in place, flash or the working copy, with no index
in RAM and no flash write (D-F54).

A typed form holds word characters only: the letters and digits the host
layouts type, folded to lower case by one table generated from the layouts,
and apostrophes between them. The firmware and a client fold with the same
table, which travels in the domain's fixture, so they cannot disagree about
what a typed form matches. A layout that types a new letter extends the
table and raises the schema minor. Where it matches is a stored number; a
client is free to write the part-of-word matches with `*` where other
letters may go, and the domain never holds one.

A replacement is text or a macro. Text is up to 128 bytes on one line: long
enough for an address or a sign-off, and short enough that the keyboard
types it in under three seconds and holds it whole while it works. Anything
longer, or of more than one line, is a macro's job, and the profile already
has macros: so a replacement may name a macro slot, and the keyboard erases
the typed form and plays it (D-F69). The row that would hold a text's place
and length holds the slot. Macro replacements follow the text ones in slot
order, so a slot is one entry and the list keeps its one encoding. Their
typed forms match the whole word only: a macro replaces what was typed, and
only a whole word is known to be all of it. The validator checks that the
slot exists, not that it holds a macro, since the macros are another domain
edited by themselves; the keyboard passes over a replacement whose slot is
empty.

The ceilings are counted (D-F14): 1,024 replacements, 1,024 typed forms, 32
bytes in a typed form and 128 in a replacement, 20,480 bytes of text, 34,824
bytes for the domain, reported on capability page 3. The maximum profile
grows from 79,873 to 114,703 bytes and still leaves the 12,288-byte reserve
below the 130,048-byte payload ceiling. The validator proves every
replacement is named without a bitmap of them: each row holds its first
typed form, checked from both sides. It orders each replacement against the
one before by holding that one whole, 128 bytes, which makes this domain's
walk the largest of the whole-profile validator's and takes that validator's
state from 392 to 412 bytes.

The domain takes the eighth and last bit of the one-byte domain mask. Every
mask bit now names a domain; a ninth domain needs a wider mask in a
candidate's BEGIN, the slot record, the capability page and the split
descriptors, which is a new store format and Profile Wire major.

The authored list is a text file in the keymap folder, `autocorrect.txt`, in
the `typo -> correction` form QMK's lists use, with `:typo: => N` for a
macro slot, encoded into a generated header that real-profile validation
refuses when stale. It ships empty.

The new domain is profile schema 4.4 (D-F28). As with every format change
(D-F10), firmware with it refuses a slot or backup saved under schema 4.3:
after the update the keyboard runs its compiled defaults until a profile is
applied again, and an older backup is not restorable. The release that
carries this says so in its notes.

Contract: [`autocorrect-domain.md`](../spec/profile-wire-v1/autocorrect-domain.md).

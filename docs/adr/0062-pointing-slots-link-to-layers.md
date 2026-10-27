# Pointing slots link to layers

Cited as D-F62.

A pointing slot that holds a mode may have a link to layers, in its record
(pointing domain version 4, bytes 11..15), so a mode and the keys it needs
travel together, and a layer can do everything the per-mode mouse button
overrides did and more; the overrides are retired in the same version, and
Arrow, the only compiled slot that had them, keeps no replacement. A link has
three parts: its **linked layers** (bytes 12..13, a 16-bit mask), which start
the mode while any of them is on; the layers its key **turns on** (bytes
14..15), any layers, while the key runs the mode; and what **ends** a mode its
layers started (byte 11). Bytes 9 and 10 are reserved.

A running mode has a source, its key (held or locked) or its linked layers. A
key source beats a layer source, so an explicit press always does what was
pressed; among layer sources the slot whose highest linked layer that is on
ranks highest wins, which matches the keys on show; whenever a source ends the
links are evaluated again. A mode its key runs owns the layers its key turns
on as one more layer owner beside the key holds and locks, so each goes off
when the mode ends unless another owner keeps it, and they go off before the
links are evaluated. A mode its layers started owns no layer. Only the linked
layers are exclusive: no layer starts two modes, but any number of keys may
turn a layer on, including one that starts another mode. That makes no chain
and no latch: while a key runs its mode the layers it turns on start nothing,
a mode its layers started never turns on another's, and when the key lets go
another mode starts from one of its layers only if a second owner keeps it on.

What ends a mode its layers started is nothing, a higher layer (one above the
highest linked layer that is on), or any other layer (one in neither of the
link's sets); the mode returns when that layer goes off, and a mode its key
runs is never ended. The pointer layer and a default layer never end it.

A mode its layers started never anchors the pointer layer or owns QMK's
auto-mouse toggle, and a separate pointer layer is stripped while it runs, as
an auto-sniping layer strips it (D-F60), so its layers keep their keys. A
running mode, whichever its source, paints in the layer stack directly beneath
the lowest layer of its link (either set) that is on, so every layer of the
link that is on shows its mapped keys over the mode's colour; each half works
that out from its own slots and the synced layers, so nothing more is
mirrored. A mode its layers started does not hold up a profile activation: at
the safe boundary it ends, and the new generation's links are evaluated
against the layers that are on. The pointer layer or a default layer in a link
would run its mode all the time; the pointing domain cannot see the settings,
so such a link validates and is inert, and a client refuses it.

Both changes raise the profile schema to 4.1 (D-F28: a later capability
raises the minor version), so a client tells this firmware from the one
before by its capability page alone.

Auto-sniping follows the same idea: its layer setting became a layer mask
(settings version 9), its on/off switch kept as a master switch so turning it
off keeps the layers, and one predicate answers which sniping layers are on.

Amended after first use (Noah, 2026-10-25), in place, since pointing version 4
was unreleased: the first version linked one layer in a direction (the mode
turns its layer on, the layer turns its mode on, or both) and hid a
layer-started mode's light unless a per-slot switch showed it over the layer,
mirroring one bit to the slave half. Painting beneath the layer replaced the
switch; trying it with the app turned the one layer into the two sets and the
switch's byte into what ends a mode. The record size is unchanged, and the
earlier layouts are not read.

Considered: a link stored per layer in the settings (it would split one
behaviour across two domains and allow several slots per layer); letting a
layer-started mode own its layer (two-way links would latch); and keeping the
overrides beside links (two ways to give a mode keys, one of them far weaker).
Contracts: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md#linked-layers),
[`settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#scalars),
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#drawing),
[`split-activity-sync.md`](../spec/split-activity-sync.md).

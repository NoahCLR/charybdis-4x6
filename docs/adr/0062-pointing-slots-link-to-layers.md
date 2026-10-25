# Pointing slots link to layers

Cited as D-F62.

A pointing slot that holds a mode may be linked to one layer, in a
direction: the mode turns its layer on, the layer turns its mode on, or both.
The link lives in the slot's record (pointing domain version 4, bytes 9..11:
the layer, the direction, and whether a mode its layer started shows its own
light), so a mode and the keys it needs travel together, and a layer can do
everything the per-mode mouse button overrides did and more; the overrides
are retired in the same version, and Arrow, the only compiled slot that had
them, keeps no replacement.

A running mode has a source, its key (held or locked) or its layer. A key
source beats a layer source, so an explicit press always does what was
pressed; among layer sources the highest linked layer that is on wins, which
matches the keys on show; whenever a source ends the links are evaluated
again. A mode its key runs owns the layer it brings as one more layer owner
beside the key holds and locks, so the layer goes off when the mode ends
unless another owner keeps it; a mode its layer started owns no layer, so a
two-way link can never latch itself on. No two slots name one layer, which
makes chains of links (A turns L on, L starts B, B ends A) impossible.

A mode its layer started never anchors the pointer layer or owns QMK's
auto-mouse toggle, and a separate pointer layer is stripped while it runs, as
an auto-sniping layer strips it (D-F60), so its layer keeps its keys. It shows
the layer's lighting unless its slot says to show its own; the slave half
learns that one bit in the mirrored mode state it already receives, sent on
change, with no new exchange. It does not hold up a profile activation: at the
safe boundary it ends, and the new generation's links are evaluated against
the layers that are on. A link to the pointer layer or to a default layer would
run its mode all the time; the pointing domain cannot see the settings, so
such a link validates and is inert, and a client refuses it.

Both changes raise the profile schema to 4.1 (D-F28: a later capability
raises the minor version), so a client tells this firmware from the one
before by its capability page alone.

Auto-sniping follows the same idea: its layer setting became a layer mask
(settings version 9), its on/off switch kept as a master switch so turning it
off keeps the layers, and one predicate answers which sniping layers are on.

Considered: a link stored per layer in the settings (it would split one
behaviour across two domains and allow several slots per layer); letting a
layer-started mode own its layer (two-way links would latch); and keeping the
overrides beside links (two ways to give a mode keys, one of them far weaker).
Contracts: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md#linked-layers),
[`settings-domain.md`](../spec/profile-wire-v1/settings-domain.md#scalars),
[`rgb-domain-v1.md`](../spec/rgb-domain-v1.md#drawing),
[`split-activity-sync.md`](../spec/split-activity-sync.md).

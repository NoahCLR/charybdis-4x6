# Combo output behaviour permission is separate from chord admission

Cited as D-F24.

Whether a completed chord's output uses its keycode's shared behaviour or its
plain action is decided by a global switch and one per combo, both on by
default, with the behaviour master and the output's definition (at the
combo's origin layer) as vetoes. It is separate from whether the chord may
fire at all, and there is no separate output layer mask. Rules:
[`participation-policy.md`](../spec/participation-policy.md), "Combo
outputs".

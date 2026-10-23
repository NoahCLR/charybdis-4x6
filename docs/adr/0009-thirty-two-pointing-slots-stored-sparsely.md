# Thirty-two pointing slots, stored sparsely

Cited as D-F09.

The keyboard has 32 pointing slots, filling the reserved hold and lock keycode
blocks (D-F51). When this was decided, 32 fixed records would have filled most
of the 5 KB profile whether or not a slot was used, so the pointing domain
stores only the slots that say something. An omitted slot is disabled and
unnamed, and storing one that way is refused, so every profile has one
encoding. Lighting keeps a colour row for every slot, since a disabled slot
keeps its colour, and the effective cache holds all 32 so pointing never reads
storage. Layout: [`pd-mode-domain-v1.md`](../spec/pd-mode-domain-v1.md).

# Device edits are saved as a complete, generation-bound profile

Cited as D-F37. Was D-L14.

A save carries the complete profile, keeping every untouched domain, bound to
the generation, digest and origin it was built from; the keyboard checks them
before staging and again after taking the candidate lease, so an edit never
lands on a profile it did not see, and the client then requires byte-equal
readback. Both halves validate everything before persistence. Combo timing
follows QMK: each combo has its own window or follows one default window, and
the hold threshold is one value for all combos. The per-domain saves this
first introduced gave way to atomic Apply (D-F40); the binding and readback
stand.

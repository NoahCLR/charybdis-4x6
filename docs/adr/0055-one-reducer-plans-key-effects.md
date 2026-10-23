# One reducer plans every key effect, projection applies them

Cited as D-F55.

Every fact the firmware decides a key by (press, tap series, leases, locks,
deferred releases) lives in one reducer state, keyed by physical position.
The reducer and its planners never touch QMK or a ledger: they plan effects
into one fixed, stack-held plan, and projection applies it to the layer,
modifier, report, held-action and pointing ledgers, which never write back
except through two named bridges (layer locks and the one-shot layer, and
pointing locks). Chosen over the slot/index reducers it replaced, which kept
their own per-slot state beside shared indexes and reached QMK side effects
directly, so overlapping keys had more than one source of truth. The cost is
the plan's
mechanics: a streaming plan projects between steps so a later step sees what
an earlier one did, and planning returns before projection so no plan stays
on the stack beneath another's effects.

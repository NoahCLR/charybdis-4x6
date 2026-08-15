# Synthetic-Key Ownership Architecture Review

Finding 09 closed in `review/2026-08-15-review-04/`, which is immutable. This
next sortable folder is the active review for Finding 08's physical/synthetic
report-ownership contract.

## Review Scope

- Define aggregate ownership for every literal report domain currently emitted
  by userspace.
- Introduce owner-scoped, idempotently releasable synthetic leases.
- Make physical event suppression authoritative at the pre-process/preflight
  seam.
- Make macro-local hold balance strict before migrating macro holds to leases.

## Prior Finding Status

| Prior finding | Status | Evidence |
| --- | --- | --- |
| Finding 08: aggregate synthetic ownership | partially resolved | Orphan macro key-up is rejected before side effects; aggregate report ownership and leases remain open |
| Finding 09: pending-release FIFO rollover | resolved | Closed review 04, direct queue runner, full host, target build, and stack gate |
| Finding 01: target stack safety | resolved | Any new ownership call chain must retain the 1,920 B reviewed budget |

## Intended Direction

Classification must occur once at the literal action boundary. A supported
action decomposes into its modifier mask and one native report-domain usage.
Acquisition must validate all components before mutating any count. A lease
records only normalized components and becomes active after every component is
owned. Release is idempotent and can affect only components in that lease.

Physical and managed counts jointly determine report visibility. Managed
transitions call QMK only at aggregate zero-to-one and one-to-zero boundaries;
physical defaults are suppressed only when userspace already knows another
owner keeps the usage visible. Pointer notifications follow those same visible
transitions.

## Landed Checkpoint: Strict Macro Balance

Macro key-up now requires an earlier unmatched key-down in the same payload.
This rule is shared by authored parsing, QMK/VIA decoding, and IR preflight.
Playback consumes its local balance entry before invoking unregister, so an
orphan release cannot touch another producer even if malformed IR bypasses a
front-end decoder.

## Open Decisions and Work

- Confirm compact state representation and measured BSS cost for keyboard,
  mouse, consumer, and system domains.
- Define saturation/underflow and rollback diagnostics.
- Migrate held actions, macro holds/chords, and literal press/release paths to
  scoped leases.
- Prove physical/basic and cross-synthetic overlap at the real hook ordering.
- Define reset reconciliation without blindly releasing physical owners.

## Current Status

Open. Finding 08 is only partially resolved and must not be marked verified
until aggregate ownership, lease migration, full host, target build, resource
measurement, documentation, and closure verification all pass.

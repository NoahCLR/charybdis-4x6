# Implementation Progress

This file tracks the audit captured in
[userspace-architecture-review.md](./userspace-architecture-review.md).

## 2026-04-14

### Follow-up quality review of the review-05 implementation

Completed in this pass:

- started with `git status --short`
- re-read the newest existing review folder,
  `review/2026-04-14-review-05/`, before reviewing the landed refactor work
- audited the current tree with focus on:
  - the handled-key materialization/refresh seam
  - the mutation-maintained key-runtime index and its public read surface
  - the new action-kind table and its test/build consequences
  - the host handled-key fixture and runtime-suite coupling
  - review/documentation integrity after the review-05 implementation
- wrote a new review folder for this follow-up audit

Key findings recorded in this review:

- no must-fix production correctness failure found
- should-fix: the new action-kind seam is over-coupled to runtime side effects,
  and the shared weak host-stub layer is now masking that coupling; the shim
  has already drifted from real function signatures
- should-fix: the key-runtime index is mutation-maintained, but its public API
  still exposes raw storage/layout and core consumers still depend on those raw
  arrays directly
- should-fix: the shared host handled-key fixture still exposes an internal
  “refresh overridden materialized contract” path that higher-level runtime
  suites rely on
- optional cleanup: `review/2026-04-14-review-05/progress.md` still reads as if
  the final full-suite/build verification had not yet happened

Areas assessed as solid in this pass:

- the handled-key contract is now authoritative on
  `handled_key_materialized_t.contract`
- the runtime index is genuinely mutation-maintained in production
- the dedicated index regression suite materially improves coverage
- the public action descriptor is smaller after removing `caps`

Verification run in this pass:

- `git status --short`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

Verification results:

- full host suite passed
- firmware build passed and produced
  `.build/bastardkb_charybdis_4x6_noah.uf2`

Checks intentionally skipped in this pass:

- none

Workspace scope:

- no sibling workspace folders were edited
- all changes in this pass are confined to
  `charybdis-4x6/review/2026-04-14-review-06/`

Next steps:

- if you want to act on this audit, the highest-value cleanup is to split the
  action-kind metadata seam from the executable dispatch-hook seam, or at least
  make the shared host stubs type-check against the real headers
- after that, narrow the key-runtime index read surface so raw slot-index
  arrays are not part of the public contract
- finally, reduce the shared host handled-key fixture back to the public
  authored/materialized seam and keep internal override-refresh helpers local
  to constructor-level tests

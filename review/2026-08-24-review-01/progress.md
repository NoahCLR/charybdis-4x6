# Live Profile Editing Progress

Thread: architecture and staged implementation of live authored-profile editing
from Charybdis Profile Studio.

Baseline at project opening: 87f356cd

Primary milestone: complete live RGB and key-behavior editing as defined in
README.md.

## Why This Folder Instead Of Review 19

review/2026-08-16-review-02 is an open full-firmware correctness audit. It owns
one must-fix, several should-fix findings, and their remediation lifecycle.

Live profile editing is a materially different architecture topic spanning a
new desktop-to-device protocol, persistent schema, runtime provider boundary,
safe activation, and new UI state. Per AGENTS.md, it opens
review/2026-08-24-review-01 rather than turning Review 19 into a second project.

Cross-thread prerequisites remain owned by Review 19. This folder records their
dependency status without duplicating or prematurely resolving its findings.

## Stage Status

| Stage | Status | Exit evidence |
| --- | --- | --- |
| 00 — Contract and baseline | planned | open |
| 01 — Live Link transport | blocked on Stage 00 and Review 19 prerequisite | open |
| 02 — Schema, store, and commit | blocked on Stage 01 | open |
| 03 — Live RGB | blocked on Stage 02 | open |
| 04 — Live key behaviors | blocked on Stage 02 | open |
| 05 — Milestone A integration | blocked on Stages 03 and 04 | open |
| 06 — Defaults, layout, and macros | blocked on Milestone A | open |
| 07 — Combos and layer structure | blocked on Stage 06 | open |
| 08 — Production closure | blocked on Stage 07 | open |

Only one stage should normally be in progress. Parallel work is allowed only
when the stage brief identifies independent work packages and file ownership
does not overlap.

## Completed Work

### 2026-08-24 — Project architecture and implementation control folder

Created:

- README.md with scope, non-goals, architecture summary, stage map, and the
  Milestone A definition
- userspace-architecture-review.md with findings, intended component
  boundaries, transaction model, testing requirements, and implementation
  sequence
- agent-workflow.md with required entry, contract, safety, verification, and
  handoff practices
- decisions.md with accepted principles and Stage 00 open decisions
- risks.md with cross-stage risks and closure evidence rules
- stages/ briefs for the complete implementation sequence

No firmware, Profile Studio, authored profile, generated docs, or sibling QMK
files were changed.

## Verification

Passed on 2026-08-24:

- `git status --short` — the task began with a clean worktree; the final status
  contains only this new review folder
- `git diff --check`
- `rg -n "[[:blank:]]+$" review/2026-08-24-review-01` — no matches
- local-link existence check for every target linked from this folder's
  `README.md`

Runtime host tests and firmware compile are intentionally skipped under the
AGENTS.md documentation-only exception because this pass changes review notes
only.

Hardware is not confirmed.

## Open Decisions

Stage 00 owns D-009 through D-014 in decisions.md:

- storage layout
- fixed capacities
- split integration shape
- Profile Studio HID adapter
- preview and apply semantics
- generation authority and drift resolution

No implementation should freeze a wire or storage format before these decisions
have measured evidence.

## Current Blockers

- Stage 00 measurements and decisions have not started.
- Review 19 Finding 1 remains a transport-safety prerequisite.
- Review 19 Finding 4 remains relevant to effect/handler alignment.
- The baseline persistence and role-swap hardware matrix remains open.

These are expected opening conditions, not project failure.

## Next Steps

1. Execute Stage 00 exactly as scoped in stages/00-contract-and-baseline.md.
2. Record measured EEPROM, static RAM, linker heap, stack, flash, and encoded
   profile budgets.
3. Prototype the desktop HID boundary without landing production protocol
   commitments.
4. Resolve D-009 through D-014 and update this architecture review if the
   intended structure changes.
5. Confirm the Review 19 transport prerequisite is resolved in its owning
   folder before Stage 01 declares transport readiness.

## Handoff History

Append new entries here in chronological order. Each entry must name:

- stage and work package
- files and contracts changed
- exact verification
- skipped or unconfirmed checks
- risks and decisions changed
- next work package

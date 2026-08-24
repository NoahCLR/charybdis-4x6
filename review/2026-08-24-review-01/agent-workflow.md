# Agent Workflow

This file defines how agents implement the live-profile project without losing
architecture decisions, verification evidence, or source ownership.

## Before Changing Code

Every agent must:

1. Follow the root AGENTS.md and start with git status --short.
2. Preserve unrelated user changes.
3. Read README.md, progress.md, decisions.md, risks.md, and the assigned stage
   brief in this folder.
4. Read the newest relevant code and tests. Stage briefs are plans, not a
   substitute for inspecting the current tree.
5. Check whether review/2026-08-16-review-02 still owns an open prerequisite.
   That review owns the lifecycle and evidence for its findings; this project
   records only the dependency result.
6. State the bounded work package before editing.

## Work Package Size

Prefer one mechanically verifiable contract per pass. Good work packages are:

- a wire encoder and decoder with malformed-frame tests
- a storage header and recovery rule with power-loss simulations
- one provider seam and all of its consumers
- one Profile Studio transport adapter with a fake-device harness
- one RGB table family migrated end to end
- one safe-activation predicate plus lifecycle tests

Avoid mixing protocol design, storage layout, runtime migration, large UI work,
and hardware validation in one pass.

## Architecture Ownership

The authored profile remains in the selected keymap folder. Shared runtime
policy and protocol code belongs under users/noah/. Profile Studio code belongs
under tools/charybdis-profile-studio/.

Expected package boundaries:

- users/noah/lib/profile/schema/: canonical field ids, capacities, validation,
  and wire encoding
- users/noah/lib/profile/store/: persistent slots, checksums, generations,
  fallback, and recovery
- users/noah/lib/profile/runtime/: effective profile provider, snapshots,
  activation, invalidation, and quiescence policy
- users/noah/lib/profile/protocol/: VIA custom-channel request and response
  handling
- users/noah/lib/compat/: QMK, VIA, EEPROM, Raw HID, and split-fork contracts
- users/noah/lib/split/ or the existing VIA reconciliation package: durable
  peer convergence, chosen explicitly in Stage 00
- tools/charybdis-profile-studio/: transport adapter, canonical desktop
  encoder, connection state, diff state, and UI actions

These paths are intended ownership, not permission to create all modules
up-front. Add only the files needed by the current stage and wire new firmware
sources through users/noah/source_manifest.mk in the same pass.

## Contract Change Rule

Any change to wire format, storage format, capacity, commit semantics, source
authority, or split authority must:

1. update decisions.md or add an explicit open decision;
2. update userspace-architecture-review.md if the intended structure or
   tradeoff changed;
3. update the current stage brief;
4. add compatibility and rejection tests;
5. record migration behavior for already-persisted data.

Raw serialization of C structs is forbidden even for a prototype that is
expected to land.

## Safe Activation Rule

An agent must not make behavior-affecting profile state visible merely because
the last transport chunk arrived.

The activation owner must prove:

- the complete candidate was received;
- schema, lengths, ranges, cross-references, and checksum are valid;
- the generation transition is legal;
- the chosen safe-boundary predicate is satisfied;
- caches and derived state are invalidated as one publication;
- failure leaves the previous profile active;
- reset or power loss selects either the old valid profile or the new valid
  profile, never a partially written candidate.

If the pass changes held-action, macro, combo, layer, pointer-mode, or modifier
lifecycles, extend the matching integration scenarios.

## Source And Device Changes

Profile Studio must treat source writes and device commits as two observable
operations. Never hide a partial outcome.

Every operation reports one of:

- draft only
- device preview active
- device persisted
- source updated
- source and device synchronized
- source updated but device failed
- device updated but source failed
- incompatible schema or capacity
- peer half pending or divergent

The UI must retain enough information to retry or reverse a partial result.

## Verification

Run targeted tests repeatedly while implementing. Before handing back runtime
or firmware work, run the full host suite and firmware compile required by
AGENTS.md.

Additional project gates should be introduced with the owning stage:

- schema round-trip, malformed input, and compatibility fixtures
- storage recovery and interrupted-write fixtures
- protocol framing, retries, idempotency, and capability negotiation
- fake-device Profile Studio transport tests
- effective-profile provider tests
- split convergence, disconnect, reconnect, and role-swap tests
- RGB frame-boundary and cache-invalidation tests
- key-runtime safe-activation and held-output lifecycle tests
- fresh target memory and stack budget checks
- Milestone A hardware matrix

Documentation-only planning changes may use the AGENTS.md documentation
exception, but must still run git diff --check.

## Required Handoff

At the end of every pass, update progress.md with:

- stage and bounded work package
- files and contracts changed
- completed behavior
- exact verification commands and results
- checks skipped or hardware not confirmed
- open risks or decisions
- next concrete work package

Also update the current stage brief:

- mark completed deliverables
- attach evidence beside exit criteria
- leave unresolved criteria visibly open

Do not call a stage complete in chat unless progress.md contains its exit
evidence.

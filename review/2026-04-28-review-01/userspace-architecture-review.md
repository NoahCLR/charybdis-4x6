# Userspace Folder Layout Review

Prompt used: `prompts/initial-architecture-review.md`.

## Scope

This review is a focused architecture pass on folder and package layout under
`users/noah/lib`. It does not reopen the closed userspace overlap review in
`review/2026-04-27-review-01/`; that folder recorded a final `close thread`
verdict and is immutable history.

The review started as a layout audit and then landed a mechanical package move.
The intended behavior is unchanged: runtime authority still belongs to the same
reducers, planners, projections, compatibility adapters, and ownership ledgers.
The refactor changes where those roles live so future edits are routed by
ownership rather than by old migration buckets.

## Current Verdict

No must-fix behavior issue was found from folder layout alone. The maintainability
issue was real: several directory names described earlier migration phases rather
than the current ownership model. The tree now matches the intended role split.

## Prior Finding Status

### Resolved: `key/runtime/core` Was a Bucket, Not a Package Boundary

Status: resolved.

The old `users/noah/lib/key/runtime/core/` bucket has been split by role:

- `users/noah/lib/key/runtime/reducer/` owns reducer state, ownership mechanics,
  and read-only state queries.
- `users/noah/lib/key/runtime/planning/` owns effect plans, release planning,
  scan planning, and tap-series flush helpers.
- `users/noah/lib/key/runtime/projection/` owns QMK-facing effect projection,
  feedback projection, PD projection, and projection snapshots.
- `users/noah/lib/key/runtime/queue/` owns pending-release queue mechanics.
- `users/noah/lib/key/runtime/trace/` owns reducer trace helpers.

Enforcement references:

- `users/noah/source_manifest.mk` wires the new source paths into the firmware
  build.
- `tests/host/noah_source_manifest.sh` mirrors the new support source groups
  for host tests.
- `tests/host/run_feature_gate_compile_tests.sh` compiles the manifest across
  feature variants and guards key-runtime boundary includes.

### Resolved: `state/runtime` Looked Like a Third Runtime Owner

Status: resolved.

The old shared-state bucket has been split into service packages:

- `users/noah/lib/state/shared/` owns internal runtime storage, reset, and the
  shared state backing store.
- `users/noah/lib/state/diagnostics/` owns runtime debug, diagnostics, and trace.
- `users/noah/lib/state/modifiers/` owns keyboard modifier state and policy.
- `users/noah/lib/split/` owns split runtime sync transport.

Enforcement references:

- `tests/host/run_feature_gate_compile_tests.sh` now allows internal runtime
  storage includes only from the shared owner, runtime trace owner, and the
  ownership modules that need storage access.
- Split sync callers include `users/noah/lib/split/runtime_sync.h`, making split
  transport visibly separate from shared runtime storage.

### Resolved: Runtime Slot Interaction Name Collided With Authored Behavior

Status: resolved.

The authored behavior package is now consistently `users/noah/lib/key/behavior/`.
The reducer-cached slot contract is `users/noah/lib/key/runtime/slot/slot_interaction.h`.
This keeps authored lookup/materialization separate from the runtime slot shape
that the reducer stores after a press resolves.

Enforcement references:

- Production and host includes now point at `key/behavior/*` for authored
  behavior and `key/runtime/slot/slot_interaction.h` for reducer slot state.
- `tests/host/include/host_handled_key_fixture.h` exercises the slot contract
  through the new path.

### Resolved: `key/runtime/effects` Was Too Thin

Status: resolved.

The effect vocabulary and queue helper moved under
`users/noah/lib/key/runtime/planning/` next to effect-plan construction and the
release/scan planners that consume it. Projection still consumes the shared
effect vocabulary through the planning header, but there is no separate
header-only effects package.

### Solid: Top-Level Domain Split Still Holds

Status: still solid.

The top-level userspace packages remain appropriate:

- `action/` owns action metadata, dispatch, lifecycle, and synthetic records.
- `compat/` owns QMK/VIA/fork compatibility adapters.
- `key/` owns authored key behavior, key ownership registries, and key runtime.
- `macro/` owns macro payload and VIA macro providers.
- `pointing/` owns PD definitions, mode handlers, policies, and PD runtime.
- `rgb/` owns RGB runtime, stages, validation, and automouse rendering.

## Intended Layout

```text
users/noah/lib/key/
  behavior/              authored handled-key behavior lookup/materialization
  ownership/             held action and repeat registries
  runtime/               QMK-facing key-runtime integration
    reducer/             canonical key-runtime state and ownership mechanics
    planning/            effect plans, release/scan planners, tap-series helpers
    projection/          effect projection and projection snapshots
    queue/               pending-release queue mechanics
    slot/                slot interaction, key-position codec, origin registry
    trace/               reducer trace helpers

users/noah/lib/state/
  shared/                internal shared runtime storage and reset
  diagnostics/           runtime debug, diagnostics, trace
  modifiers/             keyboard modifier state and policy
  ownership/             layer and keyboard modifier ownership ledgers

users/noah/lib/split/
  runtime_sync.*         split runtime sync transport
```

## Boundary Expectations

- `key/runtime/reducer/` owns key-runtime truth. Top-level runtime wrappers feed
  it and project its plans; they should not recreate independent state machines.
- `key/runtime/planning/` owns release, scan, tap-series, and effect-plan
  decisions. Adapters such as `deferred_release.c` may queue or drain planned
  effects but should not re-decide release semantics.
- `key/runtime/projection/` is the outward write point for QMK-facing side
  effects and projection snapshots.
- `state/shared/` is plumbing, not a public runtime API. New callers should use
  debug, ownership, modifier, or split APIs instead of internal storage headers.
- `split/` is transport. It mirrors already-owned state and should not become
  runtime authority.

## Verification Contract

The layout is only considered stable when these pass for the refactor:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- targeted key-runtime, modifier, PD, runtime diagnostic/trace, split sync, and
  authored profile tests for the files touched
- `python3 tools/profile_introspect.py --write`
- `python3 tools/profile_introspect.py --check`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Next Steps

1. Review the large move diff for path-only intent before staging.
2. Decide whether to close this folder-layout review now or run one follow-up
   audit pass first.
3. Use a new review folder for any later, materially different architecture
   topic after this one closes.

# Userspace Architecture Review

Date: 2026-04-13

Status: new review created after
[2026-04-12-review-03](../2026-04-12-review-03/userspace-architecture-review.md).
This pass focuses on directory layout only. The goal is to make the lib tree
match the actual editing surfaces and ownership seams already present in the
code without changing runtime behavior.

Scope: structural organization for `users/noah/lib/` and closely related
authoring headers under `users/noah/`. Logic changes, API changes, and
split-by-behavior refactors are out of scope for this review.

## Executive Summary

The repo no longer needs broad architectural invention. It needs a clearer
physical layout for the architecture it already has.

Phases 1 through 5 of that migration have now proven out the approach
cleanly:

- `split_role.c` now lives under `lib/compat/`
- `macro_dispatch.*` now lives under `lib/macro/`
- `keymap_materialize.h` now lives at `users/noah/` instead of under `lib/`
- `lib/rgb/` is now split into `core/`, `automouse/`, and `stages/`
- `lib/pointing/` is now split into `defs/`, `runtime/`, `policy/`, and
  `modes/`
- `lib/state/` is now split into `ownership/` and `runtime/`
- `lib/key/` is now split into `interaction/`, `ownership/`, and `runtime/`,
  with `runtime/effects/` and `runtime/slot/` for the execution surface

The intended end state is:

- `lib/compat/` for QMK, VIA, and split-role contract surfaces
- `lib/action/` for generic action emission and owned-keycode helpers
- `lib/macro/` for macro payloads, default seeding, and hardcoded macro
  dispatch
- `lib/key/` split by the real key-engine seams:
  - `interaction/` for authored behavior schema, handled-key resolution,
    multi-tap semantics, and keymap validation
  - `ownership/` for long-lived per-key held-action and held-repeat ownership
  - `runtime/` for delayed action replay, process/preflight/transition flow,
    effect execution, and slot reducers
- `lib/pointing/` split into:
  - `defs/`
  - `runtime/`
  - `policy/`
  - `modes/`
- `lib/rgb/` split into:
  - `core/`
  - `automouse/`
  - `stages/`
- `lib/state/` split into:
  - `ownership/`
  - `runtime/`

Two constraints govern the whole migration:

1. each pass is a pure move/update pass, not a logic refactor
2. every move updates source manifests, direct host runners, and maintainer
   docs in the same pass

## Why This Structure Fits The Current Code

### 1. Key interaction is one engine

The docs and code treat authored behavior, tap/hold timing, multi-tap, and
threshold resolution as one key-interaction engine:

- `docs/INTERACTION_MODEL.md` explicitly says multi-tap is part of the same
  tap/hold model
- `key_behavior.h` defines hold modes and timing surfaces directly inside the
  key behavior schema
- `multi_tap_engine.*` depends on `key_behavior.h`
- `handled_key.*` resolves authored behavior into one handled-key contract
- `key_runtime_slot_policy.*` consumes `hold_behavior_t` directly

That means the right top-level seam inside `lib/key/` is not
"behavior vs hold". It is:

- interaction resolution
- owned held/repeat state
- runtime orchestration and slot execution

### 2. Pointing has four distinct surfaces

The pointing code already separates cleanly into:

- generated/shared definitions
- runtime state and lifecycle
- pointer-layer policy
- mode-owned handlers and lifecycle glue

Those now live as:

- `defs/` for manifest-generated keycodes, flags, and shared mode metadata
- `runtime/` for registry materialization, lifecycle transitions, state, and
  runtime dispatch
- `policy/` for pointer-layer ownership glue
- `modes/` for mode-owned handlers and lifecycle hooks

### 3. RGB has one missing surface today: automouse

`rgb_automouse.*` and `rgb_automouse_stage.*` are one feature surface, but the
old flat RGB folder obscured that. The landed split keeps:

- `core/` for runtime orchestration, helper surfaces, and validation
- `automouse/` for auto-mouse state plus its gradient stage
- `stages/` for the remaining ordered render stages

## Migration Order

Use the lowest-risk path first:

1. low-risk ownership moves:
   - `lib/split_role.c` -> `lib/compat/`
   - `lib/action/macro_dispatch.*` -> `lib/macro/`
   - `lib/keymap_materialize.h` -> `users/noah/`
2. RGB folder split
3. pointing folder split
4. state folder split
5. key folder split

Do not start with `lib/key/`. It has the widest include surface and the most
host runners.

## Landed So Far

The completed phases all followed the same constraint set:

- move files only
- update include paths and manifests in the same pass
- update direct host runners and user-facing docs in the same pass
- run the targeted host checks before the full suite and firmware compile

Phase 1 landed the low-risk ownership moves under `compat/`, `macro/`, and
`users/noah/`.

Phase 2 landed the RGB split:

- `core/` for runtime, helpers, defaults, and validation
- `automouse/` for auto-mouse state and its stage
- `stages/` for layer, preview, pd-mode, and key-feedback stages

Phase 3 landed the pointing split:

- `defs/` for manifest-driven shared definitions
- `runtime/` for registry, lifecycle, state, and runtime dispatch
- `policy/` for pointer-layer policy
- `modes/` unchanged for mode-owned files

Phase 4 landed the state split:

- `ownership/` for layer and modifier ownership registries
- `runtime/` for shared runtime storage, debug, trace, sync, and live modifier
  state

Phase 5 landed the key split:

- `interaction/` for authored behavior schema, lookup, handled-key
  interpretation, multi-tap, and keymap validation
- `ownership/` for held-action and held-repeat ownership
- `runtime/` for delayed action replay and top-level key-runtime orchestration
- `runtime/effects/` for shared effect vocabulary and queue field layout
- `runtime/slot/` for slot reducers, slot results, and slot-local helpers

The directory-layout migration described by this review is complete.

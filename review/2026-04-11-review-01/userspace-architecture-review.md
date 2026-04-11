# Userspace Architecture Review

Date: 2026-04-11

Status: refreshed after the follow-up implementation passes tracked in
[progress.md](./progress.md).

Scope: `noah` userspace for the Charybdis 4x6. This review intentionally
ignores hardware changes and focuses on software architecture, module
boundaries, extensibility, state flow, and long-term maintainability.

## Summary

This codebase is still substantially better structured than a typical
single-file QMK keymap. The original review hotspots were real, but the current
architecture is no longer the same one that first triggered them.

The biggest follow-up items from that review have landed:

- the handled-key runtime moved from one global active key to a slot-based
  model with slot-owned pending multi-tap state
- pointing-device mode policy is now trait-driven through
  `pd_mode_manifest.h`
- the authoring/runtime header split is explicit:
  `noah_keymap_ids.h`, `noah_keymap.h`, and `noah_runtime.h` each have a
  separate job
- fork-specific QMK and VIA assumptions are centralized under
  `users/noah/lib/compat/`
- RGB rendering is decomposed into focused stage modules
- validation and host coverage now reach authored data and hook chaining more
  directly

The main remaining constraints are narrower and more explicit now:

- handled-key overlap is capped at `2` active slots
- hook overrides are a deliberate integration seam, not casual keymap-data
  authoring
- genuinely new pd-mode policy still requires a new manifest trait or a new
  central consumer

## Key Findings

### 1. The key-behavior engine is slot-based now, but still bounded

The runtime no longer serializes all handled-key behavior through one global
`active_key` / `multi_tap` pair. Shared state now carries
`active_slots[KEY_RUNTIME_ACTIVE_SLOT_CAPACITY]`, and each slot owns both active
press state and any deferred multi-tap chain for that physical key position.

Why this matters:

- overlapping handled-key behavior no longer forces everything through one
  global state object
- current profile behavior is better isolated per physical key position
- the remaining ceiling is explicit: slot capacity is currently `2`, not a
  hidden single-key limitation

Key references:

- `users/noah/lib/state/runtime_shared_state.h`
- `users/noah/lib/key/key_runtime_state.h`
- `users/noah/lib/key/key_runtime_slot.c`
- `users/noah/lib/key/key_runtime_transition.c`

### 2. The pointing-mode manifest is trait-driven now

Mode identity, generated keycodes and flags, and cross-cutting policy now come
from `pd_mode_manifest.h` plus `PD_MODE_TRAIT_*` flags consumed centrally by the
registry and pointer-layer policy.

Why this matters:

- simple new modes usually stay manifest/handler/keymap/RGB/doc work
- policy such as auto-mouse anchoring, typing-layer preference, dragscroll
  backend ownership, auto-mouse lock ownership, and owned `GUI` behavior no
  longer depends on scattered mode-identity checks
- the remaining extension seam is the trait surface itself: truly new shared
  policy still needs a new trait and a central consumer

Key references:

- `users/noah/lib/pointing/pd_mode_manifest.h`
- `users/noah/lib/pointing/pd_mode_registry.c`
- `users/noah/lib/pointing/pointer_layer_policy.c`
- `users/noah/lib/pointing/pd_modes.h`

### 3. The authoring/runtime boundary is much sharper

The header split that the original review recommended has landed:

- `users/noah/noah_keymap_ids.h` owns shared ids, generated keycodes, and
  materialized authored-data symbols
- `users/noah/noah_keymap.h` is the authoring bundle for keymap-owned
  translation units
- `users/noah/noah_runtime.h` holds the `noah_*` hook helpers

Host compile gates now enforce that runtime modules do not drift back to
`noah_keymap.h`, and keymap-owned translation units do not include
`noah_runtime.h`.

Why this matters:

- authored profile data stays on a narrower surface
- runtime modules depend on the minimum headers they actually need
- hook overrides are now clearly integration work, not something the repo
  should imply belongs in ordinary keymap data files

Key references:

- `users/noah/noah_keymap_ids.h`
- `users/noah/noah_keymap.h`
- `users/noah/noah_runtime.h`
- `tests/host/run_feature_gate_compile_tests.sh`

### 4. Fork-specific QMK contracts are still real, but now localized

Core behavior still depends on a few firmware-fork contracts, but those
assumptions now live under `users/noah/lib/compat/` instead of being scattered
across runtime modules.

Why this matters:

- upgrade risk is explicit and easier to audit
- copied or overridden QMK behavior now has a named ownership surface
- portability is still bounded by the fork, but the boundary is much easier to
  reason about

Key references:

- `users/noah/lib/compat/qmk_contract.c`
- `users/noah/lib/compat/qmk_mod_contract.c`
- `users/noah/lib/compat/qmk_via_contract.c`
- `users/noah/lib/pointing/pd_runtime.c`
- `users/noah/lib/macro/via_macro_defaults.c`

### 5. Validation and integration coverage now reach authored data directly

The host suite now covers more than runtime mechanics. It validates duplicate
`key_behaviors[]`, RGB authored data, real-profile authored data, header
boundaries, pd-mode traits, and hook chaining.

Why this matters:

- the repo is better protected against quiet authored-data drift
- verification can point to concrete commands instead of informal expectations
- the main remaining risk is that future structural changes must keep the newer
  coverage aligned, especially if handled-key slot capacity or hook boundaries
  change again

Key references:

- `users/noah/lib/key/key_behavior_lookup.c`
- `users/noah/lib/key/keymap_validation.c`
- `users/noah/lib/rgb/rgb_validation.c`
- `tests/host/run_real_profile_validation_tests.sh`
- `tests/host/run_feature_gate_compile_tests.sh`
- `tests/host/hook_chaining_test.c`

## Architecture Assessment By Area

### Architecture and separation of concerns

The strongest separation now exists in:

- `layer_ownership`
- `keyboard_mod_ownership`
- `held_action`
- `action_lifecycle`
- the `lib/compat/` boundary
- the RGB stage modules

The weakest separation now exists in:

- the fixed two-slot handled-key ceiling
- the hook-override integration seam
- future pd-mode behavior that does not fit the current trait surface cleanly

### Modularity and extensibility

Easy today:

- add a new `key_behaviors[]` row
- add or adjust layer-owned authored data
- add a simple new pd mode that fits the existing manifest + trait model
- change RGB authored data without touching runtime code

Hard today:

- raise handled-key overlap beyond the current two-slot model
- introduce a pd-mode behavior that needs brand-new cross-cutting policy
- define a hook-override pattern that preserves the current header boundary
  without special-case documentation

### Abstractions and interfaces

Good abstractions:

- layer ownership
- modifier ownership
- held action registration
- key transition planning plus slot lifecycle
- manifest-driven pd-mode identity plus traits
- RGB render stages

Leaky or deliberate seams:

- hook overrides
- QMK/fork compatibility
- the fixed handled-key slot capacity

### Code organization

The repo is organized well for firmware userspace:

- `users/noah/lib/key`
- `users/noah/lib/action`
- `users/noah/lib/pointing`
- `users/noah/lib/state`
- `users/noah/lib/rgb`
- `users/noah/lib/compat`

The previous size hotspot in `rgb_runtime.c` is no longer the same concern. The
main remaining hotspots are the key runtime orchestration files and the authored
`keymap.c` surface.

### State management and flow

The state model is still deliberate, not accidental. The key difference now is
that handled-key state is slot-owned instead of fully global.

That keeps the current runtime coherent while allowing limited overlap, but it
also means the next meaningful concurrency step is structural again: either keep
the explicit two-slot cap or finish the move toward a more general per-key FSM
table.

### Scalability risk

The main current technical-debt vectors are:

- fixed handled-key slot capacity of `2`
- pd-mode trait growth if new behaviors arrive without pruning or
  consolidation
- documentation drift when architectural follow-up closes in code and
  `progress.md`, but the higher-level review summary is not refreshed in the
  same pass

## Recommended Plan

Most of the original follow-up plan from this review is now complete and tracked
in [progress.md](./progress.md). The current next-step recommendations are
narrower:

1. Keep the two-slot handled-key model unless real user behavior needs more
   overlap, then finish the per-key FSM step instead of adding more ad hoc slot
   rules.
2. Keep new pd-mode policy trait-driven. If a new mode needs shared behavior,
   add a new `PD_MODE_TRAIT_*` flag and consume it centrally instead of
   restoring mode-identity checks.
3. Define one canonical hook-override integration pattern that matches the
   current header boundary and document that pattern wherever hook overrides are
   described.
4. Keep `README.md`, `docs/`, and this review folder in sync when architecture
   follow-up closes, so `progress.md` is not the only place that reflects the
   new state.

## Suggested Execution Order

For future architecture work, the lowest-risk order is:

1. resolve the hook-override documentation and integration boundary
2. extend handled-key concurrency only if concrete behavior requires it
3. grow the pd-mode trait surface only when a real shared policy gap appears
4. keep authored-data validation and review docs updated as part of each
   change, not after the fact

## Verification Status

The follow-up implementation plan recorded in [progress.md](./progress.md)
closed the major review items with repeated host-test runs and repeated
`qmk compile -kb bastardkb/charybdis/4x6 -km noah` checks.

The default high-signal verification surface for the current architecture is:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_real_profile_validation_tests.sh`
- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

The current architecture is not just working; many of the original review risks
are materially smaller now. Remaining work should be driven by concrete
behavioral pressure or boundary pressure, not by refactor momentum alone.

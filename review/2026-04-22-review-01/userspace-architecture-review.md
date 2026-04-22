# Userspace Architecture Review

## Scope

This review covers the combo-origin footprint work that landed on
2026-04-22. The goal was to stop treating QMK combo outputs as synthetic key
position `(0,0)` for locality-sensitive userspace behavior, plus the follow-up
runtime remediation needed after the later key-position replay widening caused
stack-backed transition plans to grow too large on hardware. It now also
covers the narrow phase-1 key-runtime core compaction that removed redundant
stored key positions from the slot-indexed core arrays, plus the phase-2
persistent runtime compaction that packed the remaining non-slot ownership and
pending-release storage.

## Decisions

### Combos now have two runtime identities

- Ownership paths still keep one representative owner key.
- Locality-sensitive paths now also keep the full physical combo footprint.

That split keeps existing ownership contracts small and stable while fixing RGB
and PD rendering semantics.

### Locality is derived from a shared bitmap primitive

- `origin_registry` stores a bitmap per representative owner key.
- Single physical keys default to a single-key bitmap.
- Combo outputs overwrite that owner entry with the full combo bitmap.
- `split_side_mask_t` derives `LEFT`, `RIGHT`, or `BOTH` from the bitmap.

This removed the need for duplicated side/key tracking in higher layers.

### QMK combo normalization stays userspace-local

- `qmk_combo_origin.c` shadows the live physical combo press stream.
- On `COMBO_EVENT`, it rewrites the representative owner key to the last chord
  key and stores the full footprint in the origin registry.
- If combo identity cannot be reconstructed cleanly at normalize time, the
  compat layer now falls back to the latest observed physical key plus an
  explicit broad locality footprint, so userspace does not regress to fake key
  `(0,0)`.
- The shadow logic uses live resolved keycodes and combo-ref-layer behavior, so
  dynamic keymaps remain authoritative.

This avoids repo-crossing patches into `../bastardkb-qmk`.

### Split sync stays semantic, not engine-level

- Key-feedback sync now ships a compact key bitmap instead of a single packed
  key.
- PD mode sync now ships a side mask instead of a single owner half.

The packet remains comfortably below the QMK RPC limit while matching the new
locality contract.

### Hot replay paths keep origin semantics in packed form

- The handled-key runtime still carries real physical origin through tap and
  delayed replay paths.
- The hot replay effect payloads now store that origin as a packed matrix index
  instead of a full `keypos_t`.
- Projection code unpacks back to `keypos_t` only at the point where the
  action is emitted.

This keeps the newer trigger-origin behavior while avoiding another silent
stack regression in the transition planners.

### Slot-indexed core arrays now derive key positions instead of storing them

- `press_tokens[]` and `tap_series[]` are indexed by physical matrix slot.
- Their entries no longer store a duplicated `key_pos`.
- Internal helpers derive `keypos_t` from the slot index when debug or runtime
  logic needs a physical key again.
- This rule is intentionally limited to slot-indexed arrays. Ownership tables
  like `leases` or `pending_releases` still carry explicit `key_pos` because
  their entries are not one-per-slot.

This keeps the compaction local to the core layout and avoids accidental
generalization onto non-slot surfaces that would break ownership semantics.

### Non-slot persistent surfaces now pack stored key positions

- `leases[]` and `pending_releases[]` are not one-entry-per-slot, so they do
  not derive position from array index.
- Instead, they now store packed matrix indices internally and unpack back to
  `keypos_t` only when a semantic surface needs to observe them.
- The public pending-release API remains semantic even though the stored slot
  representation is compact.
- The old `deferred_release_blockers[]` storage was removed because the
  current blocker contract is already derived live from press tokens.

This preserves the “derive only when identity really comes from the index”
rule while still shrinking the persistent non-slot state.

## Current Structure

- Combo ingress and QMK-specific assumptions live in
  `users/noah/lib/compat/qmk_combo_origin.c`.
- Shared locality storage lives in
  `users/noah/lib/key/runtime/origin_registry.c`.
- Hot replay key-position packing lives in
  `users/noah/lib/key/runtime/keypos_codec.h`.
- Slot-index key-position derivation for `press_tokens[]` and `tap_series[]`
  lives in `users/noah/lib/key/runtime/core/runtime.c`.
- Packed non-slot persistent storage for `leases[]` and `pending_releases[]`
  also lives in `users/noah/lib/key/runtime/core/runtime.h` and
  `users/noah/lib/key/runtime/core/runtime.c`.
- Key-feedback locality renders from the synced bitmap in
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`.
- PD trigger-side rendering derives from the synced side mask in
  `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`.
- Validation for authored combo member ambiguity lives in
  `users/noah/lib/key/interaction/keymap_validation.c`.

## Tradeoffs

- Duplicate combo outputs across distinct combos remain allowed, but overlapping
  active combos that share the same output still have an ambiguous release path
  because upstream `COMBO_EVENT` records do not expose combo identity.
- When multiple active combos with the same output are in play, locality is
  intentionally broadened to the union footprint instead of pretending a single
  side or key.
- A combo row must not repeat the same member keycode within that one row,
  because the footprint tracker cannot disambiguate that authored shape.
- Hot replay effect payloads are intentionally size-constrained. Carrying full
  `keypos_t` through stack-backed transition plans is not acceptable for this
  firmware target.
- Slot-index compaction is only valid when the slot identity already is the
  physical key identity. That is true for `press_tokens[]` and `tap_series[]`
  after combo normalization, but not for the non-slot ownership tables.
- Packed non-slot storage is a better fit for ownership tables than exposing
  internal slot layout. That keeps semantic APIs stable while reducing memory.

## Intended Invariants

- A physical key always has at least a single-key footprint.
- A combo output never falls back to fake key `(0,0)` for userspace locality.
- `KEY_FEEDBACK_MODE_KEY_HALF` may broaden to both halves for cross-half combos.
- `KEY_FEEDBACK_MODE_KEY` paints every combo key in the footprint.
- `PD_COLOR_MODE_TRIGGER_HALF` may broaden to both halves for cross-half
  combo-triggered modes.
- `key_runtime_effect_t` must stay small enough that transition plans do not
  materially expand the firmware stack footprint.
- Stack-backed plan surfaces must stay mechanically size-guarded:
  - `sizeof(key_runtime_effect_t) <= 12`
  - `sizeof(key_runtime_transition_plan_t) <= 196`
  - `sizeof(key_runtime_core_effect_plan_t) <= 196`
  - `sizeof(key_runtime_core_release_effect_plan_t) <= 196`
- `press_tokens[]` and `tap_series[]` may derive key position from slot index,
  but non-slot arrays must not silently adopt that rule.
- Non-slot persistent arrays may pack key position internally, but public
  semantic surfaces must continue to expose normal `keypos_t`.
- Stale storage that is no longer authoritative should be removed instead of
  being kept “just in case”; deferred release blockers are derived, not stored.
- All of the above must stay green under host tests and the firmware compile.

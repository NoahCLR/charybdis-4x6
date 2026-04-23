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
pending-release storage. It now also covers the truthful RGB feedback-layer
follow-up that added a persistent combo layer, truthful per-key key-behavior
semantics, explicit combo underlay / overlay routing around preview and PD
state, and split runtime sync broken into base, combo, and key-semantic
packets.

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

### Combo identity is now its own RGB layer

- Any active combo gets a steady combo color layer.
- Combos that currently own preview or PD state are routed into a combo
  underlay so those state indicators can repaint above them.
- Unrelated active combos stay in a combo overlay above preview and PD.
- Authored key-behavior feedback still renders last above both combo layers.

This keeps combo-ness visible even when the combo output also drives other
stateful overlays.

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

### Split sync now mirrors the render surfaces directly

- Base runtime sync carries only automouse, PD ids / trigger sides, and preview
  layer.
- Combo feedback sync carries separate underlay and overlay bitmaps.
- Key-feedback sync carries packed per-key semantic truth plus shared flash
  metadata.
- Packet bytes are the authoritative sync contract. The runtime no longer has
  a separate “request force-send on next tick” API for ordinary state changes.
- The key-feedback phase byte is only meaningful when the synced semantic map
  actually contains flashing semantics.
- Heartbeat cadence is now per surface:
  - active packets re-heartbeat quickly
  - empty/default packets re-heartbeat more slowly

The slave no longer reconstructs feedback from a single “current feedback
snapshot”; it renders the same layer inputs the master uses.

### Preview display now has a narrow split-only handoff bridge

- Semantic preview ownership is still determined entirely by the handled-key
  runtime.
- The exported preview display surface may briefly keep showing the same layer
  after semantic preview drops, but only when that layer is already really
  active on the master.
- This bridge exists to hide transport skew between:
  - the custom preview field in split runtime sync
  - the normal upstream split layer-state propagation
- The bridge is display-only, clears immediately if the layer is no longer
  active, and expires after a short fixed timeout.

This keeps the slave from flashing the underlying layer color during
`preview -> same real layer active` handoffs without redefining preview
ownership semantics.

### Key-behavior feedback is now truthful per key

- The runtime no longer picks one active feedback source by matrix order.
- It builds a compact per-key semantic map across all active authored sources.
- `KEY_FEEDBACK_MODE_KEY` paints exact per-key truth.
- `KEY_FEEDBACK_MODE_KEY_HALF` and `KEY_FEEDBACK_MODE_BOTH_HALVES` broaden from
  that truthful map only at render time.
- `MULTI_TAP_PENDING` is the highest semantic priority and stays visible above
  held / flashing semantics when broadened modes need to collapse multiple
  states.

This fixes the old inconsistency where two simultaneous held keys on different
sides could only show one side by matrix-order accident.

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
- Key-feedback semantics are produced in
  `users/noah/lib/key/runtime/feedback.c` and rendered from the synced
  semantic map in `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`.
- Combo feedback rendering and underlay / overlay routing live in
  `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`.
- PD trigger-side rendering derives from the synced side mask in
  `users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`.
- Split runtime transport for the three RGB-facing surfaces lives in
  `users/noah/lib/state/runtime/split_runtime_sync.c`.
- Validation for authored combo member ambiguity lives in
  `users/noah/lib/key/interaction/keymap_validation.c`.

## Tradeoffs

- Duplicate combo outputs across distinct combos remain allowed, but overlapping
  active combos that share the same output still have an ambiguous release path
  because upstream `COMBO_EVENT` records do not expose combo identity.
- When multiple active combos with the same output are in play, locality is
  intentionally broadened to the union footprint instead of pretending a single
  side or key.
- Combo feedback is now a persistent layer, which is truer to runtime state but
  does increase visual complexity compared to the older “only show authored
  key-behavior feedback” model.
- The truthful key-semantic model is exact internally, but broadened paint
  modes (`KEY_HALF`, `BOTH_HALVES`) are still intentionally lossy presentation
  choices.
- Split runtime sync now uses more than one packet. That is a better fit for
  the truthful model, but it is a wider wire contract than the previous
  all-in-one snapshot packet.
- Packet-diff syncing is now the only correctness contract for RGB split sync.
  That keeps the runtime simpler than maintaining separate dirty-flag or
  request bookkeeping, but it means sync cadence depends on matrix-scan-driven
  packet rebuilds instead of immediate call-site forcing.
- The preview handoff bridge is intentionally a display workaround, not a
  semantic runtime change. It is cleaner than extending preview through the
  whole hold, but it still exists because preview and real layer state reach
  the slave through different transports.
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
- Every active combo may also paint a steady combo layer independent of whether
  its output drives authored key-behavior feedback, preview, or PD state.
- Preview-owning and PD-owning combos must route to the combo underlay; all
  other combos must stay in the combo overlay.
- Preview display may briefly bridge `preview -> same active layer` handoffs,
  but preview ownership itself must still drop as soon as the handled-key
  runtime says the preview is over.
- `KEY_FEEDBACK_MODE_KEY_HALF` may broaden to both halves for cross-half combos.
- `KEY_FEEDBACK_MODE_KEY` paints every combo key in the footprint.
- `MULTI_TAP_PENDING` is the top key-feedback semantic priority when a
  broadened mode needs to collapse multiple simultaneous states.
- `key_feedback_flash_meta` must stay `0` whenever the synced semantic map has
  no flashing semantics.
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

## Closure Verification

Closure was checked with `prompts/closure-verification-review.md` on
2026-04-23.

### Findings

- No `must-fix` findings remain.
- No `should-fix` findings remain.
- Optional deferred audit: overlapping active combos that share the same output
  keycode still deserve a release-path audit if that authored pattern becomes
  important. This is consciously deferred because the current authored profile
  and tests do not require that pattern, and locality already broadens rather
  than falling back to fake `(0,0)`.

### Prior Finding Status

- Combo outputs using fake QMK position `(0,0)`: resolved. Code references:
  `users/noah/lib/compat/qmk_combo_origin.c` and
  `users/noah/lib/key/runtime/origin_registry.c`. Enforcement references:
  combo-origin tests, real-profile thumb-layer-lock integration, RGB layer
  render tests, full host suite, and firmware compile.
- Key-runtime stack-backed replay growth: resolved. Code references:
  `users/noah/lib/key/runtime/keypos_codec.h`,
  `users/noah/lib/key/runtime/effects/effect.h`, and
  `users/noah/lib/key/runtime/transition.c`. Enforcement references: size
  guards, key-runtime scenario/release/modifier/layer-lock tests, full host
  suite, and firmware compile.
- Slot-indexed core storage duplication: resolved. Code references:
  `users/noah/lib/key/runtime/core/runtime.c` and
  `users/noah/lib/key/runtime/core/runtime.h`. Enforcement references:
  key-runtime release matrix, runtime debug tests, full host suite, and
  firmware compile.
- Non-slot persistent storage compaction: resolved. Code references:
  `users/noah/lib/key/runtime/core/runtime.c` and
  `users/noah/lib/key/runtime/core/runtime.h`. Enforcement references:
  pending-release/runtime debug coverage, key-runtime runners, full host suite,
  and firmware compile.
- Truthful RGB feedback layers: resolved. Code references:
  `users/noah/lib/key/runtime/feedback.c`,
  `users/noah/lib/rgb/stages/rgb_combo_feedback_stage.c`,
  `users/noah/lib/rgb/stages/rgb_key_feedback_stage.c`, and
  `users/noah/lib/state/runtime/split_runtime_sync.c`. Enforcement references:
  RGB layer render tests, split runtime sync tests, real-profile validation,
  full host suite, and firmware compile.
- Split RPC churn reduction: resolved. Code reference:
  `users/noah/lib/state/runtime/split_runtime_sync.c`. Enforcement references:
  split runtime sync tests, full host suite, and firmware compile.
- Preview handoff smoothing: resolved. Code reference:
  `users/noah/lib/key/runtime/feedback.c`. Enforcement references: RGB layer
  render tests, split runtime sync tests, full host suite, and firmware compile.

Passed verification for this closure pass:

- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

Closure Verdict: close thread.

Remaining Open Findings: none. The duplicate-output combo release-path audit is
deferred as optional future work, not an open blocker for this thread.

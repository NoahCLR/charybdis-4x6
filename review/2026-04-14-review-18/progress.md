# Progress

## 2026-04-20 Runtime V2 Authoritative Release Transport Pass

- Removed the old `key_runtime_slot_result_t` transport from the authoritative handled-release path: active release and pending-multi-tap release now mutate slot state and append their reducer-owned effect plan directly into the transition plan.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.h` and `.c` now expose `key_runtime_slot_take_v2_active_release_plan(...)`, which:
  - resolves reducer-owned active release,
  - traces that release decision,
  - applies reducer-owned settlement to the live slot,
  - seeds pending multi-tap state when needed, and
  - returns the reducer-owned effect plan without routing through `key_runtime_slot_result_t`.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.h` and `.c` now expose `key_runtime_slot_take_v2_pending_multi_tap_release_plan(...)`, which does the same for pending multi-tap release, including preserve-chain settlement and delayed-action / held-lifecycle planning.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now uses those direct helpers when runtime-v2 has authoritative normalized input:
  - active and pending-multi-tap handled releases bypass `key_runtime_slot_step(...)` and `key_runtime_slot_reduce_handled_release(...)`,
  - reducer-owned effect plans append directly into `key_runtime_transition_plan_t`, and
  - unmatched handled releases now append their layer-release / owned-state cleanup effects directly into the transition plan instead of round-tripping through slot-result transport.
- This keeps the cut narrow:
  - the legacy slot reducers still exist as compatibility fallback for non-authoritative or narrowed surfaces,
  - authoritative handled-release transport no longer depends on `key_runtime_slot_result_t`,
  - but press, scan, interrupt, and flush transport still run through the old slot-result/plan assembly seams.
- `tests/host/runtime_debug_test.c` now directly proves the new bypass helpers at the exact seam they serve:
  - shadow key-up observed by runtime-v2 while the legacy slot is still live,
  - active release can seed pending multi-tap directly through the helper, and
  - pending multi-tap release can reset the live slot and emit the delayed action directly through the helper.

## 2026-04-19 Runtime V2 Effect Projection Pass

- Moved the world-application seam for handled-key runtime effects onto runtime-v2-owned projector helpers: transition and release orchestration no longer own the direct QMK-side effect switch for authored runtime effects.
- Added `users/noah/lib/runtime_v2/runtime_v2_projection.h` as the reducer-owned projector surface for:
  - `runtime_v2_project_effect(...)`, which applies one `key_runtime_effect_t`, and
  - `runtime_v2_project_pending_release_dispatch(...)`, which applies one drained deferred-release dispatch.
- `users/noah/lib/runtime_v2/runtime_v2.c` now owns the effect projector implementation for:
  - action taps,
  - held-action register/unregister,
  - release-owned-state cleanup,
  - repeat start,
  - layer press/release,
  - feedback pulse,
  - pd-mode lock tap, and
  - delayed-action repeats.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now reduces `key_runtime_transition_execute_plan(...)` to:
  - trace the effect, then
  - hand it to `runtime_v2_project_effect(...)`.
- `users/noah/lib/key/runtime/key_runtime_release.c` now routes both:
  - reducer-owned drained pending releases, and
  - legacy deferred-release queue fallback dispatches,
  through `runtime_v2_project_pending_release_dispatch(...)` instead of calling delayed-action execution directly.
- This keeps the migration cut narrow:
  - the legacy transition and release modules still transport effect queues and deferred-release items,
  - runtime-v2 now owns the side-effectful projection step for those runtime effects, and
  - weak fallback projector stubs remain in the legacy executor files for focused host runners that intentionally omit the full reducer object surface.
- `tests/host/runtime_debug_test.c` now directly proves the projector seam by checking:
  - `runtime_v2_project_effect(...)` dispatches an authored action tap and applies a pd-mode lock tap, and
  - `runtime_v2_project_pending_release_dispatch(...)` applies a deferred delayed action with the saved modifier payload intact.

## 2026-04-19 Runtime V2 Release Effect Planning Pass

- Moved handled-release effect selection onto the reducer-owned path: once runtime-v2 decides active or pending multi-tap release outcome, the default release callers no longer re-map that decision into effects locally.
- `users/noah/lib/runtime_v2/runtime_v2_release_internal.h` now defines:
  - `runtime_v2_release_effect_plan_t`,
  - `runtime_v2_pending_multi_tap_seed_t`,
  - reducer-owned slot-settlement instructions, and
  - planning APIs for active and pending multi-tap release.
- `users/noah/lib/runtime_v2/runtime_v2.c` now exposes:
  - `runtime_v2_plan_active_release_effects(...)`, which maps reducer-owned active-release decisions into:
    - concrete effect queue items,
    - pending multi-tap seed payload, and
    - slot-settlement instructions, and
  - `runtime_v2_plan_pending_multi_tap_release_effects(...)`, which does the same for pending multi-tap release, including delayed-action mod transport and preserve-chain settlement.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` now consumes that reducer-owned effect plan when runtime-v2 has authoritative normalized input, uses the returned settlement to reset legacy slot state, seeds legacy pending multi-tap storage from the reducer-owned payload, and appends the reducer-owned effect queue to the existing slot result.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now consumes the reducer-owned pending-multi-tap effect plan when runtime-v2 is authoritative, uses the returned settlement to either preserve the chain or reset the slot, and appends the reducer-owned held-lifecycle or delayed-action effects to the existing slot result.
- This keeps the migration cut narrow:
  - reducer-owned state now determines handled-release effect selection as well as handled-release outcome,
  - legacy slot/result storage still transports the emitted effects and pending multi-tap seed, and
  - production effect execution still lives in the legacy transition executor for this pass.
- `tests/host/runtime_debug_test.c` now proves four direct reducer contracts:
  - active release can buffer a pending multi-tap seed without emitting immediate effects,
  - active interrupted layer release can emit layer-release plus tap-action effects from the reducer-owned plan,
  - pending multi-tap quick release can preserve the chain without emitting immediate effects, and
  - pending multi-tap delayed-action release keeps the saved delayed-mod payload on the emitted reducer-owned effect.

## 2026-04-19 Runtime V2 Slot Retirement And Explicit Multi-Tap Flush Pass

- Moved the next mixed-ownership cleanup seam into the reducer-aware path: explicit pending-multi-tap flush/reset and forced active-slot retirement no longer leave `runtime_v2` state live after the legacy slot world has been cleared.
- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now expose:
  - `runtime_v2_take_pending_multi_tap_flush(...)` to resolve and clear reducer-owned tap-series state during explicit flush,
  - `runtime_v2_reset_pending_multi_tap(...)` to clear reducer-owned tap-series state when legacy slot storage is reset directly, and
  - `runtime_v2_retire_press_token(...)` to cancel an active reducer-owned press token when the legacy slot is forcibly flushed/reset before physical key-up.
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now bridges the legacy slot helpers into that reducer state:
  - `key_runtime_slot_take_pending_multi_tap_flush(...)` uses the reducer-owned tap-series payload when available before clearing legacy pending-multi-tap storage,
  - `key_runtime_slot_reset_pending_multi_tap(...)` clears reducer-owned tap-series state alongside legacy slot state, and
  - `key_runtime_slot_reset(...)` retires the active reducer-owned press token plus any reducer-owned tap series before zeroing the legacy slot.
- This keeps the migration cut narrow:
  - saved delayed-action mods still come from the slot-owned multi-tap payload,
  - transition/effect planning still runs through the legacy plan builder,
  - but reducer-owned lifetime now ends when the production slot lifetime ends instead of waiting for some later physical event.
- `tests/host/runtime_debug_test.c` now proves both production-shaped seams directly:
  - `key_runtime_transition_flush_foreign_multi_tap(...)` clears the reducer-owned tap series when it flushes the legacy pending chain, and
  - `key_runtime_transition_flush_active_keys_except(...)` retires the reducer-owned active press token when it flushes the legacy active slot.

## 2026-04-19 Runtime V2 Pending Multi-Tap Lifecycle Pass

- Moved the next multi-tap lifecycle seam into the reducer-owned shadow path: scan-time pending-hold promotion and chain-expiry flush now resolve from runtime-v2 token/tap-series state instead of only from slot-local timer state.
- `users/noah/lib/runtime_v2/runtime_v2.h` now keeps richer tap-series records:
  - first-tap action,
  - current tap action/repeat payload,
  - authored hold/long-hold contract for the current tap count,
  - per-series tap-hold term, and
  - the per-series multi-tap term.
- `users/noah/lib/runtime_v2/runtime_v2.c` now:
  - keeps authored multi-tap chains alive past generic time refresh until the dedicated pending-multi-tap scan resolver settles them,
  - exposes `runtime_v2_resolve_pending_multi_tap_scan(...)`,
  - resolves scan-time hold-threshold promotion, long-hold promotion, and expired-chain flush from reducer-owned token/tap-series state, and
  - clears reducer-owned tap-series state when pending multi-tap release or scan settlement fully consumes the chain.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now uses that v2 scan resolver when the normalized runtime-v2 stream is authoritative, while leaving the existing slot/effect code in place to:
  - build the actual hold/long-hold effect,
  - carry the saved delayed-action mod snapshot, and
  - reset legacy pending-multi-tap storage after the decision.
- `users/noah/lib/key/runtime/key_runtime_trace.h` and `.c` now distinguish reducer-owned pending-multi-tap scan decisions for:
  - hold-threshold promotion,
  - long-hold promotion, and
  - expired-chain flush.
- `tests/host/runtime_debug_test.c` now proves the new reducer path directly for:
  - scan-time threshold promotion of a second-tap hold,
  - late-scan long-hold promotion of the same chain,
  - expired-chain flush after the authored multi-tap term, and
  - the narrower contract that only authored hold-capable chains use `pending_hold`, while plain tap-series state can still expire independently from an active later press token.

## 2026-04-19 Runtime V2 Pending Multi-Tap Release Settlement Pass

- Moved the second handled-release caller onto the reducer-owned release planner: pending multi-tap release no longer decides tap-vs-hold-vs-long-hold by rebuilding semantics only from the live slot world.
- `users/noah/lib/runtime_v2/runtime_v2_release_internal.h` and `users/noah/lib/runtime_v2/runtime_v2.c` now expose `runtime_v2_resolve_pending_multi_tap_release(...)`, which resolves pending multi-tap release outcome from:
  - the released press token's immutable interaction snapshot,
  - the reducer-owned release timestamp / elapsed interval,
  - the resolved tap payload returned by `multi_tap_resolve_hold(...)`, and
  - whether the pending chain is still active and should be preserved.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_pending_multi_tap.c` now uses that reducer-owned pending-release planner when the normalized runtime-v2 stream is authoritative, while keeping the delayed-action mod snapshot and chain payload on the slot-owned multi-tap storage for this pass.
- This keeps the migration cut narrow:
  - reducer-owned state now decides pending multi-tap release outcome,
  - slot-owned multi-tap storage still transports saved mods / repeat payload into the emitted effects,
  - scan-time pending multi-tap hold promotion remains on the existing reducer for now.
- `tests/host/runtime_debug_test.c` now proves the new reducer API directly for:
  - preserve-chain quick release, and
  - hold-action dispatch after the tap-hold term.

## 2026-04-19 Runtime V2 Active Release Settlement Pass

- Moved the active-slot release decision onto reducer-owned press-token state while keeping pending multi-tap release settlement on the legacy shared resolver for this pass.
- `users/noah/lib/runtime_v2/runtime_v2.h` now stores the immutable press-resolved `key_runtime_slot_interaction_t` on each press token, plus a reducer-owned `slot_phase` that tracks release semantics separately from the generic token lifecycle phase.
- `users/noah/lib/runtime_v2/runtime_v2.c` now:
  - updates reducer-owned release phase only from scan/effect progression instead of reinterpreting phase on key-up,
  - keeps held-action/repeat leases alive across physical key-up until `RELEASE_OWNED_STATE_BY_KEY` unwinds them,
  - exposes `runtime_v2_resolve_active_release(...)` through `runtime_v2_release_internal.h`, and
  - resolves active release outcome from the immutable token interaction snapshot plus reducer-owned held/repeat leases.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_release_active.c` now uses that v2 release-resolution path when the normalized runtime-v2 stream is authoritative, and falls back to the legacy slot-owned resolver otherwise.
- This production cut keeps the old effect mapping and pending multi-tap reducer in place, but active release settlement for the default hook path no longer depends on mutable live slot interaction being re-derived at release time.
- `tests/host/runtime_debug_test.c` now proves:
  - release-time phase does not advance merely because key-up arrives after `tap_hold_term`, and
  - scan-time threshold promotion changes active-release outcome in the reducer the same way the legacy slot scan path does.

## 2026-04-19 Runtime V2 Owned-State Lease Observation Pass

- Moved held-action and held-repeat ownership into reducer-observed lease state so the remaining release/unwind path is no longer guessing about owned runtime state.
- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now:
  - track `LEASE_KIND_HELD_ACTION` and `LEASE_KIND_REPEAT` by physical key position,
  - expose observer hooks for held-action register/unregister and repeat start, and
  - expose `runtime_v2_release_owned_state_by_key(...)` so reducer-owned held/repeat leases can be cleared by the production release/unwind seam.
- `runtime_v2_press_token_owned_state_active(...)` now consults reducer-observed held/repeat leases before falling back to threshold-based inference, tightening blocker semantics around real owned state.
- `users/noah/lib/key/runtime/key_runtime_transition.c` now feeds the production effect executor through those reducer observers:
  - `KEY_RUNTIME_EFFECT_HELD_ACTION_REGISTER`
  - `KEY_RUNTIME_EFFECT_HELD_ACTION_UNREGISTER`
  - `KEY_RUNTIME_EFFECT_REPEAT_START`
  - `KEY_RUNTIME_EFFECT_RELEASE_OWNED_STATE_BY_KEY`
- That means the production execute-plan path now updates reducer-owned held/repeat lease state and uses the reducer-owned release-owned-state cleanup helper before the legacy held-action/repeat owner runs.
- `tests/host/runtime_debug_test.c` now proves both:
  - direct held/repeat lease observation and reducer-owned clear, and
  - the production `key_runtime_transition_execute_plan(...)` path updating and clearing those leases.
- `tests/host/runtime_v2_observer_stub.c` now covers the new held/repeat observer hooks so focused runners that omit the full reducer still link cleanly.

## 2026-04-19 Runtime V2 Pending Release Drain Ownership Pass

- Moved the next release-cleanup seam out of pure observer mode and into reducer-owned state management.
- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now give each pending release a stable sequence number and expose:
  - total pending-release count, and
  - `runtime_v2_take_pending_release_dispatches(...)`, which drains pending releases in enqueue order and clears the owning token's `pending_release_emission` / release-pending phase inside the reducer.
- `users/noah/lib/key/runtime/key_runtime_release.c` now uses that reducer-owned take path when:
  - runtime-v2 has observed the real normalized input stream, and
  - the mirrored legacy queue count matches the reducer-owned pending-release count.
- In that authoritative path, the legacy deferred-release queue is now transport-only mirror state; the reducer owns pending-release ordering and cleanup, while `key_runtime_release.c` only executes the drained actions.
- The legacy drain loop remains as compatibility fallback for low-level surfaces that still do not feed runtime-v2 or where the mirror counts do not agree yet.
- `tests/host/runtime_debug_test.c` now proves the reducer-owned drain keeps enqueue order and clears token pending-release state after the take completes.
- `tests/host/runtime_v2_observer_stub.c` now covers the new blocker/pending-release authority APIs so narrow subsystem runners keep their existing link surface.

## 2026-04-19 Runtime V2 Production Blocker Query Bridge Pass

- Moved the blocker migration one step out of pure shadow mode and into the real production path.
- `users/noah/lib/key/runtime/key_runtime_process.c` now feeds normalized physical key down/up events into `runtime_v2` from the real `noah_process_record_user(...)` path for non-synthetic records.
- `users/noah/lib/key/runtime/key_runtime_scan.c` now feeds normalized scan events into `runtime_v2` from the real key-runtime scan path, so shadow blocker timing advances under the same scan cadence as production hold promotion and deferred-release drain.
- `users/noah/lib/runtime_v2/runtime_v2.c` and `.h` now expose:
  - production-facing key/scan observer entry points,
  - an `input_stream_observed` authority latch, and
  - native blocker query helpers for:
    - any blocker
    - foreign blocker except a key position
- `users/noah/lib/key/runtime/key_runtime_transition.c` now uses the v2 blocker queries when the normalized runtime-v2 input stream is authoritative, and falls back to the legacy index blocker queries in low-level slot/unit surfaces that still mutate legacy state directly without feeding v2.
- `tests/host/key_runtime_integration_harness.c` now avoids double-feeding key/scan events into runtime-v2 when the linked real userspace already does that itself, while still keeping manual v2 injection for standalone timer/pointer/remote replay events.
- `tests/host/runtime_debug_test.c` now proves the production process hook feeds blocker queries and that the scan observer clears the quick-tap blocker once hold ownership settles.

## 2026-04-19 Runtime V2 Native Blocker Derivation Pass

- Replaced the shadow reducer's observer-fed deferred-release blocker semantics with native blocker derivation from authored token state.
- `users/noah/lib/runtime_v2/runtime_v2.c` now derives blocker truth from:
  - immutable press-token identity,
  - handled-key materialization resolved at press time against shadow layer state,
  - token-owned interruption latches,
  - token-owned timing (`pressed_at`, `hold_term_ms`), and
  - tap-series pending-hold state.
- `runtime_v2_press_token_begin(...)` now resolves handled-key contract once on press, stores blocker-relevant contract fields on the press token, and latches foreign-key interruption on already-active tokens inside the reducer instead of waiting for legacy blocker observation.
- `runtime_v2_deferred_release_blocker_count_for_keypos(...)` and projection capture now compute blocker counts directly from live press tokens, not from mirrored `deferred_release_blocker_t` records.
- `runtime_v2_observe_deferred_release_blocker_profile(...)` is now compatibility-only. The legacy blocker owner can still call it while the mixed architecture exists, but the reducer no longer depends on that observer input for blocker semantics.
- `tests/host/runtime_debug_test.c` now proves real authored blocker shapes instead of synthetic observer-fed profiles:
  - an interrupted momentary-layer tap blocks before `tap_hold_term` and expires once the hold owns state,
  - a plain handled tap keeps blocking after `tap_hold_term`, and
  - a foreign press clears the first token's quick-tap blocker through reducer-owned interruption state.

## 2026-04-19 Runtime V2 Blocker Observation Pass

- Extended the shadow reducer into deferred-release blocker ownership so blocker state is now visible in `runtime_v2` and the parity snapshot surface instead of staying entirely trapped in legacy slot/index storage.
- `users/noah/lib/runtime_v2/runtime_v2.h` and `.c` now keep one blocker record per physical key position, owned by the current press token id for that key position.
- The v2 blocker record stores the same timed profile shape as the legacy blocker owner:
  - blocks before `tap_hold_term`
  - blocks after `tap_hold_term`
- `runtime_v2` now evaluates the time-boundary locally from the owning press token's `pressed_at` and `hold_term_ms`, so the shadow path no longer depends on legacy scan-time blocker refresh to know whether a timed blocker is currently active.
- `users/noah/lib/key/runtime/key_runtime_index.c` now feeds that profile into `runtime_v2_observe_deferred_release_blocker_profile(...)` from the real blocker owner seam, and the v2 observer binds the blocker record to the currently active press token on the same physical key.
- `users/noah/lib/state/runtime/runtime_debug.h` and `users/noah/lib/key/runtime/key_runtime_debug.c` now expose legacy blocker counts for projection capture, and `runtime_v2_projection_snapshot_capture()` now records both:
  - legacy blocker counts
  - v2 shadow blocker counts
- `tests/host/runtime_debug_test.c` now proves both timed blocker directions in the shadow reducer:
  - a blocker that only exists before `tap_hold_term` expires in v2 after the threshold, and
  - a blocker that only exists after `tap_hold_term` activates in v2 after the threshold.
- Focused host runners that compile `key_runtime_index.c` without the full reducer now link `tests/host/runtime_v2_observer_stub.c`, so the new observer hook does not broaden those runner surfaces accidentally.

## 2026-04-19 Deferred Release Timed Blocker Ownership Pass

- Tightened deferred-release blocker ownership so blocker queries no longer rebuild blocker membership by rescanning every active slot and rerunning the blocker predicate over the whole live set.
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now records an explicit blocker profile per slot:
  - blocks before `tap_hold_term`
  - blocks after `tap_hold_term`
- That profile is derived from the slot's immutable press-resolved interaction plus slot-owned lifecycle latches, so timed blocker refresh now consults slot-owned facts instead of re-deriving semantics from the active-slot world.
- `users/noah/lib/key/runtime/key_runtime_index.c` now keeps both:
  - the effective deferred-release blocker subset, and
  - a smaller timed subset whose membership only changes when `tap_hold_term` is crossed.
- `users/noah/lib/key/runtime/slot/key_runtime_slot_policy.c` now re-syncs the slot index immediately when foreign-key interruption latches change, which was required once blocker membership stopped being a live recomputation.
- `tests/host/key_runtime_index_test.c` now proves both timed directions of that ownership model:
  - a quick-tap blocker can expire after `tap_hold_term` without an active-slot resync, and
  - an interrupted layer-tap blocker with nonquick tap behavior can become active after `tap_hold_term` without rescanning the active set.

## 2026-04-19 Deferred Release Blocker Index Pass

- Moved deferred-release blocker classification out of `key_runtime_transition.c` and into the key-runtime index owner layer.
- `users/noah/lib/key/runtime/slot/key_runtime_slot.c` now owns `key_runtime_slot_blocks_deferred_release_dispatch(...)`, which centralizes the blocker rule that was previously embedded in the transition module.
- `users/noah/lib/key/runtime/key_runtime_index.c` now maintains a dedicated deferred-release blocker subset, and `key_runtime_transition_has_foreign_tap_release_slot_except(...)` / `key_runtime_transition_has_any_tap_release_slot()` now query that index instead of rescanning and re-deriving blocker semantics themselves.
- `tests/host/key_runtime_index_test.c` now proves blocker ownership updates immediately on slot mutations and excludes interrupted layer taps from the blocker subset, while the release matrix and real-profile overlap suites prove the higher-level behavior stayed intact.

## 2026-04-19 Runtime V2 Pending Release Ownership Pass

- Moved the next release-path seam away from anonymous shared state by adding explicit owner identity to deferred release dispatches and shadowing them as owned v2 pending-release records.
- `users/noah/lib/key/runtime/key_runtime_shared_state.h` now stores `key_pos` alongside each deferred release action/mod snapshot, so the legacy queue no longer drops the physical-key owner identity.
- `users/noah/lib/key/runtime/key_runtime_release.c` now records that owner key position when deferring release actions and notifies runtime-v2 on both enqueue and drain, so the shadow reducer sees real production deferral/drain events instead of synthetic test-only calls.
- `users/noah/lib/runtime_v2/runtime_v2.c` and `.h` now track `pending_release_t` records separately from active press tokens, including a dedicated `v2_pending_release_count` snapshot field and a `runtime_v2_pending_release_count_for_keypos(...)` debug query.
- This keeps a deferred release bound to the releasing physical key even if that key position is pressed again before the deferred action drains, which is the exact ownership property the old anonymous queue could not represent.
- `users/noah/lib/state/runtime/runtime_debug.h` and `users/noah/lib/key/runtime/key_runtime_debug.c` now expose deferred-release owner key position and action for host assertions.
- `tests/host/key_runtime_release_matrix_test.c` now proves the legacy deferred release queue retains the releasing key position, and `tests/host/runtime_debug_test.c` now proves the v2 pending-release record survives same-key slot reuse until it is explicitly drained.

## 2026-04-19 Runtime V2 Live Lock Mutation Observation Pass

- Closed the next shadow-runtime gap for persistent lock mutations by wiring the production lock-state setters into the v2 observer APIs instead of relying on direct test-only lock calls.
- `users/noah/lib/state/ownership/layer_ownership.c` now calls `runtime_v2_layer_lock_set(...)` from `layer_ownership_set_lock_state(...)` after the authoritative layer lock mutation succeeds.
- `users/noah/lib/pointing/runtime/pd_mode_state.c` now calls `runtime_v2_pd_mode_lock_set(...)` from `pd_mode_set_lock_state(...)` after the authoritative local pd-mode lock mutation succeeds.
- This means authored lock actions and native key-runtime lock taps now reach the shadow reducer through the same real mutation seams that own production lock state, instead of only through raw key presses or explicit test helpers.
- `tests/host/runtime_debug_test.c` now proves that v2 shadow lock state updates correctly when driven by the real layer and pd-mode setters, not only by direct runtime-v2 API calls.
- Added `tests/host/runtime_v2_observer_stub.c` and wired the focused host runners that intentionally do not link the full v2 reducer to that shared stub, so minimal subsystem tests keep their narrow compile surface while the parity-aware suites continue using the real implementation.

## 2026-04-19 Runtime V2 Pd-Mode And Pointer Ownership Shadow Pass

- Extended the shadow reducer into the first wedge-prone overlap domain: pd-mode ownership and pointer anchoring.
- `users/noah/lib/runtime_v2/runtime_v2.c` now:
  - creates token-owned active pd-mode leases for raw pd-mode keys,
  - creates token-owned pointer-anchor leases for momentary anchored pd modes,
  - owns persistent pd-mode lock intents,
  - owns persistent pointer-toggle intents for lock-owned auto-mouse-toggle modes, and
  - recomputes shadow pd-mode/pointer projection from those owned records instead of from ad hoc cleanup paths.
- Preserved the production exclusivity shape in shadow form: activating a new raw pd-mode key now clears foreign pd-mode leases and foreign pd-mode lock/toggle intents before the new mode becomes authoritative.
- Added direct white-box coverage in `tests/host/runtime_debug_test.c` for:
  - anchored momentary pd-mode ownership,
  - typing-preference pd-mode ownership without pointer anchoring,
  - lock-owned pointer-toggle intent for `DRAGSCROLL`, and
  - foreign pd-mode supersession clearing old owned leases instead of leaving mixed anchor state behind.
- This still does not cut production hooks over to v2, but the replacement core now owns the same general state family as the original `NAV -> DRAGSCROLL` wedge path.

## 2026-04-19 Runtime V2 Layer And Modifier Lease Shadow Pass

- Extended the shadow reducer from identity-only state into real lease-backed ownership for the first shared domains: layers and modifiers.
- `users/noah/lib/runtime_v2/runtime_v2.c` now:
  - creates token-owned layer leases for raw `MO(layer)` presses,
  - promotes raw `LT(layer, key)` into a layer lease only after hold timing settles,
  - creates physical modifier leases for raw modifier key presses, and
  - promotes raw `MT(mod, key)` into a managed modifier lease on hold.
- Added a reducer-owned shadow projection for layer/mod state plus a simple persistent layer-lock intent, so cleanup is now "drop leases by token id and recompute projection" instead of a branchy unwind path.
- Tightened token replacement semantics in the shadow reducer so replacing a live token on the same physical key position cancels the old token and retires its owned leases immediately.
- Extended `tests/host/runtime_debug_test.c` with direct white-box coverage for:
  - momentary layer lease recompute plus persistent layer-lock composition,
  - layer-tap hold promotion creating and retiring a layer lease,
  - physical versus managed modifier ownership staying separate, and
  - live-token replacement clearing owned leases instead of leaving stale layer state behind.
- This pass still keeps production behavior on the legacy runtime, but it advances the replacement core from "tracks timing facts" to "owns and recomputes real shared state" for the first nontrivial domains.

## 2026-04-19 Runtime V2 Identity Shadow Pass

- Advanced the single-authority runtime redesign beyond pure scaffolding and into a real shadow reducer for the first migration domain: press/release identity and tap-series lifetime.
- Kept production behavior on the legacy runtime, but taught the host integration harness to feed normalized events into runtime-v2 through an explicit opt-in adapter instead of assuming every harness consumer must link the new reducer immediately.
- Added direct reducer invariants in `tests/host/runtime_debug_test.c` for:
  - release-by-position surviving release-keycode mismatch,
  - timer/scan hold promotion preserving immutable press identity, and
  - tap-series state remaining separate from active press-token storage.
- Tightened the new reducer in `users/noah/lib/runtime_v2/runtime_v2.c` so a same-key pending-hold tap series cannot expire just because tap-term time elapsed while its owning second press is still physically active. That was a real shadow-runtime bug and exactly the kind of cross-state corruption the redesign is meant to eliminate.
- Extended the runtime-debug host support manifest so the new reducer, v2 trace helpers, and pointer-layer policy debug surface are linked together coherently in white-box test builds.
- Wired the real-profile shadow replay suite onto the reducer-backed harness path with `tests/host/key_runtime_integration_runtime_v2_adapter.c`, so the existing nav/dragscroll and GUI-alt/nav overlap parity scenarios now replay against live press-token/tap-series state instead of trace encoding alone.
- This pass still does not cut over production hooks. It does move the active review from "trace substrate only" to "shadow reducer with mechanical invariants," which is the first real step toward deleting release-time reinterpretation from the architecture.

## 2026-04-19 Single-Authority Runtime V2 Foundation

- Landed the first implementation pass of the single-authority runtime redesign without cutting over production behavior yet.
- Added the preservation contract in `runtime-v2-preservation-matrix.md`, tying the redesign acceptance bar to `docs/INTERACTION_MODEL.md`, `docs/KEYMAP.md`, and the real-profile integration scenarios instead of legacy slot-model white-box behavior.
- Added the new internal `users/noah/lib/runtime_v2/` subtree with the typed v2 foundation:
  - `runtime_event_t`
  - `press_token_t`
  - `tap_series_t`
  - `lease_t`
  - `persistent_intent_t`
  - `projection_snapshot_t`
- Extended the shared runtime trace ring with a dedicated `NOAH_TRACE_RUNTIME_V2` kind and normalized input/output event ids, plus a debug-only console dump helper for trace snapshots.
- Wired the host integration harness to emit normalized v2 input events from the real hook path and to emit projection checkpoints after semantic events, instead of introducing a second synthetic test-only adapter.
- Added runtime-v2 input encode/decode coverage in `tests/host/runtime_trace_test.c`.
- Added a pointer-layer policy debug snapshot surface and coverage so projection snapshots can compare effective anchor inputs directly.
- Added real-profile shadow replay parity coverage for the two highest-priority repro families:
  - raw `LAYER_NAV` -> `DRAGSCROLL`
  - `KC_LEFT_GUI` second-tap hold to `KC_LEFT_ALT` with repeated nav-arrow taps
- The current production hooks still run the legacy runtime. This pass only lands the preservation matrix, normalized trace substrate, projection snapshot contract, and replay harness required before domain-by-domain cutover.

## 2026-04-19 Pd-Mode Auto-Mouse Ownership Remediation

- Investigated the reopened wedge against the current real repro families and the upstream QMK auto-mouse/release ordering in `../bastardkb-qmk`.
- Found a concrete userspace ownership bug in `users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`: anchored pd modes could double-own auto-mouse state by combining a synthetic lifecycle anchor with either the real held-key auto-mouse path or a lock-owned `auto_mouse_toggle()` path.
- Narrowed the synthetic lifecycle anchor to locked modes that keep auto-mouse alive but do not own the lock-time toggle path, and moved the synthetic-anchor latch into shared pd-mode runtime state so resets/tests do not depend on a translation-unit static.
- Tightened the real-profile host harness so releases use the press-time keycode identity across layer changes and so the harness now models the pre-userspace auto-mouse side effects that QMK applies before userspace record processing.
- Added focused host coverage that now proves:
  - active `DRAGSCROLL` does not add a second synthetic auto-mouse anchor,
  - locked non-toggle modes still keep auto-mouse alive,
  - lock-owned toggle modes do not double-anchor auto-mouse, and
  - the real-profile nav/dragscroll overlap path keeps `auto_mouse_key_tracker` at `1` instead of double-counting ownership.
- This narrows the most likely hardware wedge family substantially, but the regression stays open until on-device validation confirms the board no longer wedges under the original repros.

## 2026-04-19 Hardware Regression Reopened

- Captured the currently open hardware-visible wedge regression in `authored-key-overlap-wedge-regression.md`.
- This follow-up supersedes the earlier audit-time assumption that the overlap remediation was fully closed on hardware. The active review folder now treats the authored-key wedge as an open regression again until the runtime and host-harness mismatch is reconciled.
- The current leading hypothesis is a deterministic cleanup leak across authored key-runtime ownership and pre-userspace QMK side effects after layer, pointer-layer, pd-mode, or modifier transitions.
- The documented regression window is `fdc2d77` -> `29156345db90973cfc4422de1384c1a684aa94e2`, with the interruption/release-semantics widening in that later commit as the primary suspect.
- This documentation-only pass did not run new host tests or firmware builds.

## 2026-04-14 Initial Review Start

- Started a fresh active review thread in `review/2026-04-14-review-18/`.
- This folder was created instead of continuing an older review because the prior review history was intentionally removed and the user explicitly requested a fresh start.
- Review prompt used: `prompts/initial-architecture-review.md`.

## Completed Work

- Audited the current userspace/runtime structure across `users/noah/`, the keymap-owned authoring surface under `keyboards/bastardkb/charybdis/4x6/keymaps/noah/`, and the build/test enforcement scripts under `tests/host/`.
- Wrote the initial architecture review in `userspace-architecture-review.md`.
- Captured a verified baseline for the current tree before opening any new refactor thread work.
- Landed reliability remediation for two confirmed runtime bugs:
  - moved held-repeat ticking out of `matrix_scan_user()` and into the userspace housekeeping hook so repeat dispatch now runs after QMK event processing,
  - extended the shared emit seam with masked synthetic-QMK tap support and moved arrow-mode vertical taps onto that helper.
- Landed pd-mode buffered tap replay remediation for mode-owned real modifiers:
  - added a mode-owned buffered-tap masking hook on the private pd-mode hook surface and used it from `PINCH_MODE`,
  - kept delayed tap snapshot timing in `multi_tap_engine.c` but removed the need for pinch-specific runtime branching there,
  - added a generic `keyboard_mod_ownership_managed_only_mask(...)` helper so buffered taps only strip managed-only modifiers and preserve physically held GUI.
- Extended host coverage for the new hook surface, masked emit contract, and dropped-backlog repeat policy.
- Extended host coverage for pinch buffered tap replay, managed-only modifier masking, and the direct pd-mode policy query.
- Applied a follow-up architecture audit using `prompts/follow-up-architecture-audit.md`.
- Confirmed the remediation landed cleanly with no new `must-fix` or remediation-specific `should-fix` findings; the original maintainability findings remain open.
- Restored the real-profile host-test baseline by adding the missing host `keycodes.h` shim used by the direct keymap compilation runners.
- Applied another follow-up architecture audit using `prompts/follow-up-architecture-audit.md` to the buffered-tap implementation itself.
- That audit found no new `must-fix` issues, but it did identify:
  - one `should-fix`: the new buffered-tap policy query is broader than needed because it currently leaks through the public pd-mode header,
  - one `optional cleanup`: the pinch regression coverage proves the masking mechanism but does not yet exercise the real authored `KC_TRNS` path from the shipped profile.
- Landed the follow-up seam narrowing for buffered-tap policy:
  - removed `pd_mode_buffered_tap_masked_real_mods(...)` from the public pd-mode header,
  - introduced the dedicated internal header `users/noah/lib/pointing/runtime/pd_mode_buffered_tap_internal.h`,
  - narrowed production use of that header to pd runtime owner modules plus `multi_tap_engine.c`,
  - added compile-gate enforcement so host tests and production code cannot casually depend on that internal seam.
- Applied a closure-verification assessment using `prompts/closure-verification-review.md`.
- Confirmed that the buffered-tap seam work is genuinely resolved and mechanically enforced, but the thread is still not ready to close because the original `should-fix` findings on the registry DSLs and `key_runtime_internal.h` remain open.
- Confirmed the active review folder, `docs/ADDING_PD_MODE.md`, and the landed pd-mode/runtime code all describe the same ownership model.
- This assessment-only pass did not run new verification commands; it relies on the already-green buffered-tap seam-narrowing baseline recorded below.
- Landed the optional buffered-tap coverage cleanup:
  - `tests/host/pd_mode_key_runtime_integration_test.c` now authors `PINCH_MODE` as `TAP_SENDS(KC_TRNS)` like the shipped profile,
  - the integration harness now provides a local pointer-layer-to-base-layer transparent source path with a lower raw `LT(..., KC_J)` key,
  - the pinch replay assertions now verify the delayed action resolves to `KC_J` through transparent lookup instead of using a synthetic `KC_C` stand-in.
- Landed concurrent keyboard-event masking for mode-owned real modifiers:
  - added `noah_pre_process_record_user(...)` and `noah_post_process_record_user(...)` to the shared hook surface so userspace can track physical modifier presses before `process_record_user()` and restore any temporarily masked mode-owned real mods after the event,
  - added a private pd-mode keyboard-event masking query and used it from `key_runtime_process.c` so concurrent key processing hides only the managed-only subset of active mode-owned real modifiers,
  - kept `PINCH_MODE` as the owner of the managed-only `GUI` policy for both buffered tap replay and concurrent keyboard events,
  - removed the redundant file-static pinch registration latch and moved transient keyboard-event mask state into key-runtime shared state so resets and test fixtures do not depend on translation-unit statics,
  - updated `docs/ADDING_PD_MODE.md` so mode authors know that lifecycle hooks can also own concurrent keyboard-event masking policy.
- Aligned the wider host test matrix with the new hook/runtime ownership:
  - added a weak no-op fallback for the private pd-mode keyboard-event masking query in `key_runtime_process.c` so host runners that compile key runtime without the full pd registry still link cleanly,
  - added `keyboard_mod_state.c` to the shared host key-runtime source bundle,
  - updated the scenario, layer-lock, and real-profile host harnesses with only the ownership/state stubs they still own locally,
  - updated `key_runtime_preflight_test.c` so it now expects physical modifier tracking to happen in `pre_process_record_user()` instead of inside preflight.
- Landed final-outcome cleanup for concurrent keyboard-event masking:
  - added `noah_process_record_user_finalize(...)` to the shared runtime hook surface so cleanup now follows the final QMK event result instead of the helper-local `noah_process_record_user(...)` result,
  - updated the weak `process_record_user()` wrapper and the documented chaining pattern so keymap-local overrides that narrow the shared result to `false` finalize immediately instead of leaking the temporary keyboard-event mask,
  - moved the common key-runtime integration and scenario harnesses onto full QMK-style `pre/process/finalize-or-post` flow instead of calling `noah_process_record_user()` directly,
  - dropped the bespoke pre/post wrappers from the pd-mode integration test and kept only the direct low-level helper calls that intentionally inspect mid-event masking state.
- Closed the follow-up review gaps in that final-outcome cleanup:
  - broadened the shared hook contract comment so any override that chains `noah_process_record_user(...)` and returns a final `false` must finalize first, not just `&&`-style narrowing overrides,
  - extended `tests/host/hook_chaining_test.c` with an explicit pass-through-false chaining shape so a future `if (!noah_process_record_user(...)) return false;` override still has mechanical coverage for the finalize-before-return rule,
  - updated the weak default `noah_post_process_record_user(...)` in `tests/host/key_runtime_integration_harness.c` to finalize the `true` path just like production,
  - added `tests/host/key_runtime_integration_harness_test.c` plus `run_key_runtime_integration_harness_tests.sh` so the harness default `post -> finalize(true)` behavior is now enforced by the host suite instead of relying on inspection.
- Re-ran the follow-up architecture audit on that last cleanup using `prompts/follow-up-architecture-audit.md`.
- The rerun found no new remediation-specific findings. The contract wording, strong-override coverage, default harness behavior, host-suite enforcement, and active review notes now agree on the landed structure.
- Re-ran the closure review using `prompts/closure-verification-review.md`.
- Closure verdict is unchanged: this thread is still not ready to close because the original `should-fix` findings on the registry DSLs and `key_runtime_internal.h` remain open, even though the remediation-specific hook/harness findings are now resolved.
- Landed handled-key overlap remediation on the `15-04` restart branch:
  - `key_runtime_preflight.c` now settles foreign pending multi-tap chains before a press on another physical key proceeds, including authored handled presses,
  - same-key pending-chain reuse is still preserved by routing the selective flush through `key_runtime_transition_flush_foreign_multi_tap(...)`,
  - host coverage now locks that contract in at both the preflight seam and the higher-level handled-key scenario harness.
- Hardened the overlap contract further on `codex/fix-authored-key-freeze`:
  - `key_runtime_index.c` now keeps active slots out of the global pending-multi-tap index, so a foreign press can no longer flush a live active slot just because that slot is still carrying same-key multi-tap state internally,
  - `key_runtime_transition.c` now services active slots that still carry an in-slot multi-tap chain explicitly from the active scan loop, preserving hold/expiry behavior without reintroducing them to the foreign-flush index,
  - host coverage now locks the invariant in at the index seam, the preflight seam, the handled-key scenario harness, and the pd-mode key-runtime integration path.
- Closed another overlap-owned state leak on `codex/fix-authored-key-freeze`:
  - `key_runtime.c` now drains all pending fallback-hold candidates before an emitted action proceeds instead of settling only the first indexed fallback slot,
  - this keeps overlapping handled keys and shared pd-mode ownership from depending on emit order when more than one active slot is still eligible for fallback-hold activation,
  - host coverage now stages two concurrent fallback-hold candidates directly in the transition suite and asserts both are promoted before dispatch continues.
- Landed the next handled-key overlap fix on `codex/fix-authored-key-freeze`:
  - handled release now defers its tap/action emission while another tap-release-eligible handled sibling is still live instead of emitting immediately into the overlap window,
  - the deferred release emission is now key-runtime-owned shared state with the release-time keyboard mod snapshot preserved, and scan drains it once no tap-release-eligible sibling remains active,
  - host coverage now asserts three contracts directly in the release matrix suite: a live tap-release sibling defers the release dispatch, a held sibling does not, and the deferred release action drains after the sibling retires.
- Extended the handled-key overlap audit around that new release-dispatch rule:
  - the release matrix suite now classifies the immediate-safe release-time effects that can still run inside the sibling-overlap window,
  - owned-state cleanup remains immediate even when a foreign tap-release sibling is still live,
  - pending multi-tap synthetic held register/unregister still remain immediate even when a foreign tap-release sibling is still live,
  - mixed `release_owned_state + dispatch_action` releases now prove the cleanup stays immediate while only the authored action defers,
  - momentary layer release and pd-mode lock-tap releases are now mechanically covered as immediate-safe sibling-overlap effects.
- Extended the audit to the post-overlap drain seam:
  - the release matrix suite now asserts that a sibling delayed-action replay from the normal scan plan executes before the deferred release queue drains in the same scan,
  - a new dedicated delayed-action host suite now proves replay still settles pending fallback holds inside the delayed-action window while restoring the caller's saved keyboard mod state afterward.
- Landed the right-alt arrow-lock tap-path narrowing on `arrowmodefix2`:
  - handled-key release-time `KEY_RUNTIME_EFFECT_DISPATCH_ACTION` now skips fallback-hold settlement when the emitted tap action is a pd-mode lock action,
  - ordinary handled tap actions still keep the existing settle-all pending-fallback-holds policy from the overlap remediation,
  - host coverage now locks the new contract in at the transition seam and upgrades the pd-mode key-runtime integration harness to use real `action_dispatch.c`, real `KC_RIGHT_ALT` authored behavior, and the auto-mouse arrow-entry side effect instead of a test-local tap dispatcher.
- Reconciled the right-alt tap-path fix on `arrowmodefix2` after the emit-policy narrowing failed on hardware:
  - handled active-slot release now maps authored pd-mode lock taps onto `KEY_RUNTIME_EFFECT_PD_MODE_LOCK_TAP` directly instead of routing them through the generic `KEY_RUNTIME_EFFECT_DISPATCH_ACTION -> noah_emit_action_tap(...)` seam,
  - this keeps ordinary authored tap actions on the shared emit path while giving release-time pd-mode locks the same native runtime effect that already covered quick-tap lock toggles on real pd-mode keys,
  - `tests/host/key_runtime_transition_test.c` now asserts that a `KC_RIGHT_ALT` fallback tap to `ARROW_MODE_LOCK` produces the native pd-mode-lock effect and split-sync outcome instead of a generic dispatched action,
  - `tests/host/pd_mode_key_runtime_integration_test.c` and `run_pd_mode_key_runtime_integration_tests.sh` now model the real `auto_mouse_layer_off() -> layer_off() -> noah_layer_state_set_user()` stack by linking `pd_runtime.c` and `pointer_layer_policy.c` instead of using a direct layer-bit clear stub.
- Landed the authored-hold interrupt/release split after the broader `layer_interrupted` model proved unstable on hardware:
  - the slot lifecycle latch in `key_runtime_shared_state.h` now only records momentary-layer tap cancellation on foreign press instead of serving as a generic authored-hold interrupt bit,
  - `key_runtime_slot_policy.c` now sets that latch only for true momentary-layer tap paths, while immediate-hold quick-release taps and pd-mode quick-lock taps keep their own release contracts,
  - `key_runtime_transition.c` no longer infers deferred-release blockers by running the generic release resolver over live slots; blocker classification is now a separate phase/ownership helper that ignores slots once they have committed held or repeat ownership,
  - `key_runtime_transition.c` now also excludes active slots that are still carrying a pending multi-tap hold from deferred-release blocker classification, so a pending modifier-hold does not queue unrelated authored child taps behind a phantom sibling release window,
  - `key_runtime_shared_state.h`, `key_runtime_slot_policy.c`, and `key_runtime_slot_release_resolver.h` now also track foreign-key overlap for immediate-hold paths explicitly, separate from momentary-layer tap cancellation, so a used immediate hold cannot reopen its quick-release tap or preserve a first-tap multi-tap chain on release,
  - the public runtime-debug surface now exposes slot phase and momentary-layer interrupt state so host tests can assert runtime invariants directly instead of inferring them only from output effects,
  - the overlap integration coverage now exercises `KC_LEFT_GUI` double-tap-hold against raw `LT(...)`, authored nav holds, and an authored pd-mode hold while asserting quiescent runtime state after every release-order permutation,
  - host coverage now also locks the pending-multi-tap blocker contract in two places: `tests/host/key_runtime_modifier_hold_integration_test.c` proves a pending multi-tap modifier hold does not defer a processed child, and `tests/host/real_profile_thumb_layer_lock_integration_test.c` proves the raw right-side `LT(LAYER_NAV, KC_SLSH)` path keeps nav-arrow and dragscroll children immediate even while `KC_LEFT_GUI` is still in its second-tap pending-hold window,
  - that same real-profile suite now also proves a held `DRAGSCROLL` slot does not leave a stale first-tap chain behind after overlapping authored nav-arrow use, both with and without the authored right-thumb nav parent and `KC_LEFT_GUI` overlap.

## Findings Snapshot

- `must-fix`: the authored-key overlap wedge described in `authored-key-overlap-wedge-regression.md` remains open until on-device validation closes it. The current branch now models the press-identity and auto-mouse ownership seams more faithfully, but host green alone is still not sufficient evidence that the hardware wedge is gone.
- `should-fix`: the positional registry DSLs are still the main maintainability risk; `key_runtime_internal.h` is still too broad for an internal seam.
- `optional cleanup`: the keymap materialization macros and mixed-responsibility pointing bridge are acceptable now but are the next likely growth hotspots.
- `closure verdict`: keep this thread open until the hardware regression is closed on-device, and until the remaining `should-fix` items are either resolved or explicitly downgraded out of the closure bar.

## Verification

- Passed during the runtime-v2 blocker observation pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_key_runtime_index_tests.sh`
  - `sh tests/host/run_key_runtime_slot_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_admission_tests.sh`
  - `sh tests/host/run_key_runtime_feedback_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`

- Passed during the deferred-release timed blocker ownership pass:
  - `sh tests/host/run_key_runtime_index_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`

- Passed during the deferred-release blocker index pass:
  - `sh tests/host/run_key_runtime_index_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 pending release ownership pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 live lock mutation observation pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_layer_ownership_tests.sh`
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_runtime_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 identity shadow pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_key_runtime_integration_harness_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 layer/modifier lease shadow pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 pd-mode/pointer ownership shadow pass:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed during the runtime-v2 foundation landing:
  - `sh tests/host/run_key_runtime_integration_harness_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_pointer_layer_policy_tests.sh`
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
  - `git diff --check`

- Passed: `sh tests/host/run_all_host_tests.sh`
- Passed: `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during remediation:
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_slot_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during pd-mode auto-mouse ownership remediation on `codex/authored-key-wedge-debug`:
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_integration_harness_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_runtime_tests.sh`
  - `sh tests/host/run_pointer_layer_policy_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Current full-suite status:
  - At that audit snapshot, `sh tests/host/run_all_host_tests.sh` was still blocked by right-alt introspection drift in `docs/KEYMAP-OVERVIEW.md`; the later cleanup on `review-after-arrowmode` resolved that mismatch and restored the clean baseline recorded below.
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes with the current tree.
  - Earlier notes above that marked the full host suite as passed predate this unrelated dirty-tree drift and should be read as audit-time snapshots.
  - `sh tests/host/run_action_dispatch_tests.sh`
  - `sh tests/host/run_action_dispatch_tests.sh`
  - `sh tests/host/run_pd_mode_handlers_tests.sh`
  - `sh tests/host/run_held_action_tests.sh`
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_runtime_init_order_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
- Passed during pd-mode buffered tap remediation:
  - `sh tests/host/run_keyboard_mod_ownership_tests.sh`
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_runtime_tests.sh`
  - `sh tests/host/run_real_profile_validation_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during buffered-tap seam narrowing:
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during transparent-path coverage cleanup:
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during concurrent keyboard-event masking remediation:
  - `sh tests/host/run_pd_mode_tests.sh`
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
- Passed during host-matrix follow-up for concurrent keyboard-event masking:
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_runtime_trace_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during final-outcome cleanup for concurrent keyboard-event masking:
  - `sh tests/host/run_hook_chaining_tests.sh`
  - `sh tests/host/run_key_runtime_integration_harness_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during handled-key overlap remediation on the `15-04` restart branch:
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during active-vs-pending overlap hardening on `codex/fix-authored-key-freeze`:
  - `sh tests/host/run_key_runtime_slot_tests.sh`
  - `sh tests/host/run_key_runtime_index_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during pending fallback-hold settling hardening on `codex/fix-authored-key-freeze`:
  - `sh tests/host/run_key_runtime_slot_tests.sh`
  - `sh tests/host/run_key_runtime_index_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during release-dispatch deferral landing on `codex/fix-authored-key-freeze`:
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during same-family sibling-overlap effect audit on `codex/fix-authored-key-freeze`:
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
- Passed during authored-hold interrupt/release split:
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_runtime_debug_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_real_profile_thumb_layer_lock_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_key_runtime_scenario_tests.sh`
  - `sh tests/host/run_key_runtime_layer_lock_integration_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during post-overlap drain audit on `codex/fix-authored-key-freeze`:
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_delayed_action_tests.sh`
  - `sh tests/host/run_feature_gate_compile_tests.sh`
  - `sh tests/host/run_all_host_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Passed during right-alt arrow-lock tap-path narrowing on `arrowmodefix2`:
  - `sh tests/host/run_key_runtime_transition_tests.sh`
  - `sh tests/host/run_action_dispatch_tests.sh`
  - `sh tests/host/run_key_runtime_preflight_tests.sh`
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh`
  - `sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh`
  - `sh tests/host/run_key_runtime_release_matrix_tests.sh`
  - `sh tests/host/run_pd_mode_tests.sh`
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- Current full-suite status on `arrowmodefix2`:
  - `sh tests/host/run_all_host_tests.sh` is still blocked by pre-existing introspection drift between the dirty right-alt authored row in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c` and `docs/KEYMAP-OVERVIEW.md`; this pass did not touch either file.
- Cleanup follow-up on `codex/fix-authored-key-freeze`:
  - removed the leftover `NOAH_DIAGNOSTIC_*` compile gates from the normal action and key-runtime code paths
  - deleted the one-off host runners that only existed to exercise those diagnostic branches
  - collapsed the affected host suites back to permanent normal-path expectations without changing the landed overlap fix
- Lock-keycode cleanup follow-up on `review-after-arrowmode`:
  - removed the pd-mode lock authoring alias from `users/noah/noah_keymap_ids.h` and switched the remaining repo-owned authored/test surfaces to explicit generated `*_LOCK` keycodes
  - regenerated `docs/KEYMAP-OVERVIEW.md` with `python3 tools/profile_introspect.py --write`, so the right-alt and dragscroll rows now match the current authored keymap
  - reconciled the stale arrow-lock note in `review/2026-04-14-review-18/userspace-architecture-review.md` so the active review folder no longer contradicts the current runtime path
- Current full-suite status on `review-after-arrowmode`:
  - `python3 tools/profile_introspect.py --check` passes
  - `sh tests/host/run_real_profile_validation_tests.sh` passes
  - `sh tests/host/run_pd_mode_key_runtime_integration_tests.sh` passes
  - `sh tests/host/run_feature_gate_compile_tests.sh` passes
  - `sh tests/host/run_all_host_tests.sh` passes
  - `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes
- Sibling workspace folders touched: none

## Next Steps

1. Capture on-device trace snapshots for the original wedge repro families and replay them through the new v2 shadow path.
2. Decide whether the harness adapter should be enabled for additional integration suites once the next reducer domains are stable enough to justify the extra link surface.
3. Move the remaining non-release transport/orchestration seams onto the reducer path so v2 owns not just release decision, effect selection, effect projection, and authoritative release transport, but also scan/flush transport and deferred-release queue assembly end-to-end.
4. Capture and replay on-device traces for the original wedge repros now that blocker gating, pending multi-tap lifecycle, explicit flush/reset, and handled-release effect planning all have reducer-owned visibility.

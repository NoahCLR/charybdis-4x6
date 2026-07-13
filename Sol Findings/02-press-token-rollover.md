# Finding 02: Make Press-Token Allocation Safe Across `uint16_t` Rollover

## Plan metadata

- **Severity:** Must fix — long-uptime stuck-action risk
- **Status:** Planned; no remediation has landed
- **Affected surfaces:** key-runtime reducer token allocation, lease ownership queries, pending-release ownership, release planning, runtime diagnostics/tests
- **Primary files:** [`runtime.c`](../users/noah/lib/key/runtime/reducer/runtime.c), [`runtime.h`](../users/noah/lib/key/runtime/reducer/runtime.h), [`ownership_state.c`](../users/noah/lib/key/runtime/reducer/ownership_state.c), [`release_planner.c`](../users/noah/lib/key/runtime/planning/release_planner.c), pending-release queue
- **Prerequisites:** none for the correctness fix; coordinate target-stack verification with Finding 01 if reducer helpers change call depth
- **Recommended phase:** Phase 1, immediately after stack safety

## Problem statement

Press tokens use `uint16_t` IDs, reserve zero as “no owner,” and allocate with an unchecked post-increment. The 65,536th allocation receives ID zero. Lease lookup deliberately rejects zero, so release planning can fail to see a held-action or repeat lease owned by that press and choose the wrong cleanup path. A keyboard that remains powered long enough can therefore leave an action registered.

Simply skipping zero is necessary but incomplete: after wrap, an ID must not be reused while any retained token, lease, or pending release still refers to it.

## Current evidence and failure scenario

- [`key_runtime_core_state_reset()`](../users/noah/lib/key/runtime/reducer/runtime.h#L372) initializes `next_token_id` to `1u`.
- Press creation in [`runtime.c`](../users/noah/lib/key/runtime/reducer/runtime.c#L338) assigns `.token_id = state->next_token_id++` with no zero or collision check.
- [`key_runtime_core_owner_has_lease_kind()`](../users/noah/lib/key/runtime/reducer/ownership_state.c#L667) immediately returns false when `owner_token_id == 0u`.
- [`key_runtime_core_resolve_active_release()`](../users/noah/lib/key/runtime/planning/release_planner.c#L208) depends on that query to set `held_action_active` and `repeat_active` before deciding release behavior.
- Owner IDs also travel through lease and pending-release records, so a wrapped collision could associate cleanup with the wrong press even after zero itself is skipped.

Representative failure: after 65,535 allocations, a handled press gets token ID zero and activates a held action or repeat. Its physical release is observed, but release planning sees no runtime-owned lease for owner zero. The matching unregister/stop effect is omitted or misplanned, leaving a key/action active until reset.

## Required invariants

1. Token ID zero is never assigned to a press token.
2. A newly allocated ID is distinct from every ownership ID still referenced by a retained press token, active lease, pending release, or other owner-bearing runtime record.
3. Allocation advances deterministically across `0xFFFF -> 0x0001` and records the next candidate consistently.
4. Allocation either returns a unique nonzero ID or fails closed before partially creating a press token or lease.
5. Release, cancellation, deferred release, held-action, repeat, PD-mode, modifier, and layer ownership preserve their existing semantics across rollover.
6. Rollover behavior is testable without generating 65,535 physical events.

## Scope

### In scope

- Centralize press-token ID allocation in one reducer helper.
- Define which records reserve an owner ID and enforce that definition.
- Add near-wrap, collision, deferred-owner, and release-cleanup tests.
- Add diagnostics for the theoretically exhausted allocator path.

### Non-goals

- Widening every owner ID to 32 bits without first evaluating RAM impact.
- Resetting all runtime state at wrap.
- Assuming bounded simultaneously pressed keys alone proves an ID is free; inactive retained tokens and pending state matter.
- Changing press/release semantics unrelated to identity allocation.

## Implementation plan

### Step 1 — Specify owner-ID liveness

1. Enumerate every `owner_token_id` field and every query keyed by token ID.
2. Define an ID as reserved when it appears in any of:
   - a press-token slot whose state is active or retained for release/deferred settlement;
   - an active lease, including held-action and repeat leases;
   - an active pending-release slot;
   - any persistent-intent or future owner-bearing record identified by the enumeration.
3. Add an internal predicate such as `key_runtime_core_token_id_is_reserved(state, id)`. It must reject zero and scan all enumerated owner stores.
4. Document why each store is included. Add a compile/test maintenance hook so adding a new owner store cannot silently bypass allocation policy.

### Step 2 — Introduce a total allocator

1. Replace the post-increment expression with a helper returning success plus an output ID, for example `key_runtime_core_allocate_token_id(state, &id)`.
2. Allocation algorithm:
   - normalize a zero next-candidate to one;
   - test the candidate against the reservation predicate;
   - advance with explicit wrap from `0xFFFF` to `1`;
   - stop on the first free ID;
   - detect a complete cycle and return failure.
3. Because runtime capacities are far below 65,535, optimize for clear correctness. Do not introduce a 65,536-bit allocation table unless measurement justifies its RAM cost.
4. Update `next_token_id` to the normalized candidate after the returned ID, so it never persists as zero.
5. Allocate before mutating the destination token, counts, interruption flags, tap-series state, or leases. If current ordering makes this difficult, stage all fallible preconditions before the first state mutation.
6. On impossible exhaustion, increment a dedicated diagnostic, emit a debug trace, leave the key unowned, and select a defined fail-closed process result. Never silently use zero.

### Step 3 — Audit identity consumers

1. Keep zero rejection in ownership helpers as defense in depth.
2. Confirm release/cancel paths do not clear a press-token ID before its pending-release and special leases have finished.
3. Verify pending-release owner marking/clearing finds the correct retained token after wrap.
4. Verify equality checks compare full `uint16_t` values and do not use raw ordering assumptions.
5. Confirm debug snapshots and trace payloads can represent `0xFFFF` and post-wrap IDs without truncation or treating them as sentinel values.

### Step 4 — Add boundary-first tests

1. Expose a test-only state setup/helper through the existing host harness; do not add a production API just to set `next_token_id`.
2. Exercise allocations at `0xFFFE`, `0xFFFF`, and `0x0001`.
3. Hold ID `1` live while wrapping and prove the allocator selects `2`.
4. Reserve candidates independently through:
   - an active press;
   - an inactive retained token awaiting release settlement;
   - a held-action lease;
   - a repeat lease;
   - a pending release.
5. Drive each allocated press through release/cancellation and assert every registered action, lease, pending marker, and token count returns to baseline.
6. Test the exhaustion branch using a small test-only ID domain or injected reservation predicate, rather than allocating enormous production arrays.

## Test and verification plan

### Targeted existing runners

```sh
sh tests/host/run_key_runtime_release_matrix_tests.sh
sh tests/host/run_key_runtime_modifier_hold_integration_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_key_runtime_layer_lock_integration_tests.sh
sh tests/host/run_key_runtime_scenario_tests.sh
sh tests/host/run_key_runtime_integration_harness_tests.sh
sh tests/host/run_runtime_debug_tests.sh
sh tests/host/run_held_action_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
```

If internal headers, source lists, or compile-only test surfaces change:

```sh
sh tests/host/run_feature_gate_compile_tests.sh
```

### Closure gates

```sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
```

## Measurements and observability

- Record the number of reservation slots scanned in the worst wrap-collision test. Allocation occurs per press, but it should still remain bounded by compile-time runtime capacities.
- Add a debug counter for allocator exhaustion and, optionally, collision skips. The exhaustion counter must remain zero in all integration scenarios.
- Include token IDs around wrap in runtime trace fixtures so formatting and transport remain lossless.

## Risks, tradeoffs, and fallbacks

- **Scanning all owner stores adds press-path work.** It occurs only during allocation and is bounded. If profiling later shows a problem, maintain a small active-ID index; do not weaken collision checks.
- **Only scanning active press tokens misses deferred ownership.** The liveness specification and dedicated tests prevent this shortcut.
- **Allocation failure after partial mutation corrupts counts.** Make allocation the first fallible step or add an explicit rollback tested by fault injection.
- **Widening IDs reduces wrap frequency but increases every token/lease record.** Consider it only with linked RAM measurements; correct wrap handling is still required for any finite type.
- **Sequence rollover is separate.** Finding 09 covers pending-release ordering; do not conflate ID equality with sequence ordering.

## Documentation and review-note updates

- Record the owner-ID liveness definition and allocator failure policy in the active open firmware architecture review. If the relevant folder is closed, open the next sortable review folder.
- Update runtime architecture notes if owner identity becomes a formal reducer API contract.
- Update developer-facing diagnostics documentation if new counters or trace fields are exposed.
- No user-facing behavior documentation is expected unless allocation failure is surfaced to users.

## Acceptance checklist

- [ ] No press-token construction uses unchecked increment for identity.
- [ ] Zero is skipped both during allocation and when normalizing stored next state.
- [ ] All retained owner-bearing stores participate in collision detection.
- [ ] Near-wrap tests cover `0xFFFE`, `0xFFFF`, wrap, and a live low-ID collision.
- [ ] Held-action and repeat releases remain balanced for the wrapped token.
- [ ] Pending-release ownership is not stolen or orphaned across wrap.
- [ ] Exhaustion fails before partial token/lease creation and is diagnosable.
- [ ] All targeted host runners pass.
- [ ] `sh tests/host/run_all_host_tests.sh` passes.
- [ ] `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.
- [ ] Review notes describe the landed allocator and enforcement.

## Next action

Write the owner-ID liveness table from the current reducer structs, then add failing near-wrap tests to `runtime_debug_test.c` before replacing the post-increment assignment.

# Finding 08: Make Synthetic Key Ownership Safe Across Overlapping Producers

## Plan metadata

- **Severity:** Should-fix (P1 stuck-key and premature-release correctness risk)
- **Status:** In progress; strict macro-local hold balance landed first, aggregate ownership remains open
- **Affected surfaces:** owned keycode dispatch, physical key hooks, held actions, macro holds/taps, pointing-mode shortcuts, keyboard modifiers, mouse/consumer actions
- **Primary files:** [owned_keycode.c](../users/noah/lib/action/owned_keycode.c), [keyboard_mod_ownership.c](../users/noah/lib/state/ownership/keyboard_mod_ownership.c), [macro_payload_internal.h](../users/noah/lib/macro/macro_payload_internal.h), [macro_payload_run.c](../users/noah/lib/macro/macro_payload_run.c)
- **Prerequisites:** None, but finish this contract before the scan-driven macro engine in [Finding 07](07-nonblocking-macro-playback.md)
- **Recommended phase:** Phase 2, before nonblocking macro playback

## Problem statement

Modifier keys already distinguish physical and managed owners, but ordinary basic keys do not. owned_keycode_register calls register_code directly and owned_keycode_unregister calls unregister_code directly. QMK's keyboard report is not a reference-counted set: registering a key already present can force report transitions, and unregistering it removes the key even if another physical or synthetic producer still owns it.

A physical KC_C held while a macro taps KC_C can therefore disappear from the report when the macro unregisters. Different modded actions that share the same basic key have the same collision. Mouse buttons and other 8-bit actions also pass through the direct path.

Macro hold balancing adds a second ownership hole. An orphan key-up is treated as valid; it can invoke unregister even though that macro never acquired the key, potentially decrementing or releasing another producer's ownership.

## Current evidence and failure scenarios

- users/noah/lib/action/owned_keycode.c:36-59 sends non-modifier presses directly through register_code.
- users/noah/lib/action/owned_keycode.c:62-85 sends non-modifier releases directly through unregister_code.
- users/noah/lib/action/owned_keycode.c:37-45 and 63-71 protect the modifier portion of QK_MODS actions, but their basic key still uses direct register/unregister.
- users/noah/lib/action/owned_keycode.c:88-98 implements a tap as acquire, blocking delay, and release without a scoped ownership object.
- users/noah/lib/state/ownership/keyboard_mod_ownership.c:86-115 tracks physical modifier refs and suppresses a default release while managed refs remain.
- users/noah/lib/state/ownership/keyboard_mod_ownership.c:117-175 changes the live mod report only when aggregate ownership requires it.
- users/noah/lib/macro/macro_payload_internal.h:64-80 returns true when key-up has no matching key-down.
- users/noah/lib/macro/macro_payload_run.c:142-147 executes unregister before discarding the unmatched balance result.
- tests/host/macro_payload_test.c:210-220 and 231-245 explicitly encode and round-trip a payload ending in {-KC_LSFT}, preserving the unsafe contract.

Failure A:

1. Physically hold KC_C.
2. A macro or pointing action taps KC_C.
3. The synthetic register overlaps the existing report entry.
4. The synthetic unregister removes KC_C even though the physical switch remains down.

Failure B:

1. One runtime owner holds KC_LSFT.
2. A macro containing only {-KC_LSFT} is accepted.
3. Macro playback unregisters shift without a macro-owned acquisition.
4. Another owner's refcount/report can be decremented or released early.

## Required invariants

1. The live report reflects aggregate physical plus managed ownership for every supported synthetic key domain.
2. A managed acquire emits a QMK press only on aggregate transition 0 to 1.
3. A managed release emits a QMK release only on aggregate transition 1 to 0.
4. Physical press/release defaults are suppressed only when the ownership layer has already made the required report state authoritative.
5. Releasing one action's basic component cannot release another action that shares it.
6. Persistent synthetic acquisitions use owner-scoped, idempotently releasable leases.
7. An orphan or duplicate macro hold transition is invalid and changes no ownership.
8. Modifier behavior, including physical/managed coexistence and suspended mod state, remains unchanged.
9. Pointer-layer policy notifications follow aggregate visible transitions, not each internal refcount change.
10. Refcount saturation/underflow is detected and fails closed without corrupting report state.

## Scope and non-goals

In scope:

- basic keyboard usages, modifiers, QK_MODS decomposition, and the 8-bit mouse/consumer domains currently accepted by owned_keycode;
- physical ownership tracking at the existing process-record seam;
- owner-scoped leases for holds and taps;
- migration of macros, held actions, and pointing/runtime callers;
- strict macro-local hold balance.

Not in scope:

- arbitrary 16-bit QMK action ownership outside the currently supported decomposition;
- changing authored key behavior;
- rewriting QMK's report implementation;
- combining this with the nonblocking macro scheduler, beyond providing its ownership API;
- preserving intentionally unbalanced macros. Local macro payloads must be self-contained.

## Detailed implementation plan

### Step 1: Characterize and classify key domains

1. Inspect the pinned QMK register_code/unregister_code implementation for basic keyboard, modifier, mouse button, consumer, and system usages.
2. Add one internal classifier that maps an accepted action into report-domain components.
3. Reject unsupported 16-bit actions before changing any component.
4. Keep fork-specific classification in the compat/action boundary rather than spreading QMK assumptions.

The decomposition of a QK_MODS action must be atomic: acquire modifiers and the basic key together, or roll back every component on failure.

### Step 2: Add aggregate ownership for non-modifier components

Add compact per-usage physical and managed refcounts for each supported domain. For a basic key:

- physical press first increments the physical refcount;
- if managed ownership already keeps the key down, suppress QMK's duplicate default press;
- physical release decrements the physical refcount;
- if managed ownership remains, suppress QMK's default release;
- managed acquire/release calls register/unregister only when aggregate ownership crosses zero.

Apply equivalent transition rules using the correct QMK API for mouse/consumer domains. Do not assume register_code's keyboard semantics apply to every domain.

Use saturating counters plus a diagnostic error; never wrap a refcount to zero.

### Step 3: Introduce owner-scoped leases

Replace persistent register/unregister pairs with an API shaped around an explicit lease:

- acquire(action, lease_out) initializes an inactive lease, validates/decomposes the action, acquires every component, then marks it active;
- release(lease) is idempotent, releases exactly the recorded components, and clears the lease;
- tap(action) uses a local lease and cannot release ownership it did not acquire.

The lease stores only the normalized components and active state needed for release. It must not retain a pointer to stack-owned action data.

Keep legacy owned_keycode_register/unregister wrappers private or remove them once callers migrate. Do not expose an unscoped unregister as the normal runtime interface.

### Step 4: Migrate physical and synthetic callers

1. Wire physical tracking and default-suppression checks into the same hook chain currently used for modifier ownership.
2. Extend hook-chaining tests so keymap overrides still call the noah helper.
3. Migrate held-action storage to retain a lease per active held action.
4. Migrate macro hold balance to retain leases owned by the active macro execution.
5. Migrate pointing-mode persistent keys such as arrow-selection shift.
6. Keep one-shot taps local and bounded, using the lease API.
7. Audit users/noah for raw register_code, unregister_code, register_code16, unregister_code16, add/del key, and mouse-report mutation. Document every intentional boundary exception.

### Step 5: Make macro payload balance strict

1. Change macro_payload_hold_balance_note_up to fail when the key is absent.
2. Reject orphan key-up in both hardcoded parsing and QMK/VIA stream decoding.
3. Reject duplicate downs and incomplete holds, as today.
4. Validate the complete IR before any playback side effect.
5. During execution, look up the matching macro-owned lease before release; a missing lease aborts without touching aggregate ownership.
6. Replace the current tests that bless {-KC_LSFT} with negative validation/encoding tests.

### Step 6: Define reset and recovery

Runtime reset should:

- cancel/release every managed lease through its owner lifecycle before clearing counters where possible;
- reconcile the live report after counters reset;
- clear diagnostics separately from ownership only when the existing reset contract requires it.

Do not recover underflow by blindly unregistering a key. Log the invariant violation and preserve any known physical owner.

## Test plan

Add focused host cases for:

- physical KC_C held across synthetic KC_C tap;
- two synthetic owners of KC_C released in both orders;
- G(KC_C) overlapping S(KC_C), proving shared C survives the first release;
- physical modifier plus multiple managed modifiers;
- mouse button and supported consumer overlap;
- pointer-policy notifications only on aggregate transitions;
- acquire rollback after a component fails;
- idempotent lease release, refcount saturation, and unmatched release;
- hardcoded and VIA orphan key-up rejection with zero report side effects;
- macro abort releasing only macro-owned leases;
- held action, macro, and pointing producer overlap.

Run during implementation:

~~~sh
sh tests/host/run_owned_keycode_tests.sh
sh tests/host/run_keyboard_mod_ownership_tests.sh
sh tests/host/run_held_action_tests.sh
sh tests/host/run_action_lifecycle_tests.sh
sh tests/host/run_macro_payload_tests.sh
sh tests/host/run_via_macro_action_lifecycle_tests.sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_hook_chaining_tests.sh
~~~

For hook/header/source-boundary changes:

~~~sh
sh tests/host/run_feature_gate_compile_tests.sh
sh tests/host/run_qmk_contract_checks.sh
~~~

Closure requires:

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

Add debug snapshots for physical and managed counts, plus counters for saturation, underflow attempts, unsupported domains, acquire rollback, and idempotent double release. Avoid per-scan logging; emit only new invariant violations or expose snapshots through existing runtime diagnostics.

Measure BSS cost of the ownership tables in the firmware map. Prefer domain-sized arrays/bitsets over a generic sparse allocator unless profiling proves otherwise.

## Risks, tradeoffs, and fallback

- **Hook ordering:** physical tracking after QMK default handling cannot suppress a destructive transition. Enforce ordering in tests.
- **Domain misclassification:** mouse/consumer usages need their native report APIs. Confirm against the pinned fork.
- **Memory growth:** full uint8_t physical+managed arrays are simple but have a measurable BSS cost. Size by supported usage range and report it.
- **Lease migration gaps:** one unscoped caller can reintroduce cross-release. Finish with a raw-API grep and explicit exception list.
- **Behavior change for orphan macros:** rejection is intentional and should be documented.
- **Fallback:** if all domains cannot land safely together, first land strict macro balance and keyboard-basic ownership behind one API, then add mouse/consumer support before declaring the finding resolved.

## Documentation and review-note updates

When this lands, document the lease/aggregate-report contract in the runtime architecture note, update macro syntax docs to say key-up requires a matching key-down, and record intentional raw QMK report boundaries. Update progress.md with code/tests/BSS impact; if the relevant runtime review is closed, open the next sortable review folder.

## Implementation checkpoints

### 2026-08-15 — Strict macro-local release boundary

- Orphan key-up now fails hold-balance validation instead of being accepted as
  a no-op balance change.
- Hardcoded compilation, QMK/VIA stream decoding, and IR preflight all share
  that strict rule.
- Playback removes the macro-local balance entry before invoking unregister;
  a missing acquisition therefore aborts without a release side effect.
- Tests cover direct payload rejection, encoded-stream rejection, and a
  deliberately malformed IR with zero report operations.
- `docs/KEYMAP.md` now defines explicit key-up as requiring an earlier unmatched
  key-down for the same macro.

Aggregate basic/mouse/consumer ownership, scoped leases, physical overlap, and
the remaining acceptance checklist are still open. This checkpoint is not
Finding 08 closure.

## Acceptance checklist

- [ ] Physical and synthetic ownership can overlap for every supported domain.
- [ ] Shared basic keys survive release of one modded action.
- [ ] Persistent callers use scoped leases.
- [ ] Orphan macro key-up is rejected before side effects.
- [ ] Refcount overflow/underflow cannot wrap or release another owner.
- [ ] Hook order and raw-QMK boundary exceptions are mechanically checked.
- [ ] Ownership BSS cost is measured.
- [ ] Targeted tests, full host suite, and firmware compile pass.
- [ ] Docs and the active review match the implementation.

## Next action

Next, write the failing physical-`KC_C` plus synthetic-tap integration test.
Then define the normalized component/lease structure and prove aggregate
zero-to-one/one-to-zero transitions before migrating persistent callers.

# Finding 09: Remove Rollover from Pending-Release FIFO Ordering

## Plan metadata

- **Severity:** Should-fix (P1 long-uptime release-order correctness risk)
- **Status:** Planned; uint16_t sequence ordering still wraps
- **Affected surfaces:** key-runtime pending-release queue, deferred release draining, debug projections, core-state layout
- **Primary files:** [pending_release_queue.c](../users/noah/lib/key/runtime/queue/pending_release_queue.c), [runtime.h](../users/noah/lib/key/runtime/reducer/runtime.h), [deferred_release.c](../users/noah/lib/key/runtime/deferred_release.c)
- **Prerequisites:** Coordinate state-layout work with Finding 01's stack remediation and Finding 02's press-token rollover tests
- **Recommended phase:** Phase 1, coordinated with stack-safety work

## Problem statement

Pending releases receive a uint16_t sequence number and are ordered with ordinary numeric comparisons. The counter is incremented without a zero guard. When sequence 65,535 and sequence 0 coexist, zero is treated as older numerically but is also invisible to the ordered lookup that starts at previous_sequence = 0 and requires a greater value.

The result can be incorrect debug/projection order, starvation, or an undrained release after enough queue insertions. Widening the counter only postpones the same class of bug. Because the queue is bounded to 120 slots, explicit FIFO linkage is simpler and removes time-based ordering entirely.

## Current evidence and failure scenario

- users/noah/lib/key/runtime/reducer/runtime.h:162-181 stores uint16_t sequence in both the compact slot and snapshot.
- users/noah/lib/key/runtime/reducer/runtime.h:301-315 stores 120 slots and next_pending_release_sequence in core state.
- users/noah/lib/key/runtime/reducer/runtime.h:372-381 initializes the counter to one.
- users/noah/lib/key/runtime/queue/pending_release_queue.c:88-107 chooses the oldest eligible entry with raw numeric comparison.
- users/noah/lib/key/runtime/queue/pending_release_queue.c:110-143 enumerates ordered entries from previous_sequence = 0 using pending->sequence > previous_sequence.
- users/noah/lib/key/runtime/queue/pending_release_queue.c:221-229 assigns state->next_pending_release_sequence++ without skipping zero.
- users/noah/lib/key/runtime/queue/pending_release_queue.c:273-288 repeatedly selects and removes the numerically oldest eligible release.
- users/noah/lib/key/runtime/queue/pending_release_queue.c:323-335 removes a matching entry by array index rather than FIFO position.

Failure:

1. Hold at least one pending entry across the counter boundary while other entries are inserted/drained.
2. Enqueue sequence 65,535, then enqueue sequence 0.
3. Ordered snapshot lookup cannot select sequence 0 because it is not greater than its initial zero sentinel.
4. Oldest selection may choose zero before older live entries, violating FIFO.
5. A release can become invisible or drain out of order.

## Required invariants

1. Enqueue order is represented independently of a wrapping clock/counter.
2. Every active slot appears exactly once in the FIFO chain.
3. Head and tail are both invalid when and only when the queue is empty.
4. Tail insertion is O(1); ordered iteration is O(N); removal preserves remaining order.
5. Draining skips entries with active owners but selects the first eligible entry in FIFO order.
6. Removing a matching deferred release unlinks the oldest matching entry, not the lowest array slot.
7. Queue count equals the number of reachable active slots.
8. No operation can create a cycle or leave head/tail pointing at an inactive slot.
9. Owner-token pending flags clear only after the last release for that owner is unlinked.
10. The compact-slot size limit and target RAM/stack budgets remain satisfied.

## Scope and non-goals

In scope:

- replace sequence ordering with an index-linked bounded FIFO;
- centralize unlink/count/token cleanup;
- add direct queue unit tests and corruption assertions in host builds;
- remove obsolete sequence fields/counter if no consumer needs them.

Not in scope:

- changing release semantics or blocker policy;
- increasing pending-release capacity;
- combining the queue with press-token storage;
- allocating/sorting a 120-entry temporary array on the stack;
- treating a uint32_t counter as a complete rollover fix.

## Detailed implementation plan

### Step 1: Represent order explicitly

Replace pending_release_slot_t.sequence with a uint8_t next_queue_index. Add uint8_t pending_release_head_index and pending_release_tail_index to key_runtime_core_state_t. Use UINT8_MAX as the invalid sentinel and add a static assertion that KEY_RUNTIME_CORE_PENDING_RELEASE_CAPACITY is less than UINT8_MAX.

Remove next_pending_release_sequence. Remove sequence from pending_release_t unless a debug consumer proves it is required; if a diagnostic ordinal is useful, calculate it during traversal rather than storing it.

Confirm the exact sizeof pending_release_slot_t and core state before and after. The intended design should not grow the slot.

### Step 2: Centralize list operations

Add small internal helpers for:

- initializing head/tail sentinels;
- appending an allocated slot to tail;
- finding a slot's predecessor;
- unlinking a slot with known predecessor;
- iterating the Nth active entry;
- finding the first FIFO entry eligible to drain;
- finding the oldest FIFO entry matching key position/action/mod state.

One unlink helper must perform all of:

1. reconnect predecessor or move head;
2. update tail when removing the last node;
3. clear the slot;
4. decrement count exactly once;
5. reevaluate the owner token's pending-release flag.

Do not duplicate removal bookkeeping in take and observe-drained paths.

### Step 3: Preserve blocker and owner semantics

The first eligible-to-drain search should walk from head and skip entries whose owner token is active. It may remove a later eligible node while an earlier active-owner node remains linked. The linked order of all remaining entries must stay unchanged.

Keep the global deferred-release blocker behavior at pending_release_queue.c:269-270 unchanged unless a separate finding intentionally changes it.

### Step 4: Add structural validation

In host/debug builds, validate:

- traversal terminates within capacity;
- every reachable slot is active;
- no active slot is unreachable;
- reachable count equals pending_release_count;
- empty/nonempty head/tail rules;
- tail's next index is invalid.

Production code should avoid a full validation scan on every hot-path operation. Run it at unit-test seams or behind the existing diagnostics gate.

### Step 5: Update mirrors and state initialization

Update:

- core-state initialization/reset;
- debug snapshots and projections that expose pending order;
- host stubs/fixtures using positional initializers;
- size assertions and feature-gate builds;
- any review diagrams that still describe sequence sorting.

No new source file should be necessary. If one is introduced, update users/noah/source_manifest.mk and host mirrors.

## Test plan

Create a focused tests/host/pending_release_queue_test.c and runner, then add that runner to run_all_host_tests.sh. Cases:

- FIFO append and ordered enumeration across reused array slots;
- removal of head, middle, tail, and only entry;
- skip an active-owner head and drain the first eligible later entry;
- later owner release makes the original head eligible without reordering it;
- duplicate keypos/action/mod entries remove the oldest matching one;
- count/token flags after partial and final owner removal;
- fill to 120, reject overflow, drain, and reuse every slot;
- more than 65,536 enqueue/drain operations with one long-lived blocked entry, proving no rollover concept remains;
- reset and structural validation;
- core/slot sizeof assertions.

Run:

~~~sh
sh tests/host/run_pending_release_queue_tests.sh
sh tests/host/run_key_runtime_release_matrix_tests.sh
sh tests/host/run_key_runtime_layer_lock_integration_tests.sh
sh tests/host/run_key_runtime_scenario_tests.sh
sh tests/host/run_key_runtime_integration_harness_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_runtime_debug_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
~~~

Closure requires:

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

Record queue high-water mark and structural-validation failures under existing diagnostics. Compare:

- pending_release_slot_t size;
- key_runtime_core_state_t size;
- stack usage of enqueue, ordered lookup, and drain;
- scan/drain operation counts for a full queue.

The new implementation must not introduce a capacity-sized automatic array, especially while Finding 01 is open.

## Risks, tradeoffs, and fallback

- **Link corruption:** centralized unlink helpers and traversal bounds are mandatory.
- **Middle removal cost:** predecessor search is O(N), but current code already scans O(N), capacity is bounded, and ordered enumeration improves from repeated sorting scans.
- **State-layout drift:** compile gates and static size assertions must cover all feature combinations.
- **Debug API churn:** remove sequence rather than leaving a misleading wrapping diagnostic. Derive display ordinals if needed.
- **Fallback:** a wrap-triggered renumber can be correct only with a non-stack scratch representation and rigorous live-order tests; explicit FIFO linkage is preferred because it eliminates the hazard.

## Documentation and review-note updates

Update the runtime architecture/review note with the explicit FIFO invariant, measured state sizes, and the new focused runner. If the relevant runtime review is closed, open the next sortable review folder rather than amending closure history.

## Acceptance checklist

- [ ] No wrapping sequence participates in pending-release order.
- [ ] Every removal path uses one unlink helper.
- [ ] Active-owner skipping preserves FIFO order.
- [ ] More than 65,536 operations pass with a long-lived entry.
- [ ] Structural invariants and memory sizes are tested.
- [ ] No capacity-sized stack scratch is added.
- [ ] Targeted tests, full host suite, and firmware compile pass.
- [ ] Review notes match the explicit FIFO design.

## Next action

Write the focused queue test and reproduce the current 65,535/0 failure through direct core-state setup. Then replace sequence fields with head/tail/next indices in one local patch while keeping release policy unchanged.

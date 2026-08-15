# Finding 14: Reduce Macro Cache RAM Without Losing Validation

## Plan metadata

- Severity: medium optimization with high memory impact
- Status: verified
- Recommended phase: Phase 6 memory optimization, after [Finding 04](04-via-macro-byte-validation.md) and [Finding 07](07-nonblocking-macro-playback.md)
- Affected surfaces:
  - users/noah/lib/macro/macro_payload.h
  - users/noah/lib/macro/macro_slot_provider.h
  - users/noah/lib/macro/macro_slot_provider.c
  - users/noah/lib/macro/via_macro_provider.c
  - users/noah/lib/macro/macro_dispatch.c
  - users/noah/noah_keymap_ids.h
  - tests/host/macro_dispatch_test.c
  - tests/host/macro_payload_test.c
  - tests/host/via_macro_action_lifecycle_test.c
- Prerequisites:
  - Resolve the active macro IR lifetime required by the nonblocking playback design in [Finding 07](07-nonblocking-macro-playback.md).
  - Preserve the invalid-payload rejection contract from the VIA byte-validation finding.
  - Obtain a successful target build and a reproducible map or symbol-size report using one pinned toolchain.

## Problem statement

Every logical macro slot reserves a full 512-byte intermediate representation whether the slot is empty, invalid, rarely used, or never used. The two static cache arrays therefore consume tens of kilobytes of BSS for a profile that currently has only a small number of populated VIA macros and no populated hardcoded macros.

The cache buys zero-decode repeat playback, but its memory cost scales with the maximum slot count rather than real use. This reduces firmware RAM headroom and makes unrelated runtime safety fixes harder.

## Current evidence and quantified baseline

- users/noah/lib/macro/macro_payload.h:9 sets MACRO_PAYLOAD_IR_MAX_BYTES to 512.
- users/noah/lib/macro/macro_payload.h:24-27 makes each macro_payload_ir_t 514 bytes before enclosing alignment.
- users/noah/lib/macro/macro_slot_provider.h:22-31 stores a state plus a full IR in every macro_slot_cache_t.
- users/noah/lib/macro/via_macro_provider.c:21 reserves one cache entry for each VIA macro.
- users/noah/noah_keymap_ids.h:27 defines 64 VIA macro slots.
- users/noah/lib/macro/macro_dispatch.c:13 reserves one cache entry for every hardcoded macro.
- users/noah/noah_keymap_ids.h:154 defines 16 hardcoded slots.
- The fresh pre-change target symbols measured 518 bytes per slot:
  - VIA cache: 64 × 518 = 33,152 bytes
  - hardcoded cache: 16 × 518 = 8,288 bytes
  - combined: 41,440 bytes
- The same linked image measured total BSS at 65,196 bytes, making these arrays approximately 63.3 percent of BSS.
- The audited authored profile populated 12 VIA defaults and zero hardcoded slots, so most reserved IR storage cannot produce a cache hit.

Failure-pressure scenario:

1. A runtime safety fix needs additional persistent state or stack headroom.
2. The linker image already carries 41,280 bytes of per-slot macro IR cache.
3. Even empty slots reserve full capacity.
4. RAM pressure is paid continuously to optimize a workload that uses only a minority of slots.

## Required invariants

1. Valid macro output is byte-for-byte and action-for-action identical.
2. Invalid payloads remain cached as invalid or otherwise avoid repeated expensive parsing on every press.
3. VIA writes and reset operations invalidate all affected metadata and IR entries before subsequent playback.
4. A macro being executed asynchronously owns stable IR or source data for its entire lifetime.
5. No cache eviction can corrupt, replace, or invalidate the active macro.
6. Slot-count contracts and VIA keycode ranges remain unchanged.
7. Empty and out-of-range slots remain safe and deterministic.
8. RAM improvement is measured from a target map or symbols, not estimated only from host sizeof.
9. The selected design has a bounded worst-case parse cost and no unbounded allocation.

## Scope

- Replace full IR per logical slot with a bounded cache architecture.
- Keep compact per-slot state where it prevents repeated invalid parsing.
- Add target memory reporting and a regression budget.
- Measure cold and warm playback costs.
- Coordinate ownership of the active IR with nonblocking macro playback.

## Non-goals

- Do not reduce VIA's configured 64-slot user-facing capacity.
- Do not weaken payload validation or permit playback directly from unchecked storage.
- Do not introduce heap allocation.
- Do not encode authoring policy into the generic slot provider.
- Do not stream arbitrary bytes to QMK send-string tables without validated decoding.

## Candidate architectures

Evaluate these with the same tests and measurements:

### Candidate A: Metadata per slot plus one active scratch IR

- Keep a compact state byte or generation per logical slot.
- Decode into one shared IR when playback begins.
- Transfer that IR into the nonblocking player's owned active buffer, or make the scratch itself unavailable until playback completes.
- Re-decode valid slots on each later invocation.
- Expected static cost: approximately one IR plus compact metadata.
- Tradeoff: lowest RAM, highest repeat parse cost.

### Candidate B: Small fixed LRU plus active IR

- Keep compact state or source generation per slot.
- Cache a configurable two to four compiled IR entries tagged by provider, slot, and generation.
- Pin or separately copy the active entry while it is executing.
- Expected static cost: a few kilobytes rather than 41 kilobytes.
- Tradeoff: more policy and invalidation logic, but repeated common macros stay fast.

### Candidate C: Compact validated index over stable encoded storage

- Validate and record slot boundaries and validity metadata.
- Have the scheduler consume validated encoded operations from stable source storage.
- Expected static cost: metadata and scheduler state only.
- Tradeoff: largest execution-model change and tight coupling to VIA mutation rules.

Start with Candidate A as the correctness baseline. Select Candidate B only if measured cold-parse latency is meaningful in normal use. Candidate C should be considered only if it naturally follows the nonblocking playback architecture and remains mechanically validated.

## Implementation plan

### Phase 1: Make memory and workload measurements reproducible

1. Add host static assertions or a small test report for sizeof macro_payload_ir_t and macro_slot_cache_t.
2. After a successful target compile, capture:
   - total data and BSS from the ELF
   - sizes of via_macro_slots and hardcoded_macro_slots from sorted symbols
   - remaining RAM headroom using the same toolchain
3. Record populated VIA and hardcoded slot counts from generated profile introspection, without changing authored data.
4. Add counters in host tests for compile calls, cache hits, invalid hits, and evictions.

### Phase 2: Separate logical slot metadata from stored IR

1. Replace macro_slot_cache_t as the public per-slot model with two concepts:
   - compact logical-slot metadata: unchecked, valid, invalid, plus a source generation if needed
   - a bounded compiled-entry object containing provider identity, slot, generation, IR, and replacement state
2. Keep provider APIs explicit about whether they require an output IR, a lookup, or playback.
3. Ensure hardcoded and VIA providers cannot collide when they use the same numeric slot.
4. Preserve invalid-state caching without reserving an IR for invalid or unchecked entries.

### Phase 3: Define active IR ownership

1. Make the macro scheduler acquire a compiled IR through an explicit handle or copy.
2. Prevent eviction while a handle is active.
3. If there is one global macro execution at a time, use that constraint to avoid an extra active copy, but enforce it in tests.
4. Define cancellation and completion paths that always release or unpin the entry.
5. Verify a VIA buffer mutation during playback cannot alter the active decoded operation stream.

### Phase 4: Implement bounded caching and invalidation

1. Implement Candidate A first with compact metadata and one owned decode buffer.
2. Benchmark repeated playback. If parsing cost crosses the agreed scan-time budget, add the smallest LRU that meets it.
3. Invalidate by provider and slot where the changed range can be identified reliably; otherwise invalidate all VIA metadata and VIA-tagged entries.
4. Hardcoded cache state should remain independent of VIA invalidation.
5. Use monotonic source generations with wrap-safe equality or reset all tags on generation wrap.
6. Add compile-time bounds for LRU entry count and IR capacity.

### Phase 5: Add a target memory gate

1. Add a deterministic post-build script or documented command that reads ELF/map symbols.
2. Gate the combined static macro-cache storage to an agreed ceiling. A reasonable initial acceptance target is no more than 8 KiB and at least 32 KiB reclaimed from the audited baseline.
3. Also record total BSS so memory merely moved into another unnamed buffer cannot pass.
4. Make the report name the toolchain and ELF path.
5. Keep a narrow update procedure for an intentional budget change; never silently relax the threshold.

## Test and verification plan

Add or extend tests for:

- Cold valid load and playback.
- Warm valid playback under the chosen cache policy.
- Invalid payload is rejected and does not reparse until invalidated.
- Empty slot behavior.
- Provider and slot tags prevent collisions.
- Deterministic LRU replacement, if selected.
- Active entry cannot be evicted.
- Cancellation and completion release the entry.
- VIA single-slot/range mutation invalidates all affected entries.
- VIA reset invalidates entries.
- Hardcoded state survives VIA-only invalidation.
- Generation rollover cannot create a false cache hit.
- Maximum-length 512-byte IR remains bounded and valid.
- Nonblocking playback continues safely while another macro request arrives according to the chosen queue/reject policy.

Run targeted checks repeatedly:

1. sh tests/host/run_macro_payload_tests.sh
2. sh tests/host/run_macro_dispatch_tests.sh
3. sh tests/host/run_via_macro_action_lifecycle_tests.sh
4. sh tests/host/run_via_macro_defaults_tests.sh
5. sh tests/host/run_action_lifecycle_tests.sh
6. sh tests/host/run_feature_gate_compile_tests.sh

Closure gates:

1. sh tests/host/run_all_host_tests.sh
2. qmk compile -kb bastardkb/charybdis/4x6 -km noah
3. Run the new ELF/map memory report against the successfully compiled target.

## Observability and measurements

- Baseline and final total BSS.
- Baseline and final bytes attributable to macro cache, metadata, and active scheduler IR.
- Cold compile time and warm lookup time in a host microbenchmark.
- Cache hit, miss, invalid-hit, and eviction counts under a representative sequence of the 12 populated VIA slots.
- Maximum main-loop work for a cold maximum-length payload.
- Firmware text-size delta; large code growth can offset an overly elaborate cache.

## Risks, tradeoffs, and fallbacks

- One scratch IR conflicts with asynchronous playback unless ownership is explicit.
- Parsing on every press may trade RAM for input latency; measure rather than assume.
- LRU metadata and invalidation can become more complex than the workload warrants.
- A direct-from-storage player can observe VIA mutation unless it snapshots or version-checks the source.
- Aggressive packing can create alignment or enum-size assumptions across toolchains. Use fixed-width fields and static assertions.
- If nonblocking playback is not ready, land only metadata separation and a small fixed compiled cache that gives the current synchronous player stable ownership.

## Documentation and review-note updates

- Update macro runtime documentation with cache capacity, invalidation, and active-playback ownership.
- Update the active open architecture review progress.md during implementation, or open the next sortable review folder if the relevant review is closed.
- Record exact before/after symbol sizes, total BSS, toolchain, cache policy, and benchmark results.
- If a new memory-report command is added, document it in README.md or the appropriate developer workflow page.

## Acceptance checklist

- [x] Per-slot metadata no longer embeds a 512-byte IR.
- [x] Active asynchronous playback owns stable decoded data.
- [x] Valid, invalid, empty, mutation, reset, provider independence, and busy cases are tested; Candidate A has no eviction.
- [x] 40,841 bytes are reclaimed from the 41,440-byte linked baseline.
- [x] Combined named macro storage is 599 B and below the 8 KiB gate.
- [x] Static BSS falls to 25,524 B and recovered RAM expands linker heap to 212,608 B; GNU `size` BSS stays constant because it includes that heap.
- [x] Cold and warm valid playback each perform one bounded decode; invalid warm playback performs none.
- [x] Targeted macro, lifecycle, and feature-gate checks pass.
- [x] The full host suite passes.
- [x] The target QMK compile and ELF/map memory gate pass.
- [x] Documentation and Review 15 match the selected architecture.

## Next action

Preserve the 8 KiB named-storage, 26,000 B static-BSS, 200 KiB linker-heap,
one-byte metadata, active-immutability, and exact-capacity gates. Reconsider an
LRU only if measured real-world macro-trigger latency becomes noticeable.

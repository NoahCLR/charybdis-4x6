# Macro Slot Storage Architecture Review

Review 14 is closed as the immutable Finding 15 RGB optimization snapshot.
Finding 14 changes macro-runtime storage and asynchronous ownership, so this
work uses the next sortable review folder.

## Findings

### Optimize — Full compiled IR is reserved for every logical slot

The linked target stores 64 VIA and 16 hardcoded `macro_slot_cache_t` entries.
Each entry includes a 514-byte compiled IR even when the slot is unchecked,
empty, invalid, or never invoked. The current ELF attributes 33,152 B to
`via_macro_slots` and 8,288 B to `hardcoded_macro_slots`, for 41,440 B of
always-resident slot storage.

### Must preserve — The player borrows its IR for the whole asynchronous run

`macro_payload_start_ir()` stores the supplied IR pointer. The caller must keep
that object immutable until the finish callback runs. Any shared-buffer design
must therefore reject another slot-provider start before decoding over the
active bytes, and invalidation during playback must defer metadata reset until
completion.

## Prior Finding Status

| Prior finding | Status | Preservation requirement |
| --- | --- | --- |
| Finding 04: VIA macro byte validation | resolved | Invalid high bytes remain rejected and cached without playback side effects. |
| Finding 07: nonblocking macro playback | resolved | Scan-driven playback continues to borrow stable decoded IR until completion or cancellation. |
| Finding 08: synthetic-key ownership | resolved | Macro key actions continue through owned-keycode leases and cleanup. |
| Finding 14: macro-cache RAM | open | This review owns implementation and closure evidence. |
| Finding 15: RGB render work | resolved | Review 14 is closed and remains unchanged. |

## Selected Architecture

Use Candidate A from the finding plan:

- one fixed-width state byte per logical slot;
- one shared 514-byte IR for slot-provider compilation and active playback;
- one global active-metadata pointer and deferred-stale flag;
- invalid results remain cached until explicit invalidation;
- valid slots are re-decoded when playback begins;
- a second slot-provider request is rejected as busy before the shared IR can
  be changed;
- VIA invalidation during playback marks the active metadata stale while the
  decoded bytes remain immutable, then resets that metadata in the finish
  callback.

The single global macro engine already rejects concurrent playback. Sharing
one provider IR makes that constraint explicit at the storage seam and avoids
an LRU, provider/slot tags, generations, eviction, and another active copy.

## Resource Policy

- `macro_slot_metadata_t` must remain exactly one byte.
- Combined slot metadata, shared IR, and its ownership bookkeeping must remain
  below 8 KiB in the target symbol report.
- The target must reclaim at least 32 KiB from the measured 41,440-byte slot
  baseline.
- The RP2040 linker includes its remaining heap in ELF BSS, so GNU `size` BSS
  is expected to remain 245,592 B. The gate instead bounds the real static-BSS
  span and requires at least 200 KiB of linker heap; moving storage to an
  unnamed static buffer cannot pass.
- No heap allocation is permitted.

## Verification Strategy

- Unit-test stable active bytes across invalidation and attempted concurrent
  starts.
- Prove valid playback follows the selected re-decode policy, while invalid
  metadata suppresses repeated parsing until invalidation.
- Preserve empty, out-of-range, cancellation, VIA mutation, and reset behavior.
- Add a deterministic ELF budget checker for named macro storage and total BSS.
- Run targeted macro/lifecycle/feature gates, the complete host suite, a clean
  target compile, the memory budget, and the reviewed-path stack gate.

## Reconciliation Note

The findings above are the audit-time baseline. The current tree implements
the selected one-buffer architecture. Slot metadata no longer embeds IR,
active ownership is global and explicit, and the target memory gate measures
named storage, the static-BSS boundary, and the inverse linker-heap boundary.

## Closure Evidence

- Code authority: `macro_slot_provider.c` owns one shared 514-byte IR, the
  active metadata pointer, and deferred invalidation.
- Mechanical size contract: `macro_slot_metadata_t` is fixed-width and
  statically asserted to one byte.
- Behavior enforcement: provider, dispatch, payload, VIA lifecycle, defaults,
  action-lifecycle, and full-host tests cover re-decode, negative caching,
  exact 512-byte capacity, busy rejection, mutation, and active immutability.
- Target resource enforcement: `check_firmware_memory_budget.py` requires at
  most 8 KiB named macro storage, at least 32 KiB reclaimed, at most 26,000 B
  static BSS, and at least 200 KiB linker heap.
- Target result: 599 B named macro storage, 40,841 B reclaimed, 25,524 B static
  BSS, 212,608 B linker heap, and 151,024 B text.
- Fresh reviewed stack maxima remain 1,904/1,920 B main and 336/768 B split;
  macro preflight paths are 776 B hardcoded and 1,184 B VIA.

## Current Architecture Assessment

Finding 14 is **resolved**. Logical capacity now costs one byte per slot, one
validated decoded program is pinned for the only active execution, invalid
payloads remain negatively cached, and target RAM/stack budgets mechanically
enforce the intended result.

## Recommended Next Refactor Sequence

1. Preserve the memory, provider-lifetime, and exact-capacity fixtures.
2. Keep Finding 16 lookup optimization independent of macro ownership.
3. Reopen macro storage only if concurrent macro execution becomes a supported
   runtime contract.

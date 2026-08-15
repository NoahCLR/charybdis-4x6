# Macro Slot Storage Progress

## Why This Review Exists

Review 14 is closed and remains immutable. Finding 14 is a distinct macro RAM
and asynchronous-ownership topic, so it uses Review 15.

## 2026-08-15 — Audit Baseline

- Work began from clean branch `sol` after commit `6a3d1137`.
- The repository's `prompts/initial-architecture-review.md` template guided the
  architecture pass.
- The current ELF reports 151,000 B text, 0 B data, and 245,592 B BSS.
- `hardcoded_macro_slots` is 8,288 B and `via_macro_slots` is 33,152 B.
- Combined per-slot cache storage is therefore 41,440 B.
- `macro_payload_start_ir()` borrows the IR pointer for the full scan-driven
  run, so the selected one-buffer design must pin that buffer until finish.

## Selected Design

Candidate A: one-byte logical-slot metadata plus one shared, pinned compiled
IR. Invalid results stay cached. Valid programs are decoded on each playback.
Concurrent slot-provider requests fail busy before compilation can overwrite
the active IR. Invalidation of the active slot is applied after completion.

## Implementation Sequence

1. [x] Capture linked baseline and active-IR lifetime.
2. [x] Add metadata-size and shared-buffer ownership tests.
3. [x] Replace per-slot IR storage with compact metadata and one active IR.
4. [x] Add deterministic target memory-budget enforcement.
5. [x] Run targeted, full-host, target, resource, and stack verification.
6. [x] Reconcile Finding 14 documentation and record the closure verdict.

## Red Evidence

- Before production changes, `run_macro_slot_provider_tests.sh` failed its new
  one-byte metadata assertion: the host object was 520 B.
- The original target symbols totaled 41,440 B: 8,288 B hardcoded and 33,152 B
  VIA.

## Implementation

- Replaced per-slot compiled IR with fixed one-byte validation metadata.
- Added one shared 514-byte IR and explicit active metadata ownership.
- Valid slots re-decode once per invocation; invalid slots short-circuit until
  invalidation.
- Busy provider starts route through the engine's existing rejection path
  before any source read or shared-buffer write.
- Active VIA invalidation defers metadata reset until the finish callback while
  preserving the decoded bytes.
- Exact 512-byte IR acceptance and over-capacity rejection are covered.
- Added a fresh-ELF memory gate and host parser fixtures. Because the ChibiOS
  linker turns unused RAM into its `.heap` NOLOAD range, the gate measures
  static BSS and heap boundaries rather than treating GNU `size` BSS as live
  static allocation.

## Verification Passed

- `sh tests/host/run_macro_slot_provider_tests.sh`
- `sh tests/host/run_macro_dispatch_tests.sh`
- `sh tests/host/run_macro_payload_tests.sh`
- `sh tests/host/run_via_macro_defaults_tests.sh`
- `sh tests/host/run_via_macro_action_lifecycle_tests.sh`
- `sh tests/host/run_action_lifecycle_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_tool_tests.sh`
- `PYTHONPYCACHEPREFIX=/tmp/noah-host-pycache sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_tool_tests.sh`
- Fresh instrumented QMK build followed by
  `tools/check_firmware_stack_budget.py` against its ELF and map.
- `git diff --check`

## Verification Reconciliation

- The first instrumented runner attempt was sandbox-blocked from cleaning the
  sibling QMK artifact directory. The approved rerun produced fresh artifacts.
- That fresh checker correctly failed two stale manifest adjacencies. Both now
  include the linked `macro_payload_start_ir` frame; the same fresh ELF/map
  then passed every reviewed path.

## Target Evidence

- Named macro storage: 41,440 B → 599 B; 40,841 B reclaimed.
- Hardcoded metadata: 8,288 B → 16 B.
- VIA metadata: 33,152 B → 64 B.
- Shared IR plus ownership bookkeeping: 519 B.
- Static BSS span: 25,524 B; linker heap: 212,608 B.
- GNU `size`: 151,024 B text, 0 B data, 245,592 B BSS. The BSS figure includes
  the linker-reserved heap and therefore remains constant by design.
- Text delta from Finding 15: +24 B.
- Reviewed maxima: 1,904/1,920 B main and 336/768 B split.
- No sibling QMK source was edited.

## Closure Verdict

**CLOSED — Finding 14 resolved.** Compact metadata, active IR ownership,
negative caching, full-capacity decoding, documentation, complete host suite,
ordinary target build, target memory gate, and fresh reviewed-path stack gate
agree. This folder is immutable closure history after the Finding 14 commit.

## Next Steps

- Preserve these gates while implementing Finding 16.

# Live Profile Editing Progress

Thread: architecture and staged implementation of live authored-profile editing
from Charybdis Profile Studio.

Baseline at project opening: 87f356cd

Primary milestone: complete live RGB and key-behavior editing as defined in
README.md.

## Why This Folder Instead Of Review 19

review/2026-08-16-review-02 is an open full-firmware correctness audit. It owns
one must-fix, several should-fix findings, and their remediation lifecycle.

Live profile editing is a materially different architecture topic spanning a
new desktop-to-device protocol, persistent schema, runtime provider boundary,
safe activation, and new UI state. Per AGENTS.md, it opens
review/2026-08-24-review-01 rather than turning Review 19 into a second project.

Cross-thread prerequisites remain owned by Review 19. This folder records their
dependency status without duplicating or prematurely resolving its findings.

## Stage Status

| Stage | Status | Exit evidence |
| --- | --- | --- |
| 00 — Contract and baseline | in progress | measured baseline, frozen contracts, and executable golden fixtures landed; real-board probe remains |
| 01 — Live Link transport | initial real-board path confirmed; broader host matrix pending | native connection, reads, and serialized writes worked from Profile Studio |
| 02 — Schema, store, and commit | initial two-half persistent commit confirmed; recovery and production-policy evidence pending | an RGB profile committed and activated successfully through the D-022 barrier |
| 03 — Live RGB | engineering vertical slice hardware-confirmed; broader persistence matrix pending | Studio compiles and applies authored RGB successfully on the real two-half board |
| 04 — Live key behaviors | engineering vertical slice complete; hardware and timing evidence pending | Studio compiles supported authored behaviors and the gated provider publishes them through the runtime lookup seam |
| 05 — Milestone A integration | in progress; initial two-half live-edit path operational | RGB and standard layout apply are confirmed; custom behavior, recovery, and resource matrix remain |
| 06 — Defaults, layout, and macros | replanned around device-first sessions; standard-VIA layout apply hardware-confirmed; cadence remediation parked with R-21 open | base-key apply works; cadence cause unexplained and parked, live-owner memory policy failing, complete device readback, generation-bound drafts, peer/reboot/role matrix, macros, and defaults remain |
| 07 — Combos and layer structure | blocked on Stage 06 | open |
| 08 — Production closure | blocked on Stage 07 | open |

Only one stage should normally be in progress. Parallel work is allowed only
when the stage brief identifies independent work packages and file ownership
does not overlap.

## Completed Work

### 2026-08-24 — Project architecture and implementation control folder

Created:

- README.md with scope, non-goals, architecture summary, stage map, and the
  Milestone A definition
- userspace-architecture-review.md with findings, intended component
  boundaries, transaction model, testing requirements, and implementation
  sequence
- agent-workflow.md with required entry, contract, safety, verification, and
  handoff practices
- decisions.md with accepted principles and Stage 00 open decisions
- risks.md with cross-stage risks and closure evidence rules
- stages/ briefs for the complete implementation sequence

No firmware, Profile Studio, authored profile, generated docs, or sibling QMK
files were changed.

### 2026-08-25 — Stage 00 measured contract baseline

Completed:

- fresh full-host, firmware, memory, and reviewed-stack baseline against sibling
  QMK `sol` at `aac9f637ee`
- exact existing and accepted EEPROM maps
- fixed Profile Wire and storage ceilings
- full Profile Studio field classification
- Profile Wire v1 blob, action, RGB, behavior, transport, error, and fixture
  contracts
- source/device/split authority and safe-activation state tables
- accepted D-009 through D-016, including the corrected bank-aware resource
  model

Implementation packages opened without overlapping ownership:

- Review 19 malformed mirror-frame prerequisite and its sanitizer test
- Profile Studio injected adapter, fake device, and serialized request
  coordinator

No sibling QMK source file was edited. The sibling checkout was used for fresh
build artifacts only.

### 2026-08-25 — Stage 02 dual-slot storage foundation

Landed the bounded storage-only work package without advancing the overall
stage status:

- canonical 32-byte slot-header encoder/decoder with packed 4-bit schema
  components, CRC16 header protection, payload CRC32, and payload FNV-1a;
- injectable storage IO with read-only boot discovery;
- strict committed-slot compatibility, checksum, and top-level canonical blob
  validation;
- newest-valid-slot boot selection, last-known-good fallback, and explicit
  equal-generation conflict detection;
- sequential 32-byte-bounded inactive-slot staging, readback verification, and
  the two-byte commit marker as the final write;
- strict generation exhaustion at `UINT32_MAX`; storage format v1 does not
  wrap generation counters;
- focused normal and ASan/UBSan host coverage for interruption, corruption,
  conflict, ordering, compatibility, and rollover/exhaustion behavior.

The package intentionally does not add Raw HID mutation commands, runtime
activation, RGB/key-behavior decoding, reset, split reconciliation, or Profile
Studio changes. QMK scan-context ownership and its concrete EEPROM adapter
remain part of the later Stage 02 integration pass.

Verification passed for this package:

- `sh tests/host/run_profile_store_tests.sh` (normal and ASan/UBSan)
- `sh tests/host/run_profile_storage_layout_tests.sh`
- `sh tests/host/run_profile_wire_v1_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`
- `git diff --check`

The initial target checkpoint recorded a 48,396 B SRAM0–3 `.data + .bss`
regression metric, a 213,744 B linker/core-memory span at boot, a 1,880 B worst
reviewed main-process path, and a 336 B worst reviewed split-slave path. D-016
later corrected the resource terminology; these are policy/accounting values,
not total physical-RAM or runtime-free-memory claims.
At this storage-only checkpoint the functions were not yet reachable from a
runtime owner. The read-only runtime integration below supersedes these target
numbers. No sibling QMK source file was edited.

### 2026-08-25 — Stage 02 read-only QMK store discovery

Made the persistent-store foundation reachable on firmware without opening a
mutation path:

- added a slot-bounded QMK EEPROM adapter whose store IO deliberately has no
  write callback;
- added a two-phase boot owner that initializes after VIA defaults and performs
  exactly one dual-slot discovery from matrix-scan context;
- surfaced a discovered committed digest, generation, origin, and
  equal-generation conflict count through the existing Profile Wire status
  pages while continuing to report compiled defaults as the active runtime;
- retained truthful capabilities: candidate chunk size and supported-domain
  mask remain zero, and no candidate-write, commit, RGB-schema, or
  behavior-schema feature bit is advertised;
- strengthened equal-generation identity to include schema, flags, length,
  CRC32, FNV digest, compiled-default digest, and action-ABI digest in addition
  to generation and origin;
- extended the last-known-good power-loss matrix across the real six-write
  15-byte transaction, including every partial final-marker write;
- wired runtime init order, the common source manifest, VIA on/off plus
  RGB/split compile variants, and an explicit reviewed stack path for boot
  discovery.

Verification passed for the combined tree:

- `sh tests/host/run_profile_store_tests.sh` (normal and ASan/UBSan)
- `sh tests/host/run_profile_store_runtime_tests.sh`
- `sh tests/host/run_profile_wire_v1_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`

Fresh linked resources are 25,660 B SRAM0–3 BSS, 22,980 B SRAM0–3 data, a
48,640 B `.data + .bss` regression metric against the 51,000 B policy, and a
213,496 B linker/core-memory span at boot. The new boot-discovery reviewed path
is 480 B; the existing worst reviewed paths remain 1,880/1,920 B for the main
process and 336/768 B for the split slave. The ratios describe policy margins,
not physical stack or total-RAM headroom.

This package discovers and reports persistent metadata only. It does not read
a committed profile into an effective provider, activate it, expose candidate
writes or commits, reset slots, or reconcile the peer half. Hardware EEPROM
discovery is not confirmed. No sibling QMK source file was edited; the sibling
checkout was used only for build artifacts.

### 2026-08-25 — Stage 02 generic blob and semantic-action codecs

Landed the reusable schema-only work package without advancing the overall
stage status:

- firmware encoding and strict zero-copy decoding for the canonical eight-byte
  `NLP1` header and ordered four-byte domain envelopes;
- firmware field-by-field encoding and decoding for every Profile Wire v1
  semantic action kind, with caller-supplied layer, PD-mode, VIA-macro, and
  hardcoded-macro capacity counts;
- stable generic rejection categories and offsets for truncated, overlong,
  unknown, duplicate, out-of-order, reserved, noncanonical, trailing, and
  invalid-action input;
- a single exact-byte blob/action fixture consumed directly by both firmware C
  tests and the Profile Studio JavaScript tests;
- normal and ASan/UBSan coverage, including every truncated golden prefix, all
  unknown action tags and reserved action flags, boundary capacities, and a
  deterministic malformed-input corpus;
- userspace source-manifest and full-host runner wiring plus compile-time
  alignment with the accepted 4,064-byte slot, eight-layer, and 16-slot
  hardcoded-macro ceilings.

This generic layer deliberately does not interpret RGB or key-behavior payload
records. It also does not validate the negotiated action-ABI digest or logical
cross-references, mutate the store, activate a candidate, or synchronize the
peer half. Those are separate Stage 02 validator/runtime packages.

Verification passed:

- `sh tests/host/run_profile_blob_v1_tests.sh` (normal and ASan/UBSan)
- `npm run check` from `tools/charybdis-profile-studio/` (53 tests)
- `sh tests/host/run_feature_gate_compile_tests.sh`

Combined-tree closure gates also passed after source-manifest wiring:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh` — 48,640 B SRAM0–3
  `.data + .bss` regression metric, 213,496 B linker/core-memory span at boot
- `sh tests/host/run_firmware_stack_budget_checks.sh` — 1,880 B main and
  336 B split worst reviewed paths
- `git diff --check`

### 2026-08-25 — Stage 02/04 reader-backed key-behavior domain codec

Landed the cross-language behavior payload package without advancing Stage 02
or the Stage 04 vertical slice:

- exact firmware encoding and reader-backed decoding for the frozen row,
  sparse-step, branch, timing, flags, hold-mode, repeat-rate, and semantic-action
  byte contract;
- canonical encoder sorting plus strict decoder rejection for duplicate or
  out-of-order targets and tap indexes;
- fail-closed negotiated behavior ceilings that may lower but never raise the
  frozen v1 maxima of 64 rows, 128 populated steps, five steps per row, 100 Hz,
  and 4,052 behavior-payload bytes;
- one shared payload/envelope/blob/digest fixture executed by firmware C and
  Profile Studio JavaScript;
- a bounded reader abstraction with arbitrary base offsets, injected read
  failures, at-most-12-byte reads, and a payload-independent validated handle
  statically capped at 128 bytes;
- row and step accessors that re-resolve row indexes from the validated domain,
  avoiding caller-controlled stored offsets.

The package does not allocate or retain a contiguous 4 KiB candidate, and it
does not materialize maximum native row/step arrays. The remaining behavior
seam is whole-profile action-ABI/cross-reference validation followed by a
generation-owned runtime action/handled-key view and quiescent publication.

Focused verification passed:

- `sh tests/host/run_key_behavior_domain_v1_tests.sh` (normal and ASan/UBSan)
- `npm run check` from `tools/charybdis-profile-studio/` (63 tests)
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh` (combined tree, including the generic,
  behavior, RGB, store, wire, and candidate profile packages)
- `git diff --check`

The QMK compile and firmware memory/stack gates were intentionally not rerun by
this slice because the candidate-coordinator agent owns the sequential
combined-tree firmware/resource build. This avoids concurrent `.build`
artifact races; its result belongs in the coordinator's verification entry.

### 2026-08-25 — Stage 02/03 matching RGB domain codec foundation

Landed matching desktop and reader-backed firmware schema packages early
without advancing Stage 03's blocked status:

- froze domain `0x10` version 1 as a 16-byte header plus fixed-order records for
  all Milestone A RGB surfaces;
- encoded the five-stage enable mask, complete layer and stable PD color
  identities, auto-mouse fade, combo feedback, complete ordered tap-branch and
  key-feedback colors, tap policy, locality, and every renderer group table;
- canonicalized named and inline groups into at most 16 unique 58-bit bitmaps,
  with zero reserved LED bits, lexicographic profile-local ids, validated
  references, and authored repaint order retained;
- enforced eight layers, six registered PD ids, four compiled tap colors, 32
  aggregate group rows, compiled feature inclusion, maximum brightness, exact
  geometry, canonical ordering, and strict reserved/truncation/trailing rules;
- kept the normalized domain and parsed Studio model in a semantic JSON fixture
  while moving codec limits, exact payload and whole-blob bytes, FNV-1a, and
  CRC32 into one shared C/JavaScript fixture;
- composed the RGB domain through the generic desktop profile-blob codec and
  added focused canonicalization, corruption, completeness, enum/reference,
  feature, and maximum-capacity tests;
- added a firmware decoder whose retained view contains only the injected
  bounded reader, header counts, and negotiated limits, with a 40-byte 32-bit
  static ceiling and no payload/dictionary/table materialization;
- exposed record-at-a-time accessors for every RGB section, with one 16-byte
  header read during decode and all later validation/materialization reads
  bounded to at most 11 bytes;
- wired the firmware decoder into the canonical userspace source manifest,
  feature-gate compile matrix, and full host suite.

Verification passed:

- `sh tests/host/run_profile_rgb_v1_tests.sh` — normal and ASan/UBSan
- `npm run check` from `tools/charybdis-profile-studio/` — all 63 live-link
  tests passed
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `git diff --check`

This slice deliberately does not add an extension command, webview or UI
change, device write, effective RGB provider, preview/rollback, runtime
activation, persistence, or split reconciliation. QMK/resource gates remain
for the final combined firmware pass because other firmware packages were
still active. The next RGB package is compiled-default
materialization and a generation-owned effective provider before consumer
migration.

### 2026-08-25 — Stage 02 standalone candidate transaction coordinator

Landed the bounded candidate mutation foundation without enabling device
writes:

- froze exact 32-byte begin, chunk, validate, abort, immediate admission, and
  operation-status layouts, including transaction width, canonical padding,
  stable errors, structured validation locations, and an explicit no-timeout
  rule;
- added a strict frame/status codec and exact-byte fixture with all-length and
  reserved-byte rejection coverage;
- added one bounded mailbox whose receive side only validates/copies and whose
  scan owner alone performs injected backend begin/write/read/abort and
  validation; the later incremental-validator checkpoint supersedes this
  package's initial checksum-only work bound;
- enforced nonzero transaction ids, schema/domain/action-ABI/capacity checks,
  sequential chunks, staged-byte comparison for idempotent duplicates,
  poisoning on conflicting retries, stable status sequencing, and idempotent
  successful/no-op abort;
- added reset/power-loss ownership tests that prove pending/staged volatile
  state is not resumed after owner initialization and an explicit long-idle
  test for the v1 no-autonomous-timeout contract;
- wired both firmware sources into the common source manifest, the VIA on/off
  feature compile matrix, and the full host runner.

Focused verification passed:

- `sh tests/host/run_profile_candidate_transaction_tests.sh` (normal and
  ASan/UBSan)
- `sh tests/host/run_profile_wire_v1_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`

The coordinator is deliberately disconnected from the QMK hook and no
candidate, commit, activation, preview, or domain capability was enabled by
this package. It has no commit API and no profile-sized RAM buffer. The next
integration seam is an EEPROM/store staging backend plus QMK callback/scan
routing, but that must wait for both domain validators and must land together
with truthful candidate capability/status advertising. The combined QMK build
passed; resource gates remain at a 48,640/51,000 B SRAM0–3 `.data + .bss`
policy metric, a 213,496 B linker/core-memory span at boot, and 1,880 B
main-stack and 336 B split-stack worst reviewed paths. Final
`git diff --check` passed. The shared combined-tree full-host run also passed
after all candidate, RGB, and behavior source wiring settled.

### 2026-08-25 — RP2040 resource-truth reconciliation

Accepted D-016 is now the durable resource-accounting contract for this
project. The repository no longer presents the 51,000 B `.data + .bss` limit
or the 204,800 B minimum linker-managed SRAM0 span as RP2040 hardware
capacities. They remain conservative regression policies and are reported as
such.

This pass:

- added the canonical human-facing memory model in
  `docs/architecture/memory-budgets.md` and linked it from the architecture
  guide, change guide, and source map;
- added the same mandatory resource-truth rules to `AGENTS.md`, so future
  agents begin from the physical bank model and do not infer runtime
  high-water from linker output;
- upgraded `tools/check_firmware_memory_budget.py` from threshold-only parsing
  to exact RP2040 bank, boundary, overlap, and conservation checks while
  retaining the previous CLI names only as documented compatibility aliases;
- expanded the memory-tool test suite to cover exact accounting, both policy
  failures, compatibility aliases, and invalid bank layouts;
- reconciled the active review's baseline, architecture, risks, stages, and
  decision log with that contract; and
- corrected the pointing-runtime stack comment so the 40 B value cannot be
  confused with physical stack headroom.

The current linked checkpoint per half is 270,336 B of physical RP2040 SRAM,
48,640 B for the `.data + .bss` regression metric, 48,648 B for the fixed
SRAM0 prefix, 213,496 B for the linker-managed SRAM0 free/core-memory span at
boot, and 56,104 B of exact fixed linked occupancy across all banks. SRAM4 has
224 B unassigned after its fixed reservations. The worst reviewed main path is
1,880 B: 40 B below its 1,920 B policy threshold and 680 B below the 2,560 B
physical process-stack boundary. These are link-time and reviewed-path facts;
runtime allocator and global/interrupt stack high-water remain unmeasured.

Verification passed:

- `python3 -m py_compile tools/check_firmware_memory_budget.py`
- `sh tests/host/run_firmware_memory_budget_tool_tests.sh` (14 tests)
- `sh tests/host/run_firmware_memory_budget_checks.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `git diff --check`

No sibling QMK source was changed; the sibling tree was used only for the
required build and linker evidence. A real-board runtime high-water probe is a
future measurement, not a prerequisite for using the now-correct static
accounting.

## Verification

Passed on 2026-08-24:

- `git status --short` — the task began with a clean worktree; the final status
  contains only this new review folder
- `git diff --check`
- `rg -n "[[:blank:]]+$" review/2026-08-24-review-01` — no matches
- local-link existence check for every target linked from this folder's
  `README.md`

Runtime host tests and firmware compile are intentionally skipped under the
AGENTS.md documentation-only exception because this pass changes review notes
only.

Hardware is not confirmed.

### 2026-08-25 — Stage 02 whole-profile validation and disconnected staging

Landed the next bounded Stage 02 package without making device mutation
reachable:

- added a reader-backed whole-profile validator that incrementally streams
  CRC32/FNV-1a, validates the canonical `NLP1` envelope and both domain codecs,
  enforces declared/required/allowed domains plus the action-ABI digest, and
  resolves behavior actions against exact compiled layer, PD-mode, VIA-macro,
  and hardcoded-macro identities;
- added a candidate-to-store backend that stages sequential chunks in the
  inactive EEPROM slot and exposes validation through borrowed payload-
  independent views; marker-last commit remains a separate explicit API that
  the candidate coordinator cannot call, and copied validated views retain an
  immutable absolute slot base across later candidate staging;
- added a slot-bounded writable QMK EEPROM adapter while preserving the
  read-only adapter used by boot discovery;
- added exact desktop candidate codecs and a conservative upload coordinator
  with sequential 20-byte chunks, operation-sequence polling, safe busy retry,
  non-idle preflight refusal, input snapshotting, deterministic cleanup abort,
  and no retry after ambiguous transport outcomes; and
- wired the new firmware sources, focused host runners, and Studio checks into
  their aggregate gates.

Focused verification passed:

- `sh tests/host/run_profile_validator_v1_tests.sh` (normal and ASan/UBSan)
- `sh tests/host/run_profile_candidate_store_backend_tests.sh` (normal and
  ASan/UBSan)
- `sh tests/host/run_profile_store_runtime_tests.sh`
- `npm run check` in `tools/charybdis-profile-studio/` (81 tests)
- `npm run screenshots` in `tools/charybdis-profile-studio/` (all four
  baselines regenerated byte-identically)
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh` (48,640 B SRAM0–3
  `.data + .bss` policy metric; 213,496 B linker-managed free/core-memory span
  at boot; 56,104 B exact fixed linked occupancy)
- `sh tests/host/run_firmware_stack_budget_checks.sh` (1,880 B main and 336 B
  split-slave worst reviewed paths)
- `git diff --check`

At this checkpoint, production QMK routing, candidate/domain capability
advertising, installed invalidators, and split convergence remained
deliberately absent. The safe predicate landed afterward but was still
uninstalled at this point in the history; the later D-021 checkpoint supersedes
that implementation state. The isolated candidate scan owner then composed
bounded marker-last commit and
provider activation behind custom-save `0x13`; it is not yet reachable from
QMK. The
compiled-default materializer and the isolated effective-profile provider
foundation have now landed. The provider uses the established publication
primitive, fails closed during invalidation, guards safe-boundary reentrancy,
requires a safe predicate for behavior activation, bounds nested views to the
checksummed blob, and exposes an explicit active-backing-aware storage-reuse
reservation. The store/backend seam now uses that reservation before every
target-slot mutation, releases it on all terminal paths, and can hand a durable
validated record to provider-owned pending activation. It remains disconnected
from QMK routing and runtime consumers.
The executable work-budget gate now accepts the incremental whole-profile
validator: the schema-maximal 3,216-byte behavior profile completes in 1,061
whole-profile steps, including 897 behavior-domain steps with at most one
12-byte read each and zero reference rescans. The maximal RGB domain completes
in 58 steps with at most one 16-byte read each. Every step is mechanically
capped at one reader operation and 20 newly observed bytes. Cortex-M0+ compile
coverage enforces the 348-byte whole-validator state under its explicit
352-byte regression policy. This closes the unbounded domain-work blocker;
production routing still awaits installation of the landed safe predicate,
invalidators, and the runtime QMK owner; real hardware timing remains
unconfirmed.

Stage 00 baseline commands passed on 2026-08-25:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `sh tests/host/run_firmware_memory_budget_checks.sh`
- `sh tests/host/run_firmware_stack_budget_checks.sh`

The initial Stage 00 target evidence was a 48,396 B SRAM0–3 `.data + .bss`
metric against a 51,000 B policy and a 213,744 B linker/core-memory span at
boot. The then-current reconciled tree measured 48,664 B and 213,472 B
respectively.
The 1,880 B worst reviewed main path has 40 B to the 1,920 B reviewed-path
policy and 680 B to its 2,560 B physical process-stack boundary; the 336 B
worst reviewed split-slave path is measured against its 768 B reviewed-path
policy. These gates cover named paths, not global or interrupt high-water.

## Open Decisions

D-009 through D-016 are accepted. The decisions no longer block schema,
storage-contract, or transport-boundary implementation.

The `node-hid` choice remains subject to its recorded escape hatch: if the
actual VS Code-host packaging/board spike fails, D-012 must be amended to select
the packaged helper without changing the injected adapter contract.

## Current Blockers

- The real-board HID/VS Code-host spike has not run.
- RGB and key-behavior domain codecs plus the real compiled-default
  materializer now match across firmware and Studio. The provider lifecycle and
  its destructive-store reservation are hardened and tested; the production
  safe predicate and first callback-only behavior consumer/invalidator are
  implemented, while provider-owner installation, RGB invalidation, QMK commit
  routing, split convergence, and remaining consumer migration remain open.
  Whole-profile action-ABI and compiled-
  reference validation has landed.
- The baseline persistence and role-swap hardware matrix remains open.

These are expected opening conditions, not project failure.

## Next Steps

1. Install the landed production safe-boundary predicate with the first
   invalidators before connecting the validated staging path to QMK routing,
   then measure scan timing on the real RP2040 before advertising mutation
   capabilities.
2. Add split reconciliation before advertising durable mutation capabilities.
3. Build the effective RGB accessors, then migrate the eight renderer families
   without allowing direct compiled-table bypasses.
4. Build the effective key-behavior accessor and migrate lookup after the safe
   interaction boundary is mechanically enforced.
5. Run the real-board transport and persistence matrix when hardware is
   available.

## Handoff History

Append new entries here in chronological order. Each entry must name:

- stage and work package
- files and contracts changed
- exact verification
- skipped or unconfirmed checks
- risks and decisions changed
- next work package

### 2026-08-25 — Stage 02 compiled defaults and provider checkpoint

- Added a payload-independent compiled-default materializer and a shared exact
  C/JavaScript fixture for the real 1,089-byte RGB-plus-behavior profile.
- Added the first executable validator work-budget gate. At that checkpoint it
  exposed unbounded whole-domain work; the incremental-validator checkpoint
  below supersedes those audit-time measurements.
- Added a disconnected generation-owned provider with coherent bounded reads,
  safe-boundary and invalidation guards, copied reader-backed snapshots,
  rollback/fallback, and explicit active-backing-aware reuse reservations.
- Kept QMK mutation routing, commit/activation ownership, capabilities, split
  convergence, and runtime consumer migration disabled.
- Focused normal and ASan/UBSan materializer, work-budget, and provider tests
  passed; the provider and materializer also passed Cortex-M0+ / feature-matrix
  compile coverage, and the compiled fixture passed its JavaScript test.
- Final combined verification passed: `run_feature_gate_compile_tests.sh`,
  `run_all_host_tests.sh`, Profile Studio `npm run check` (82 tests), a clean
  QMK firmware compile, the RP2040 memory policy gate (48,640 B `.data + .bss`,
  213,496 B boot core-memory span, 56,104 B fixed bank occupancy), the reviewed
  stack gate (1,880 B main / 336 B split-slave), and `git diff --check`.
- The next work package at that checkpoint was incremental bounded validation;
  it has since landed, so storage/provider integration is now next.

### 2026-08-25 — Stage 02 incremental validation checkpoint

- Replaced one-shot whole-domain validation with caller-owned incremental RGB
  and key-behavior state machines. Begin performs no I/O; every step performs
  zero or one reader call and observes at most 20 new bytes.
- Behavior action-reference events preserve structural-error precedence and
  target/tap/hold/long-hold ordering without rereading rows or steps.
- The schema-maximal 3,216-byte behavior profile completes in 1,061
  whole-profile steps: 897 behavior-domain steps, one read / 12 bytes maximum
  per step, and zero reference rescans. Maximal RGB completes in 58 domain
  steps at one read / 16 bytes maximum.
- The 32-bit behavior validator is 120 bytes, RGB validator is 68 bytes, and
  whole-profile validator is 348 bytes. Their 120-, 72-, and 352-byte ceilings
  are regression policies, not RP2040 hardware capacities.
- Focused normal and ASan/UBSan verification passed through
  `run_key_behavior_domain_v1_tests.sh`, `run_profile_rgb_v1_tests.sh`,
  `run_profile_validator_v1_tests.sh`, and
  `run_profile_validator_work_budget_tests.sh`; the domain and whole-validator
  runners also passed Cortex-M0+ compilation.
- Combined verification passed through
  `run_profile_candidate_store_backend_tests.sh`,
  `run_profile_store_runtime_tests.sh`,
  `run_profile_candidate_transaction_tests.sh`,
  `run_feature_gate_compile_tests.sh`, `run_all_host_tests.sh`, the QMK firmware
  compile, both firmware resource gates, and `git diff --check`. The linked
  target remains at 48,640 B for the SRAM0–3 `.data + .bss` policy metric,
  213,496 B for the boot core-memory span, and 56,104 B fixed linked occupancy;
  reviewed paths remain 1,880 B main and 336 B split-slave.
- Production QMK mutation routing and capability advertising remain disabled.
  At that checkpoint the next package was storage/provider commit and activation
  ownership; the store/provider seam has since landed. Real-board scan timing
  remains a hardware acceptance check. No Profile
  Studio UI changed, so screenshots were intentionally not regenerated. The
  QMK build/resource gates wrote generated sibling build artifacts only; no
  sibling source file was edited.

### 2026-08-25 — Stage 02 active-backing store integration checkpoint

- Added an injected store reuse guard that runs after candidate compatibility
  checks but before the target marker is invalidated. Every admitted prepare is
  paired with release on abort, durable commit, checksum rejection, and I/O or
  readback failure; release failure leaves the store fail-closed.
- The candidate backend installs a provider-backed guard for the target slot's
  complete 4,064-byte payload range. Active and pending backing cannot be
  overwritten, while an overlapping rollback is discarded only during an
  admitted reservation.
- Durable commit records are cross-checked against the validated view before
  the backend requests provider activation. Publication remains a separate
  safe-boundary poll and is not silently performed by storage code.
- Focused normal and ASan/UBSan store/backend tests cover reservation ordering,
  denied writes before marker mutation, all major release paths, exact durable
  identity checks, activation handoff, and runtime rollback diverging from the
  store's nominal inactive slot.
- Verification passed: `sh tests/host/run_profile_store_tests.sh`,
  `sh tests/host/run_profile_candidate_store_backend_tests.sh`,
  `sh tests/host/run_effective_profile_provider_tests.sh`,
  `sh tests/host/run_profile_candidate_transaction_tests.sh`,
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`, and
  `sh tests/host/run_firmware_stack_budget_checks.sh`.
- Fresh RP2040 accounting reports 270,336 B physical SRAM per MCU, 48,648 B
  for the SRAM0-3 `.data + .bss` policy metric, 213,488 B for the boot
  core-memory span, and 56,112 B fixed linked occupancy. Reviewed stack paths
  remain 1,880 B main and 336 B split-slave.
- QMK routing, the `0x13` commit frame, production safe predicate/invalidators,
  capabilities, split convergence, and runtime consumers remain disabled. The
  next package is the commit/activation scan owner. No Profile Studio UI changed,
  so screenshots were intentionally not regenerated. The QMK build/resource
  gates wrote generated sibling build artifacts only; no sibling source file
  was edited.

### 2026-08-25 — Stage 02 bounded durable commit and activation checkpoint

- Replaced the cold marker-last store commit with explicit begin/step phases.
  Begin performs no EEPROM I/O; every step performs exactly one read or write
  of at most 20 bytes through header write/readback, payload checksum readback,
  canonical top-level shape verification, and final-marker write/readback.
- Distinguished an unconfirmed completing marker write or marker readback from
  a safely failed write as `durability unknown`. Power-loss tests still select
  the old generation, new generation, or no committed profile according to the
  exact byte boundary; the prior committed slot is never destroyed first.
- Added custom-save `0x09` value `0x13` as the exact durable-commit request,
  plus COMMITTING/ACTIVATING states, commit operation status, activation-failed
  and durability-unknown errors, and shared C/JavaScript golden frames.
- The isolated scan owner now advances bounded persistence, preserves
  transaction/digest identity for idempotent retries, requests provider
  activation only after confirmed durability, waits for the safe boundary, and
  retries a transiently blocked activation request before polling it.
- Profile Studio now has a separate prepared-candidate commit coordinator. It
  waits across commit and activation, recovers a lost commit acknowledgement
  from exact status, refuses digest mismatch before mutation, and treats
  unknown marker durability as an unsafe-to-retry reconciliation case. Source
  Apply remains unchanged, preserving the prepare/source/commit ordering in
  D-013 for later UI integration.
- Verification passed: `sh tests/host/run_profile_store_tests.sh`,
  `sh tests/host/run_profile_candidate_transaction_tests.sh`,
  `sh tests/host/run_profile_candidate_store_backend_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`, focused Node candidate
  tests (24 tests), Profile Studio `npm run check` (88 tests),
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, and `git diff --check`.
- Fresh RP2040 accounting reports 270,336 B physical SRAM per MCU, 25,684 B
  SRAM0–3 `.bss`, 48,664 B for the `.data + .bss` regression metric,
  213,472 B for the boot core-memory span, and 56,128 B fixed linked occupancy.
  Reviewed stack paths remain 1,880 B main and 336 B split-slave.
- Production QMK mutation routing and capability advertising remain disabled.
  The next package is the production safe-boundary predicate and first domain
  invalidators, followed by QMK owner wiring and real-board scan timing. Split
  convergence and runtime key/RGB consumer migration remain open. No Profile
  Studio UI changed, so screenshots were intentionally not regenerated. The
  QMK build/resource gates wrote generated sibling build artifacts only; no
  sibling source file was edited.

### 2026-08-25 — Stage 02 production activation-predicate checkpoint

- Added the production safe-boundary predicate with frozen reason bits and an
  exact count snapshot for physical presses, tap series, runtime leases,
  deferred releases, persistent intents, managed HID outputs, modifiers and
  one-shots, macro work, combo-origin work, and peer convergence.
- The key runtime exposes its five authoritative aggregate counts through a
  narrow public snapshot. Owned keycodes now maintain an O(1) aggregate active
  usage count, including managed mouse buttons, instead of requiring an HID
  usage-table scan while activation waits.
- Peer observation fails closed when absent or unreadable. Predicate status is
  published through the existing bounded generation primitive, so USB/status
  readers either receive one complete observation or retain their prior copy.
- Focused verification passed through the activation-policy runner (normal,
  ASan/UBSan, QMK one-shot-disabled, and Cortex-M0+), owned-keycode tests,
  runtime-debug tests, the feature-gate compile matrix, the full host suite,
  the production QMK build, and both firmware resource gates.
- Fresh RP2040 accounting reports 270,336 B physical SRAM per MCU, 25,684 B
  SRAM0–3 `.bss`, 48,664 B for the `.data + .bss` regression metric,
  213,472 B for the boot core-memory span, and 56,128 B fixed linked
  occupancy. Reviewed stack paths remain 1,880 B main and 336 B split-slave.
- The predicate is compiled into production firmware but is not installed into
  a production provider owner. QMK mutation routing and capability advertising
  remain disabled. Domain invalidators, split convergence, key/RGB consumer
  migration, and real-board timing remain next. No Profile Studio UI changed,
  so screenshots were intentionally not regenerated. The QMK build/resource
  gates wrote generated sibling build artifacts only; no sibling source file
  was edited.

Next steps:

1. Add effective RGB/behavior invalidator owners that can consume the
   provider's callback-only generation view without replaying the compiled
   virtual blob on hot paths.
2. Install the provider and predicate through the scan owner only after those
   invalidators exist; retain fail-closed peer status until split convergence
   is implemented.
3. Repeat full host, firmware, and fresh resource gates after owner/consumer
   integration, then run the hardware timing gate before enabling any mutation
   capability.

### 2026-08-26 — Stage 04 effective behavior-consumer checkpoint

- Centralized Profile Wire v1 semantic-action translation in a bidirectional
  runtime adapter. The compiled-default materializer now uses the same native
  mapping as live behavior materialization, while its canonical 1,089-byte
  fixture and action-ABI digest remain unchanged.
- Added ordered target lookup and exact-row step access to the validated
  behavior-domain view. Exact-row access re-resolves and compares caller row
  metadata before reading steps, preserving the reader as the bounds authority.
- Added a caller-owned, double-banked effective behavior view. Its provider
  invalidator copies only validated domain metadata and swaps one local epoch;
  it performs no payload traversal during provider publication.
- Migrated production key-behavior lookup behind the optional effective seam.
  An installed live domain fully replaces compiled rows, supports add/change/
  remove semantics, converts every Milestone A action and hold mode, and rejects
  step tokens from an older epoch. With no installed owner, or for RGB-only and
  compiled generations, the existing direct authored tables remain exact.
- Focused verification passed: `sh tests/host/run_key_behavior_domain_v1_tests.sh`,
  `sh tests/host/run_key_behavior_lookup_tests.sh` (normal, ASan/UBSan, and
  Cortex-M0+ source compiles), `sh tests/host/run_profile_compiled_defaults_v1_tests.sh`,
  behavior/keymap/real-profile validation, all required key-runtime scenario,
  release, modifier, PD-mode, layer-lock and integration runners, action/
  ownership/macro runners, and `sh tests/host/run_feature_gate_compile_tests.sh`.
- The state is compiled but no production instance is allocated or installed;
  QMK mutation routing and capability advertising remain disabled. The current
  schema-bounded ordered lookup intentionally precedes a real-board timing
  decision about a compact index.
- Final checkpoint verification passed: `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, and
  `git diff --check`. The fresh ordinary image remains at 25,684 B SRAM0–3
  `.bss`, 22,980 B `.data`, 48,664/51,000 B `.data + .bss` policy span,
  213,472 B SRAM0–3 linker/core-memory span at boot, and 56,128 B fixed linked
  occupancy across the unique banks. Those are per-half linked facts and policy
  metrics, not total-RAM or runtime high-water claims. Reviewed paths remain
  1,880/1,920 B on the main process stack and 336/768 B on the split-slave
  stack; that gate covers only its named paths.
- Profile Studio UI behavior did not change, so screenshots were intentionally
  not regenerated. The QMK build and resource gates wrote generated artifacts
  in the sibling firmware build tree only; no sibling source was edited.
- The remaining gates for this stage are the RGB invalidator, production owner,
  split convergence, and real-hardware timing before capability advertising.

Next steps:

1. Add the callback-only effective RGB view and frame-generation capture before
   installing either consumer into the production scan owner.
2. Install provider, predicate, and both consumer states as one owner, retaining
   fail-closed peer status until profile-generation split convergence exists.
3. Measure worst-case behavior lookup and scan activation timing on the real
   split keyboard before choosing an index or advertising mutation capability.

### 2026-08-26 — Effective RGB view and frame-token checkpoint

- Added a caller-owned, double-banked effective RGB view. Its provider
  invalidator copies only the validated reader-backed RGB metadata and active
  identity; instrumentation confirms it performs no payload reads or derived
  cache construction during provider publication.
- Added a captured frame token and explicit status check. The token is a copied
  view rather than a pointer into a runtime bank, and any later publication
  changes its local epoch so access fails stale instead of mixing generations.
- Compiled identities and validated behavior-only profiles explicitly retain
  the direct authored RGB fallback. The runtime remains uninstalled, no RGB
  stage reads it yet, and production rendering is therefore unchanged.
- Focused verification passed: `sh tests/host/run_effective_rgb_runtime_tests.sh`
  (normal, ASan/UBSan, and Cortex-M0+ source compile),
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_effective_profile_provider_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`, both firmware resource
  gates, and `git diff --check`.
- Because the runtime remains uninstalled, the fresh ordinary image is
  unchanged at 25,684 B SRAM0–3 `.bss`, 22,980 B `.data`, 48,664/51,000 B
  `.data + .bss` policy span, 213,472 B SRAM0–3 linker/core-memory span at
  boot, and 56,128 B fixed linked occupancy across the unique banks. Those are
  per-half linked facts and policy metrics, not total-RAM or runtime high-water
  claims. Reviewed paths remain 1,880/1,920 B on the main process stack and
  336/768 B on the split-slave stack; that gate covers only its named paths.
- Profile Studio UI and authored profile data did not change, so screenshots
  and introspection regeneration were intentionally skipped. No sibling source
  was edited; QMK/resource checks wrote generated sibling build artifacts only.

Next steps:

1. Capture one effective RGB token at the renderer's frame boundary and migrate
   layer colors/render modes as the first table family, retaining exact compiled
   fallback behavior.
2. Migrate the remaining seven families one at a time with parity tests and a
   direct-array source gate, then install RGB and behavior consumers through
   the single production owner.
3. Add split profile-generation convergence and real-hardware render/lookup
   timing before enabling mutation capabilities.

### 2026-08-27 — Effective layer-RGB consumer checkpoint

- Added `rgb_effective_config.c` as the single compiled/live adapter for layer
  colors, render modes, reusable group bitmaps, and layer-group rows. Direct
  compiled table access for this family is now centralized there.
- The RGB orchestrator captures one effective frame token at the first QMK LED
  chunk and threads it through normal layer rendering, auto-mouse destination
  rendering, and layer preview. A later profile publication makes the token
  stale; the layer compositor clears its local frame and fails closed rather
  than reading across generations.
- Removed the cached converted layer-color array. Colors, modes, and solid-color
  flags are now render-local, while the mapped-key LED map is independent of
  profile generation and therefore remains safe across live render-mode changes.
- Extended the RGB render fixture so compiled and live colors and modes differ.
  It proves reader-backed colors, mapped-only rendering, canonical reusable
  group bitmaps, exact compiled fallback parity, and stale-frame refusal. The
  runner also rejects direct authored layer-table reads outside the adapter.
- Focused verification passed:
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_effective_rgb_runtime_tests.sh`,
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a restored ordinary QMK
  compile plus memory check after stack instrumentation, and `git diff --check`.
- Fresh linked resource facts are per RP2040 half: 270,336 B physical SRAM;
  SRAM0–3 `.bss` is 25,756/26,000 B under the regression policy, `.data` is
  22,984 B, and `.data + .bss` is 48,740/51,000 B. The SRAM0–3 linker/core-
  memory span at boot is 213,400 B against the 204,800 B minimum, and fixed
  linked occupancy across unique SRAM banks is 56,200 B. These are link-time
  facts and policy metrics, not total-RAM headroom or runtime high-water
  measurements.
- The stack gate remains 1,880/1,920 B for its worst named main-process path
  and 336/768 B for its worst named split-slave path. It covers only the paths
  in the stack manifest and is not a global or interrupt-stack maximum.
- The effective RGB runtime owner is still not installed, so the shipped image
  retains exact compiled RGB behavior. Profile Studio UI and authored profile
  inputs did not change; screenshots and introspection regeneration were
  intentionally skipped. QMK/resource checks wrote generated artifacts in the
  sibling firmware build tree only; no sibling source was edited.

Next steps:

1. Migrate pointing-mode colors, locality, and pointing-mode group rows through
   the same captured effective frame and add the matching direct-read gate.
2. Migrate auto-mouse fade, combo feedback, and key-feedback families one at a
   time with live-vs-compiled parity and stale-generation tests.
3. Install RGB and behavior consumers through one production owner only after
   split profile-generation convergence is fail-closed, then measure real-board
   render and lookup timing before advertising mutation capabilities.

### 2026-08-27 — Effective pointing-mode RGB consumer checkpoint

- Extended `rgb_effective_config.c` with compiled/live accessors for
  pointing-mode colors, locality, and stage-group rows. The adapter maps native
  mode flags to stable mode ids and decodes reusable canonical group bitmaps;
  direct compiled pointing-table access is centralized there.
- Reworked the pointing-mode stage to compose into a caller-owned frame using
  the same effective token captured at the RGB frame boundary. It no longer
  retains converted color/locality caches, and invalid or stale reads clear the
  incomplete overlay before application.
- Preserved the existing locality rules, owner-key rendering, mode-specific
  groups, all-mode groups, and inherited group colors for the compiled
  fallback. The effective runtime owner is still not installed, so production
  behavior remains the exact compiled profile.
- Added a dedicated live-vs-compiled renderer fixture whose live color,
  locality, group placement, and inheritance differ from the compiled tables.
  It also publishes a new generation during the final group read and proves
  that no mixed-generation LEDs are applied. The renderer source gate now
  rejects direct reads of both migrated table families outside the adapter.
- Focused verification passed:
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_effective_rgb_runtime_tests.sh`,
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a restored ordinary QMK
  compile plus memory check after stack instrumentation, and `git diff --check`.
- Fresh linked resource facts are per RP2040 half: 270,336 B physical SRAM;
  SRAM0–3 `.bss` is 25,716/26,000 B under the regression policy, `.data` is
  22,984 B, and `.data + .bss` is 48,700/51,000 B. The SRAM0–3 linker/core-
  memory span at boot is 213,440 B against the 204,800 B minimum, and fixed
  linked occupancy across unique SRAM banks is 56,160 B. Removing the six-mode
  color/locality caches reduced linked `.bss` and fixed occupancy by 40 B from
  the preceding layer-consumer checkpoint. These remain link-time facts and
  policy metrics, not total-RAM headroom or runtime high-water measurements.
- The stack gate remains 1,880/1,920 B for its worst named main-process path
  and 336/768 B for its worst named split-slave path. It covers only the paths
  in the stack manifest and is not a global or interrupt-stack maximum.
- Profile Studio UI and authored profile inputs did not change, so screenshots
  and introspection regeneration were intentionally skipped. QMK/resource
  checks wrote generated artifacts in the sibling firmware build tree only; no
  sibling source was edited.

Next steps:

1. Migrate auto-mouse fade mode and end color through the captured effective
   frame with live-vs-compiled and stale-generation coverage.
2. Migrate combo feedback and key-feedback colors/groups one family at a time,
   extending the direct-read gate after each pass.
3. Install RGB and behavior consumers through one production owner only after
   split profile-generation convergence is fail-closed, then measure real-board
   render and lookup timing before advertising mutation capabilities.

### 2026-08-27 — Effective auto-mouse RGB consumer checkpoint

- Extended `rgb_effective_config.c` with compiled/live accessors for the
  auto-mouse stage-enable flag, fade mode, and end color. The auto-mouse stage
  no longer reads the authored fade configuration directly or retains its
  converted end color across profile generations.
- Routed the base-stage decision through the effective stage-enable flag. A
  disabled live auto-mouse stage now falls through to ordinary effective layer
  rendering instead of intercepting the base path.
- The fade composes its start and destination scenes from the same captured
  profile frame. It checks that frame again after all reader-backed composition
  and before touching any LEDs, so a publication during the final read leaves
  the existing LED output untouched.
- Extended the live-vs-compiled render fixture so the compiled profile follows
  its actual layer destination while the live fixture uses an all-key end
  color. The test also covers live stage disablement and final-read
  invalidation. The renderer source gate now rejects direct auto-mouse
  configuration reads outside the effective adapter.
- Focused verification passed:
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_effective_rgb_runtime_tests.sh`,
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a restored ordinary QMK
  compile plus memory check after stack instrumentation, and `git diff --check`.
- Fresh linked resource facts are unchanged from the pointing-mode checkpoint
  and are per RP2040 half: 270,336 B physical SRAM; SRAM0–3 `.bss` is
  25,716/26,000 B under the regression policy, `.data` is 22,984 B, and `.data
  + .bss` is 48,700/51,000 B. The SRAM0–3 linker/core-memory span at boot is
  213,440 B against the 204,800 B minimum, and fixed linked occupancy across
  unique SRAM banks is 56,160 B. Removing the three-byte converted-color cache
  caused no measurable linked-span change. These remain link-time facts and
  policy metrics, not total-RAM headroom or runtime high-water measurements.
- The stack gate remains 1,880/1,920 B for its worst named main-process path
  and 336/768 B for its worst named split-slave path. It covers only the paths
  in the stack manifest and is not a global or interrupt-stack maximum.
- The effective runtime owner is still not installed, so production retains
  exact compiled RGB behavior. Profile Studio UI and authored profile inputs
  did not change; screenshots and introspection regeneration were intentionally
  skipped. QMK/resource checks wrote generated sibling build artifacts only;
  no sibling source was edited.

Next steps:

1. Migrate combo-feedback color and locality through the captured effective
   frame with live-vs-compiled and stale-generation coverage.
2. Migrate key-feedback colors, branches, tap-commit mode, locality, and groups,
   then extend the direct-read gate to the last renderer family.
3. Install RGB and behavior consumers through one production owner only after
   split profile-generation convergence is fail-closed, then measure real-board
   render and lookup timing before advertising mutation capabilities.

### 2026-08-27 — Effective combo-feedback RGB consumer checkpoint

- Extended `rgb_effective_config.c` with compiled/live accessors for the combo
  stage-enable flag, color/locality, canonical reusable-group bitmaps, group
  colors, and inherited-color semantics. Direct compiled combo-table access is
  now centralized in the adapter.
- Reworked the combo underlay and overlay passes to compose into the runtime's
  caller-owned frame from the same effective token captured at the RGB frame
  boundary. The stage no longer retains a converted combo color across profile
  generations, and the runtime applies LEDs only after composition succeeds.
- Invalid group reads clear the incomplete chunk. A profile generation
  published during the final group lookup makes the captured token stale and
  also clears the chunk before LED application. Disabling the live combo stage
  similarly produces no combo frame.
- Added a dedicated live-vs-compiled fixture. Its compiled fallback uses
  keys-only locality and an explicit-color group, while the live golden profile
  uses key-half locality plus a differently placed inherited-color canonical
  group. The renderer source gate now rejects direct combo-table reads outside
  the effective adapter.
- Focused verification passed:
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_effective_rgb_runtime_tests.sh`,
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a restored ordinary QMK
  compile plus memory check after stack instrumentation, and `git diff --check`.
- Fresh linked resource facts are per RP2040 half: 270,336 B physical SRAM;
  SRAM0–3 `.bss` is 25,708/26,000 B under the regression policy, `.data` is
  22,984 B, and `.data + .bss` is 48,692/51,000 B. The SRAM0–3 linker/core-
  memory span at boot is 213,448 B against the 204,800 B minimum, and fixed
  linked occupancy across unique SRAM banks is 56,152 B. Removing the
  persistent converted combo color reduced linked `.bss` and fixed occupancy
  by 8 B from the auto-mouse checkpoint. These are link-time facts and policy
  metrics, not total-RAM headroom or runtime high-water measurements.
- The stack gate remains 1,880/1,920 B for its worst named main-process path
  and 336/768 B for its worst named split-slave path. It covers only the paths
  in the stack manifest and is not a global or interrupt-stack maximum.
- The effective runtime owner is still not installed, so production retains
  exact compiled RGB behavior. Profile Studio UI and authored profile inputs
  did not change; screenshots and introspection regeneration were intentionally
  skipped. QMK/resource checks wrote generated sibling build artifacts only;
  no sibling source was edited.

Next steps:

1. Migrate key-feedback colors, ordered tap-branch colors, tap-commit mode,
   locality, semantic group rows, and reusable-group bitmaps through the same
   captured effective frame.
2. Extend the direct-read gate to the final renderer family and prove
   live-vs-compiled parity, every feedback semantic, stage disablement, and
   stale-final-read refusal.
3. Install RGB and behavior consumers through one production owner only after
   split profile-generation convergence is fail-closed, then measure real-board
   render and lookup timing before advertising mutation capabilities.

### 2026-08-27 — Effective key-feedback RGB consumer checkpoint

- Extended `rgb_effective_config.c` with compiled/live accessors for the
  key-feedback stage-enable flag, ordered tap-branch colors, committed/hold/
  long-hold colors, locality, canonical reusable-group bitmaps, semantic group
  selectors, and group colors. Direct compiled key-feedback table access is
  centralized in the adapter.
- Moved `key_feedback_tap_commit_mode()` into the effective adapter because it
  controls whether the key engine emits a tap-commit pulse, not only how RGB is
  painted. A publication that invalidates this reader-backed decision returns
  `KEY_FEEDBACK_TAP_COMMIT_OFF` fail-closed.
- Reworked the final RGB stage to resolve colors as render-local state and
  compose into the runtime's caller-owned frame from the same effective token
  captured at the RGB frame boundary. The old persistent converted-color cache
  is gone, and the runtime applies LEDs only after composition succeeds.
- Preserved the six visible semantic states, tap-branch selection and clamp,
  flash visibility, keys-only/key-half/left/right/both locality, all-semantic
  group ordering, semantic-specific groups, canonical group bitmaps, and
  inherited group colors.
- Added a dedicated live-vs-compiled fixture. It distinguishes compiled
  keys-only locality and explicit group color from live key-half locality and
  inherited group placement, covers all six visible semantics, verifies branch
  three selects the second live branch color, and proves stage-disablement.
  Publication during the final group lookup clears the incomplete frame;
  publication during the tap-policy read returns the fail-closed policy. The
  renderer source gate now rejects direct key-feedback table reads outside the
  effective adapter, completing migration coverage for all current RGB
  renderer families.
- Focused verification passed:
  `sh tests/host/run_rgb_layer_render_tests.sh`,
  `sh tests/host/run_effective_rgb_runtime_tests.sh`,
  `sh tests/host/run_profile_rgb_v1_tests.sh`,
  `sh tests/host/run_rgb_validation_tests.sh`,
  `sh tests/host/run_real_profile_validation_tests.sh`,
  `sh tests/host/run_key_runtime_scenario_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a restored ordinary QMK
  compile plus memory check after stack instrumentation, and `git diff --check`.
- Fresh linked resource facts are per RP2040 half: 270,336 B physical SRAM;
  SRAM0–3 `.bss` is 25,684/26,000 B under the regression policy, `.data` is
  22,984 B, and `.data + .bss` is 48,668/51,000 B. The SRAM0–3 linker/core-
  memory span at boot is 213,472 B against the 204,800 B minimum, and fixed
  linked occupancy across unique SRAM banks is 56,128 B. Removing the
  persistent key-feedback color cache reduced linked `.bss` and fixed
  occupancy by 24 B from the combo checkpoint. These are link-time facts and
  policy metrics, not total-RAM headroom or runtime high-water measurements.
- The stack gate remains 1,880/1,920 B for its worst named main-process path
  and 336/768 B for its worst named split-slave path. It covers only the paths
  in the stack manifest and is not a global or interrupt-stack maximum.
- The effective runtime owner is still not installed, so production retains
  exact compiled RGB behavior. Profile Studio UI and authored profile inputs
  did not change; screenshots and introspection regeneration were intentionally
  skipped. QMK/resource checks wrote generated sibling build artifacts only;
  no sibling source was edited.

Next steps:

1. Install the RGB and behavior consumers through one production profile owner
   after split generation convergence is fail-closed.
2. Exercise preview/rollback, commit, reboot, disconnect/reconnect, both USB
   orientations, and role swap on the real keyboard before exposing mutation
   controls as ready.
3. Measure real-board render time, behavior lookup time, allocator high-water,
   and stack evidence for the newly reachable owner/activation paths.

### 2026-08-27 — Stage 02 split authority/protocol foundation checkpoint

- Added the dedicated live-profile split protocol foundation selected by
  D-011/D-017 instead of extending the VIA storage regions. The exact 32-byte
  v1 codec carries complete durable record metadata, compatibility identities,
  bounded 14-byte payload chunks, canonical zero padding, and CRC8.
- Added the caller-owned D-014 authority model. It orders durable generations
  independently of USB role, detects disconnected concurrent commits and
  same-tuple corruption, rejects unsupported v1 schema/flags and incompatible
  firmware identities, and publishes one coherent local/peer/status snapshot.
- Added the activation-policy peer observer. It reports resolved only for
  matching compiled defaults or an exact committed record with no transfer in
  progress. Missing, unreadable, malformed, incompatible, stale, conflicting,
  corrupt, in-transfer, or incoherent state fails closed.
- Added fixed metadata/begin/chunk golden frames plus normal, ASan/UBSan, and
  Cortex-M0+ coverage for the authority decision table, publication races,
  saturation, transfer pending, round trips, every truncated frame length,
  checksum, reserved bytes, padding, enum/status, and transfer bounds. The new
  runner is part of the full host suite and both sources are in the production
  userspace manifest.
- Corrected the active Stage 02 and architecture notes so they no longer say
  RGB invalidation or RGB consumer migration is absent. Every current RGB
  renderer family and both domain invalidators have landed; the production
  provider owner and all mutation/activation/peer capability advertising
  remain intentionally absent.
- Focused verification passed:
  `sh tests/host/run_profile_split_foundation_tests.sh`,
  `sh tests/host/run_profile_activation_policy_tests.sh`,
  `sh tests/host/run_profile_store_tests.sh`,
  `sh tests/host/run_qmk_via_split_sync_tests.sh`,
  `sh tests/host/run_qmk_via_sync_state_tests.sh`,
  `sh tests/host/run_qmk_via_sync_protocol_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final checkpoint verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a clean ordinary
  `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`, a second memory check,
  and `git diff --check`.
- Fresh linked resource facts remain unchanged per RP2040 half because no
  persistent production authority instance is allocated: physical SRAM is
  270,336 B; SRAM0–3 `.bss` is 25,684/26,000 B under the regression policy;
  `.data` is 22,984 B; `.data + .bss` is 48,668/51,000 B; the SRAM0–3
  linker/core-memory span at boot is 213,472 B against the 204,800 B minimum;
  and fixed linked occupancy across unique banks is 56,128 B. These are
  link-time facts and policy metrics, not runtime high-water measurements or
  total-RAM headroom.
- Reviewed stack paths remain 1,880/1,920 B for the main process and 336/768 B
  for the split-slave context. This gate covers only the paths named in its
  manifest; the new foundation is not reachable from a QMK callback yet.
- Profile Studio UI and authored profile inputs did not change, so screenshots
  and introspection regeneration were intentionally skipped. The QMK builds
  wrote generated artifacts in the sibling firmware tree only; no sibling
  source file was edited.

Next steps:

1. Add an exact peer-store backend that imports the sender's generation,
   physical origin, flags, CRC, payload digest, compiled digest, and action ABI
   into an inactive slot, validates it incrementally, and acknowledges only
   after marker readback proves durability.
2. Add the bounded scan-owned reconciler and QMK transaction wiring with retry,
   disconnect/reconnect, role-change restart, conflict/corruption stop states,
   and observer invalidation on every peer-status loss.
3. Integrate boot discovery, provider/predicate/behavior/RGB ownership, and
   diagnostics while keeping mutation and peer capabilities disabled until the
   full host and real-keyboard matrices pass.

### 2026-08-27 — Exact peer-profile durable import checkpoint

- Added `profile_peer_store_backend`, a caller-owned bounded receiver that
  serializes peer import through the existing candidate backend, validator,
  store, provider reuse guard, and inactive EEPROM slot. It preserves the
  sender's generation, stable physical origin, persistent flags, schema,
  length, CRC/FNV identity, compiled-default digest, action ABI, and payload-
  derived domain mask unchanged.
- Added exact host/peer serialization and retry rules. Active duplicate begin
  reports progress; fully repeated chunks are read back and accepted only when
  byte-identical; gaps, partial overlaps, correlation errors, and conflicting
  repeats abort the prepare. Stale generations, equal-generation concurrent
  origins, same-tuple record disagreement, incompatible firmware, and domain
  capacity mismatches are rejected before destructive writes.
- Validation and marker-last commit remain incremental through the shared
  scan-step interfaces. A peer commit succeeds only after marker readback and
  field-by-field comparison of every durable identity field except the local
  slot number. The isolated receiver never requests activation, so a zero-flag
  reset record leaves compiled behavior active for the later convergence owner.
- Corrected persistence compatibility and reboot truth. The store now rejects
  candidates and boot records whose compiled-default digest differs from the
  current authored profile, derives the domain mask from the checksummed blob
  during boot/commit shape validation, and retains that derived mask in the
  selected record. The read-only boot owner now computes the real compiled and
  action-ABI identities instead of using a zero placeholder.
- Made final-marker ambiguity sticky. `DURABILITY_UNKNOWN` now blocks every
  later prepare until a conclusive boot selection rescans both slots, avoiding
  accidental invalidation of a commit that may already be durable. A reboot-
  discovered exact record still fails closed as not-yet-idempotently-validated
  until the future production boot owner reruns whole-profile validation.
- Added normal and ASan/UBSan peer-backend coverage for exact generation 5
  import over an empty store, reboot identity, zero-write idempotence, reset
  flags without activation, stale/conflict/corruption/incompatibility refusal,
  duplicate/gap/overlap/conflicting chunks, domain-mask mismatch, reservation
  release, and ambiguous marker reconciliation. Store/runtime tests now cover
  old compiled-default rejection. The runner is in the full host suite, the
  source is in the userspace manifest, and real VIA/split bodies are in the
  explicit feature compile matrix.
- Focused verification passed:
  `sh tests/host/run_profile_store_tests.sh`,
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_profile_candidate_store_backend_tests.sh`,
  `sh tests/host/run_profile_peer_store_backend_tests.sh`,
  `sh tests/host/run_profile_split_foundation_tests.sh`,
  `sh tests/host/run_profile_activation_policy_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a clean ordinary
  `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`, a second memory check,
  a final full host suite, and `git diff --check`. The first target compile
  exposed one GCC enum-comparison warning in a static assertion; explicit
  unsigned casts fixed it before the passing builds.
- Fresh linked resource facts are per RP2040 half: physical SRAM is 270,336 B;
  SRAM0–3 `.bss` is 25,692/26,000 B under the regression policy; `.data` is
  22,984 B; `.data + .bss` is 48,676/51,000 B; the SRAM0–3 linker/core-memory
  span at boot is 213,464 B against the 204,800 B minimum; and fixed linked
  occupancy across unique banks is 56,136 B. The extra 8 B versus the prior
  checkpoint comes from the durable store's derived-domain/reconciliation
  state; no production peer-receiver instance is allocated yet. These remain
  link-time facts and policy metrics, not runtime high-water measurements or
  total-RAM headroom.
- Reviewed stack paths remain 1,880/1,920 B for the worst named main path and
  336/768 B for the worst named split-slave path. The boot-discovery named path
  is 488 B. The gate covers only its manifest paths; peer import is not yet
  reachable from a QMK callback.
- Profile Studio UI and authored profile inputs did not change, so screenshots
  and introspection regeneration were intentionally skipped. Firmware builds
  wrote generated artifacts in the sibling QMK tree only; no sibling source
  file was edited. Mutation, activation, and peer capability bits remain
  disabled.

Next steps:

1. Add the scan-owned split reconciler and QMK RPC registration, including
   metadata exchange, chunk scheduling, acknowledgement/error mapping, retry,
   reconnect, role-change restart, and authority publication on every loss.
2. Replace the read-only discovery shell with one production owner that opens
   compiled defaults once, boot-validates a committed blob, owns the writable
   store/candidate/peer state, and installs provider, activation predicate,
   behavior, and RGB invalidators without duplicate backends.
3. Exercise commit/reboot/interruption/reconnect, both USB orientations, role
   swap, reset-to-compiled, and exact durable convergence on the real keyboard
   before advertising mutation or peer support.

### 2026-08-28 — Bidirectional split reconciler checkpoint

- Added a caller-owned, payload-independent profile reconciler. The current
  QMK master can push a newer local record or explicitly pull a newer sibling
  record; authority remains the durable generation/origin/digest tuple rather
  than current USB role. Exact generation and origin are preserved through the
  existing peer-store backend without a local counter increment.
- Kept all expensive work in matrix scan. The split callback only performs
  strict frame decode, one publication-protected mailbox copy, and a cached
  response copy. Each scan advances at most one RPC, payload read/write, or
  validator/marker-last commit step. Retry backoff is bounded from 50 to
  1,000 ms; disconnect, malformed response, role change, and passive-peer
  expiry invalidate convergence, while conflict, corruption, and
  incompatibility stop without overwriting either record.
- Extended Profile Split v1 with the exact `PAYLOAD_REQUEST` frame needed for
  master-initiated QMK RPC to pull a newer slave. Added the separate
  `qmk_profile_split_transport` adapter and appended
  `PUT_PROFILE_SPLIT_SYNC`. The adapter and reconciler are compiled, but the
  production firmware does not initialize/register them and all live mutation,
  activation, and peer capability bits remain disabled.
- Corrected Profile Studio's forced-artifact mapping for this `MASTER_RIGHT`
  keyboard: left now builds with `FORCE_SLAVE` and right with `FORCE_MASTER`.
  The source check enforces that exact relationship. This exposed R-17: with no
  hand pin or `EE_HANDS`, upstream QMK derives left/right from current master,
  so transport role is not a durable physical origin. Production mutation
  remains blocked until physical-half identity is explicitly provisioned.
- Added two-half real-store reconciliation coverage for compiled convergence,
  newer-master push, newer-slave pull, disconnect/reconnect, passive timeout,
  malformed and lost responses, role change, concurrent-origin conflict,
  same-tuple corruption, incompatible firmware, `UINT32_MAX`, exact record
  identity, and per-scan RPC/storage exclusion. Added separate exact QMK
  registration, callback, framing, malformed-size, cached-busy, and exchange
  tests. Both runners execute normal and ASan/UBSan builds and are part of the
  full host suite.
- Focused verification passed:
  `sh tests/host/run_profile_split_foundation_tests.sh`,
  `sh tests/host/run_profile_peer_store_backend_tests.sh`,
  `sh tests/host/run_profile_split_reconciler_tests.sh`,
  `sh tests/host/run_qmk_profile_split_transport_tests.sh`,
  `sh tests/host/run_profile_activation_policy_tests.sh`,
  `sh tests/host/run_qmk_via_split_sync_tests.sh`,
  `sh tests/host/run_qmk_via_split_mirror_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `npm run check` and `npm run screenshots` from Profile Studio,
  `python3 tools/profile_introspect.py --write`,
  `python3 tools/profile_introspect.py --check`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, a clean ordinary
  `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`, a second memory check,
  and `git diff --check`.
- Fresh linked resource facts are per RP2040 half: physical SRAM is 270,336 B;
  SRAM0–3 `.bss` is unchanged at 25,692/26,000 B under the regression policy;
  `.data` is 22,996 B; `.data + .bss` is 48,688/51,000 B; the SRAM0–3
  linker/core-memory span at boot is 213,448 B against the 204,800 B minimum;
  and fixed linked occupancy across unique banks is 56,152 B. The 12 B
  initialized-data increase is the appended split transaction-table entry; no
  production reconciler instance is allocated. These are linked facts and
  policy metrics, not total-RAM headroom or runtime high-water measurements.
- Reviewed stack paths remain 1,880/1,920 B for the worst named main path and
  336/768 B for the worst named split-slave path; the boot discovery path is
  488 B. This is evidence for the named manifest paths only. The unregistered
  profile callback is not yet a reachable production stack path. Firmware
  builds wrote generated sibling QMK artifacts only; no sibling source changed.

Next steps:

1. Provision and expose a stable physical-half identity that cannot change
   with USB role, with tests for both orientations and role swaps.
2. Replace the read-only boot shell with one writable profile owner that runs
   boot whole-profile validation, owns store/candidate/peer/reconciler state,
   installs provider and domain invalidators, and arbitrates its transaction
   work with the existing VIA split owner.
3. Enable an engineering-only capability and run the real-keyboard matrix for
   behavior and RGB commit, activation, reboot, interruption, reconnect,
   role swap, reset-to-compiled, and rollback before exposing live mutation.

### 2026-08-28 — Flash-provisioned physical-half identity checkpoint

- Accepted D-018 and added a narrow QMK compatibility boundary for stable
  physical identity. `NOAH_PHYSICAL_HALF=left` compiles origin `0` and a left
  `is_keyboard_left_impl()` override; `NOAH_PHYSICAL_HALF=right` compiles
  origin `1` and a right override. A generic artifact exposes neither and the
  query fails closed. Supplying both definitions is a compile error.
- Kept durable identity independent of transport role. `FORCE_MASTER` and
  `FORCE_SLAVE` still control the dual-USB role only; no profile authority
  comparison derives origin from those flags or from current master status.
- Updated Profile Studio's paired build action. The physical-left artifact now
  receives `FORCE_SLAVE=yes` plus `NOAH_PHYSICAL_HALF=left`; physical right
  receives `FORCE_MASTER=yes` plus `NOAH_PHYSICAL_HALF=right`. The build helper
  now accepts multiple explicit QMK environment assignments, and the source
  check enforces both exact pairs.
- Added host coverage for unprovisioned refusal, null-output refusal, left and
  right origins, left/right QMK handedness, and conflicting provisioning.
  Feature compile gates cover both provisioned production bodies, and both
  real side-specific QMK builds produced distinct UF2 artifacts successfully.
- Updated the hook inventory, Profile Studio documentation, split contract,
  active architecture, Stage 02 status, risk lifecycle, and canonical memory
  facts. R-17 is partially resolved in code; hardware evidence remains open.
- Focused verification passed:
  `sh tests/host/run_qmk_physical_half_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`, and `npm run check` from
  Profile Studio (88 live-link tests).
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`, `npm run screenshots`, both exact
  side-specific `qmk compile` commands, the ordinary
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, an ordinary rebuild and
  second memory check after stack instrumentation, and `git diff --check`.
- Fresh ordinary linked resource facts are per RP2040 half: physical SRAM is
  270,336 B; SRAM0–3 `.bss` is 25,692/26,000 B under the regression policy;
  `.data` is 22,996 B; `.data + .bss` is 48,688/51,000 B; the SRAM0–3
  linker/core-memory span at boot is 213,448 B against the 204,800 B minimum;
  and fixed linked occupancy across unique SRAM banks is 56,152 B. These are
  link-time values and policy metrics, not total-RAM headroom or runtime
  high-water measurements.
- Reviewed stack paths remain 1,880/1,920 B for the worst named main path and
  336/768 B for the worst named split-slave path; boot discovery remains 488 B.
  The gate covers only manifest paths. No new profile owner or callback became
  reachable in this checkpoint.
- Profile Studio screenshots were regenerated and remained byte-identical.
  Firmware builds wrote generated artifacts in the sibling QMK tree only; no
  sibling source file changed. Live mutation, activation, and peer capability
  bits remain disabled.

Next steps:

1. Move VIA split callback EEPROM work behind the same scan-owned durable-I/O
   scheduler required by the profile owner; a scan-only round robin is not
   enough because current VIA callbacks can access wear-level storage from the
   split thread.
2. Replace the read-only boot shell with one provisioned writable owner that
   consumes D-018, validates a committed blob incrementally, owns the sole
   store/provider/candidate/peer/reconciler instances, installs behavior/RGB
   invalidators, and registers the split transport.
3. Enable mutation only in an engineering artifact, then run both USB
   orientations, role swap, commit/reboot/interruption/reconnect,
   reset-to-compiled, RGB, and behavior hardware tests before advertising the
   capability in normal firmware.

### 2026-08-28 — Scan-owned durable-I/O arbitration checkpoint

- Accepted D-019 and removed durable work from both production VIA split
  callbacks. The write-through mirror now admits one bounded best-effort frame
  and applies it from scan context. Durable VIA reconciliation now queues one
  exact request, returns BUSY while scan owns processing, and returns a cached
  terminal response only for the exact request that produced it. Malformed
  callbacks retain bounded structured errors without entering storage.
- Added one rotating scan scheduler across live-profile boot discovery, the
  queued VIA mirror, and durable VIA reconciliation. It starts at the next
  owner, visits idle owners once, and stops after the first consumed step, so a
  continuously busy owner cannot starve the others and those subsystems cannot
  perform two work steps in one scheduler grant.
- Preserved best-effort mirror semantics: a full mailbox keeps the first frame
  and durable reconciliation repairs a dropped later mirror. Receiver
  verification and local digest work remain finite and take priority inside
  the VIA reconciliation grant so commit polling cannot skip verification.
- Added normal and sanitizer callback instrumentation. Every metadata,
  validity, recovery, dynamic-keymap, macro, and region read/write effect
  increments a test counter; every callback invocation asserts the counter is
  unchanged. Scheduler tests cover deterministic idle order, round-robin
  fairness, idle skipping, and one-consumer-per-scan behavior. Runtime init
  tests enforce the new scheduler position.
- Updated the userspace manifest, hook/runtime documentation, Stage 02 record,
  risk lifecycle, architecture review, canonical memory facts, and reviewed
  stack paths. The split-thread manifest no longer describes EEPROM calls that
  are unreachable from callbacks; it now covers sync decode/admission, cached
  response encoding, and mirror mailbox admission, while the main-process
  manifest covers the scan-owned mirror writes and all three indirect scheduler
  entries.
- Focused verification passed:
  `sh tests/host/run_qmk_durable_io_tests.sh`,
  `sh tests/host/run_runtime_init_order_tests.sh`,
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_qmk_via_split_mirror_tests.sh`,
  `sh tests/host/run_qmk_via_split_sync_tests.sh`,
  `sh tests/host/run_hook_chaining_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`,
  `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`, and
  `git diff --check`. The first instrumented stack pass exposed two stale
  mirror path names in the new manifest; the paths were corrected to the linked
  byte-write functions before the required stack command passed.
- Fresh ordinary linked resource facts are per RP2040 half: physical SRAM is
  270,336 B; SRAM0–3 `.bss` is 25,788/26,000 B under the regression policy;
  `.data` is 22,996 B; `.data + .bss` is 48,784/51,000 B; the SRAM0–3
  linker/core-memory span at boot is 213,352 B against the 204,800 B minimum;
  and fixed linked occupancy across unique SRAM banks is 56,248 B. The 96 B
  BSS increase is the bounded mirror/sync mailboxes plus scheduler state. These
  are link-time facts and policy metrics, not total-RAM headroom or runtime
  high-water measurements.
- The worst named reviewed main path is 1,800/1,920 B. The worst named split
  path remains the 328/768 B base-state RPC; the largest new VIA callback path
  is 292 B and the queued mirror write path is 424 B in main scan context. The
  gate covers named manifest paths only.
- Profile Studio UI and authored profile inputs did not change, so screenshots
  and introspection regeneration were intentionally skipped. Firmware builds
  wrote generated artifacts in the sibling QMK tree only; no sibling source
  file changed. Live profile mutation, activation, and peer capabilities remain
  disabled.

Next steps:

1. Replace the read-only boot shell with one provisioned writable owner that
   consumes D-018 and the existing scheduler entry, boot-validates committed
   blobs, and owns the sole store/provider/candidate/peer/reconciler instances.
2. Install the behavior and RGB invalidators, safe-activation predicate, and
   profile split transport through that owner without creating another EEPROM
   path; add production boot, mutation, rollback, and split integration tests.
3. Enable capabilities only in an engineering artifact and run both USB
   orientations, role swap, commit/reboot/interruption/reconnect,
   reset-to-compiled, RGB, and behavior hardware tests before normal exposure.

### 2026-08-30 — Production-owner safety foundations checkpoint

- Accepted D-020 and hardened the disconnected components that the single
  writable owner will compose. The compiled-default materializer now derives
  the validator's exact domain, action-ABI, layer, PD-mode, macro, and RGB
  compatibility from the real authored profile instead of schema-wide maxima.
- Replaced whole-slot boot discovery inside the store with begin/step state.
  Each step performs at most one read; fixed reads are at most 32 bytes and
  payload reads honor the supplied budget. Both slot identities and canonical
  domain shape are checked before the established selection/conflict decision.
  The current temporary read-only runtime still uses the cold compatibility
  wrapper; production scheduling remains the next owner checkpoint.
- Added bounded committed-record adoption through the reader-backed whole-
  profile validator. Reboot adoption performs no write and retains the exact
  validated view selected by durable metadata. An override record requests the
  validated snapshot; an override-disabled reset generation requests compiled
  fallback while preserving its durable authority identity.
- Added an explicit `NONE`/`HOST`/`PEER` admission lease to the one shared
  candidate backend. Host contention remains queued as retryable busy without
  poisoning its transaction; peer contention cannot retarget host staging;
  abort and successful activation release admission. Profile split transport
  registration is now idempotent for the same reconciler and rejects a second
  owner.
- Added a 384-byte 32-bit store-state regression policy. The added boot cursor
  state increased linked SRAM0–3 `.bss` by 88 bytes; this is a link-time policy
  fact, not physical RAM use or runtime high-water evidence.
- Focused normal/sanitizer verification passed:
  `sh tests/host/run_profile_store_tests.sh`,
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_profile_candidate_store_backend_tests.sh`,
  `sh tests/host/run_profile_candidate_transaction_tests.sh`,
  `sh tests/host/run_profile_peer_store_backend_tests.sh`,
  `sh tests/host/run_profile_compiled_defaults_v1_tests.sh`,
  `sh tests/host/run_qmk_profile_split_transport_tests.sh`,
  `sh tests/host/run_profile_split_reconciler_tests.sh`,
  `sh tests/host/run_qmk_durable_io_tests.sh`, and
  `sh tests/host/run_feature_gate_compile_tests.sh`.
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -c -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`, the ordinary
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`, a second memory check,
  and `git diff --check`. The first stack pass correctly rejected the stale
  boot-discovery manifest path; the manifest was updated to the linked
  incremental state-machine path before the required command passed.
- Fresh ordinary linked resource facts are per RP2040 half: physical SRAM is
  270,336 B; SRAM0–3 `.bss` is 25,876/26,000 B under the regression policy;
  `.data` is 22,996 B; `.data + .bss` is 48,872/51,000 B; the SRAM0–3
  linker/core-memory span at boot is 213,264 B against the 204,800 B minimum;
  and fixed linked occupancy across unique SRAM banks is 56,336 B. These are
  linked facts and policy metrics, not total-RAM headroom or runtime high-water
  measurements.
- The worst named main-process path remains 1,800/1,920 B; the boot discovery
  path is now 384 B. The worst named split-slave path remains 328/768 B. The
  stack gate covers only the manifest paths. Authored profile inputs and the
  Profile Studio UI did not change, so introspection regeneration and
  screenshots were intentionally skipped. Builds wrote generated sibling QMK
  artifacts only; no sibling source changed. Mutation, activation, and peer
  capability bits remain disabled.

Next steps:

1. Replace `profile_store_runtime` with the one production owner that consumes
   flash-provisioned physical identity, advances incremental boot selection and
   adoption inside the existing durable scheduler entry, and owns the sole
   store, provider, candidate transaction/backend, peer backend, reconciler,
   activation policy, behavior runtime, and RGB runtime instances.
2. Alternate host activation and peer convergence work explicitly, add an
   abandoned pre-commit host timeout, publish coherent owner status, install
   behavior/RGB invalidators, and register the split transport exactly once.
3. Enable mutation only in an engineering artifact, then run the two-half
   behavior/RGB commit, reboot, interruption, reconnect, role-swap,
   reset-to-compiled, rollback, and host/peer contention hardware matrix.

### 2026-08-31 — Gated live-profile owner checkpoint

- Accepted D-021 and composed the previously isolated live-profile pieces into
  one caller-owned graph. The owner holds the compiled profile and validator,
  committed reader and compatibility state, store, provider, shared candidate
  backend, host transaction, peer backend, split reconciler, activation policy,
  behavior runtime, RGB runtime, and one durable descriptor. It has no
  profile-sized RAM buffer.
- Boot now advances incremental slot discovery and semantic committed-record
  adoption. The durable descriptor is withheld until semantic validation
  succeeds; an accepted committed record is adopted without rewriting EEPROM,
  and reset-to-compiled preserves the durable authority identity. The validated
  boot record then reconciles in full before activation is requested, allowing
  an older half to import its newer peer without deadlocking on the activation
  policy's convergence requirement.
- Added admission-first scheduling across host and peer work. A peer lease
  blocks host work; a host lease allows only convergence-only peer work; and an
  idle owner rotates host, split, and peer activation work. Convergence-only
  split mode may exchange metadata, serve local content, or push the selected
  local record, but refuses a peer-newer import without storage effects.
- Added a 15-second pre-commit host timeout and protocol error 19 (`TIMEOUT`).
  The timeout cannot expire a received candidate while it is committing,
  activating, in the host command mailbox, or durability-unknown. The owner also
  surfaces durability-unknown and activation-failed terminal states.
- Kept the graph engineering-only. `NOAH_LIVE_PROFILE_OWNER=yes` requires an
  explicit left/right physical half and VIA, and links the owner through a
  separate source list. The ordinary firmware remains the read-only discovery
  shell, does not allocate the owner, and still advertises no live mutation,
  activation, or peer capability.
- The engineering runtime owns one static owner, writable EEPROM access,
  physical identity, split exchange, and one transport registration after graph
  initialization. Exact owner state is 3,084 bytes on the 32-bit target, within
  its 4,096-byte engineering policy.
- The first engineering stack analysis exposed an unbounded dynamic stack path
  in QMK's EEPROM update helper. The adapter now enforces a 32-byte I/O boundary
  and uses the direct bounded write primitive; the profile store already owns
  change detection and deliberate writes, so the extra update read was not part
  of the storage contract. After the boot-reconciliation fix, the gate also
  rejected a stale optimized metadata-path adjacency; the manifest was expanded
  to the newly linked reconciler and metadata frames before the dedicated gate
  passed.
- A final independent audit exposed the boot-order deadlock above before this
  checkpoint was committed. A two-owner host regression now boots generation 1
  opposite generation 2, converges both durable records to generation 2, and
  verifies that both owners activate successfully. Partial behavior/RGB runtime
  installation rollback is also covered.
- Focused verification passed:
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_profile_candidate_store_backend_tests.sh`,
  `sh tests/host/run_profile_owner_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `python3 tests/host/firmware_stack_budget_tool_test.py`, and
  `npm run check` from `tools/charybdis-profile-studio/`.
- Final ordinary verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`, and
  `git diff --check`. The largest named ordinary main-process path is
  1,800/1,920 bytes; boot discovery is 384 bytes; and the largest named split
  path is 328/768 bytes. These are manifest-path estimates, not global or
  interrupt-stack maxima.
- The side-specific left engineering artifact linked successfully. Its
  dedicated `sh tests/host/run_live_profile_owner_stack_budget_checks.sh` gate
  passed: split metadata exchange is the largest named owner main-process path
  at 1,104/1,920 bytes, and the largest named profile split callback is
  264/768 bytes. Host marker-last write is 816 bytes, host validation is 824
  bytes, and behavior/RGB publication is 1,072 bytes.
- The engineering memory-policy check intentionally remains red:
  `NOAH_MEMORY_BUDGET_TARGET=bastardkb_charybdis_4x6_noah_live_owner_stack_left sh tests/host/run_firmware_memory_budget_checks.sh`
  reports SRAM0–3 `.bss` 28,740/26,000 bytes and `.data + .bss`
  51,740/51,000 bytes. Its linker/core-memory span is 210,400/204,800 bytes and
  passes that separate policy. Each half has 270,336 bytes of physical SRAM;
  the failed regression margins are not physical-RAM limits or runtime
  high-water evidence.
- Fresh ordinary linked facts remain per RP2040 half: SRAM0–3 `.bss` is
  25,876/26,000 bytes, `.data` is 22,996 bytes, `.data + .bss` is
  48,872/51,000 bytes, the linker/core-memory span is 213,264/204,800 bytes,
  and fixed linked occupancy across unique SRAM banks is 56,336 bytes.
- Authored profile inputs and the Profile Studio UI did not change, so
  introspection regeneration and screenshots were intentionally skipped.
  Builds wrote generated artifacts in the sibling QMK tree only; no sibling
  source file changed.

Next steps:

1. Route coherent owner status and candidate-channel commands into the gated
   runtime, including explicit resolution when peer authority supersedes an
   in-flight host candidate; keep advertised write capabilities disabled.
2. Measure allocator and stack high-water on both real halves, then run both USB
   orientations, role swap, behavior/RGB commit, reboot, interruption,
   reconnect, reset-to-compiled, rollback, and host/peer contention tests.
3. Make an explicit resource-policy decision from the linked accounting and
   hardware evidence before enabling engineering mutation, and enable normal
   live-edit capability only after the two-half safety matrix passes.

### 2026-08-31 — Coherent owner status and pre-commit supersession checkpoint

- Added one protocol-neutral owner snapshot that coherently reports the
  effective provider, semantically validated committed record, candidate
  transaction, split authority, compiled/action identities, safe-boundary
  state, and convergence state. `profile_store_runtime` exposes copy-out status
  and candidate adapters while retaining sole ownership of the static owner;
  no mutable owner pointer crosses the boundary.
- The engineering-owner VIA read surface now publishes the real behavior/RGB
  schema support, supported domains, action ABI, compiled digest, active and
  pending provider identity, validated committed identity, candidate state,
  peer identity, convergence, and conflict state. General status page 0 latches
  the snapshot and page 1 serves the same snapshot, preventing torn two-page
  reads.
- Preserved the frozen capability contract: candidate-write, commit,
  activation, peer-operation, and preview bits remain clear,
  `candidate_chunk_max` remains zero, candidate status `0x18` remains
  unhandled, and all mutation commands remain unrouted in ordinary and current
  engineering-owner builds. The future receive adapter is therefore not a
  hidden write path.
- Added pre-commit peer supersession. While HOST owns staging, a coherent,
  compatible peer whose generation is greater than or equal to the host's
  reserved candidate generation cancels staging before commit begins,
  including a queued commit. Cancellation aborts only the inactive staged
  slot, releases admission, preserves the transaction/digest/last-operation
  correlation, increments the operation sequence, and reports stable error 20
  (`PEER_SUPERSEDED`). Durable, committing, activating, and durability-unknown
  phases cannot be cancelled this way.
- Tests cover peer generation below, equal to, and above the reserved host
  generation; cancellation of a queued commit before marker-last durability;
  preservation of the previous durable record and status correlation; refusal
  to cancel durable phases or discard their queued mailbox; and acceptance of
  a later begin after supersession.
- The remaining authority blocker is now explicit: a newer peer observed after
  host commit begins can still leave HOST activation unable to import that peer
  while convergence-only split mode is active. Mutation stays unavailable
  until a post-commit convergence barrier or deterministic distributed commit
  resolution covers that race and its interruption/reboot cases.
- Focused verification passed:
  `sh tests/host/run_profile_candidate_transaction_tests.sh`,
  `sh tests/host/run_profile_owner_tests.sh`,
  `sh tests/host/run_profile_store_runtime_tests.sh`,
  `sh tests/host/run_profile_wire_v1_tests.sh`,
  `sh tests/host/run_profile_split_reconciler_tests.sh`,
  `sh tests/host/run_qmk_contract_checks.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`, and
  `python3 tests/host/firmware_stack_budget_tool_test.py`.
- Final ordinary verification passed:
  `sh tests/host/run_all_host_tests.sh` (including the Profile Studio
  `npm run check` and all 88 live-link tests),
  `sh tests/host/run_firmware_stack_budget_checks.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_memory_budget_checks.sh`, and
  `git diff --check`. Ordinary linked resource facts remain unchanged per
  RP2040 half: `.bss` 25,876/26,000 B, `.data` 22,996 B, combined policy span
  48,872/51,000 B, linker/core-memory span 213,264/204,800 B, and fixed linked
  occupancy 56,336 B. The largest named ordinary main and split paths remain
  1,800/1,920 B and 328/768 B respectively.
- The side-specific engineering owner again passed
  `sh tests/host/run_live_profile_owner_stack_budget_checks.sh`. Its coherent
  VIA status path is 616/1,920 B, split metadata exchange is the largest named
  main path at 1,280/1,920 B, and the largest profile split callback remains
  264/768 B. These are reviewed linked paths only, not global or interrupt
  stack high-water measurements.
- The engineering memory-policy audit intentionally remains red:
  `NOAH_MEMORY_BUDGET_TARGET=bastardkb_charybdis_4x6_noah_live_owner_stack_left sh tests/host/run_firmware_memory_budget_checks.sh`
  reports `.bss` 28,740/26,000 B and `.data + .bss` 51,740/51,000 B. Its
  linker/core-memory span passes at 210,400/204,800 B and fixed linked occupancy
  is 59,200 B. These are conservative policy failures, not physical exhaustion;
  every half has 270,336 B of RP2040 SRAM and runtime high-water remains to be
  measured on hardware.
- Authored profile inputs and the rendered Profile Studio UI did not change, so
  introspection regeneration and screenshots were intentionally skipped. The
  JavaScript protocol constant changed and was covered by `npm run check`.
  Builds wrote artifacts in the sibling QMK tree only; no sibling source file
  changed.

Next steps:

1. Specify and implement the post-commit split-authority rule: decide which
   durable identity wins, how both halves converge after interruption or role
   swap, and when host activation can safely leave its lease.
2. Add a separate explicit engineering-mutation build gate that enables
   routing and advertising atomically only after that rule is enforced; then
   run the real two-half behavior/RGB commit, reboot, reconnect, role-swap,
   interruption, reset, and contention matrix.
3. Measure allocator and stack high-water on both halves and make an explicit
   resource-policy decision before promoting the owner or mutation capability
   into normal firmware.

### 2026-09-02 — Distributed commit-barrier checkpoint

- Replaced the unresolved post-commit authority race with the D-022
  distributed prepare/commit barrier. A host candidate is staged and validated
  locally, prepared on the peer, committed locally, authorized to commit on the
  peer, and activated only after both halves report the exact durable identity.
- Split candidate commit from activation authorization so neither half can
  publish a candidate before the barrier is satisfied. Unexpected post-commit
  authority loss now fails closed as `AUTHORITY_FAILED`; conflicting peer
  commit and prepare-yield outcomes are separately observable protocol errors.
- Hardened interruption and arbitration behavior: receiver prepares expire,
  absent aborts are idempotent, aborts cannot be starved by a busy mailbox,
  local commit failure releases a prepared peer, role changes restart the
  handshake, crossed simultaneous prepares resolve deterministically, and
  retry scheduling remains valid across the timer high bit.
- Reconciled Profile Studio's upload coordinator with the new preparation,
  convergence, and authority-failure states. Barrier waits use the firmware's
  60-second no-progress window plus host allowance; acknowledged transfer
  progress, rather than arbitrary polling, refreshes that window.
- Added maximum-size candidate evidence: a 4,064-byte payload reaches the
  prepare barrier within the firmware window. Host tests cover commit/abort
  idempotence, lease expiry, role swap, simultaneous prepares, storage failure,
  exact two-half convergence, and fail-closed publication.
- Focused transaction, owner, reconciler, storage, QMK contract/transport, and
  Profile Studio live-link verification passed. Final
  `sh tests/host/run_all_host_tests.sh` passed, including Profile Studio
  `npm run check` and all 92 live-link tests.
- Required ordinary firmware checks passed:
  `sh tests/host/run_firmware_stack_budget_checks.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`, and
  `sh tests/host/run_firmware_memory_budget_checks.sh`. The ordinary image is
  unchanged in policy terms per RP2040 half: SRAM0–3 `.bss` is
  25,876/26,000 B, `.data` is 22,996 B, combined is 48,872/51,000 B, the
  linker/core-memory span is 213,264/204,800 B, and fixed linked occupancy is
  56,336 B.
- The left-side gated engineering owner passed
  `sh tests/host/run_live_profile_owner_stack_budget_checks.sh`. Its largest
  named main-process path is split metadata exchange at 1,504/1,920 B; the
  coherent status read is 624/1,920 B; and the largest named profile split
  callback is 264/768 B. These are reviewed linked paths only, not global or
  interrupt-stack high-water measurements.
- Fresh engineering linked accounting reports an exact 3,172-byte owner,
  SRAM0–3 `.data` 23,000 B, `.bss` 28,828/26,000 B, combined
  51,828/51,000 B, linker/core-memory span 210,312/204,800 B, and fixed linked
  occupancy 59,288 B. The BSS and combined regression policies remain red;
  this is not physical exhaustion. Each half has 270,336 B of physical SRAM,
  and hardware allocator/stack high-water evidence is still absent.
- Mutation routing and advertised write capabilities remain disabled in both
  ordinary and current engineering-owner artifacts. This checkpoint is a
  verified safety foundation, not yet a user-testable live-edit release.
- Authored profile inputs and rendered Profile Studio layout did not change,
  so introspection regeneration and screenshots were intentionally skipped.
  Firmware builds wrote generated artifacts in the sibling QMK tree; no
  sibling source file changed.

Next steps:

1. Add one explicit side-specific engineering-mutation gate that couples the
   candidate command route and advertised write capabilities so they cannot
   drift apart.
2. Build labeled left/right engineering UF2 artifacts and run the first real
   two-half RGB and key-behavior upload test through Profile Studio.
3. Capture allocator and stack high-water during upload, commit, activation,
   reconnect, reboot, role swap, interruption, reset, and contention before
   deciding whether to revise policy or promote mutation into normal firmware.

### 2026-09-03 — First testable engineering live-edit checkpoint

- Added the explicit D-023 `NOAH_LIVE_PROFILE_MUTATION=yes` gate. It requires
  the side-specific owner and VIA, routes candidate status/write/abort/validate
  and save/commit commands, and advertises candidate-write, persistence,
  activation, and peer-reconciliation capabilities only as one complete set.
  Ordinary and owner-only builds remain read-only.
- Connected Profile Studio's parsed source model to the canonical Profile Wire
  encoder for the complete current Milestone A RGB and supported key-behavior
  surfaces. The real authored profile compiles byte-for-byte to the 1,089-byte
  firmware fixture; unused source group declarations are intentionally omitted
  to match the compiled-default materializer.
- Connected `Apply live` to staged upload and commit. The UI exposes progress
  and failure states, and the device service reports success only after a fresh
  status read identifies the candidate digest as both committed and active.
  Read-only firmware cannot enable the control.
- Built labeled side-specific hardware-test artifacts:
  `bastardkb_charybdis_4x6_noah_live_edit_left.uf2` for the physical left half
  and `bastardkb_charybdis_4x6_noah_live_edit_right.uf2` for the physical right
  half. Both include the owner and mutation gates; neither is a production
  firmware recommendation. Their SHA-256 digests are respectively
  `adc391872f2b7cf7ff0eb3b8a9d4e538a18e16c5d85e8445a4188df248d91d0d`
  and
  `9d66bf27939fd1f43ef18e8900481a876282b3206213ac815eaa7c195d6e6c8e`.
- Focused Profile Wire and feature-gate tests pass. Profile Studio's check
  passes with 98 live-link tests, including real-source exact compilation,
  mutation-capability gating, successful staged commit, and fail-closed final
  digest mismatch. The dedicated mutation-owner stack gate passes with split
  metadata at 1,328/1,920 B, coherent VIA status at 680/1,920 B, Raw HID write
  dispatch at 392/1,920 B, and the profile split callback at 264/768 B. These
  are named linked paths, not global or interrupt-stack high-water evidence.
- Fresh optimized engineering linked accounting remains `.data` 23,000 B,
  `.bss` 28,828/26,000 B, combined 51,828/51,000 B, linker/core-memory span
  210,312/204,800 B, and fixed linked occupancy 59,288 B per half. The first
  two regression policies fail, but this is not physical exhaustion: each half
  has its own 270,336 B of physical SRAM. Runtime allocator/stack high-water is
  still unknown.
- Hardware has not yet been flashed or exercised. This checkpoint is software-
  verified and ready for the first real test; it is not Milestone A closure.

Next steps:

1. Flash the labeled image matching each physical half and run one small RGB
   edit followed by one key-behavior edit through `Apply source` then
   `Apply live`.
2. Verify both halves, reboot persistence, reconnect, and both USB orientations;
   preserve the ordinary firmware artifact as the fallback.
3. Capture allocator/stack high-water and complete the interruption, role-swap,
   contention, reset, and recovery matrix before any production promotion.

### 2026-09-04 — First hardware finding and slave-scan remediation checkpoint

- The first hardware apply found one Raw HID interface and compatible source
  capacities, but the commit made no observable progress and left candidate
  transaction 1 in `PREPARING_PEER`. Studio status reported `peerKnown=false`
  even though normal keys on the second half remained functional.
- Root cause was firmware reachability, not the physical split link. Upstream
  QMK calls `matrix_scan_user()` only on the master and
  `matrix_slave_scan_user()` on the slave. Noah had only the master hook, so
  the slave never advanced compiled-profile validation, constructed its owner
  graph, registered `PUT_PROFILE_SPLIT_SYNC`, or processed the profile and VIA
  durable mailboxes. Core key transport continued independently, exactly
  matching the hardware observation.
- Added D-024's dedicated slave entry point. The weak QMK slave hook chains to
  `noah_matrix_slave_scan_user()`, which runs only the shared durable-I/O
  scheduler; key runtime, macro playback, combo retirement, and shared-state
  senders remain master-only. This also restores the scan-owned VIA mirror and
  reconciliation receiver paths on the slave.
- Extended hook chaining and runtime-order tests to enforce the slave path and
  its deliberately narrow workload. Updated both reviewed-path stack manifests
  to follow the now-shared `noah_qmk_durable_io_matrix_scan()` callsite.
- Hardened Profile Studio's recovery surface. General status flags now expose
  second-half detection, convergence, pending-candidate, and safe-boundary
  state; refresh reads the candidate transaction; live apply requires a known,
  converged peer; a matching recoverable transaction can resume; and a
  mismatched or unsafe transaction is refused with a two-half cold-recovery
  instruction. The UI names candidate states instead of showing only numeric
  ids.
- Focused verification passed:
  `sh tests/host/run_hook_chaining_tests.sh`,
  `sh tests/host/run_runtime_init_order_tests.sh`,
  `sh tests/host/run_qmk_durable_io_tests.sh`,
  `sh tests/host/run_qmk_profile_split_transport_tests.sh`,
  `sh tests/host/run_profile_owner_tests.sh`,
  `sh tests/host/run_feature_gate_compile_tests.sh`,
  `python3 tools/profile_introspect.py --check`, and
  `sh tests/host/run_tooling_checks.sh`.
- Final verification passed:
  `sh tests/host/run_all_host_tests.sh`,
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah`,
  `sh tests/host/run_firmware_stack_budget_checks.sh`,
  `sh tests/host/run_live_profile_owner_stack_budget_checks.sh`,
  `npm run check`, `npm run screenshots`, and `git diff --check`.
  Profile Studio reports 101 passing live-link tests. The ordinary memory gate
  passed at 25,868 B SRAM0–3 `.bss`, 22,996 B `.data`, 48,864/51,000 B policy
  span, 213,272 B linker/core-memory span, and 56,328 B fixed occupancy per
  half.
- The engineering mutation image still intentionally fails the two known
  conservative policies: 28,828/26,000 B SRAM0–3 `.bss` and
  51,832/51,000 B `.data + .bss`. Its linker/core-memory span passes at
  210,304/204,800 B and fixed linked occupancy is 59,296 B. These policy
  failures are not physical SRAM exhaustion; each RP2040 half has 270,336 B of
  physical SRAM, and runtime high-water remains unmeasured. Reviewed owner
  stack paths pass; the largest named main path is split metadata exchange at
  1,344/1,920 B and the largest named split callback remains 264/768 B.
- Rebuilt the corrected labeled artifacts. The physical-left UF2 SHA-256 is
  `524166857cc61ed056927f7797e184f7dae8ac27013fd90f3bec13838cf6f0cc`;
  the physical-right UF2 SHA-256 is
  `2b866a05bf3dd2eab1d776ff95602b4ffe46b2757058f5bf2c817544347d70cd`.
  Builds wrote generated artifacts in the sibling QMK tree only; no sibling
  source file changed.

Next steps:

1. Power-cycle both halves together to discard the old pre-commit transaction,
   then flash the corrected left/right UF2 pair to the matching physical halves.
2. Reconnect and confirm Studio reports both `Second half detected: Yes` and
   `Halves converged: Yes`; then apply one obvious RGB change and one supported
   `key_behaviors[]` change live.
3. Reboot to verify persistence, then continue the reconnect, USB-orientation,
   role-swap, held-input, interruption, contention, reset, and runtime
   high-water hardware matrix before production promotion.

### 2026-09-04 — Standard VIA layer-key live apply checkpoint

- The first corrected two-half hardware run successfully applied RGB, but a
  base-layer keycode edit did not change typed output. This was a host scope
  gap: the Milestone A custom blob intentionally contains RGB and
  `key_behaviors[]`, while `keymaps[][]` remains standard VIA-owned state.
- Added a dedicated Profile Studio VIA layout codec. It maps all 56 authored
  `LAYOUT(...)` positions to the keyboard's 10x6 matrix contract, resolves the
  current profile's complete keycode vocabulary to 16-bit QMK values, and
  rejects unresolved expressions before any device mutation.
- `Apply live` now reads all 280 authored positions in the five-layer profile,
  writes only differing positions with standard VIA set-keycode commands, and
  immediately verifies every changed position through VIA readback. Unused
  matrix cells are not overwritten.
- The standard firmware path remains authoritative: each accepted write is
  persisted in the dynamic keymap, sent through the immediate split mirror,
  and marked for durable VIA reconciliation. Studio currently proves readback
  on the USB-connected half; explicit peer-convergence reporting for VIA-owned
  storage remains open.
- Focused Profile Studio verification passes with 106 live-link tests,
  including matrix drift enforcement against pinned QMK, exact layout/matrix
  compilation, fail-closed expression handling,
  standard VIA framing, semantic diff, write-only-on-change behavior, immediate
  readback, and operation ordering after the custom profile commit.
- Firmware-contract verification passes:
  `sh tests/host/run_qmk_contract_checks.sh`,
  `sh tests/host/run_qmk_via_command_classifier_tests.sh`,
  `sh tests/host/run_qmk_via_split_mirror_tests.sh`,
  `sh tests/host/run_qmk_via_split_sync_tests.sh`,
  `sh tests/host/run_qmk_via_storage_regions_tests.sh`, and
  `sh tests/host/run_real_profile_validation_tests.sh`.
  `qmk compile -kb bastardkb/charybdis/4x6 -km noah` also passes. The compile
  wrote build output in the sibling QMK tree but changed no sibling source.
- `npm run screenshots -- --out /private/tmp/profile-studio-live-layout-screenshots`
  passes and the rendered Layout view was inspected. `git diff --check` passes.
- The full host suite advanced through all runtime, profile, VIA, validation,
  and real-profile checks, then stopped in the tooling section because an
  existing dirty generated `profile-layer-LAYER_BASE.svg` still renders `X`
  while the current authored `keymap.c` is back to `KC_Y`. That pre-existing
  generated preview (and its related dirty Studio images/export) was preserved
  rather than overwritten; it is unrelated to the new live-layout code.

Next steps:

1. Install/reload the updated Profile Studio extension; no firmware reflash is
   needed because the current engineering firmware already implements the
   standard VIA keymap and split-mirror paths.
2. Change one base-layer letter, apply the staged source edit, choose `Apply
   live`, and verify the Live apply card reports 280 checked keys and one
   changed key.
3. Exercise that key from its physical half, reboot both halves, and reverse
   the USB orientation. Record all three results before adding visible VIA peer
   convergence and the remaining Stage 06 pull/reset/macro controls.

### 2026-09-04 — First usable live-edit milestone hardware-confirmed

- Real-board testing now confirms the two highest-priority user paths: an RGB
  color change applies live, and a normal base-layer keycode change applies and
  changes the character typed without reflashing.
- The second half was connected and functional during the test. This confirms
  the corrected split reachability needed by the profile apply and the standard
  VIA dynamic-keymap path used for the base key.
- This evidence accepts the first usable engineering milestone, not production
  closure. A custom `key_behaviors[]` row, reboot persistence, reconnect,
  opposite USB orientation, role swap, interrupted apply, and runtime
  high-water remain unverified on hardware.
- The implementation is saved in `97692e95` (`Apply authored layer keys live
  through VIA`); this follow-up checkpoint records the hardware outcome without
  folding unrelated generated preview/export changes into the milestone.

Next steps:

1. Pause feature expansion and audit the live-edit architecture and file growth
   before adding macros, defaults, combos, or layer-structure mutation.
2. Measure pointing report cadence and trace the master scan hot path so the
   reported mouse polling-frequency regression is explained with data rather
   than assumed to be acceptable.
3. Run the remaining reboot, USB-orientation, role-swap, custom-behavior, and
   recovery tests only after that audit identifies which live-edit work is
   allowed on steady-state scans and which must be deferred or removed.

### 2026-09-04 — Device-resident profile goal and authority replan

- Reassessed the first usable source-driven milestone against the intended
  product experience. The current firmware can persist and activate RGB and
  custom behaviors, expose generation/digest status, and read/write VIA layout
  keys, but Profile Studio cannot download the committed custom-profile payload
  or reconstruct the complete configuration from the keyboard.
- Accepted D-026: the exact committed device generation is the target authority
  for a connected live session. The three C files remain compiled defaults,
  recovery input, and explicit human-reviewable source import/export; the
  keyboard stores canonical structured values rather than literal C text.
- Added `docs/architecture/device-resident-profile.md` as the maintained target
  contract. It defines complete payload readback, a unified logical snapshot
  across Profile Wire and standard VIA, generation-bound drafts, stale-write
  refusal, explicit directional operations, reset semantics, performance
  requirements, and the end-to-end completion criterion.
- Updated the root README, both Profile Studio guides, architecture index and
  source map, active architecture assessment, decisions, and Stages 06–07 so
  they distinguish current source-driven behavior from the device-first goal.
  The old statement that C is the only live source of truth is no longer the
  documented target.
- Replanned Stage 06 to put polling instrumentation, steady-state performance
  remediation, chunked committed-profile readback, unified device snapshots,
  and source import/export before macros/defaults or further surface growth.
- This checkpoint changes documentation and architecture direction only. It
  does not claim that complete device readback, conflict-safe device-first
  editing, or the performance remediation has been implemented.

Next steps:

1. Add hardware-visible pointing/main-loop cadence measurements and regression
   thresholds, then remove unconditional idle metadata encoding and repeated
   live RGB materialization.
2. Design and implement a bounded Profile Wire read command for the exact
   committed payload, correlated by generation and digest.
3. Build the Studio device snapshot and generation-bound draft lifecycle before
   extending live coverage to macros, defaults, combos, or layer structure.

### 2026-09-04 — First-grade control-software product goal

- Promoted the project goal from device-resident live editing to first-grade
  Charybdis control software. The normal product journey starts from the
  connected keyboard and does not require an open firmware repository or C
  knowledge.
- Added `docs/tooling/PROFILE_STUDIO_PRODUCT_GOAL.md` as the maintained product
  contract. It specifies the complete connect/open/edit/preview/apply/readback
  journey, backup and restore, recovery, compatibility guidance, field
  classification, packaging boundary, performance bar, and completion
  criterion.
- Accepted D-027. One logical profile may use VIA and custom physical storage,
  but it now requires one authoritative manifest and coordinator covering all
  domain digests. External VIA writes must be adopted or exposed as conflicts;
  a partial cross-store write cannot be called a complete profile commit.
- Updated the root and Profile Studio documentation, device-resident technical
  contract, architecture index and assessment, active project overview, field
  classification, Stage 06, Stage 08, and risk register to follow that goal.
- Added R-22 for false cross-store atomicity and R-23 for remaining repo-bound.
  This checkpoint changes documentation and project acceptance only; it does
  not claim those risks are implemented or resolved.

Next steps:

1. Measure and remediate the observed pointing-cadence regression before
   expanding live functionality.
2. Freeze the logical-profile manifest, cross-store commit/recovery behavior,
   and external VIA edit semantics.
3. Implement complete device readback and a repository-independent device
   snapshot, ending in a real-keyboard open/edit/apply/readback checkpoint.

### 2026-09-04 — Cadence regression investigated, then parked; suite restored

- Restored the host suite. The previous checkpoint left the committed
  `LAYER_BASE` introspection SVG one glyph out of step with `keymap.c`, which
  failed `run_profile_introspection_checks.sh` and `run_tooling_checks.sh` and,
  under `set -eu`, stopped `run_all_host_tests.sh` at check 58 of 75. The same
  checkpoint added `live-link/runtime-cadence-v1.js` without registering it in
  the Studio check list or giving it a test, so 478 lines of wire decoding were
  not syntax-checked. Both are fixed in `5c6f5b7b`.
- Quantified the symptom against `refactor/aug` (`87f356cd`): reported mouse
  rate fell from ~450 Hz to ~300 Hz on the live-edit engineering build, which
  is about 1.1 ms of added work per main-loop iteration. Polling is unthrottled
  on both branches, so poll rate is main-loop rate.
- Bounded the split reconciler's steady-state cost with a new host probe
  (`8bf794cb`): a converged pair scanned across 5 s of elapsed time at 450 Hz
  performs 5 exchanges, 0 EEPROM reads and 0 writes across 2250 scans. The
  reconciler is eliminated as a cause and that budget is now enforced.
- Eliminated, with evidence rather than inspection: the pointing task, the poll
  throttle, split pointing, VIA macro reseeding, the split mirror step, and the
  layer key-LED map rebuild. Two hypotheses this pass raised were then
  falsified by measurement — per-batch RGB re-materialization and per-accessor
  `copy_active` are real waste but land in microseconds per second, and the
  reader-backed RGB view does not touch flash at all, because QMK's
  wear-levelling driver mirrors the whole 16 KiB logical EEPROM in RAM and
  `wear_leveling_read()` is a `memcpy` from that cache.
- Measured fresh RAM state and recorded it against R-07. The ordinary build
  holds 132 B of `.bss` policy slack; the live-owner build is 2,820 B over the
  `.bss` gate and 820 B over the combined gate, so it does not pass its own
  memory policy, while roughly 205 KB of physical SRAM stays unused.
- Parked the cadence regression by owner decision. Hardware bisect and the
  compile-gated on-device recorder were both declined. The candidate inventory
  of everything that newly runs every cycle, the eliminated causes, the
  uncosted remainder, and the available measurement paths are recorded in
  `pointing-cadence-investigation.md` so the next pass resumes instead of
  re-deriving.
- R-21 stays open and blocking for Stage 06 closure. Stage 06 now also carries
  the live-owner memory-policy failure as an explicit deliverable. No
  performance fix was applied, and no threshold is enforced yet.

Next steps:

1. Milestone 1 is parked, not met. Do not treat the live surface as cleared for
   expansion on cadence grounds.
2. Proceed to the logical-generation manifest across VIA and custom storage,
   including cross-store failure recovery and external VIA edit detection.
3. Resume R-21 from `pointing-cadence-investigation.md` when an owner-approved
   measurement path is available; the 1.1 ms magnitude is the constraint any
   candidate has to satisfy.

### 2026-09-04 — Memory policy corrected; runtime context zero-initialised

The memory gates were built on an unsound assumption, found while answering
what the real RP2040 RAM headroom is.

- `noah_runtime_singleton`, the 18,848 B runtime context, sat in `.data`
  because four members carried non-zero defaults. The linked initialiser image
  was **4 non-zero bytes out of 18,848**, so the build spent 18 KB of flash on
  a 99.98% zero image plus a boot-time copy of it, and charged those bytes to
  the `.data` side of the budget.
- That exposed the defect in the gate. `.bss` and `.data` placement is an
  initialisation artifact, not a memory property. Zeroing the initialiser moved
  the identical 18,848 B from `.data` to `.bss` and the `.bss` gate went from
  PASS at 25,868 B to FAIL at 44,716 B on a change that **reduced** total
  memory by 8 B and freed 18,796 B of flash. A gate that fails a build for
  using less memory is measuring the wrong thing.
- The static initialiser was also incomplete. It set four of the seven non-zero
  core defaults that `key_runtime_core_state_reset()` sets, and no production
  path called that reset, so firmware booted with
  `token_allocation_failed_packed_key_pos`, `preview_display_last_semantic_layer`
  and `preview_display_bridge_layer` at `0` instead of their `UINT8_MAX`
  sentinels. Every host test calls the reset and therefore could never see it.
  The two preview fields can produce a brief wrong preview layer at boot; the
  third is inert given its only consumers. Production and test now share one
  definition of the defaults.

Landed:

- `noah_runtime_singleton` is zero-initialised. `noah_runtime_shared_state_post_init()`
  applies the core defaults and runs first in `noah_keyboard_post_init_user()`,
  declared on the public `runtime_reset.h` seam so the internal-header layering
  gate stays satisfied. The init-order test records the new first stage.
- `check_firmware_memory_budget.py` enforces static RAM as `.data` + `.bss` and
  the core-memory floor. The `.bss`-only limit is retained as an opt-in flag,
  no longer enforced by default, and the rationale the limits never carried is
  now in the module docstring.

Measured after the change: ordinary `.data`+`.bss` 48,864 B against 51,000 B
(2,136 B slack, PASS) and flash load image 163,780 B down to 144,984 B. The
live-owner build is 51,820 B, **820 B over the static RAM gate** — a real
overage, where the old report showed a misleading 2,820 B `.bss` failure.

Verification: `run_all_host_tests.sh`, `run_runtime_init_order_tests.sh`,
`run_feature_gate_compile_tests.sh`, fresh `qmk compile`, memory and stack
budget checks, `git diff --check`.

Next steps:

1. Close the live-owner build's 820 B static RAM overage, or re-derive the
   51,000 B ceiling with recorded rationale. Physical headroom is ~205 KB, so
   this is a policy decision, not a hardware one.
2. Then the logical-generation manifest across VIA and custom storage.
3. R-21 stays parked and blocking; see `pointing-cadence-investigation.md`.

### 2026-09-04 — Memory gates re-derived from measured demand

Follow-up to the policy correction above, on the principle that real headroom
must not be reported as a constraint.

- Measured what actually consumes the linker-managed arena. `__heap_base__` to
  the end of SRAM0-3 is newlib's `malloc` region reached through `_sbrk`. The
  only reachable caller in this firmware is `rand()` from `rgb_task_render`,
  which allocates its reentrancy state once, on the order of tens of bytes.
  `srand` is never called and ChibiOS core/heap allocation is not linked in.
  The old 204,800 B floor reserved roughly 200 KB of SRAM for that.
- Because the floor implicitly capped static RAM near 51 KB, the live-owner
  build's 820 B "overage" was an artifact of an undocumented reservation, not a
  memory shortage.

Landed:

- The arena floor is now a derived safety guard at 4 KiB, about two orders of
  magnitude above observed demand.
- Static RAM is one regression tripwire at 57,344 B, sitting above the largest
  supported variant, explicitly documented as a change detector rather than a
  hardware limit, to be raised deliberately with a recorded reason.
- Every run now prints policy distance and **true static RAM headroom** as
  separate lines, so R-07's confusion is structural rather than a footnote.

Measured after the change, both PASS: ordinary static RAM 48,872 B with
209,168 B of true headroom; live-owner 51,820 B with 206,224 B. SRAM0-3 is
262,144 B. The Stage 06 memory-policy deliverable is closed; it described a
constraint that did not exist.

Verification: `run_all_host_tests.sh`, fresh ordinary and live-owner
`qmk compile`, memory and stack budget checks, `git diff --check`.

Next steps:

1. Proceed to the logical-generation manifest across VIA and custom storage.
2. R-21 stays parked and blocking; see `pointing-cadence-investigation.md`.

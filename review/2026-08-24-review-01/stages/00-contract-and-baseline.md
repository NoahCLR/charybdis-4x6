# Stage 00 — Contract And Baseline

Status: in progress

## Objective

Turn the architecture direction into measured, testable contracts before
production storage or protocol code lands.

This stage closes open decisions D-009 through D-016 and produces the budgets
and fixtures every later stage depends on.

## Entry Criteria

- Project README.md, decisions.md, risks.md, and
  userspace-architecture-review.md have been reviewed against the current tree.
- The worktree state is understood and unrelated changes are preserved.
- The current Review 19 prerequisite status is recorded.

## Work Packages

### 00A — Current Resource And Storage Map

Measure from fresh ordinary and instrumented target builds:

- firmware text and read-only data;
- per-half RP2040 physical banks, fixed linked sections, the SRAM0–3
  `.data + .bss` regression metric, and the SRAM0–3 linker/core-memory span at
  boot;
- regression-policy thresholds separately from physical capacity and runtime
  high-water evidence;
- interrupt, process, core-1, and split-thread stack allocations plus reviewed
  path budgets;
- named large runtime symbols;
- total logical EEPROM and backing flash;
- exact EECONFIG, VIA config, dynamic keymap, encoder, macro, and free address
  ranges;
- current macro defaults and representative live macro usage;
- the cost of increasing logical EEPROM, if considered.

Create a checked address-map document or compile-time report. Do not rely on a
stale ELF or GNU size's informational BSS number: that number includes the
linker-reserved core-memory span and stack sections and must not be added to
the separately reported linked regions.

### 00B — Canonical Model Inventory

Map every currently parsed Profile Studio field to one category:

- Milestone A RGB
- Milestone A key behavior
- later live policy
- standard VIA-owned state
- compiled capability ceiling
- compiled safety ceiling
- source-only metadata
- executable firmware and therefore not live

For every live field, record:

- stable semantic type and range;
- default source location;
- runtime consumer;
- validation owner;
- cache or lifecycle invalidation;
- persistence and split requirement.

### 00C — Profile Wire v1 Proposal

Specify:

- version and compatibility fields;
- capability response;
- domain ids and deterministic order;
- fixed-width encodings and byte order;
- sparse versus fixed records;
- string policy, if any;
- profile, domain, and candidate digests;
- transaction and error ids;
- unknown-field behavior;
- maximum candidate size;
- migration and factory-reset behavior.

Produce golden byte fixtures for at least:

- empty/default metadata;
- representative RGB profile;
- representative behavior with tap, repeat hold, long hold, timing overrides,
  and layer or macro action;
- maximum-size valid records;
- truncated, overlong, duplicate, unknown, and cross-reference-invalid input.

### 00D — Storage And Activation Decision

Resolve D-009 and D-010 with measured options. The chosen design must specify:

- exact EEPROM ranges and compile-time non-overlap checks;
- last-known-good and candidate layout;
- power-loss behavior at every write boundary;
- checksum and generation metadata;
- compiled-default fallback;
- candidate staging memory and stack cost;
- fixed capacities reported to Studio.

Define the quiescence predicate and activation publication at contract level.
Implementation belongs to Stage 02.

### 00E — Desktop Transport Spike

Prototype device enumeration and one VIA protocol-version request through an
isolated adapter. The spike may use a temporary harness, but the decision must
record:

- supported platform and architecture;
- VS Code extension-host compatibility;
- packaging implications;
- device identity filters;
- permission behavior;
- disconnect behavior;
- VIA contention behavior;
- fake adapter interface.

Do not land UI-specific HID calls or freeze a bulk protocol based solely on the
spike.

### 00F — Split And Authority Decisions

Resolve:

- D-011 split integration shape;
- D-013 preview/apply ordering;
- D-014 generation and drift authority.

Write state tables for source, compiled defaults, USB half, peer half, preview,
pending commit, and partial failure.

## Expected Files

Planning and measurement work primarily updates this review folder, test
fixtures, and resource tooling. Production profile runtime modules should not
be created in this stage unless a tiny compile probe is necessary to measure a
decision.

Likely references:

- users/noah/config.h
- users/noah/rules.mk
- users/noah/lib/compat/qmk_via_storage_contract.h
- users/noah/lib/compat/qmk_via_storage_regions.c
- users/noah/lib/compat/qmk_via_split_sync.c
- tools/check_firmware_memory_budget.py
- tools/check_firmware_stack_budget.py
- tools/charybdis-profile-studio/extension.js
- sibling QMK VIA, EEPROM, Raw HID, combo-introspection, and auto-mouse code

Do not edit sibling QMK in this stage.

## Deliverables

- [x] Fresh resource report — `../stage-00-baseline.md`
- [x] Corrected bank and policy model — D-016 and
  `../stage-00-baseline.md`
- [x] Exact EEPROM address map — `../stage-00-baseline.md`
- [x] Compile-time storage partition contract —
  `users/noah/lib/profile/storage/profile_storage_layout.h` with focused host
  compile guards
- [x] Field classification matrix — `../field-classification.md`
- [x] Profile Wire v1 specification — `../profile-wire-v1.md`
- [x] Golden fixture plan — required fixture list in `../profile-wire-v1.md`
  plus executable capability/status reads in
  `tests/fixtures/profile_wire_v1_reads.fixture`
- [x] Storage and power-loss decision — D-009
- [x] Fixed capacity decision — D-010
- [x] Safe activation contract — `../authority-state-table.md`
- [ ] HID adapter spike result — the fake/coordinator and lazy `node-hid`
  adapter are implemented and packaged; native read-only enumeration succeeds,
  but no matching attached interface was present and the real-board VS Code
  host probe is still required
- [x] Split integration decision — D-011
- [x] Source/device authority state table — `../authority-state-table.md`
- [ ] Updated decisions.md, risks.md, architecture review, and progress.md —
  decision/progress updates in progress with first implementation packages

## Verification

Baseline evidence captured on 2026-08-25:

- `sh tests/host/run_all_host_tests.sh` — pass
- `sh tests/host/run_profile_storage_layout_tests.sh` — pass
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — pass
- `sh tests/host/run_firmware_memory_budget_checks.sh` — pass
- `sh tests/host/run_firmware_stack_budget_checks.sh` — pass

Measured values and the exact sibling fork commit are recorded in
`../stage-00-baseline.md`.

At minimum:

- fresh qmk compile -kb bastardkb/charybdis/4x6 -km noah
- sh tests/host/run_firmware_memory_budget_checks.sh
- sh tests/host/run_firmware_stack_budget_checks.sh after the required fresh
  instrumented build
- current full host suite baseline
- existing QMK contract and split-sync runners
- HID spike with a real board, explicitly marked as exploratory
- git diff --check

Name any sibling-QMK inspection and confirm whether sibling files remained
unchanged.

## Exit Criteria

Stage 00 is complete only when:

- D-009 through D-016 are accepted or explicitly deferred with a blocker that
  prevents dependent implementation;
- storage regions and capacities fit measured resource budgets;
- Profile Wire v1 can encode the Milestone A model without native C layout;
- source/device and split authority have deterministic state transitions;
- the safe activation contract names its required runtime evidence;
- a viable packaged or packageable HID path has been demonstrated;
- later agents can implement without inventing protocol fundamentals.

## Handoff

The handoff names the first Stage 01 work package and lists every frozen v1
contract. Any intentionally unfrozen detail must have an owner and must not
affect persisted or wire compatibility.

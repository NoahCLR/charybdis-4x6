# Stage 00 — Contract And Baseline

Status: planned

## Objective

Turn the architecture direction into measured, testable contracts before
production storage or protocol code lands.

This stage closes open decisions D-009 through D-014 and produces the budgets
and fixtures every later stage depends on.

## Entry Criteria

- Project README.md, decisions.md, risks.md, and
  userspace-architecture-review.md have been reviewed against the current tree.
- The worktree state is understood and unrelated changes are preserved.
- The current Review 19 prerequisite status is recorded.

## Work Packages

### 00A — Current Resource And Storage Map

Measure from fresh ordinary and instrumented target builds:

- firmware text, data, BSS, true static RAM, linker heap, and stack reserves;
- named large runtime symbols;
- total logical EEPROM and backing flash;
- exact EECONFIG, VIA config, dynamic keymap, encoder, macro, and free address
  ranges;
- current macro defaults and representative live macro usage;
- the cost of increasing logical EEPROM, if considered.

Create a checked address-map document or compile-time report. Do not rely on a
stale ELF or GNU size's informational BSS number without separating the
linker-reserved heap.

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

- [ ] Fresh resource report
- [ ] Exact EEPROM address map
- [ ] Field classification matrix
- [ ] Profile Wire v1 specification
- [ ] Golden fixture plan
- [ ] Storage and power-loss decision
- [ ] Fixed capacity decision
- [ ] Safe activation contract
- [ ] HID adapter decision and spike result
- [ ] Split integration decision
- [ ] Source/device authority state table
- [ ] Updated decisions.md, risks.md, architecture review, and progress.md

## Verification

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

- D-009 through D-014 are accepted or explicitly deferred with a blocker that
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

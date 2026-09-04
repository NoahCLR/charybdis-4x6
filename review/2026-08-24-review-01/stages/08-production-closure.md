# Stage 08 — Production Closure And Migration

Status: blocked on Stage 07

## Objective

Prove the complete live-profile architecture is maintainable, recoverable,
resource-safe, documented, and ready to close.

No new live domain is added in this stage unless closure testing exposes a
missing requirement that invalidates an earlier claim.

## Entry Criteria

- Stages 00 through 07 have recorded exit evidence.
- All intended live, compiled-capability, safety, source-only, and executable
  boundaries are documented.
- The latest tree and review folder are internally coherent.

## Closure Work

### Compatibility And Migration

Test and document:

- no persisted live profile;
- valid Profile Wire v1;
- corrupt metadata, checksum, and body;
- unknown compatible field;
- unknown incompatible schema;
- capacity decrease and increase;
- firmware flash with changed compiled defaults;
- factory reset;
- source push after reset;
- downgrade behavior;
- role swap during or after migration.

### Resource And Performance

Measure:

- final firmware flash;
- per-half RP2040 bank occupancy, fixed linked sections, and policy margins;
- SRAM0–3 linker/core-memory span at boot plus allocator high-water on real
  hardware;
- physical interrupt/process/split stack allocations, reviewed-path policy
  margins, and hardware stack high-water;
- EEPROM region utilization;
- worst-case candidate and profile sizes;
- behavior lookup cost;
- RGB frame cost;
- Raw HID transfer and commit duration;
- split convergence duration;
- wear implications of preview and apply frequency.

Update automated budgets so future regressions fail mechanically.

### Architecture Enforcement

Add or confirm gates for:

- direct profile-array reads outside allowed default/provider code;
- wire/storage struct layout leakage;
- storage region overlap;
- region/effect/handler/digest alignment;
- missing source manifest entries;
- protocol version changes without fixtures/migration;
- fixed capacity mismatches between firmware and Studio;
- unhandled provider generation invalidation.

### Documentation

Update:

- root README
- Profile Studio guide and package README
- architecture source map, runtime flow, and change guide
- key runtime, RGB, interaction, pointer, macro, VIA, and split docs where
  behavior changed
- setup, first connection, open/refresh from keyboard, preview, apply to
  keyboard, explicit source import/export, reset, recovery, limits, and
  troubleshooting
- screenshots and hover states
- generated profile docs where authored inputs changed

### Product Experience And Packaging

Prove the critical user journeys in
`docs/tooling/PROFILE_STUDIO_PRODUCT_GOAL.md`:

- first connection and complete open from keyboard;
- edit, validation, preview, undo/redo, and verified apply;
- stale draft, external VIA edit, missing peer, disconnect, and partial failure;
- named backup, restore, import/export, and factory reset;
- incompatible firmware/schema guidance and recovery;
- normal configuration without an open firmware repository;
- accessible, actionable primary UI with development diagnostics kept
  available but secondary.

The reusable transport, schema, device-session, and profile-model core must not
depend on VS Code workspace parsing. The VS Code extension may remain a
supported shell, but it cannot be the only path to the repository-independent
normal workflow required by the product goal.

### Hardware Closure

Run the union of:

- Review 19 prerequisite hardware evidence;
- Milestone A matrix;
- Stage 06 layout/macro/defaults matrix;
- Stage 07 combo/layer matrix;
- migration, corruption, downgrade, and factory-reset cases;
- both USB orientations, forced roles, dual USB, reconnect, and prolonged use.

Record firmware hashes or commits and exact board setup.

## Deliverables

- [ ] Compatibility and migration matrix
- [ ] Final resource and performance report
- [ ] Mechanical architecture gates
- [ ] Complete user and developer documentation
- [ ] Product-journey and repository-independent packaging evidence
- [ ] Final screenshots and generated docs
- [ ] Full automated verification
- [ ] Full hardware verification
- [ ] Risks closed with evidence
- [ ] Decisions reconciled with landed tree
- [ ] Closure verdict in progress.md and userspace-architecture-review.md

## Verification

Required:

- every targeted runner named by changed subsystems
- schema, store, protocol, provider, split, Studio, migration, and architecture
  gates
- npm run check and screenshots with real hover captures where needed
- profile introspection write/check when authored inputs changed
- all profile validation and compile gates required by current AGENTS.md
- full host suite
- fresh ordinary firmware compile and memory gate
- fresh instrumented stack build and stack gate
- qmk compile -kb bastardkb/charybdis/4x6 -km noah
- complete hardware closure matrix
- git diff --check

## Closure Bar

The project can close only when:

- the current code matches the intended architecture;
- all live surfaces and explicit non-live boundaries are accurate;
- compiled defaults, persisted state, source state, USB half, and peer half have
  deterministic authority and recovery;
- tests and compile gates mechanically enforce the major seam claims;
- all required resource budgets pass on fresh artifacts;
- full host suite and firmware compile pass;
- all required hardware matrices pass;
- docs and this review folder match the current tree;
- progress.md includes final verification and no required next work remains.

After a closure verdict, this folder becomes immutable history. Any later
architecture or remediation opens the next sortable review folder.

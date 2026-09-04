# Stage 06 — Live Defaults, Layout, And Macros

Status: replanned — performance remediation and device readback precede further
surface expansion

## Objective

Establish the device-first editing lifecycle, then bring the lower-risk
remaining Profile Studio surfaces into one logical profile without replacing
standard VIA ownership.

This stage integrates existing live layout and VIA macro capabilities and
converts practical config defaults into runtime policy where feasible.

## Entry Criteria

- Milestone A is complete and remains green.
- Standard VIA keymap and macro storage still pass their persistence and split
  contracts.
- The Stage 00 field classification matrix is current.

## Layout And VIA Macros

Layout keys and VIA macros already have live EEPROM representations. Profile
Studio should:

- send standard VIA commands through the Live Link adapter;
- read device state as the connected editing baseline;
- retain source parsing as explicit import and canonical source export;
- show generation-aware source/device semantic diff;
- support device apply, refresh, source import/export, reset, retry, local
  commit, and peer convergence states;
- batch layout changes safely and preserve layer/keycode validation;
- encode macro payloads using the established QMK/VIA byte grammar;
- invalidate macro caches after accepted mutation;
- avoid a second custom storage representation for the same state.

Existing custom split reconciliation remains authoritative for durable peer
repair.

## Config Field Classification

For every Defaults control, implement or document one category:

### Live policy candidates

- QMK tapping term through a supported dynamic or per-key callback
- combo term through the per-combo callback path
- custom behavior default timings
- normal, sniping, drag-scroll, and pointing-mode DPI policy
- auto-sniping enable and layer
- auto-mouse enable, target layer, and timeout
- default RGB appearance within compiled safety limits
- runtime feedback-stage enable flags
- feedback flash period
- auto-mouse RGB dead time
- RGB inactivity behavior if a safe runtime seam exists

### Compiled capability or safety

- maximum tap-count depth
- maximum logical layers
- feature code inclusion
- hard electrical brightness ceiling
- resource-sensitive RGB task cadence unless an upstream-safe runtime hook is
  deliberately added

Each live field needs a runtime getter or application owner. Do not scatter
global mutable variables under old macro names.

## Runtime Application

Changing a policy applies its side effects explicitly:

- recompute and apply current CPI;
- safely move or disable auto-mouse target layer;
- update timers without underflow or stale progress;
- invalidate timing views or RGB caches;
- defer behavior-affecting defaults to the strict safe boundary;
- preserve compiled safety ceilings.

## Likely Files

- Profile Studio layout, macro, defaults, diff, and operation coordination
- users/noah/lib/profile/ policy domain
- key behavior default resolution
- pointing compat and policy modules
- auto-mouse compat wrappers
- RGB runtime configuration wrappers
- standard VIA command and split tests
- authored profile docs and tooling docs

## Deliverables

- [x] Standard VIA layout source push with semantic diff and connected-half readback
- [ ] Steady-state pointing cadence instrumentation and accepted regression
      threshold — **parked 2026-09-04**; R-21 occurred and is unexplained. See
      `../pointing-cadence-investigation.md`
- [ ] Live-owner build passes its own memory policy (`.bss` and `.data`+`.bss`
      gates currently fail; see the same file)
- [ ] Idle profile scheduler and live RGB materialization remediation
- [ ] Chunked committed custom-profile readback
- [ ] Unified device snapshot across Profile Wire and standard VIA
- [ ] One logical-generation manifest and coordinator across custom and VIA
      stores, including explicit handling of writes made by another VIA client
- [ ] Generation-bound drafts and stale-write refusal
- [ ] Explicit source import and canonical source export
- [ ] Visible peer-convergence evidence, layout pull/reset/retry controls
- [ ] Standard VIA macro Live Link integration
- [ ] Device/source diff, device refresh/apply, source import/export, reset, and recovery
- [ ] Every Defaults field classified
- [ ] Practical live-policy fields implemented
- [ ] Compiled capacity and safety fields visibly identified in Studio
- [ ] Side-effect application and safe-boundary tests
- [ ] Updated docs, screenshots, risks, decisions, stage, and progress

## Verification

Required coverage includes:

- standard VIA keymap and macro command tests
- macro provider, defaults, payload, dispatch, and lifecycle tests
- real-profile validation
- tapping, combo, behavior-default, pointing, auto-mouse, and RGB targeted
  tests for changed fields
- split persistence, reconnect, and role swap
- fake-device UI operations and partial failures
- Profile Studio checks and screenshots
- feature gates
- full host suite
- firmware compile and fresh resource gates
- real-board layout, macro, timing, pointer, auto-mouse, and RGB-default matrix
- Milestone A regression matrix
- git diff --check

## Exit Criteria

- Layout and VIA macros can be edited live from Profile Studio without a
  second persistence system.
- A connected session opens from readable device state without requiring the C
  files to match or exist.
- Layout, macros, custom domains, and policy are covered by one truthful logical
  generation, or a partial/cross-store state is visibly refused rather than
  reported as an atomic commit.
- Source and both halves reconcile visibly through explicit directional actions.
- Every Defaults control is either live with tested side effects or clearly
  labeled as a compiled capability/safety field requiring flash.
- Milestone A remains green.
- Resource and hardware evidence are recorded.

## Handoff

Stage 07 receives a unified user experience for existing live data and runtime
policy. It may now tackle dynamic structures whose indices cross-reference
multiple domains.

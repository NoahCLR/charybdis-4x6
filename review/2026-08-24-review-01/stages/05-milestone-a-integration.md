# Stage 05 — Milestone A Integration And Hardware Closure

Status: in progress; first two-half hardware test ready

## Objective

Turn the live RGB and live key-behavior vertical slices into one coherent,
daily-use feature and satisfy every Milestone A criterion in README.md.

This stage owns integration, recovery, source/device UX, diagnostics, resource
closure, and hardware proof. It does not add new profile domains merely to make
the milestone look broader.

## Entry Criteria

- Stage 03 exit evidence is complete.
- Stage 04 exit evidence is complete.
- Both domains share Profile Wire v1, store, protocol, provider publication,
  generation, and split reconciliation.
- Remaining risks for RGB and behaviors are listed with concrete closure
  matrices.

## Integration Scenarios

### Combined Candidate

One candidate may change RGB and behavior fields together. The system must:

- validate both domains and all cross-references before activation;
- reject the whole candidate when either domain is invalid;
- publish both domains as one generation;
- invalidate RGB and behavior-derived caches after publication;
- persist and split-sync one committed identity;
- pull or push the matching semantic source changes as one user operation.

### Source And Device Coordinator

Finish the Stage 00-selected operation semantics for:

- draft only;
- preview live;
- cancel preview;
- apply to device;
- persist;
- update source;
- push source to device;
- pull device to source;
- reset device to compiled defaults;
- retry local persistence;
- retry peer convergence;
- recover from source-success/device-failure;
- recover from device-success/source-failure.

No status may collapse local commit and peer convergence into one success unless
both are proven.

### Diagnostics

Profile Studio and firmware diagnostics must expose:

- protocol and schema;
- capacities and feature flags;
- source and compiled-default digests;
- active, pending, committed, and peer generations/digests;
- preview state;
- safe-boundary wait reason;
- last validation, protocol, storage, activation, and split error;
- reset/fallback reason.

### Recovery

Exercise:

- unplug during candidate upload;
- unplug while waiting for safe activation;
- reset during candidate persistence at each durable transition;
- disconnect between local commit and peer convergence;
- peer absent across several edits;
- reconnect with older, newer, equal, and disagreeing generations;
- forced master and forced slave;
- both halves connected to USB;
- incompatible schema after a new firmware flash;
- corrupt persistent header or checksum;
- factory reset and source repush.

## User Experience Bar

The normal path should read as:

1. Connected — compatible.
2. Draft differs from source/device.
3. Previewing on keyboard, when supported.
4. Waiting for keys to be released, when required.
5. Active on USB half.
6. Persisted.
7. Synchronized to peer.
8. Source and device agree.

Errors name the incomplete step and retain an explicit retry or reset path.

## Likely Files

- profile operation coordinator in tools/charybdis-profile-studio/
- source patch transactions and semantic diff
- combined profile schema/validation/provider code
- runtime diagnostics and trace events
- split convergence diagnostics
- recovery and integration tests
- Profile Studio UI, docs, and screenshots
- README.md and relevant docs/ user-facing guides
- this review folder

## Deliverables

- [ ] Combined RGB plus behavior transaction
- [ ] Whole-candidate rejection and atomic publication
- [ ] Complete source/device operation coordinator
- [ ] Partial-failure recovery UX
- [ ] Full diagnostics
- [ ] Recovery and migration behavior
- [ ] Automated integration matrix
- [ ] Fresh target resource closure
- [ ] Milestone A real-hardware matrix
- [ ] Updated root/user docs, screenshots, review findings, risks, and progress

## Verification

In addition to every Stage 03 and 04 runner:

- combined candidate integration tests
- candidate/source/device partial-failure tests
- fake-device end-to-end UI tests
- protocol retry and recovery tests
- storage interruption tests at every commit transition
- full split convergence and role-swap suite
- runtime diagnostics and trace tests
- Profile Studio npm run check and screenshots
- actual hover/tooltip captures for changed hover content
- profile introspection and all-profile validation where source changes
- feature gates
- full host suite
- all-profile compile checks required by current AGENTS.md
- fresh ordinary firmware compile and memory gate
- fresh instrumented stack build and stack gate
- qmk compile -kb bastardkb/charybdis/4x6 -km noah
- complete hardware matrix on the real split board
- git diff --check

## Milestone A Hardware Matrix

Record each row for both USB orientations where applicable:

| Scenario | Required observation |
| --- | --- |
| RGB preview and cancel | both halves preview coherently and restore committed state |
| RGB persist and reboot | committed rendering returns after power cycle |
| Behavior apply while idle | new action becomes active once and persists |
| Behavior apply while physical key held | old behavior completes; activation waits; no stuck output |
| Behavior apply during repeat/macro/combo/mode/layer ownership | activation waits or rejects per contract; all outputs release |
| Combined RGB plus behavior candidate | both switch to one generation |
| Peer unplugged during commit | USB half commits; peer converges after reconnect |
| USB role swap | newest valid committed generation wins according to authority rules |
| Dual USB | no split-brain or unsafe simultaneous host mutation |
| Reset during persistence | old, new, or compiled default state selected per recovery contract |
| Corrupt/incompatible store | compiled defaults load and diagnostics explain fallback |
| Push, pull, partial source failure | Studio reports exact state and recovery action |

## Exit Criteria

Milestone A closes only when:

- every README.md milestone criterion has named evidence;
- all combined automated and hardware matrices pass;
- root and tooling documentation describe the actual UX and limitations;
- fresh target resource budgets and firmware compile pass;
- risks R-01 through R-13 are closed or explicitly shown not to block this
  milestone;
- progress.md records a closure verdict and remaining later-stage scope;
- the architecture review agrees with the landed tree.

Once this folder records a final project closure verdict it becomes immutable.
Milestone A completion alone does not close the whole project; later stages may
continue in this folder until Stage 08.

## Handoff

Stage 06 begins from a stable released Milestone A. New domains reuse the
effective-profile and source/device architecture and may not regress the
Milestone A hardware matrix.

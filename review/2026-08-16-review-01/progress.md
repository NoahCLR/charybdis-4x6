# Follow-Up Architecture Audit Progress

## Why This Review Exists

`aae9c445` landed the four software remediations from Review 17. This pass
audits the code that remediation produced, using
`prompts/follow-up-architecture-audit.md`.

Review 17 was not continued. Its software findings are closed and its only
remaining obligations are physical hardware matrices, so an audit of newly
landed code is materially different work. Review 17 carries a reconciliation
note pointing to this folder. Its own findings section remains the audit-time
snapshot of the tree at `709d942e`.

## 2026-08-16 — Audit Pass

### Method

Four parallel read-only auditors were dispatched across key runtime/ownership,
split/RGB/pointing, architecture/interfaces, and tests/gates/docs. Three
terminated immediately on an API session usage limit and returned nothing. One
covering test and gate coverage was relaunched.

The areas below were therefore audited directly. Every finding recorded was
verified by reading the actual code path and attempting to disprove it first.

### Covered By The Relaunched Auditor

Host-suite stubbing strategy, the `rg`-based compile guards, and the firmware
budget tooling. All three of its findings were re-verified by hand — against the
scripts, and against the linked ELF with `objdump`/`nm` — before being recorded.
One was re-characterized: the memory-gate blind spot is bounded by the heap
minimum, not open-ended, so the mutation it described would pass only up to
roughly 7.5 KB.

### Covered Directly

- Cross-context publication scheme: both shapes, every call site of the parity
  helpers, slot selection, and reader retry behavior.
- The report-ownership seam from `aae9c445`, including whether an event can
  settle twice, checked against upstream QMK control flow in
  `../bastardkb-qmk/quantum/action.c` rather than against assumption.
- Production readers of `split_runtime_sync_remote`, to test the prior claim
  that some published base-domain state is never consumed.
- Host test runner coverage: every runner and every test translation unit.

### Not Covered

Recorded as audit debt in the review note. Architecture and module boundaries,
API and interface cleanliness across `users/noah/`, macro and VIA subsystem
correctness, and documentation accuracy beyond the files `aae9c445` touched.

### Findings Recorded

1. **Should fix:** `users/noah/lib/action/synthetic_record.c` is never linked
   into any host binary; eight test units stub `noah_synthetic_record_active()`
   to always return false. `process.c` branches on it at four points, including
   the guard on the new report-ownership seam, so that guard has no behavioral
   coverage at all.
2. **Should fix:** three compile-gate guards use `if rg …; then fail; fi`, which
   reports clean when `rg` exits non-zero for any reason. No `command -v`
   preflight exists. These were passing vacuously before ripgrep was installed.
3. **Should fix:** the coherent-read helpers write the caller's destination
   before re-checking the generation, so an exhausted retry budget leaves a torn
   cache. Both the helper contract and the RGB caller comments claim the
   opposite. The reachable case is safe because the in-flight pre-check skips
   the copy entirely; the gap is between the code and its stated invariant.
4. **Should fix:** the memory gate measures `.bss` only, while the 20 KB runtime
   singleton lives in `.data`. Growth there is bounded indirectly by the heap
   minimum, leaving about 7,512 B of silent headroom.
5. **Optional:** four base-domain fields in `split_runtime_sync_remote` are
   written and never read in production, and `base_generation` has no production
   reader because every live base-domain reader takes a single field.

### Verified Solid

- Publication shape parity. The in-place and slot shapes carry incompatible
  parity conventions, and mixing them would be a serious bug. Every call site
  was traced; no cross-application exists.
- The ownership seam settles each event exactly once. Upstream returns early on
  a false result without running post-process, which is why the explicit
  finalize call in `users/noah/hooks.c` is correct rather than a double-settle.
- Host runner coverage is complete.

### Verification

No source was changed during this pass, so no test or build gate was re-run.
The gates that certify the audited tree are recorded against `aae9c445` in
`review/2026-08-15-review-17/progress.md`: full host suite, target compile,
memory budget, and fresh linked stack budget, all green.

## Current Verdict

The audit is **open and incomplete**. What was examined holds up. Three of four
planned areas were never examined, so this folder closes nothing and must not be
read as a clean bill of health.

## Next Steps

1. Decide the coherent-read contract: stage reads into a local and commit only
   on a settled generation, or correct the comments to state what is actually
   guaranteed and why exhaustion is unreachable.
2. Decide the dead base-domain state and `base_generation` together.
3. Give `synthetic_record.c` behavioral coverage and add the `rg` preflight.
   Both are small and both close gaps where a gate currently proves less than
   its name suggests.
4. Run the still-unaudited areas: architecture and boundaries, API and interface
   cleanliness, macro and VIA correctness, documentation accuracy.
5. Only after software closure, run the Finding 05 and Finding 10 physical
   matrices.

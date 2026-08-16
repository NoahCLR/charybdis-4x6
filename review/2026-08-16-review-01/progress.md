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

## 2026-08-16 — Full Pass Completed

The earlier partial pass was completed. Five auditors covered key runtime and
ownership, split/RGB/pointing, macro and VIA/compat, architecture boundaries and
documentation, and test and gate coverage. An initial four-way fan-out died on an
API session usage limit and was relaunched.

Every finding was re-verified by hand before recording. Two auditor
characterizations were corrected rather than repeated: the memory-gate blind spot
is bounded by the heap minimum rather than open-ended, and a suspected
feature-flag coverage gap turned out to be covered by individual test runners.

**Two must-fix defects found**, both in older code and both requiring an
interleaving the tests do not construct:

1. Token replacement orphans held-action and repeat leases, leaving an action
   registered. Reachable in this profile through combo-origin keypos rewriting.
2. The VIA split master cannot renegotiate after the peer loses its snapshot
   session, retrying one chunk at 1 Hz forever. This blocks Finding 05's pending
   hardware matrix, which exercises exactly that condition.

Ten should-fix and nine optional items are recorded in the review note, along
with a substantial Verified Solid section covering boundaries, build integrity,
VIA frame validation, macro lifecycle, chunked rendering, and the two mechanisms
introduced by `aae9c445`.

One documentation miss was self-inflicted: `aae9c445` updated
`Sol Findings/implementation-progress.md` but left the roadmap saying Review 17
regressed Finding 08.

## 2026-08-16 — Implementation Pass 1: the two must-fixes

Landed in `474651bc`. Both were driven by a failing test first.

### Must-fix 1 — held actions survive token replacement

`key_runtime_core_adopt_runtime_owned_state_leases()` in
`users/noah/lib/key/runtime/reducer/ownership_state.c` hands held-action and
repeat leases from a cancelled press token to the token replacing it, called from
`key_runtime_core_press_token_begin()`.

Adoption rather than clearing, deliberately. The by-token sweep excludes these
kinds because their applied bindings are retired by a planned release effect, and
the observe path has no effect plan. Clearing the lease there would leave the
action registered with nothing able to release it — worse than the original bug.
The physical key never went up, so the action should stay held; the replacing
token's own release now retires it by key position on the normal path.

Test: `test_replaced_press_token_still_releases_owned_state()` in
`tests/host/key_runtime_scenario_test.c`. Proven load-bearing — with the adoption
call neutered, the replace-then-release sequence emits **zero** effects.

Note on test placement: this was first written against `runtime_debug_test.c`
asserting `core_lease_count`, which was the wrong observable. That harness does
not run the applied registry, so lease teardown there depends on a simulated
callback and the test would only have been checking the simulation. The scenario
harness runs the real projection while capturing applied effects, so the
assertion is on the emitted `RELEASE_OWNED_STATE_BY_KEY`.

### Must-fix 2 — the split master renegotiates after peer session loss

`noah_qmk_via_response_requires_new_session()` and
`noah_qmk_via_abandon_session()` in `users/noah/lib/compat/qmk_via_split_sync.c`.
All three tick paths — push chunk, push commit, pull chunk — now distinguish a
peer that reports `SNAPSHOT_REQUIRED` from a generic failure, and restart from
metadata instead of retrying a frame that can never be accepted. Transport
failures still take the plain retry, and the backoff is kept so a peer that
cannot accept a session does not spin.

Harness: the fake peer in `tests/host/qmk_via_split_sync_test.c` now owns a
snapshot session, opened by `SNAPSHOT_BEGIN` and losable once mid-transfer, and
replies with the same `ERROR`/`SNAPSHOT_REQUIRED` the real receiver gives. The
previous peer was stateless and unconditionally acknowledged, so peer-side
session loss could not be expressed at all.

**The first version of this test passed against the broken code.** It compared
the mirrored regions, which already matched because an earlier push had completed
before the session loss. Tracing the transmit phase showed the real behavior:
from t=1600 ms onward the master sat in `PUSH_CHUNK` indefinitely, retry count
climbing 5 → 17, `SNAPSHOT_BEGIN` count frozen, peer session dead. The assertion
was rewritten around the one thing only a fresh `SNAPSHOT_BEGIN` can restore —
the peer's session becoming active again. Recorded because it is the same failure
mode this review is about: a green test proving less than it appears to.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0, no failures
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_memory_budget_checks.sh` —
  BSS 25,524 B unchanged, heap 212,312 B
- `PYTHON=/usr/bin/python3 sh tests/host/run_firmware_stack_budget_checks.sh` —
  worst main path 1,872 B of 1,920 B, unchanged
- Focused regression sweeps across the key-runtime and VIA/split suites

Finding 05's hardware matrix is no longer blocked by must-fix 2.

## 2026-08-16 — Implementation Pass 2: gate integrity

Findings 5, 6, 7 and 8. These were grouped deliberately: each is a gate that was
green while proving less than its name suggested, so closing them first protects
every later change.

### Finding 6 — guards can no longer pass vacuously

`noah_host_require_tool()` in `tests/host/noah_host_qmk_env.sh`, called for `rg`
by `run_owned_keycode_tests.sh`, `run_macro_payload_engine_tests.sh` and
`run_feature_gate_compile_tests.sh`. The `if rg <forbidden>; then fail; fi` shape
treats any non-zero exit as "nothing found", and `set -e` is suppressed inside an
`if`, so a missing tool or a stale path read as success. Verified by running with
`PATH=/usr/bin:/bin`: both scripts now exit 1 with an explicit message instead of
reporting clean.

### Finding 8 — the pd-to-core include gate now matches something

`run_feature_gate_compile_tests.sh` gated `key/runtime/reducer/runtime.h` only,
while the sanctioned bridge reaches core through `ownership_state.h`. The pattern
matched nothing, so any pd module could take the same route and obtain
`key_runtime_core_state_t` untouched. It now gates any `reducer/*.h`, which
catches exactly one file today — the allowlisted bridge. Verified by planting an
`ownership_state`-adjacent include in `pd_mode_snapshot.c`: the gate fails, and
passes again once reverted.

### Finding 5 — `synthetic_record.c` has behavioral coverage

New `tests/host/synthetic_record_test.c` and runner link the real module, which
no host binary previously did. Covers the depth counter across nesting, the
non-matrix key position, press/release ordering for both the userspace and QMK
dispatch paths, and the tap-count carry.

Both mutations named in the review are caught: removing the depth increment pair
fails the active-flag assertion, and changing the synthetic keypos to `(0, 0)`
fails the non-matrix assertion.

### Finding 7 — the memory gate measures all of static RAM

`tools/check_firmware_memory_budget.py` now reads `__data_base__`/`__data_end__`
alongside the BSS bounds and enforces `.data + .bss` against a new
`--max-static-ram` limit, currently 49,824 B against 51,000 B. `.bss` alone
excluded `noah_runtime_singleton`, the largest static object, because non-zero
initializers put it in `.data`; growth there previously showed up only as
indirect heap loss with roughly 7.5 KB of silent headroom. The tool's own tests
gained a shared layout fixture plus cases for the data span and for rejecting a
missing data boundary.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0, no failures
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- memory gate: BSS 25,524 B, data 24,300 B, static RAM 49,824 B of 51,000 B,
  heap 212,312 B
- stack gate: worst main path 1,872 B of 1,920 B, unchanged
- guard preflight and include gate both verified to fail when they should

## 2026-08-16 — Implementation Pass 3: VIA mutation classification

Finding 3, the last remaining correctness defect.

`noah_qmk_via_classify_mutation()` in `users/noah/lib/compat/qmk_via_contract.c`
now treats a buffer write as a mutation whenever any byte can land inside the
region, rather than only when the whole write fits.

The distinction was checked against upstream rather than assumed, and it is not
uniform across commands:

- `nvm_dynamic_keymap_update_keycode()`
  (`../bastardkb-qmk/quantum/nvm/eeprom/nvm_dynamic_keymap.c:95`) rejects an
  out-of-range write outright, so classifying it as a non-mutation is correct
  and was left alone.
- `nvm_dynamic_keymap_update_buffer()` (`:140-151`) and
  `nvm_dynamic_keymap_macro_update_buffer()` (`:171-181`) apply byte by byte,
  keeping every byte whose offset lands inside the region and dropping the rest.

So only the two buffer commands were misclassified. An over-long or straddling
write changed storage while userspace recorded no mutation, leaving the digest
advertising pre-write content and the halves silently divergent. Only a write
starting past the region touches nothing and remains a non-mutation, which keeps
the conservative reclassification from costing spurious digest recomputes.

Two existing assertions in `tests/host/qmk_via_command_classifier_test.c`
asserted the old, wrong expectation and were changed deliberately: they claimed
the over-long keymap write and the straddling macro write were rejected, which
is exactly the misclassification. Replaced with cases proving both are
mutations, plus new cases proving writes past the region are not, and an
out-of-range keycode write still is not.

Proven load-bearing: restoring the old range check fails the over-long keymap
assertion.

### Out of scope, recorded for a decision

`payload_size` is a byte and the raw HID buffer is 32 bytes, so a frame claiming
a payload larger than it carries makes upstream read past `command_data` while
copying into storage. That is an upstream out-of-bounds read that userspace
cannot prevent by classifying, since returning false is what lets the command
through. Consuming such frames in `via_command_kb()` would stop it but changes
host-visible VIA behavior, so it is left as a separate decision rather than
folded into this fix.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0, no failures
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- memory gate: static RAM 49,824 B of 51,000 B, heap 212,312 B
- stack gate: PASS, unchanged

## 2026-08-16 — Implementation Pass 4: coherent-read contract

Finding 4. The three helpers in `users/noah/lib/split/runtime_sync.h` now copy
into a local staging buffer and commit to the caller's destination only after
confirming the generation is unchanged, so a helper that returns false has
written nothing. Cost is a few dozen bytes of stack on the render path, which is
not one of the 43 reviewed stack paths; the reviewed worst case is unchanged at
1,872 B.

Test: `test_failed_coherent_read_leaves_the_destination_untouched()` in
`tests/host/split_runtime_sync_test.c`.

**What the test does and does not pin, stated plainly.** It covers the reachable
failure — an in-flight publication, where the pre-check skips the copy — and is
load-bearing for it: removing the `in_flight` pre-check fails it. It does not
cover retry exhaustion, because that needs a full publication inside each of four
sub-microsecond copies, which the transport cannot produce and which cannot be
forced without adding a hook to production code. That half is closed by
construction rather than by test.

An earlier version of this test was written believing it proved the staging
change; neutering the staging left it green, because the in-flight path never
wrote the destination even before. The test comment now says which half it pins
so a later reader does not draw the wrong conclusion from its passing.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0, no failures
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- stack gate PASS, worst main path 1,872 B unchanged

## Current Verdict

The audit is **complete and open**. Coverage is full across the userspace. Both
must-fix defects are now closed; ten should-fix items and the cleanup list are
outstanding.

## Next Steps

1. ~~Must-fix 1~~ — landed in `474651bc`.
2. ~~Must-fix 2~~ — landed in `474651bc`.
3. ~~Gate integrity: `rg` preflight, pd-to-core include gate, `synthetic_record.c`
   coverage, memory gate `.data` blind spot~~ — landed.
4. Remaining should-fix items: ~~the VIA out-of-range classification
   (finding 3)~~ and ~~the coherent-read contract (finding 4)~~ landed; the
   pulse key position (finding 10),
   `RGB_LEFT_LED_COUNT` derivation (finding 11), and the dead public functions
   (finding 9).
5. Reconcile the roadmap and the documentation gaps (finding 12).
6. Only after software closure, run the Finding 05 and Finding 10 physical
   matrices. Finding 05's matrix is blocked on must-fix 2.

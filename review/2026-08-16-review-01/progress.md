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

## 2026-08-16 — Implementation Pass 5: findings 11 and 10

### Finding 11 — the split boundary has one source of truth

`users/noah/config.h` now defines `RGB_LEFT_LED_COUNT` and derives
`RGB_MATRIX_SPLIT` from it as `{RGB_LEFT_LED_COUNT, RGB_MATRIX_LED_COUNT - RGB_LEFT_LED_COUNT}`.
Userspace half-painting helpers clamp against the count while upstream clamps
against the split, so a disagreement left LEDs at the boundary outside every
userspace chunk while the base effect kept driving them. Derivation makes that
impossible rather than merely discouraged, which is stronger than the assert the
finding suggested.

An added `_Static_assert` in `rgb_helpers.h` was reverted: focused host tests
build with small `RGB_MATRIX_LED_COUNT` values against the header's fallback of
29, so the assert fired on configurations that are not the firmware. The
derivation is the enforcement; the assert would only have restated it where it
does not apply.

`docs/KEYMAP-OVERVIEW.md` was regenerated with `tools/profile_introspect.py
--write`, since `users/noah/config.h` is an authored introspection input. The
introspection gate caught this in the full suite rather than it being noticed by
hand.

### Finding 10 — the keypos-less pulse API is gone

`key_feedback_pulse_arm()` is removed from `feedback.c` and `feedback.h`. It set
every pulse field except `feedback_pulse_key_pos`, so the semantic and
broad-owner maps attributed the pulse to a stale key, or to `{0,0}` on fresh
state, which is a valid position.

Deleted rather than fixed, because it had **no production caller**: the runtime
arms pulses through `key_runtime_core_feedback_projection_set_pulse()`, which
does set the position. Five test files carried stubs for it, four of them in
binaries that do not even link `feedback.c`, so those stubs satisfied nothing.
The two real uses in `tests/host/runtime_debug_test.c` moved to a local helper
that mirrors the production path including the key position.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `python3 tools/profile_introspect.py --check`
- memory gate: static RAM 49,824 B of 51,000 B; stack gate PASS, unchanged

## 2026-08-16 — Implementation Pass 6: dead surface removal

Finding 9 plus the 17 unused `static inline` helpers found in the direct sweep.
167 lines removed across 17 files, no behavior change.

Six public functions, each with a declaration and a definition and nothing else
in the tree: `handled_key_resolution_uses_implicit_hold` and
`handled_key_resolution_uses_fallback_hold`,
`key_runtime_core_flashing_feedback_started_at`,
`noah_runtime_diag_test_backend_seed_watchdog_reboot` (a test-backend-guarded
no-op with no test), `pd_any_display_mode_active`, and
`split_runtime_sync_combo_is_dirty`.

The first two were the reason this ranked above ordinary cleanup: they sat one
suffix away from the live internal predicate
`handled_key_resolution_uses_fallback_hold_behavior`, so calling the dead public
wrapper and believing it was the one the reducer consults was an easy mistake.

The 17 inline helpers included a complete hold-contract predicate vocabulary —
`hold_fires_at_threshold`, `hold_registers_while_held`, `hold_repeats_while_held`,
`hold_sends_on_release`, and the `handled_key_hold_contract_*` family — that
nothing ever called. That is a designed abstraction which was bypassed, and
removing it makes the surface match how holds are actually resolved.

Removing them exposed one cascade: `automouse_rgb_timeout_window_open` had a
single remaining reference from a helper deleted in the same pass. A repeat
sweep after the removals found it, and a third sweep is clean.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `sh tests/host/run_feature_gate_compile_tests.sh` — PASS
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- memory gate: static RAM 49,824 B of 51,000 B; stack gate PASS, unchanged
- repeat dead-symbol sweep over `users/noah` reports nothing remaining

Compilation and linking of the current source lists is the enforcement here, per
the audit template: no negative gate was added to assert these names stay absent.

## 2026-08-16 — Implementation Pass 7: documentation reconciliation

Finding 12, the last should-fix item.

- `docs/architecture/source-map.md` gained `split/runtime_sync_dirty.c/h`, which
  existed in the manifest but not in the inventory the doc bills as the way to
  check a package against the tree, and a `shared/runtime_publication.h` entry
  describing it as the single-writer generation contract to read before changing
  split remote or pd-mode snapshot storage. It had appeared in no doc at all.
- `docs/HOOK_OVERRIDES.md` gained a section for the three QMK hooks defined
  outside `hooks.c`: `via_init_kb()`, `via_command_kb()` and
  `is_keyboard_master_impl()`. These are not weak `*_user` hooks, so a
  keyboard-level definition replaces them outright with no chaining helper and no
  compile gate. The entry for `via_command_kb()` spells out that replacing it
  drops VIA mutation classification, macro reseeding and split-mirror marking, so
  storage would change without the digest or the peer learning about it.
- `Sol Findings/00-overarching-remediation-roadmap.md` no longer says Review 17
  regressed Finding 08 or partially reopened Finding 15. Both closed in
  `aae9c445`; the roadmap now says so and points here. This was the
  self-inflicted miss: that commit updated `implementation-progress.md` and left
  the roadmap stale.
- `README.md` anchors corrected: the `RIGHT_THUMB` row link pointed into the
  `LEFT_THUMB` row, and the custom split RPC link pointed at a comment line
  rather than `SPLIT_TRANSACTION_IDS_USER`.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `git diff --check` clean
- anchor targets confirmed by reading the cited lines

## 2026-08-16 — Hardware regression 1: slave auto-mouse fade

Reported from hardware: the auto-mouse timeout fade renders on the master but is
a solid color on the slave. Working on `dev`.

**Cause.** `split_runtime_sync_auto_mouse_elapsed()` in
`users/noah/lib/split/runtime_sync.c` gated the elapsed read on
`noah_qmk_contract_auto_mouse_active()`. `dev` read it unconditionally. The gate
arrived with `df6df3f4` ("Sample split runtime time once per tick"), which is not
an ancestor of `dev`.

The predicate is semantically inverted for this use.
`is_auto_mouse_active()` is `is_activated || mouse_key_tracker || layer_hold_check()`,
and `../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c:267`
recomputes `is_activated` from **each mouse report** via `auto_mouse_activation()`,
which is true only while movement exceeds the threshold or a button is held. It
goes false the moment the pointer stops, while `timer.active` keeps counting
toward the timeout — and that countdown is exactly what the fade renders. So the
gate zeroed the published progress for precisely the fade window.

Master-only symptom follows directly: `automouse_rgb_current_progress()` reads
the clock directly on the master and mirrors the synced value on the slave.

An initial reading of this was wrong and worth recording: `is_activated` looks
like it should stay true for the whole timeout, which would have made the gate
harmless. Reading the upstream task rather than assuming its meaning is what
settled it.

**Fix.** The read is unconditional again. The locked-mode suppression stays in
the packet builder, where the sent value is decided.

**Tests.** `test_inactive_auto_mouse_skips_elapsed_lookup()` asserted the
regression as the expected behavior and was replaced by
`test_idle_auto_mouse_still_publishes_fade_progress()`, which requires progress to
be published while the pointer is idle, plus
`test_locked_mode_suppresses_fade_progress()` to keep the lock path pinned.
Load-bearing: reinstating the gate fails the new test.

**Throughput.** Restoring this costs roughly `ACTIVE_SPAN / AUTOMOUSE_RGB_SYNC_STEP`
extra base sends per timeout window — about 8 to 9 RPCs spread across one fade,
not per scan, because progress is quantized and the base packet only sends when
the quantized value changes. The reported 380 to 470 fps gain came from the
runtime lookup hot path, one-timer-sample, and key-feedback dirty gating, which
are untouched.

### Still open, not yet root-caused

Two further hardware regressions were reported and are **not** explained by this
fix. They have hypotheses only, and rollback to `dev` was considered and
rejected: the optimizations are worth keeping, so each needs the same treatment
as above — find the precise defect, fix it, keep the speed.

1. Tap-window white flash missing on the slave. Hypothesis: `6a9285f3` changed
   per-domain sends from continue-on-failure to return-on-first-failure with a
   backoff that gates the whole tick, so one transient RPC failure can starve
   key-feedback packets. Unproven.
2. VIA edits no longer reach the slave, so slave RGB layer colors do not pick up
   newly mapped keys. Separate channel from the runtime sync tick. Slave-side
   cache invalidation was checked and ruled out: the receiver does invalidate RGB
   layer maps on commit. Hypothesis: the `SNAPSHOT_REQUIRED` handling added in
   `474651bc` restarts the session on a status that also occurs benignly.
   Unproven, and it is this session's own change.

## 2026-08-16 — Hardware regression 2: VIA edits stop reaching the slave

Reported from hardware: VIA changes no longer replicate, so slave RGB layer
colors do not pick up newly mapped keys. Working on `dev`.

**Cause, and it was this session's own change.** `474651bc` made any
`SNAPSHOT_REQUIRED` response abandon the session and restart from metadata. That
status is not only "session lost". `noah_qmk_via_handle_pull_chunk()` returns it
whenever `noah_qmk_via_local_state_is_clean()` is false, which includes
`!shared.digest_valid` — the peer merely still computing its digest — and
`noah_qmk_via_handle_snapshot_commit()` returns it for `!all_received`. Those are
transient and clear on a retry.

Restarting instead re-enters metadata, is rejected again for the same reason, and
the transfer never completes. A self-healing retry became a livelock.

**Fix.** Retry first; renegotiate only after
`VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT` (3) consecutive rejections, with the
counter cleared on any progress and at init. Transient rejections keep their
pre-existing self-healing behavior, and a genuinely lost session still escapes in
bounded time.

**Tests.** `test_transient_peer_rejection_is_retried_without_renegotiating()`
compares the same push with and without transient rejections and requires the
session count to be identical. The fixture peer is permanently dirty and opens
sessions on its own, so a raw begin count is not a stable invariant — the same
trap that made the first version of the session-loss test pass against broken
code. Load-bearing: setting the reject limit to 1, which is exactly the shipped
regression, fails it.

**Correction to an earlier claim.** Removing renegotiation entirely now fails no
test. `noah_qmk_via_local_digest_tick()` also resets the phase to metadata, so it
provides a second recovery path. The "retries one chunk at 1 Hz forever, escape
requires a reboot" characterization in this review's must-fix 2 is therefore
stronger than the evidence supports: renegotiation bounds the recovery time
rather than being the only way out. The session-loss test passes either way and
is not load-bearing for that logic. Worth re-deriving on hardware before the
Finding 05 matrix rather than trusting the original wording.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- stack gate PASS; static RAM 49,824 B of 51,000 B

### Superseded hypothesis

The tap-window regression was earlier attributed to `6a9285f3`'s
return-on-first-failure backoff. That was wrong and is withdrawn; see the pass
below for the actual cause.

## 2026-08-16 — Hardware regression 3: slave tap-window feedback

Reported from hardware: the tap-window white — shown while a multi-tap count can
still advance — renders on the master but not on the slave.

**Cause.** The rendered state is a *pending tap series*
(`key_feedback_tap_series_shows_pending_feedback()`, tap_count > 1), not a
projected effect and not the flashing feedback. It is established while
processing a key event, at the tap-series assignments in
`users/noah/lib/key/runtime/reducer/runtime.c`, each of which bumps
`next_feedback_sequence`.

Neither dirty-marking site covered that:

- `key_runtime_transition_execute_plan()` announces only non-empty plans, and
  the press that opens the window defers its tap, so the plan is empty.
- `noah_key_runtime_scan()` compares the feedback sequence against its value at
  the start of *that scan*, but the bump already happened during the earlier key
  event.

`dev` marked key feedback dirty unconditionally whenever a scan had core work,
and an open tap window is core work, so the next scan pushed the semantic map.
The narrowing in `5a56146d` removed that blanket marking without replacing it on
the key-event path.

**Fix.** `key_runtime_process_notify_planless_feedback_change()` applies the
scan's existing before/after check to the press and release paths, so a key event
that changes rendered feedback without producing a plan announces it. The
optimization is kept: nothing is marked when neither the plan nor the sequence
changed.

**Two wrong turns, recorded because both looked plausible.** First I pursued the
flash-visibility blink, which is a different feature — corrected by Noah. Then I
tracked the last-marked sequence in the dirty module and compared against it per
scan, which broke two existing contracts: a scan after an already-announced press
began marking a second time, and suppressing that in turn silenced the
visible-deadline case. The existing tests encode "mark when *this* step changed
feedback state", and the fix had to match that shape rather than replace it.

**Test.** `test_feedback_dirty_tracks_pending_tap_window()` in
`tests/host/real_profile_thumb_layer_lock_integration_test.c` taps a real-profile
multi-tap key, then presses again to enter the higher tier, and requires the
press itself to mark key feedback dirty. Load-bearing: removing the press-path
announcement fails it.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- stack gate PASS, worst main path 1,872 B unchanged; static RAM 49,824 B

All three reported hardware regressions now have an identified cause and a fix.
None has been confirmed on hardware yet.

## 2026-08-16 — Hardware regression 2, second attempt: restore write-through mirroring

The first attempt was wrong. Confirmed on hardware: the fade and tap-window
fixes worked, the VIA one changed nothing.

**What the earlier attempts got wrong.** `474651bc` blamed `SNAPSHOT_REQUIRED`
handling; hardware disproved it. Then a `recovery_required` latch looked like the
cause, but the fix for it broke
`test_two_dirty_halves_reseed_current_master_before_authority`, which showed the
design does have recovery paths — pull from a newer peer, and reseed defaults
when both halves are dirty. That attempt was reverted rather than shipped.

**The actual difference from `dev`.** `dev` mirrored every VIA storage command to
the other half as it happened: `via_command_kb()` called
`noah_qmk_via_split_sync_command()`, which forwarded the raw command over RPC for
the peer to apply. 99 lines, no state machine. `f881e8a3` replaced that with the
durable snapshot/digest/generation reconciliation and removed the immediate path.

Reconciliation is the right mechanism for surviving disconnects and power cycles
and the wrong one for a single live keymap edit: until a session completes, the
slave keeps rendering the old keycodes.

**Fix.** `users/noah/lib/compat/qmk_via_split_mirror.c` restores write-through on
its own transaction, `PUT_VIA_KEYMAP_MIRROR`, deliberately independent of the
reconciliation protocol so neither can block or corrupt the other. The receiver
applies the command, invalidates the RGB layer maps and macro provider, and
restarts its local digest so the durable layer does not advertise a stale one.
`via_command_kb()` mirrors first, then notes the mutation, so reconciliation
still owns durability and recovery.

Sending is best effort by design: a dropped mirror leaves the halves briefly out
of step and reconciliation repairs it, whereas retrying would put storage writes
on the scan path.

**Tests.** `test_keymap_write_mirrors_immediately_and_still_reconciles()` requires
both paths to run for one edit, and `test_non_mutating_command_is_not_mirrored()`
keeps the mirror from becoming a second, looser classification path.
Load-bearing: removing the mirror call reproduces the regressed state and fails.

**Cost.** Static RAM 49,824 to 49,836 B. The worst reviewed main path moved 1,872
to 1,880 B because a second caller of `dynamic_keymap_reset` stopped LTO inlining
`nvm_dynamic_keymap_update_keycode` into it, so the reviewed path in
`tools/firmware_stack_budget.json` gained that frame. Still inside the 1,920 B
budget.

### Verification

- `sh tests/host/run_all_host_tests.sh` — exit 0
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`
- `python3 tools/profile_introspect.py --check`
- memory gate: static RAM 49,836 B of 51,000 B, heap 212,304 B
- stack gate PASS, worst main path 1,880 B of 1,920 B

Not yet confirmed on hardware.

## Current Verdict

The audit is **complete**, and every software finding it raised is closed: both
must-fix defects, all ten should-fix items, and the dead-surface cleanup. What
remains is hardware-only.

Two items were deliberately left as decisions rather than silently absorbed, and
are recorded above: the upstream VIA out-of-bounds read reachable from a
malformed frame, which cannot be closed without changing host-visible protocol
behavior, and the write-only split base-domain state plus its generation, which
is uniformity that currently has no consumer.

## Next Steps

1. ~~Must-fix 1~~ — landed in `474651bc`.
2. ~~Must-fix 2~~ — landed in `474651bc`.
3. ~~Gate integrity: `rg` preflight, pd-to-core include gate, `synthetic_record.c`
   coverage, memory gate `.data` blind spot~~ — landed.
4. Remaining should-fix items: ~~the VIA out-of-range classification
   (finding 3)~~ and ~~the coherent-read contract (finding 4)~~ landed; the
   ~~pulse key position (finding 10)~~ and ~~`RGB_LEFT_LED_COUNT` derivation
   (finding 11)~~ landed, and ~~the dead public functions (finding 9)~~ landed. and ~~the
   documentation reconciliation (finding 12)~~ landed. All should-fix items are
   closed.
5. Only after software closure, run the Finding 05 and Finding 10 physical
   matrices. Finding 05's matrix is blocked on must-fix 2.

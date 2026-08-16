# Follow-Up Architecture Audit — Landed Ownership and Publication Work

This audit uses `prompts/follow-up-architecture-audit.md`. It covers the tree at
`aae9c445`, which landed the four software remediations recorded in
`review/2026-08-15-review-17/progress.md`.

## Why This Folder

Review 17 is the prior thread. Its software findings all closed in `aae9c445`,
and its only remaining obligations are physical: the Finding 05 persistence and
role-swap matrix and the Finding 10 arrow feel matrix. This pass audits the code
that remediation produced, which is materially different work from the
remediation thread itself, so it opens the next sortable folder rather than
appending to an audit whose findings section is now historical. Review 17
carries a reconciliation note pointing here.

## Scope And Completeness

**This pass is partial and should not be treated as a full-codebase audit.**
Four parallel auditors were dispatched across key runtime/ownership,
split/RGB/pointing, architecture/interfaces, and tests/gates/docs. Three
terminated immediately on an API session usage limit and produced no findings.
The areas below were audited directly instead, with every finding verified
against the code by hand.

Audited directly and reported here:

- the cross-context publication scheme and its readers;
- the report-ownership seam introduced in `aae9c445`, including its interaction
  with upstream QMK control flow;
- host test runner coverage.

A relaunched auditor covered the host suite's stubbing strategy, the `rg`-based
compile guards, and the firmware budget tooling. Its three findings were each
re-verified by hand against the code and the linked ELF before being recorded
here, and one was re-characterized: the memory-gate blind spot is bounded by the
heap minimum rather than open-ended.

**Not yet audited, and still open for a later pass:** architecture and module
boundaries as a whole, API/interface cleanliness across `users/noah/`,
macro/VIA subsystem correctness, and documentation accuracy beyond the files
touched by `aae9c445`.

## Findings

### Should fix — Coherent-read helpers promise a guarantee they do not provide

`split_runtime_sync_remote_read_combo()` and its two siblings
(`users/noah/lib/split/runtime_sync.h:223-277`) copy into the caller's
destination buffers *before* re-checking the generation. When every retry fails,
the helper returns false with the destination already holding a mixture of two
packets.

Both the helper's contract comment
(`users/noah/lib/split/runtime_sync.h:216-222`) and the caller's comments
(`users/noah/lib/rgb/core/rgb_runtime.c:70-73` and `:110-112`) state the
opposite: that a false read leaves the cached maps untouched so the frame
renders the last coherent snapshot. The caller passes the live frame cache
directly (`rgb_runtime.c:75`, `:113-114`), so there is no separate buffer
protecting it.

The reachable half of this is genuinely safe. An in-flight publication is caught
by the `in_flight` pre-check, which `continue`s before writing anything, so the
common case cannot tear. Exhausting the budget requires a complete publication
to land inside each of four sub-microsecond copies, which the transport cannot
produce at millisecond publication spacing.

So this is not a live rendering defect. It is a documented invariant that the
code does not enforce, on the exact seam the previous review was opened to make
trustworthy, and a future change that widens a map or shortens the retry budget
would silently convert it into one. Fix by either staging into a local and
committing only on a settled generation, or by correcting both comments to state
what is actually guaranteed: an in-flight publication never writes the
destination, and retry exhaustion is unreachable given transport timing. If the
comment route is chosen, say why it is unreachable, because the current wording
reads as a promise.

### Should fix — `synthetic_record.c` has no behavioral coverage, and it gates the new ownership seam

`users/noah/lib/action/synthetic_record.c` is never linked into any host test
binary. No runner under `tests/host/` references it. Eight test translation
units define their own no-op replacement for `noah_synthetic_record_active()`,
for example `tests/host/key_runtime_scenario_harness.c:396` and
`tests/host/real_profile_thumb_layer_lock_integration_test.c:599`, all returning
false. `run_feature_gate_compile_tests.sh` reaches the file with
`-fsyntax-only`, which proves it parses, not that it behaves.

That matters because `users/noah/lib/key/runtime/process.c` branches on
`noah_synthetic_record_active()` at four points: lines 104, 229, 260, and 287.
Line 229 is the guard on `key_runtime_process_settle_report_ownership()`, the
seam `aae9c445` introduced. Its job is to stop synthetic emissions from being
credited as default-handler report ownership. Nothing tests that the guard
works, because in every host binary the function is a stub that always says
"not synthetic".

Two mutations that leave the whole suite green:

1. Delete the depth increment/decrement pair around `process_record_user` in
   `synthetic_record.c:29-33`. `noah_synthetic_record_active()` then always
   returns false, synthetic dispatch stops bypassing the physical runtime, and
   synthetic records start registering as physical key origins.
2. Change `MAKE_KEYEVENT(UINT8_MAX, UINT8_MAX, pressed)` at
   `synthetic_record.c:12` to `(0, 0, pressed)`. Synthetic records then alias
   real matrix position (0,0) and corrupt its per-key state.

This is a coverage hole rather than a known defect, but it sits directly under
the seam this thread just spent a review cycle making trustworthy. The fix is to
link the real module into at least one integration binary and drive a synthetic
emission through it.

### Should fix — Three compile-gate guards pass vacuously when their search fails

`run_macro_payload_engine_tests.sh:47` and `run_feature_gate_compile_tests.sh:60`,
`:70`, `:82` use the shape `if rg -n '<forbidden>' …; then fail; fi`. A non-zero
exit from `rg` makes the condition false, so the guard silently reports clean,
and `set -e` does not fire because it is suppressed inside an `if` condition.
`rg` exits 127 when missing and 2 on a bad path, so both a missing tool and a
moved file read as "no violations found". No `command -v rg` preflight exists
anywhere under `tests/` or `tools/`.

This is not hypothetical: until 2026-08-16 ripgrep was not installed on the
development machine, so these guards were passing vacuously while Review 17
recorded the suite as green.

Mutation missed: move `users/noah/lib/macro/macro_payload_run.c` elsewhere and
reintroduce a blocking `wait_ms(` call in the new location. `rg` exits 2 on the
stale path and the guard reports clean.

By contrast `run_owned_keycode_tests.sh:29,39` uses a capture-and-compare shape
whose empty result fails a string comparison, so it fails loudly instead. Both
patterns were re-run and their allowlists are still exactly accurate. A
`command -v rg` preflight at the top of each script is the whole fix.

### Should fix — The memory gate bounds `.bss` only, and the largest consumer is in `.data`

`tools/check_firmware_memory_budget.py:97` computes
`__bss_end__ - __bss_base__` and checks it against 26,000 B. Confirmed by
`objdump -h` on the linked ELF: `.bss` is 0x63b4 = 25,524 B at VMA 0x20005ef0,
while `.data` is 0x5eec = 24,300 B at 0x20000000. `noah_runtime_singleton`
(0x4f40 = 20,288 B) and `split_runtime_sync_remote` sit inside `.data`, entirely
below the measured span, because the singleton carries non-zero initializers.

So the gate bounds roughly half of static RAM and excludes its single largest
object. This is why the ~36 bytes `aae9c445` added to the runtime context and
modifier ownership produced no movement at all in the reported BSS figure.

The growth is not completely unbounded: `.data` growth pushes the linker heap
down, and the same script enforces a 204,800 B heap minimum. Current heap is
212,312 B, so about **7,512 B** of `.data` growth would pass silently before any
gate objected. That is a real but bounded blind spot, not an open door. Worth
either measuring `.data` explicitly or renaming the reported figure so it stops
reading as a total-RAM budget in the roadmap.

### Optional cleanup — The base domain publishes state nothing reads

`split_runtime_sync_remote.active_mode_id`, `.locked_mode_id`,
`.pd_mode_owner_sides`, and `.pd_mode_owner_bitmap` are written by the base RPC
(`users/noah/lib/split/runtime_sync.c:352-357`) and never read anywhere in
production. Verified by searching every use of `split_runtime_sync_remote`
across `users/noah`: the only production readers of that struct are
single-field, `automouse_progress` at `users/noah/lib/rgb/automouse/rgb_automouse.c:26`
and `key_preview_layer` at `users/noah/lib/rgb/stages/rgb_preview_stage.c:14`.
RGB and pd-mode UI take mirrored identity and ownership from pd-mode's own
published state, which the same RPC feeds directly from the packet at
`runtime_sync.c:366`.

That is roughly 11 bytes of shadow state kept coherent for no consumer.

### Optional cleanup — `base_generation` guards a domain with no grouped reader

Because every live base-domain reader takes exactly one field, the base
publication generation (`users/noah/lib/split/runtime_sync.c:350`, `:360`) has
no production reader; only tests observe it. It costs one byte and two
increments per base publication.

This was a deliberate uniformity choice when the scheme landed, and uniformity
across four RPCs has real value for anyone adding a grouped base reader later.
Recorded so the choice is explicit rather than assumed, and so it is re-decided
together with the dead fields above: if those fields go, the generation has even
less to protect.

## Verified Solid

These were specifically probed and found correct.

- **Publication shape parity.** The two shapes have incompatible parity
  conventions: in-place publication increments twice per publication so odd
  means in flight, while slot publication increments once so odd is an ordinary
  settled state. Applying the wrong convention would be a serious bug. Every
  call site was traced: `noah_runtime_publication_in_flight` appears only on the
  three in-place split readers (`runtime_sync.h:227`, `:246`, `:265`) and
  `noah_runtime_publication_slot` only on the pd-mode slot paths
  (`pd_mode_runtime_shared_state_internal.h:59`, `:63`, `:83`, `:110`). No
  cross-application exists.
- **Slot selection.** The writer fills the slot the current generation does not
  select and then advances it, so the published slot after a publication is the
  one just filled, and a reader mid-copy of the old slot is caught by the
  settled re-check rather than by the writer reusing its slot.
- **No double-settle on the ownership seam.** The concern was that a consumed
  press would call `noah_process_record_user_finalize(..., false)` from
  `users/noah/hooks.c:25` and then have QMK call `post_process_record_user()`,
  settling the same event a second time as `keep_processing = true` and
  crediting report ownership for a press that never reached a default handler —
  reintroducing exactly the regression `aae9c445` fixed. Upstream disproves it:
  `process_record()` in `../bastardkb-qmk/quantum/action.c:293-300` returns
  early on a false result from `process_record_quantum()` without calling
  `post_process_record_quantum()`. Each event settles exactly once, and the
  contract is documented at `users/noah/noah_runtime.h:9-13`.
- **Host runner coverage.** Every `run_*.sh` in `tests/host/` is reached by
  `run_all_host_tests.sh` except three, all deliberate:
  `run_real_profile_validation_tests.sh` runs via
  `run_all_profile_validation_tests.sh`, `run_all_profile_compile_tests.sh` is a
  documented separate gate, and the two budget checkers require a freshly linked
  ELF and enforce their own staleness. Every `*_test.c` is compiled by some
  runner.
- **Test source lists cannot drift from the firmware build.**
  `tests/host/noah_source_manifest.sh:78` rejects any hand-picked source not
  present in the real make manifest, so a host binary cannot quietly test a
  different source set than the firmware links. 92 of 94 production `.c` files
  are linked into some host binary.
- **The Finding 08 precedent is genuinely closed.**
  `tests/host/key_runtime_physical_ownership_integration_test.c` links the real
  `owned_keycode.c` and `keyboard_mod_ownership.c` rather than stubbing them,
  and asserts through the debug snapshots. The no-op
  `owned_keycode_track_physical_event` stubs that remain in sibling harnesses are
  acceptable because another runner drives the real module through the same
  `key_runtime_integration_harness.c`.

## Prior Finding Status

Status of the Review 17 thread as of `aae9c445`. Evidence and the full
verification command list are in `review/2026-08-15-review-17/progress.md`.

| Prior finding | Status | Evidence |
| --- | --- | --- |
| 08 — Synthetic-key ownership | resolved | Report ownership settles in the finalize hook on the final event result (`users/noah/lib/key/runtime/process.c`); modifier ownership separates physical from report counts (`keyboard_mod_ownership.c`); enforced by `tests/host/key_runtime_physical_ownership_integration_test.c`, which fails against the pre-fix tree. Single-settle confirmed against upstream control flow. |
| 15 — RGB render work | resolved | Each domain publishes as one generation; readers retry bounded and never block the worker. Enforced by seam-driven interleaving tests in `split_runtime_sync_test.c`, `pd_mode_test.c`, and `rgb_layer_render_test.c`. The contract-comment gap above does not affect the reachable path. |
| 10 — Pointing backlog bounds | partially resolved | Inactive-axis debt is cancelled at the dominant-axis transition with X-to-Y and Y-to-X coverage; the flashed timing and feel matrix is still hardware-pending. |
| 11 — Dragscroll stall recovery | resolved | Saturating residual accumulation with a documented cap and total `abs32`; near-limit, reverse-axis, expiry, and pinch-reuse coverage. |
| 05 — VIA split persistence | partially resolved | Unchanged by this pass. Software paths remain green; the physical two-half, power-cycle, reconnect, and role-swap matrix is still required. |

## Current Conclusion

The landed work holds up under direct scrutiny. The publication scheme is
correct on the axis most likely to be wrong, and the ownership seam is correct
against real upstream control flow rather than against an assumption about it.
Both were checked by trying to break them, not by reading them approvingly.

The findings are all about enforcement rather than live defects, which is the
honest summary: four places where something is asserted or assumed but not
mechanically held. The most consequential is that `synthetic_record.c` has no
behavioral coverage at all while gating the very seam this thread just repaired,
followed by three guards and a budget figure that measure less than they appear
to. Two cleanup items concern state that is published and never consumed.

There is a pattern worth naming. Finding 08 hid because every harness stubbed
the ledger it depended on; the `rg` guards passed vacuously because a missing
tool reads as success; the memory figure looks like a RAM budget while measuring
half of RAM. In each case the gate was present and green while proving less than
its name suggested. That, rather than any single bug, is the risk this codebase
should keep auditing for.

This audit is **not complete**. Three of four planned areas were never examined,
so this folder must not be read as a clean bill of health for the codebase as a
whole. It closes no thread.

## Remaining Open Findings

1. **Should fix:** link `synthetic_record.c` into an integration binary and
   drive a synthetic emission through it, so the guard protecting the new
   ownership seam is behaviorally covered.
2. **Should fix:** add a `command -v rg` preflight to the three guard scripts so
   a failed search cannot read as a clean result.
3. **Should fix:** reconcile the coherent-read helpers with their documented
   contract, either by staging reads or by correcting the comments.
4. **Should fix:** measure `.data` explicitly in the memory gate, or rename the
   reported figure so it stops reading as a total static-RAM budget.
5. **Optional:** remove or justify the write-only base-domain mirror fields in
   `split_runtime_sync_remote`, and re-decide `base_generation` alongside them.
6. **Audit debt:** architecture and module boundaries, API/interface
   cleanliness, macro/VIA subsystems, and documentation accuracy remain
   unaudited in this pass.
5. **Hardware verification:** Finding 05's persistence and role-swap matrix.
6. **Hardware verification:** Finding 10's arrow timing and feel matrix.

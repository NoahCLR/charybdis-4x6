# Follow-Up Architecture Audit — Full Userspace Pass

This audit uses `prompts/follow-up-architecture-audit.md`. It covers the tree at
`95a33973`, which includes the four software remediations recorded in
`review/2026-08-15-review-17/progress.md`.

## Why This Folder

Review 17 is the prior thread. Its software findings all closed in `aae9c445`,
and its only remaining obligations are physical: the Finding 05 persistence and
role-swap matrix and the Finding 10 arrow feel matrix. This pass audits the code
that remediation produced plus the rest of the userspace, so it opens the next
sortable folder rather than appending to an audit whose findings section is now
historical. Review 17 carries a reconciliation note pointing here.

## Scope

Complete pass over `users/noah/` and the authored profile. Five areas were
audited: key runtime and ownership, split/RGB/pointing, macro and VIA/compat,
architecture boundaries and documentation, and test and gate coverage.

Every finding below was verified by hand against the code before being recorded.
Where an auditor's characterization did not survive that check it was corrected
rather than repeated; two were.

## Must Fix

> Both must-fix findings were remediated in `474651bc`. They are kept here as the
> audit-time record; see this folder's `progress.md` for what landed and how it
> was proven.

### 1. Token replacement orphans held-action leases, leaving a key stuck down

`key_runtime_core_release_leases_for_token()`
(`users/noah/lib/key/runtime/reducer/ownership_state.c:658`) deliberately skips
`LEASE_KIND_HELD_ACTION` and `LEASE_KIND_REPEAT`. Its other two callers
compensate — the release-settlement path plans a `RELEASE_OWNED_STATE_BY_KEY`
effect, and `key_runtime_core_finalize_non_handled_release()` runs the
key_pos-keyed sweep. The cancel path in `key_runtime_core_press_token_begin()`
(`users/noah/lib/key/runtime/reducer/runtime.c:483`) does neither, because it
runs inside the observe path, which has no effect plan.

Sequence:

1. `KEY_DOWN` at P for a handled key whose hold registers an action. Token T1.
2. Scan past `tap_hold_term` creates lease `{HELD_ACTION, owner=T1, key_pos=P}`
   and presses the action.
3. A second `KEY_DOWN` at P with no intervening release cancels T1. The lease
   survives, still owned by T1. New token T2.
4. `KEY_UP` at P: `key_runtime_core_resolve_active_release()`
   (`users/noah/lib/key/runtime/planning/release_planner.c:215-216`) asks
   `key_runtime_core_owner_has_lease_kind(state, T2, …)`, which is false, so no
   release effect is planned.

The lease and the applied held-action binding both survive and the action stays
pressed. Recovery requires a later press at P reaching a hold threshold, or a
later release falling through to the unmatched-release path.

Step 3 is reachable in this profile, not hypothetical.
`noah_qmk_combo_origin_normalize_record()`
(`users/noah/lib/compat/qmk_combo_origin.c:718`) rewrites a combo output's
`record->event.key` to the combo owner's keypos, so a combo whose owner key is
already holding a registered action delivers exactly that second `KEY_DOWN`.
`COMBO_ENABLE = yes` in `users/noah/rules.mk:20` and the profile defines combos.

The repo already models token replacement — `cancelled_press_count` exists, and
`tests/host/runtime_debug_test.c:2347` performs two `KEY_DOWN`s at one keypos —
but that test asserts only the layer-lease case, which is precisely the kind the
sweep does clean. The held-action case is uncovered.

### 2. No renegotiation when the peer loses its snapshot session mid-transfer

All three master tick paths — push chunk
(`users/noah/lib/compat/qmk_via_split_sync.c:611`), push commit (`:631`), and
pull chunk (`:661`) — collapse every non-matching response into
`noah_qmk_via_schedule_retry()`. That function (`:138`) only grows the backoff to
its 1000 ms cap and never touches `noah_qmk_via_tx_phase`. Phase is assigned only
on success (`:586`, `:597`, `:650`). No path inspects `response.status`.

Sequence: the master is mid-push when the slave resets (TRRS brownout, replug).
`noah_qmk_via_split_sync_init()` zeroes the receiver, so `active == false`. Every
subsequent `PUSH_CHUNK` is rejected with `SNAPSHOT_REQUIRED`, the master retries
the same chunk at 1 Hz forever, and `SNAPSHOT_BEGIN` is never re-sent. The halves
stay divergent with `recovery_required = true` persisted on the slave. Escape
requires a fresh local VIA edit, a role change, or a reboot.

This one predicts a hardware-test failure. Finding 05's outstanding obligation is
exactly the physical disconnect, power-cycle, and reconnect matrix, which is the
scenario that triggers this. It is worth fixing before that matrix is run.

Existing coverage does not reach it: `test_each_outbound_boundary_recovers_from_loss`
(`tests/host/qmk_via_split_sync_test.c:399`) drops the *transport* result, and its
fake peer is stateless and unconditionally acknowledges, so peer-side session
loss is never modelled.

## Should Fix

### 3. Out-of-range VIA buffer writes mutate storage but classify as non-mutations

`noah_qmk_via_classify_mutation()` (`users/noah/lib/compat/qmk_via_contract.c:119`)
returns false when `payload_size > capacity - offset`, so `via_command_kb()`
(`users/noah/lib/macro/via_macro_defaults.c:219`) records no effects: no
mutation note, no digest restart, no macro-cache invalidation. Returning false
hands the command to upstream `via_command()`, which *partially applies* it —
`nvm_dynamic_keymap_macro_update_buffer()`
(`../bastardkb-qmk/quantum/nvm/eeprom/nvm_dynamic_keymap.c:174`) writes every byte
whose `offset + i` is in range and drops the rest.

Result: EEPROM changes while the advertised digest keeps its pre-write value, so
split metadata reports the halves equal while storage has diverged. It also feeds
finding 2, because a later push then carries a digest the receiver cannot
reproduce. Untrusted-input only; a conformant VIA host never sends this frame.

### 4. Coherent-read helpers promise a guarantee they do not provide

`split_runtime_sync_remote_read_combo()` and its two siblings
(`users/noah/lib/split/runtime_sync.h:223-277`) copy into the caller's
destination *before* re-checking the generation, so an exhausted retry budget
leaves a mixture. Both the helper contract (`:216-222`) and the caller comments
(`users/noah/lib/rgb/core/rgb_runtime.c:70-73`, `:110-112`) state the opposite,
and the caller passes the live frame cache directly.

The reachable half is safe: an in-flight publication is caught by the `in_flight`
pre-check, which continues before writing anything. Exhaustion requires a
complete publication inside each of four sub-microsecond copies, which the
transport cannot produce. This is a documented invariant the code does not
enforce, on the seam the previous review existed to make trustworthy.

### 5. `synthetic_record.c` has no behavioral coverage and gates the ownership seam

The module is never linked into any host binary; eight test units define a no-op
`noah_synthetic_record_active()` returning false. `process.c` branches on it at
lines 104, 229, 260, and 287 — line 229 being the guard on
`key_runtime_process_settle_report_ownership()`, whose job is to stop synthetic
emissions being credited as default-handler report ownership.

Two mutations leave the whole suite green: deleting the depth increment pair at
`synthetic_record.c:29-33`, and changing `MAKE_KEYEVENT(UINT8_MAX, UINT8_MAX, …)`
at `:12` to `(0, 0, …)` so synthetic records alias real matrix position (0,0).

### 6. Three compile-gate guards pass vacuously when their search fails

`run_macro_payload_engine_tests.sh:47` and `run_feature_gate_compile_tests.sh:60`,
`:70`, `:82` use `if rg -n '<forbidden>' …; then fail; fi`. A non-zero `rg` exit
makes the condition false, and `set -e` is suppressed inside an `if`. `rg` exits
127 when missing and 2 on a bad path, so both read as "no violations". No
`command -v rg` preflight exists anywhere. This was live until ripgrep was
installed on 2026-08-16, while Review 17 recorded the suite as green.

`run_owned_keycode_tests.sh:29,39` uses a capture-and-compare shape that fails
loudly instead; both its allowlists are still exactly accurate.

### 7. The memory gate bounds `.bss` only, and the largest consumer is in `.data`

`tools/check_firmware_memory_budget.py:97` measures `__bss_end__ - __bss_base__`.
`objdump -h` confirms `.bss` is 25,524 B at 0x20005ef0 while `.data` is 24,300 B
at 0x20000000, and `noah_runtime_singleton` (20,288 B) sits inside `.data`. The
gate therefore excludes the single largest static object, which is why the ~36
bytes `aae9c445` added produced no movement in the reported figure. Growth is
bounded indirectly by the 204,800 B heap minimum, leaving about **7,512 B** of
silent headroom.

### 8. The pd-runtime to key-core include gate matches nothing

`run_feature_gate_compile_tests.sh` greps `users/noah/lib/pointing/runtime` for
`reducer/runtime.h`, but the sanctioned bridge reaches core through
`pd_mode_key_runtime_bridge.c:3` → `reducer/ownership_state.h:3` → `runtime.h`.
The pattern matches nothing today, and any pd module can include
`ownership_state.h` to obtain the full `key_runtime_core_state_t` without
tripping it. The contract the error message states is not the contract enforced;
only the two call-name checks still bite.

### 9. Six dead public functions, two of them a naming trap

Each has exactly two whole-tree references — declaration and definition:
`handled_key_resolution_uses_implicit_hold` and
`handled_key_resolution_uses_fallback_hold` (`handled_key.h:155-156`),
`key_runtime_core_flashing_feedback_started_at`
(`reducer/ownership_state.h:22`), `noah_runtime_diag_test_backend_seed_watchdog_reboot`
(`state/diagnostics/runtime_diag.h:41`, a test seam with no test),
`pd_any_display_mode_active` (`pointing/defs/pd_mode_flags.h:102`), and
`split_runtime_sync_combo_is_dirty` (`split/runtime_sync_dirty.h:10`).

The first two matter beyond flash: they sit one suffix away from the live
internal predicate `handled_key_resolution_uses_fallback_hold_behavior`
(`handled_key_internal.h:26`), so a maintainer can call the dead public wrapper
believing it is the one the reducer consults.

### 10. `key_feedback_pulse_arm()` never sets the pulse key position

`users/noah/lib/key/runtime/feedback.c:264` sets timer, sequence, active, kind
and tap branch, and clears the queued slot, but leaves `feedback_pulse_key_pos`
at its previous value — `{0,0}` on fresh state, which is a *valid* keypos. The
semantic and broad-owner maps then attribute the pulse to the wrong key. Compare
`key_runtime_core_feedback_projection_set_pulse()`
(`projection/feedback_projection.c:20`), which does set it. Latent, because the
function has no production caller, but it is live API on `feedback.h:298`. Fix or
delete it.

### 11. `RGB_LEFT_LED_COUNT` duplicates `RGB_MATRIX_SPLIT[0]` with nothing enforcing agreement

`users/noah/lib/rgb/core/rgb_helpers.h:180` hard-codes `29` with only a comment
saying it must match. The authority is `users/noah/config.h:93`
(`RGB_MATRIX_SPLIT {29, 29}`). No static assert, test, or derivation links them.

Change the split to `{30, 28}` and upstream clamps the left half to 30 while
`rgb_runtime_normalize_local_range()` (`rgb_runtime.c:144-150`) re-clamps to 29.
LED 29 then falls outside every userspace chunk on the left half and is rejected
on the right, so the base effect keeps driving it while every layer, pd-mode,
combo and key-feedback stage skips it. Silent, with no diagnostic. Deriving the
split from the count closes it.

### 12. Documentation gaps that would mislead a change

- `docs/architecture/source-map.md:193` lists `split/` as `runtime_sync.c/h`
  only; `runtime_sync_dirty.c/h` exist and are in the manifest. The doc bills
  itself as the way to check package inventory against the tree.
- `users/noah/lib/state/shared/runtime_publication.h` appears in **no** doc,
  despite being the worker-versus-main concurrency contract for split and
  pd-mode snapshot storage. Anyone editing that storage has no doc route to the
  seqlock rules.
- `docs/HOOK_OVERRIDES.md` omits three userspace-defined QMK hooks: `via_init_kb()`
  and `via_command_kb()` (`users/noah/lib/macro/via_macro_defaults.c:212,216`) and
  `is_keyboard_master_impl()` (`users/noah/lib/compat/split_role.c:17`). A
  keyboard-level `via_command_kb()` would silently drop VIA mutation
  classification, macro reseeding, and split-mirror marking.
- `Sol Findings/00-overarching-remediation-roadmap.md:154` still says "Review 17
  regressed Finding 08" and `:230` that it "partially reopened Finding 15". Both
  closed in `aae9c445`. This was missed when `implementation-progress.md` was
  updated in that same commit.

## Optional Cleanup

- **23 dead functions beyond finding 9.** 17 unused `static inline` helpers
  across headers, including an entire hold-contract predicate vocabulary
  (`hold_fires_at_threshold`, `hold_registers_while_held`,
  `hold_repeats_while_held`, `hold_sends_on_release`, `handled_key_hold_contract_*`)
  that nothing calls — a designed abstraction that was bypassed.
- **Write-only split base-domain state.** `split_runtime_sync_remote.active_mode_id`,
  `.locked_mode_id`, `.pd_mode_owner_sides`, `.pd_mode_owner_bitmap` are written
  at `runtime_sync.c:352-357` and never read; the only production readers of that
  struct are single-field. `base_generation` consequently has no production
  reader either. Decide them together.
- **Display state in the key-runtime core.** `preview_display_bridge_*` and
  `preview_display_last_semantic_layer` (`reducer/runtime.h:361-364`) are touched
  only by `key/runtime/feedback.c` for RGB hysteresis, are absent from the
  projection snapshot, yet own part of `key_runtime_core_state_reset()`'s
  special-casing.
- **Unguarded divisor.** `NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR`
  (`pd_mode_dragscroll.c:169`) is the one tunable in this subsystem without a
  static assert, while its sibling `dragscroll_axis_divisor()` runtime-guards. A
  keymap override of `0` divides by zero on the first locked-axis scroll.
- **Duplicate frame symbol.** `rgb_runtime.c:22` and
  `rgb_automouse_stage.c:20` both declare `static rgb_runtime_frame_t rgb_runtime_frame_primary`
  — two distinct ~232 B objects sharing an identifier. Never live
  simultaneously, but confusing next to the static assert policing a 96 B
  snapshot budget.
- **Frame type in the wrong package.** `rgb_runtime_frame_t` is declared in
  `rgb/stages/rgb_layer_stage.h:20` but is the shared type for `core/` and
  `automouse/`, inverting the layering `source-map.md:65` documents.
- **Single-slot pulse queue** (`projection/feedback_projection.c:26-41`) silently
  drops a middle pulse when three arrive inside one flash half-period.
  Newest-wins looks intentional; say so in a comment.
- **Test-only reducer entry points** with no production callers:
  `key_runtime_core_flush_multi_tap`, `key_runtime_core_flush_active_keys_except`,
  `key_runtime_core_observe_release_dispatch_deferred` / `_drained`,
  `key_runtime_core_pending_release_count_for_keypos`,
  `key_runtime_core_has_any_deferred_release_blocker`.
- **Two stale README anchors.** `README.md:236` cites `keymap.c#L435` for the
  `RIGHT_THUMB` row, which starts at `:442`; `README.md:426` cites
  `config.h#L42` for the custom split RPCs, which are at `:54`.

## Verified Solid

Confirming enforcement matters as much as finding leaks. These were specifically
probed.

- **Publication shape parity.** The in-place and slot shapes carry incompatible
  parity conventions and mixing them would be serious. Every call site was
  traced: `in_flight` appears only on the three in-place split readers, `slot`
  only on the pd-mode paths. No cross-application.
- **The ownership seam settles each event exactly once.** The concern was that a
  consumed press would call finalize with `false` from `hooks.c:25` and then have
  QMK call `post_process_record_user()`, crediting report ownership to a press
  that never reached a default handler. Upstream disproves it: `process_record()`
  (`../bastardkb-qmk/quantum/action.c:293-300`) returns early without
  post-processing.
- **VIA frame validation.** `payload_length` is bounded before the memcpy, range
  checks use the non-wrapping `payload_length <= region_length - offset` form
  throughout, and every region read and write funnels through
  `noah_qmk_via_storage_range_valid()`.
- **Macro playback lifecycle.** The shared active IR is guarded at all three
  entry points, so a second dispatch cannot rewrite an in-flight IR. No lease
  leak on partial-acquire or error paths. Slot indexing is safe by static assert.
- **Pending-release FIFO and token allocator.** Unlink validates head, tail and
  prev-link coherence; capacity exhaustion executes inline rather than dropping;
  token-ID reuse would need ~65k presses within one key's press-to-release.
- **Chunked RGB rendering.** All five iterations traced on both halves against
  upstream limits. No stage paints outside its chunk, and per-frame snapshot
  invalidation fires exactly once per frame on both halves.
- **Mode-switch state.** The outgoing pd mode always runs its reset before the
  incoming one activates, so the dragscroll state shared by DRAGSCROLL and PINCH
  is never inherited. The arrow and dragscroll fixes from `aae9c445` hold.
- **Boundaries that hold mechanically.** The keymap/runtime header split is gated
  in both directions across discovered profile paths; raw QMK layer actions are
  rejected in `keymaps[][]`, combo outputs and `key_behaviors[]`, with the
  firmware build hard-failing through `users/noah/rules.mk`; `lib/compat/`
  centralization holds; `runtime_context_internal.h` has no reach-arounds.
- **The keymap is genuinely data-driven.** `keymap.c` and `rgb_config.c` contain
  zero function definitions and one include each.
- **Build integrity.** All 94 production `.c` files are in `source_manifest.mk`
  with no stale or duplicate entries; `noah_source_manifest.sh:78` rejects any
  hand-picked test source absent from the real make manifest, so host binaries
  cannot silently test a different source set than the firmware links.
- **Feature-flag matrix.** `RGB_PD_MODE_ACTIVE_HALF_ENABLE` and
  `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` looked ungated because the gate script names
  only six macros, but individual test runners compile them on while the gate's
  default config compiles them off. Both paths are covered.
- **Host runner coverage.** Every `*_test.c` is compiled by some runner, and
  every runner is reached except three deliberate exclusions.
- **The Finding 08 precedent is closed.**
  `key_runtime_physical_ownership_integration_test.c` links the real ownership
  modules rather than stubbing them.

## Prior Finding Status

| Prior finding | Status | Evidence |
| --- | --- | --- |
| 08 — Synthetic-key ownership | resolved | Report ownership settles at finalize on the final event result; modifier ownership separates physical from report counts; enforced by the new integration suite, which fails against the pre-fix tree. Single-settle confirmed against upstream. Caveat: the synthetic guard itself is uncovered, finding 5. |
| 15 — RGB render work | resolved | Each domain publishes as one generation; readers retry bounded and never block the worker. Enforced by seam-driven interleaving tests. The contract-comment gap in finding 4 does not affect the reachable path. |
| 10 — Pointing backlog bounds | partially resolved | Inactive-axis debt cancels at the dominant-axis transition, verified independently this pass. Flashed timing and feel matrix still hardware-pending. |
| 11 — Dragscroll stall recovery | resolved | Saturating accumulation and total `abs32` verified this pass, including the `INT32_MIN` case. |
| 05 — VIA split persistence | partially resolved | Finding 2's renegotiation gap is fixed in `474651bc` and covered by a peer that can lose its session mid-transfer, so the hardware matrix is unblocked. The physical two-half, power-cycle, reconnect, and role-swap matrix is still required. |
| 03, 04, 07, 14 — VIA/macro validation and RAM | resolved | Re-verified this pass: frame validation, text-byte validation at decode and replay, tap-list bounds, shared-IR guarding, and static-asserted slot indexing all hold. |

## Current Conclusion

The recently landed work holds up: the publication scheme and the ownership seam
were both probed adversarially and neither broke. The two must-fix findings are
older code, and both are the kind that only appear under interleavings the tests
do not construct — a second key-down at a live slot, and a peer that disappears
mid-transfer.

The pattern worth naming is that most findings are about **enforcement rather
than logic**. Finding 08 originally hid because every harness stubbed the ledger
it depended on. The `rg` guards passed because a missing tool reads as success.
The memory figure looks like a RAM budget while measuring half of RAM. The
pd-to-core include gate greps for a pattern that no longer occurs. In each case a
gate was present and green while proving less than its name suggested. That is
the failure mode this codebase should keep auditing for, and it is more valuable
than any single bug on this list.

## Remaining Open Findings

1. ~~Must fix: held-action leases across token replacement~~ — landed in
   `474651bc`.
2. ~~Must fix: split snapshot renegotiation~~ — landed in `474651bc`.
3. **Should fix:** items 3 through 12 above.
4. **Optional:** the cleanup list above.
5. **Hardware verification:** Finding 05's persistence and role-swap matrix,
   unblocked by `474651bc`.
6. **Hardware verification:** Finding 10's arrow timing and feel matrix.

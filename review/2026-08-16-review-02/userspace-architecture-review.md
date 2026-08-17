# Full Codebase Review — Firmware Userspace and Authored Profile

This review uses `prompts/initial-architecture-review.md`. It covers the tree at
`bcd9b187`, a complete pass over `users/noah/` and
`keyboards/bastardkb/charybdis/4x6/keymaps/noah/`. Python tooling under `tools/`
was explicitly out of scope at the user's request.

## Why This Folder

`review/2026-08-16-review-01` (Review 18) records a closure verdict — every
software finding it raised is closed and only hardware matrices remain — so it
is immutable history. Five commits landed after that verdict (`f2d3a025`,
`ec3ab7b1`, `3172e22f`, `7c2042a9`, `bcd9b187`) and none of them has been
reviewed. This pass audits the whole tree including that new code, so it opens
the next sortable folder rather than appending to a closed audit.

## Scope and Method

Every finding below was read out of the current source and traced to its callers
before being recorded; where the fork's behavior mattered it was checked against
`../bastardkb-qmk` rather than assumed. Both gates were run against this tree
and pass (see `progress.md`).

The five post-closure commits received the most attention, because they are the
least-reviewed code in the tree and three of them touch seams the previous
review had just finished hardening: the split base packet, the VIA
reconciliation state machine, and VIA write-through.

## Must Fix

### 1. The split mirror's payload-length guard wraps and lets an over-long VIA buffer write read off the end of the RPC buffer

`noah_qmk_via_split_mirror_apply_command()`
(`users/noah/lib/compat/qmk_via_split_mirror.c:49` and `:64`) guards both buffer
commands with:

```c
size = data[3];
if (length < (uint8_t)(4u + size)) {
    return;
}
```

`length` and `size` are both `uint8_t`. `4u + size` is computed as `unsigned
int`, then cast back down: for `size >= 252` the sum wraps to `0..3`, the
comparison is false, and the guard passes. The call that follows,
`dynamic_keymap_set_buffer(offset, size, &data[4])` (`:53`) or
`noah_qmk_via_macro_set_buffer(...)` (`:68`), then reads `size` bytes — up to 255
— starting four bytes into a buffer that is at most `RPC_M2S_BUFFER_SIZE` (32)
bytes long, and writes what it finds into EEPROM.

The window is exactly `size ∈ [252, 255]`, which reads about 220 bytes past the
end of the slave's `rpc_m2s_buffer`.

Reachability is real, not theoretical. `noah_qmk_via_classify_mutation()`
deliberately ignores `payload_size` for these two commands
(`users/noah/lib/compat/qmk_via_contract.c:125`, the fix from finding 3 of the
previous review) and only rejects `offset >= capacity`. So a VIA frame of
`[id_dynamic_keymap_set_buffer, 0, 0, 255, …]` classifies as a mutation with
`SPLIT_MIRROR`, `via_command_kb()`
(`users/noah/lib/macro/via_macro_defaults.c:232`) forwards the raw 32 bytes to
the mirror, and the slave executes the path above. Untrusted-host input only — a
conformant VIA host never sends `size > 28` — but this is the half of the
codebase that the previous review's finding 3 explicitly made *more* reachable,
and the guard exists precisely to stop it.

Upstream `via_command()` has the same unchecked read on the master
(`../bastardkb-qmk/quantum/via.c:453-457`, `// size <= 28` by comment only), and
Review 18 recorded that as a deliberate upstream decision. This one is different:
it is our code, it has an explicit guard, and the guard does not work.

Fix: compare in a type that cannot wrap, e.g.
`if ((uint16_t)length < (uint16_t)4u + (uint16_t)size) return;`, in both cases.

## Should Fix

### 2. `f2d3a025` publishes auto-mouse fade progress from a free-running timer, so an idle keyboard broadcasts a phantom fade forever

`split_runtime_sync_auto_mouse_elapsed()`
(`users/noah/lib/split/runtime_sync.c:106-123`) now returns
`noah_qmk_contract_auto_mouse_elapsed_at(now)` unconditionally. The commit's
reasoning is right — `is_auto_mouse_active()` means "the pointer is in use right
now", not "the layer is on", so gating on it blanked the slave for exactly the
fade window — but the replacement has no gate at all, and the underlying clock is
not a bounded countdown.

`auto_mouse_get_time_elapsed_at()`
(`../bastardkb-qmk/quantum/pointing_device/pointing_device_auto_mouse.c:114`) is
a bare `(uint16_t)(now - auto_mouse_context.timer.active)`.
`pointing_device_task_auto_mouse()` refreshes `timer.active` only while
`is_auto_mouse_active()` is true, and zeroes it when the layer times out (`:277`).
Once the pointer is idle the subtraction free-runs and wraps every 65.536 ms ×
1000 ≈ 65.5 s.

With `AUTO_MOUSE_TIME 1200` and `AUTOMOUSE_RGB_DEAD_TIME 400`,
`automouse_rgb_progress()` clamps anything past 1200 to the maximum, so the
published `automouse_progress` is nonzero for about 65.1 s of every 65.5 s
cycle, dropping to zero for 400 ms and then ramping through 25 quantized steps
(`AUTOMOUSE_RGB_SYNC_STEP` = `RGB_MATRIX_LED_FLUSH_LIMIT` = 32 over an 800-unit
span). Two consequences:

- `split_runtime_base_packet_is_active()` (`runtime_sync.c:188`) returns true
  whenever progress is nonzero, which selects
  `SPLIT_RUNTIME_SYNC_ACTIVE_HEARTBEAT_MS` (250) instead of
  `..._IDLE_HEARTBEAT_MS` (1000) (`:258-262`). A completely idle keyboard now
  sends the base packet at 4 Hz permanently, plus ~25 change-driven sends per
  wrap — roughly 285 sends per 65.5 s where it used to send ~65. Each receive
  also republishes the pd-mode remote display generation
  (`pd_mode_state.c:375-412`, which publishes unconditionally). The commit
  message estimates "about 8-9 extra base sends per timeout window", which is
  correct for a real timeout window but does not account for the wrap repeating
  forever.
- The phantom progress can reach the LEDs. `rgb_runtime_automouse_stage_should_render()`
  (`rgb_automouse_stage.c:86`) gates only on the auto-mouse layer being in
  `layer_state`, and `pointer_layer_policy_apply()`
  (`pointing/policy/pointer_layer_policy.c:90`) forces that layer on whenever
  `pointer_layer_policy_auto_mouse_anchored()` holds. An unlocked pd-mode anchor
  keeps the layer on while `is_auto_mouse_active()` goes false, so the fade
  re-runs from the start once per wrap. `pd_any_display_mode_locked()`
  (`rgb_automouse.c:29`) suppresses this only for *locked* modes.

The master-side reader `automouse_rgb_current_progress()` (`rgb_automouse.c:24`)
was already ungated, so this is not a regression the commit invented — it is a
pre-existing sharp edge that the commit propagated to the split packet and to
the transport budget.

Fix that keeps the commit's intent: gate the published value on the auto-mouse
layer actually being active — `layer_state_cmp(layer_state, noah_qmk_contract_auto_mouse_layer())`
— rather than on `is_auto_mouse_active()`. That still publishes progress while
the pointer is idle and the layer is counting down, which is the case the commit
fixed, and it stops publishing once the layer is gone.

### 3. The base sync domain has a publication generation but no coherent reader, contradicting its own header contract

`runtime_sync.h:18-21` states the rule plainly: "every domain is published as
one publication generation and main-context readers copy a domain through the
`split_runtime_sync_remote_read_*` helpers below instead of touching the fields
directly." Three such helpers exist (`:226`, `:250`, `:274`) and Review 18's
finding 4 fix correctly gave all three proper staging.

There is no `split_runtime_sync_remote_read_base()`. The two main-context
readers of that domain touch the fields directly:

- `users/noah/lib/rgb/automouse/rgb_automouse.c:26` — `split_runtime_sync_remote.automouse_progress`
- `users/noah/lib/rgb/stages/rgb_preview_stage.c:14` — `split_runtime_sync_remote.key_preview_layer`

`split_runtime_sync_slave_base_rpc()` (`runtime_sync.c:360-370`) still wraps the
whole group in `begin`/`end`, so `base_generation` is written on every base
packet and read by nobody. The reachable risk is low — both fields are naturally
aligned scalars, and the group's only multi-byte member
(`pd_mode_owner_bitmap`) is consumed inside the RPC itself — but this is the
same shape as the previous review's closing observation: a mechanism that is
present, costed, and documented as load-bearing while proving nothing. Either
add the base read helper and route both readers through it, or narrow the header
contract to the three domains it actually covers and drop `base_generation`.

Review 18 listed the write-only base-domain fields as optional cleanup and noted
`base_generation` "consequently has no production reader either". This finding is
the same object seen from the contract side, and it should be decided together
with that cleanup rather than separately.

### 4. Two VIA commands are classified as `SPLIT_MIRROR` but the mirror cannot apply them

`noah_qmk_via_command_effects()` (`users/noah/lib/compat/qmk_via_contract.c:72`)
and `noah_qmk_via_classify_mutation()` (`:143-148`) attach
`NOAH_QMK_VIA_COMMAND_EFFECT_SPLIT_MIRROR` to `id_set_keyboard_value` /
`id_layout_options` and to `id_eeprom_reset`. The mirror's own switch,
`noah_qmk_via_split_mirror_apply_command()`
(`users/noah/lib/compat/qmk_via_split_mirror.c:33-88`), has no case for either
and falls through to `default: return;`.

The result for a layout-options write is a split RPC that is built, sent, and
discarded, and — because the early `return` skips
`noah_qmk_via_split_sync_note_local_storage_changed()` at `:93` — the receiver
does not even restart its digest. Correctness is preserved only because layout
options live in the reconciled `VIA_CONFIG` region
(`qmk_via_storage_regions.c:15`, `:30-52`), so the durable layer repairs it. The
effect flag is nonetheless claiming a capability the mirror does not have.
(`id_eeprom_reset` is inert today because `VIA_EEPROM_ALLOW_RESET` is not defined
anywhere in this tree — it is only referenced by the two `#ifdef`s in
`qmk_via_contract.c` — but it becomes live the moment someone enables it.)

Two switches over the same command-id domain with no mechanical link is exactly
the coupling this codebase keeps getting bitten by. Either handle both commands
in the mirror, or stop setting `SPLIT_MIRROR` for commands the mirror does not
implement, and add a gate that keeps the two lists in agreement.

### 5. `key_runtime_core_resolve_pending_multi_tap_release()` copies an indeterminate value into its resolution

`users/noah/lib/key/runtime/planning/release_planner.c:388` declares
`key_feedback_pulse_kind_t action_feedback_kind;` with no initializer. It is
assigned only inside `key_runtime_core_release_hold_action_feedback_kind()`,
which is called at `:389` behind a short-circuit:

```c
bool action_feedback = series_tap_count > 1u && key_runtime_core_release_hold_action_feedback_kind(token, decision.action, elapsed, &action_feedback_kind);
```

When `series_tap_count <= 1` the helper never runs, and the indeterminate value
is read at `:407` into `.action_feedback_kind`. No observable misbehavior today:
the only consumer, `key_runtime_core_plan_pending_multi_tap_release_effects()`
(`:510-512`), checks `resolution->action_feedback` first, which is false on that
path. It is still a read of an uninitialized object, and the sibling site 44
lines up (`:344`) initializes the same variable to `KEY_FEEDBACK_PULSE_HOLD`
before the identical call. Match `:344`.

### 6. An unattributable combo output repaints the entire board

`noah_qmk_combo_origin_normalize_record()`
(`users/noah/lib/compat/qmk_combo_origin.c:723-728`) handles the case where a
`COMBO_EVENT` press matches neither a pending output entry nor an untracked
active combo by calling `combo_origin_bitmap_fill_all_keys(bitmap)` and storing
that as the combo's footprint, both in the active cache and in the origin
registry.

Every downstream consumer treats that footprint as truth:
`noah_qmk_combo_origin_active_bitmaps_partitioned()` ORs it into the combo
overlay, so `rgb_runtime_combo_feedback_stage_render_overlay()` paints all 58
LEDs in the combo color for as long as the combo is held;
`key_origin_registry_side_mask()` reports both halves; and any key-feedback
semantic attributed to that owner keypos broadens to all keys through
`key_feedback_apply_semantic_for_owner()`.

The path is instrumented (`unmatched_delayed_output_count`) and reachable —
`combo_origin_pending_output_store()` refuses when more than
`COMBO_BUFFER_LENGTH` (4) candidates are outstanding, incrementing
`cache_full_refusal_count` — and this profile has seven combos with three
overlapping pairs on `MS_BTN1`/`MS_BTN2`/`VOLUME_MODE` and `KC_N`/`KC_M`. The
registry entry does self-heal, because `noah_pre_process_record_user()` resets it
via `key_origin_registry_set_single()` (`key/runtime/process.c:265`) on the next
physical event at that key, so the blast radius is bounded in time — but a
whole-board flash is a very loud way to express "I could not attribute this".
Falling back to the single owner keypos (which the same branch already computed)
degrades far more gracefully.

### 7. `NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR` is still the one unguarded divisor in its subsystem

`users/noah/lib/pointing/modes/pd_mode_dragscroll.c:169` divides by the macro
directly, while its sibling `dragscroll_axis_divisor()` (`:119-122`)
runtime-clamps to 1 and every other tunable in the file carries a
`_Static_assert`. A keymap override of `0` divides by zero on the first
locked-axis scroll. Carried forward unchanged from Review 18's optional list; it
is a one-line `_Static_assert` and it keeps getting deferred.

## Optional Cleanup

- **Three diagnostic counters wrap silently while their siblings do not.**
  `state->cancelled_press_count++` (`reducer/runtime.c:485`),
  `state->orphan_release_count++` (`:704`) and
  `state->release_keycode_mismatch_count++` (`:713`) are unguarded `uint8_t`
  increments; `token_allocation_failure_count` (`:475`) and
  `pending_release_validation_failure_count`
  (`queue/pending_release_queue.c:500`) both saturate at `UINT8_MAX`. A wrapped
  counter reads as "healthy" in `runtime_diag`.
- **The press path scans all slots while the scan path uses the active bitmap.**
  `key_runtime_core_press_token_begin()` (`reducer/runtime.c:511`),
  `key_runtime_core_interrupt_active_keys_on_other_press()` (`:959`),
  `key_runtime_core_flush_active_keys_except()` (`:983`) and
  `key_runtime_core_settle_pending_fallback_hold()`
  (`planning/scan_planner.c:600`) all iterate
  `KEY_RUNTIME_CORE_PRESS_TOKEN_CAPACITY` unconditionally, resolving a keypos per
  slot, while `key_runtime_core_scan()` (`:1158`) and
  `key_runtime_core_refresh_for_time()` (`:417`) walk
  `press_token_active_bitmap` with `__builtin_ctz`. 48 slots is cheap, but the
  optimization is now half-applied and the two shapes will drift.
- **`key_runtime_process_notify_planless_feedback_change()` is in the wrong
  file.** Added by `7c2042a9`, it is named for `process.c`, defined in
  `press.c:24`, declared in `transition.h:39-43`, and called from both `press.c`
  and `release.c`. `process_internal.h` is the header both callers already
  include for shared process-layer helpers.
- **Queries on the render and publish paths mutate core state.**
  `key_feedback_pulse_active()` (`key/runtime/feedback.c:265`) advances the
  queued pulse and restarts its timer; `key_feedback_preview_layer()` (`:350`)
  drives the preview bridge hysteresis. Both are called from the RGB render
  (`rgb_runtime.c:103`, `rgb_preview_stage.c:14`) *and* from the split packet
  builders (`runtime_sync.c:145`, `:168`), so which caller advances the state
  machine depends on tick order. The two consumers happen to agree today because
  a promotion restarts the timer, so a second call inside the same frame still
  sees the pulse active — but that is a coincidence of the implementation, not a
  stated invariant. This is the same object as Review 18's "display state in the
  key-runtime core" cleanup, seen from the caller side.
- **`key_runtime_core_scan_press_token_has_runtime_owned_state()`**
  (`planning/scan_planner.c:151-155`) mixes a null check into a ternary
  (`token && x != KC_NO ? true : repeat_active_at(token ? pos : (keypos_t){0})`)
  in a way that reads as a bug and is not one. Two `if`s would say the same
  thing.
- **`VIA_SPLIT_SYNC_SESSION_REJECT_LIMIT` counting is slightly imprecise.**
  `noah_qmk_via_session_reject_count` (`compat/qmk_via_split_sync.c:90`) is
  cleared on progress (`:155`) and on reaching the limit (`:190`), but not when
  the phase is reset for an unrelated reason —
  `noah_qmk_via_schedule_session_refresh()` (`:200`) and the digest tick (`:877`)
  both jump to `METADATA` leaving a partial count behind, so the next transfer
  can renegotiate one rejection early. Harmless; worth a comment or a clear.

## Verified Solid

Probing what holds matters as much as what leaks. These were specifically
checked this pass and are correct.

- **The coherent-read fix from Review 18 finding 4 is real.** All three helpers
  (`runtime_sync.h:226`, `:250`, `:274`) copy into stack staging buffers and
  commit to the caller's destination only after
  `noah_runtime_publication_settled()`, so a failed read genuinely writes nothing.
  The header comment now describes what the code does.
- **`3172e22f`'s retry-before-renegotiate is sound and bounded.** The transient
  case (peer digest still computing) is handled by retry, and the terminal case
  (peer lost its session) escapes after three rejections
  (`qmk_via_split_sync.c:184-195`). The escape lands in `METADATA`, where a
  `BUSY` peer is retried rather than restarted
  (`noah_qmk_via_process_metadata_response()`, `:535`), so the livelock the
  commit describes cannot re-form.
- **`bcd9b187`'s mirror does not weaken the reconciliation layer.** It runs on
  its own transaction ID, its receiver restarts the local digest so the durable
  layer cannot advertise stale content (`qmk_via_split_mirror.c:93`), and
  `via_command_kb()` still notes the mutation afterwards. Finding 1 is a bug
  inside the mirror, not an objection to the design.
- **Held-action lease adoption across token replacement is correct.**
  `key_runtime_core_press_token_begin()` allocates the new token id *before*
  cancelling the old one (`reducer/runtime.c:474` then `:486`), so the old id is
  still reserved by `key_runtime_core_token_id_is_reserved()` and cannot be
  handed out; `key_runtime_core_adopt_runtime_owned_state_leases()` (`:487`,
  `reducer/ownership_state.c:673`) then re-owns exactly the two kinds
  `key_runtime_core_release_leases_for_token()` deliberately skips. Review 18's
  must-fix 1 is properly closed.
- **`RGB_LEFT_LED_COUNT` is now derived, not duplicated.** `users/noah/config.h:98-102`
  makes the count authoritative and computes `RGB_MATRIX_SPLIT` from it;
  `rgb_helpers.h:181-183` keeps only a host-test fallback. Review 18's finding 11
  is closed by construction rather than by discipline.
- **`owned_keycode`'s refcount arrays cannot be indexed out of range.**
  `OWNED_KEYCODE_USAGE_CAPACITY` (`action/owned_keycode.c:10`) is sized to
  `QK_MOUSE_ACCELERATION_2 + 1`, and `owned_keycode_is_report_usage()` (`:55`) is
  the only gate that sets `has_basic`, so every index is bounded by the same
  upper keycode the capacity is derived from.
- **The pending-release FIFO invariants hold.** `unlink` validates head, tail and
  the prev-link before mutating (`queue/pending_release_queue.c:265-284`), and
  the host-only structural validator (`:438`) checks reachability against
  activity in both directions.
- **The macro IR walker is bounds-safe.** Every opcode in
  `macro_payload_ir_next()` (`macro/macro_payload_run.c:79`) checks remaining
  length before consuming, `TAP_LIST` is additionally capped at
  `MACRO_PAYLOAD_MAX_TAP_KEYS`, and `macro_payload_ir_preflight()` (`:139`)
  requires a clean hold balance and exact end alignment before the engine starts.
- **The keymap stayed data-driven through `ec3ab7b1`.** `keymap.c` still contains
  zero function definitions and one include, and the new num-layer space
  tap-hold and numpad thumb keys are authored entirely as table data.
- **RGB frame writes stay inside the chunk.** `rgb_runtime_normalize_local_range()`
  (`rgb/core/rgb_runtime.c:129`) clamps to both `RGB_MATRIX_LED_COUNT` and the
  half boundary before any stage runs, and every frame writer indexes
  `[led_min, led_max)`.

## Prior Finding Status

Review 18 closed all twelve of its software findings; this pass re-checked the
ones whose fixes touched load-bearing seams, and tracked the optional items it
left open.

| Prior finding | Status | Evidence |
| --- | --- | --- |
| 18-1 — Held-action leases orphaned by token replacement | resolved | Allocation-before-cancel ordering plus `key_runtime_core_adopt_runtime_owned_state_leases()` re-owns exactly the skipped kinds; re-traced this pass. |
| 18-2 — No renegotiation on peer session loss | resolved, refined | `474651bc` added the escape, `3172e22f` bounded it at three rejections. Both directions verified against the receiver's `SNAPSHOT_REQUIRED` sources. |
| 18-3 — Out-of-range VIA buffer writes classified as non-mutations | resolved, but see finding 1 | The classifier change is correct. It also makes finding 1's frame reach the mirror, which is why that guard now matters. |
| 18-4 — Coherent-read helpers did not stage | resolved | Staging buffers in all three helpers; contract and code agree. |
| 18-9 / 18-10 / 18-11 — dead public surface, pulse keypos, RGB split derivation | resolved | Confirmed absent / derived in this tree. |
| 18-optional — unguarded dragscroll divisor | open | Now finding 7. |
| 18-optional — write-only split base-domain state and `base_generation` | open | Now finding 3, with the contract angle added. |
| 18-optional — display state in the key-runtime core | open | Now the fourth optional item, with the caller-side ordering concern added. |
| 18-hardware — Finding 05 persistence/role-swap matrix, Finding 10 arrow feel matrix | open | Still hardware-only. Finding 1 should land before the persistence matrix is run, since that matrix exercises the mirror path. |

## Current Architecture Assessment

The structure is in good shape and the boundaries are real. The reducer /
planner / projection split in `lib/key/runtime/` holds: planning functions take
state and return effect plans, application is a separate pass, and the two
directions of the keymap-versus-runtime header rule are gated in the build. The
compat layer genuinely centralizes fork assumptions — every VIA, auto-mouse,
combo and split-role dependency in the tree goes through `lib/compat/`, and this
review found no reach-arounds. The authored profile is data, and the build hard-
fails on invalid profile data before compiling a single object.

The three functional subsystems added or reworked most recently — split
publication, VIA reconciliation, VIA write-through — are each individually well
reasoned, and the commit messages behind them are unusually honest about what
they trade. That is worth saying explicitly because the findings above are
concentrated there: they are new code, not weak code.

The pattern in this review's findings is narrower than Review 18's "gates that
prove less than their name". It is **guards and contracts that are one type,
one call site, or one switch case short of what they claim**:

- a length guard that wraps in `uint8_t` (finding 1)
- a clock read that is correct for the case it was written for and unbounded
  outside it (finding 2)
- a documented reader contract with no reader for one of its four domains
  (finding 3)
- an effect flag whose consumer has no case for two of its commands (finding 4)
- an initializer present at one call site and missing at its twin (finding 5)

None of these is an architectural problem. All five are the kind of defect that
survives review because the surrounding design reads as careful — which it is.
The structural response is not a refactor; it is to make each of these
agreements mechanical, the way `RGB_LEFT_LED_COUNT` was turned from a comment
into a derivation.

The one place where the design itself invites trouble is the pulse and preview
state machines living in the key-runtime core and being advanced by whichever of
two unrelated consumers ticks first (fourth optional item). That is worth a
decision, not a patch.

## Recommended Next Refactor Sequence

1. **Finding 1** — widen both mirror length guards. One line each, and it should
   land before the Review 18 Finding 05 hardware matrix, which exercises this
   path.
2. **Finding 5** — initialize `action_feedback_kind` to match its twin. One line.
3. **Finding 2** — regate the published auto-mouse elapsed on the auto-mouse
   layer rather than on `is_auto_mouse_active()`, and extend the existing
   `split_runtime_sync_test` case to require that an idle board with the layer
   off publishes zero progress across a timer wrap.
4. **Finding 4** — decide whether the mirror handles `id_set_keyboard_value` and
   `id_eeprom_reset` or stops advertising `SPLIT_MIRROR` for them, then add a
   gate that keeps `noah_qmk_via_command_effects()` and the mirror's switch in
   agreement.
5. **Finding 3 together with Review 18's write-only base-domain cleanup** — one
   decision: either add `split_runtime_sync_remote_read_base()` and route
   `rgb_automouse.c` and `rgb_preview_stage.c` through it, or drop
   `base_generation` and narrow the header contract to the three domains it
   covers.
6. **Finding 6** — fall back to the single owner keypos instead of the whole
   board, and assert it in `qmk_combo_origin_test.c`.
7. **Finding 7 and the counter-saturation cleanup** — two small mechanical
   changes that close the last of Review 18's optional list.
8. **Only then**, the two hardware matrices that remain open from Review 18.

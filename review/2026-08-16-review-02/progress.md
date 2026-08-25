# Review 19 Progress — Full Firmware Codebase Review

Thread: full-tree architecture and correctness review of `users/noah/` and the
authored profile at `bcd9b187`. Findings live in
`userspace-architecture-review.md` in this folder.

## Reconciliation Note — 2026-08-25

The original sections through `Next Steps` are the audit-time snapshot at
`bcd9b187`. Remediation history is appended below rather than rewriting that
baseline. Findings 1 and 4 are now resolved; Review 19 as a whole remains open.

## Why This Folder Instead of Review 18

`review/2026-08-16-review-01` records a closure verdict ("The audit is
**complete**, and every software finding it raised is closed"), so per
`AGENTS.md` it is immutable history. Five commits landed after that verdict and
none of them had been reviewed:

| Commit | Subject |
| --- | --- |
| `f2d3a025` | Publish auto-mouse fade progress while the pointer is idle |
| `ec3ab7b1` | Update authored profile: num-layer space tap-hold and numpad thumb keys |
| `3172e22f` | Retry transient VIA sync rejections instead of renegotiating |
| `7c2042a9` | Announce plan-less feedback changes from key events |
| `bcd9b187` | Restore write-through VIA mirroring alongside reconciliation |

This pass covers the whole tree including those commits, so it opens the next
sortable folder rather than appending to a closed audit.

## Completed Work

Review only. **No source files were changed in this pass** — the worktree is
clean apart from this folder.

Read in full: the key runtime core (`reducer/`, `planning/`, `queue/`,
`projection/`, `slot/`, `feedback.c`, `process.c`, `press.c`, `release.c`),
ownership (`held_action.c`, `owned_keycode.c`), pointing (`pd_mode_state.c`,
`pd_mode_dragscroll.c`, `pointer_layer_policy.c`, `pd_mode_flags.h`), split
(`runtime_sync.c/h`, `runtime_publication.h`), compat (`qmk_via_split_sync.c`,
`qmk_via_split_mirror.c`, `qmk_via_contract.c`, `qmk_via_storage_regions.c`,
`qmk_combo_origin.c`, `qmk_auto_mouse_contract.h`), macro
(`macro_payload_run.c`, `via_macro_defaults.c`), RGB (`rgb_runtime.c`,
`rgb_layer_stage.c`, `rgb_automouse*.c`, `rgb_helpers.h`), wiring (`hooks.c`,
`runtime_init.c`, `rules.mk`, both `config.h`), and the authored profile
(`keymap.c`).

Cross-checked against `../bastardkb-qmk` where fork behavior was load-bearing:
`pointing_device_auto_mouse.c` (elapsed clock and `is_auto_mouse_active()`
semantics), `via.c` (`id_dynamic_keymap_set_buffer` size handling),
`split_common/transport.h` (`RPC_M2S_BUFFER_SIZE`).

### Findings

One must-fix, six should-fix, six optional cleanups, plus a Verified Solid
section and a Prior Finding Status table. Full detail with file references is in
`userspace-architecture-review.md`.

Headline: **`noah_qmk_via_split_mirror_apply_command()`'s payload-length guard
casts `4u + size` back to `uint8_t`, so it wraps and passes for `size ∈ [252,
255]`, letting the slave read ~220 bytes past its 32-byte RPC buffer and write
them to EEPROM.** Reachable from a malformed VIA frame because the classifier
deliberately ignores `payload_size` for that command (Review 18's finding 3
fix). Both buffer cases in the mirror are affected.

The other five substantive items are of the same shape — a guard, contract, or
initializer one type or one call site short of its claim — and are listed with
suggested fixes in the review.

## Verification

Both required gates were run against `bcd9b187` before the review was written,
and both pass. They establish the baseline the findings are measured against;
none of the findings is currently caught by either gate.

- `sh tests/host/run_all_host_tests.sh` — exit 0, 67 runners reported passed
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — exit 0, linked and UF2 emitted
- `git status --short` — clean before and after both gates
- `command -v rg` — ripgrep 15.2.0 present, so the guard scripts Review 18's
  finding 6 hardened are running against a real tool on this machine

Not run, and not required for a review-note-only change: `qmk compile` variants,
the memory and stack gates, and `python3 tools/profile_introspect.py --check`
(no authored input changed).

Not confirmed on hardware.

## Current Verdict

The architecture is sound and the recently landed work is well reasoned; the
findings concentrate in new code because it is new, not because it is weak. One
must-fix should land before the outstanding Review 18 Finding 05 hardware matrix
is run, because that matrix exercises the mirror path the defect lives on.

## Next Steps

1. Fix the mirror length guard in both cases
   (`users/noah/lib/compat/qmk_via_split_mirror.c:49`, `:64`) by comparing in a
   wider type, and add a `via_macro_defaults_test.c` case that drives
   `size = 255` through `via_command_kb()` and asserts the mirror rejects it.
2. Initialize `action_feedback_kind`
   (`users/noah/lib/key/runtime/planning/release_planner.c:388`) to match the
   sibling at `:344`.
3. Regate the published auto-mouse elapsed on the auto-mouse layer rather than
   on `is_auto_mouse_active()`, and extend `split_runtime_sync_test.c` to
   require zero published progress for an idle board with the layer off across a
   timer wrap.
4. Decide the `SPLIT_MIRROR` classification mismatch for `id_set_keyboard_value`
   and `id_eeprom_reset`, and add a gate tying
   `noah_qmk_via_command_effects()` to the mirror's switch.
5. Decide the base sync domain together with Review 18's write-only
   base-domain cleanup: add `split_runtime_sync_remote_read_base()` or drop
   `base_generation` and narrow the header contract.
6. Replace the combo all-keys fallback with the single owner keypos and cover it
   in `qmk_combo_origin_test.c`.
7. Add the `NOAH_DRAGSCROLL_CROSS_AXIS_DECAY_DIVISOR` static assert and saturate
   the three unguarded diagnostic counters in `reducer/runtime.c`.
8. Only after the above, run the two outstanding hardware matrices carried over
   from Review 18 (Finding 05 persistence/role-swap, Finding 10 arrow feel).

## 2026-08-25 — Finding 1 Remediation

### Completed Work

- Widened both payload-length comparisons in
  `users/noah/lib/compat/qmk_via_split_mirror.c` so `4 + size` cannot wrap at
  declared sizes 252–255.
- Added `tests/host/qmk_via_split_mirror_test.c`, which registers the production
  receiver and invokes the captured RPC callback rather than duplicating its
  switch or guard. It covers both dynamic-keymap and macro-buffer commands,
  accepts the exact 28-byte payload boundary, rejects the next size, and rejects
  every declared size from 252 through 255 without a sink call, storage-state
  change, or touched sink buffer.
- Added `tests/host/run_qmk_via_split_mirror_tests.sh`, with both a normal build
  and an AddressSanitizer/UndefinedBehaviorSanitizer build, and wired it into
  `tests/host/run_all_host_tests.sh`.
- Added the mirror transaction identifier to the host transaction stub so the
  production compatibility module is compiled unchanged by the test.

### Finding Status

| Finding | Status | Evidence |
| --- | --- | --- |
| 19-1 — split-mirror payload-length wrap | resolved | Widened production guards; actual registered receiver exercised for both commands and sizes 252–255 under ASan/UBSan; targeted, contract, compile-gate, full-host, and firmware-build checks pass. |
| 19-2, 19-3, and 19-5 through 19-7 and optional items | open | Not changed by this bounded remediation. |
| 19-4 — unsupported commands advertised as mirrored | resolved later | See the subsequent 2026-08-25 Finding 4 remediation record. |
| Review 18 Finding 05 hardware matrix | open | Its Finding 1 software prerequisite is resolved, but the hardware persistence/role-swap matrix was not run. |

### Verification

- `sh tests/host/run_qmk_via_split_mirror_tests.sh` — exit 0; normal and
  ASan/UBSan variants each reported `qmk via split mirror tests passed`
- `sh tests/host/run_qmk_via_split_sync_tests.sh` — exit 0; all three variants
  passed
- `sh tests/host/run_qmk_contract_checks.sh` — exit 0
- `sh tests/host/run_feature_gate_compile_tests.sh` — exit 0
- `sh tests/host/run_all_host_tests.sh` — exit 0, including the new normal and
  sanitized receiver tests
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — exit 0; linked and UF2
  emitted
- `git diff --check` — exit 0

Not confirmed on hardware. No sibling QMK source was changed.

### Current Verdict

Finding 1 meets the closure bar: current code matches the intended guard,
mechanical coverage exercises the actual receiver under sanitizers, the full
host suite passes, the firmware builds, and this review record matches the
tree. This does **not** close Review 19; Finding 4 was resolved in the later
record below, while the other findings and optional items retain their prior
status.

### Next Steps

Finding 4 was handled in the next bounded remediation below. The Review 18
Finding 05 hardware persistence/role-swap matrix can now be run without the
split-mirror guard defect as an outstanding software prerequisite.

## 2026-08-25 — Finding 4 Remediation

### Ownership Decision

Layout-options writes and EEPROM reset are durable-reconciliation commands, not
write-through commands. Layout options already live in the reconciled
`VIA_CONFIG` region, while EEPROM reset spans storage beyond the mirror's narrow
command transport. Both remain classified mutations with their local cache and
reseed effects, but neither carries `SPLIT_MIRROR`.

### Completed Work

- Removed `SPLIT_MIRROR` from the layout-options and EEPROM-reset classifications
  in `users/noah/lib/compat/qmk_via_contract.c`.
- Separated durable mutation notification from optional immediate mirroring in
  `users/noah/lib/macro/via_macro_defaults.c`. Every classified mutation now
  reaches `noah_qmk_via_split_sync_note_mutation()`; only supported commands
  enter `noah_qmk_via_split_mirror_command()`.
- Extended `tests/host/qmk_via_split_mirror_test.c` and its runner to compile the
  production classifier and production receiver together. The gate enumerates
  all 256 command IDs with encoder support enabled and requires every classified
  `SPLIT_MIRROR` command to be accepted by the registered receiver and restart
  its digest.
- Added classifier and VIA-hook assertions that layout options and EEPROM reset
  are not mirrored but still schedule durable reconciliation.
- Preserved Finding 1's exact-boundary and sizes-252–255 checks in both the
  normal and ASan/UBSan variants.

### Finding Status

| Finding | Status | Evidence |
| --- | --- | --- |
| 19-1 — split-mirror payload-length wrap | resolved | Existing widened guards and sanitizer-backed production receiver regression remain green. |
| 19-4 — unsupported commands advertised as mirrored | resolved | Unsupported commands are durable-only; exhaustive production classifier/receiver gate enforces the mirror capability claim. |
| 19-2, 19-3, and 19-5 through 19-7 and optional items | open | Not changed by this bounded remediation. |
| Review 18 hardware matrices | open | Not run in this pass. |

### Verification

- `sh tests/host/run_qmk_via_split_mirror_tests.sh` — exit 0; production
  classifier/receiver contract plus Finding 1 regression passed normally and
  under ASan/UBSan
- `sh tests/host/run_qmk_via_command_classifier_tests.sh` — exit 0
- `sh tests/host/run_via_macro_defaults_tests.sh` — exit 0
- `sh tests/host/run_qmk_via_split_sync_tests.sh` — exit 0; all three variants
  passed
- `sh tests/host/run_qmk_contract_checks.sh` — exit 0
- `sh tests/host/run_feature_gate_compile_tests.sh` — exit 0
- `sh tests/host/run_all_host_tests.sh` — exit 0
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` — exit 0; linked and UF2
  emitted
- `git diff --check` — exit 0

Not confirmed on hardware. No sibling QMK source was changed.

### Current Verdict

Finding 4 meets the closure bar: the ownership decision is explicit, the
production classifier/receiver relationship is mechanically enforced, the
durable-only path is covered through `via_command_kb()`, the full host suite
passes, and the firmware builds. Review 19 remains open for Findings 2, 3, and
5–7 and its optional items.

### Next Steps

Continue with Finding 5, then Findings 2 and 3 in the recorded sequence before
the remaining optional cleanup and hardware matrices.

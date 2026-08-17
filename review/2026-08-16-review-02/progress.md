# Review 19 Progress — Full Firmware Codebase Review

Thread: full-tree architecture and correctness review of `users/noah/` and the
authored profile at `bcd9b187`. Findings live in
`userspace-architecture-review.md` in this folder.

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

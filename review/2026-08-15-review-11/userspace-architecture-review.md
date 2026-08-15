# Dragscroll Stall-Recovery Architecture Review

Review 10 remains open only for Finding 05's physical two-half verification.
This review uses a new folder because dragscroll gesture lifecycle is a
materially different architecture topic and the Phase 4 roadmap orders Finding
11 before Finding 10.

## Findings

### Should-fix — Post-stall motion refreshes stale gesture state before expiry

`handle_dragscroll_mode()` accumulates the current report and assigns
`last_motion_time` before evaluating buffer age. Consequently, the first report
after a gap can inherit a stale axis lock and residual motion instead of
starting from the intended two-stage expiry policy.

- Code: `users/noah/lib/pointing/modes/pd_mode_dragscroll.c`
- Configuration: `users/noah/config.h`
- Missing enforcement: `tests/host/pd_mode_handlers_test.c`

Expiry must use the age of the prior gesture. A gap greater than the lock
timeout clears only the lock; a gap greater than the buffer timeout clears the
lock and both residual buffers. Current motion may be accumulated only after
that cleanup.

### Should-fix — One gesture decision uses multiple timer samples

The handler samples `timer_read32()` once, then independently calls
`timer_elapsed32()` for buffer expiry, rate limiting, and lock age. These reads
can straddle an exact timeout boundary and make one invocation internally
inconsistent. The host fixture also does not count timer calls, so the intended
single-sample contract is not mechanically enforced.

- Code: `users/noah/lib/pointing/modes/pd_mode_dragscroll.c`
- Test fixture: `tests/host/include/host_runtime_reset_fixture.h`
- Test surface: `tests/host/pd_mode_handlers_test.c`

Use one `uint32_t now` and unsigned subtraction for every age calculation.
Expose timer-call counters only in the host fixture and assert one read plus
zero elapsed-helper calls per invocation.

## Prior Finding Status

| Prior finding | Status | Evidence and preservation requirement |
| --- | --- | --- |
| Finding 05: VIA split persistence | partially resolved | Software and target gates pass in Review 10; physical two-half verification remains pending and is independent of this work. |
| Finding 17: split timer sampling | resolved | `split_runtime_sync_elapsed_at()` and split-sync tests enforce one sampled outbound timestamp; dragscroll should follow the same local consistency principle without coupling state. |
| Finding 11: dragscroll stall recovery | resolved | `pd_mode_dragscroll.c` expires prior state before accumulation; `pd_mode_handlers_test.c` and `pd_mode_test.c` enforce boundaries, wrap, timer budget, reset, and shared pinch wiring. Targeted runners, full host, firmware, and fresh target stack gates pass. |

## Reconciliation Note

The findings above are the audit-time baseline. They are resolved in the
current tree by the landed structure and enforcement described below; they are
not open defects after this review's closure.

## Areas That Are Solid

- Dragscroll state is private to one translation unit and reset through one
  complete `reset_dragscroll_mode()` assignment.
- DRAGSCROLL and PINCH intentionally reuse the same handler and reset callback
  through `pd_mode_manifest.h`, so one lifecycle correction covers both modes.
- Lock and buffer deadlines are already separate configuration values; no
  threshold, direction, divisor, ratio, or rate-limit retuning is required.
- Axis ratio arithmetic already widens its products to `int64_t`.

## Intended Structure

The handler remains one synchronous, bounded gesture adapter:

1. sample `now` exactly once;
2. derive prior motion age with wrap-safe unsigned subtraction;
3. expire stale buffers/lock before reading current motion into state;
4. accumulate current motion and refresh `last_motion_time` only when nonzero;
5. freeze pointer X/Y as today;
6. apply rate limiting and lock refresh from the same sample;
7. emit at most the existing single scroll report and record `last_scroll_time`.

Finding 10's broader saturation and discrete-tap backlog policy must not be
folded into this pass. That work follows after this lifecycle seam is stable.

## Current Architecture Assessment

Finding 11 is resolved. The module boundary remains local, the two-stage expiry
is explicit, and all decisions use one wrap-safe sample without introducing a
new service or coupling to Finding 10's backlog policy. DRAGSCROLL and PINCH
remain manifest-driven users of the same handler and reset callback.

## Recommended Next Refactor Sequence

1. Preserve Review 11 as immutable closure history.
2. Open the next sortable review for Finding 10 backlog bounds.
3. Keep Finding 11's 360 B target path and boundary tests as regression gates.

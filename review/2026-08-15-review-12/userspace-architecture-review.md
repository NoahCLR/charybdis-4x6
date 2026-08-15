# Pointing Tap-Backlog Architecture Review

Review 10 remains open only for Finding 05's physical two-half verification,
and Review 11 is immutable closure history. This review uses the next sortable
folder because bounded discrete pointing output is a distinct Phase 4 runtime
topic.

## Findings

### P1 — Extended motion reports can monopolize the firmware loop

`pd_mode_axis_emit()` drains every whole threshold synchronously. With signed
16-bit reports, one handler call can dispatch hundreds of synthetic taps before
the main loop can return to matrix scanning, split work, or RGB service.

- Shared code: `users/noah/lib/pointing/modes/pd_mode_handler_common.h`
- Callers: arrow, volume, brightness, and zoom mode handlers
- Missing enforcement: `tests/host/pd_mode_handlers_test.c`

One handler invocation needs a hard tap budget. Excess same-direction intent
may remain only as bounded whole-tap debt plus the original sub-threshold
residual. Any whole taps above that bound must be discarded deterministically
and exposed through diagnostics.

### P1 — Arrow dominance cannot represent `INT16_MIN` magnitude

`arrow_update_dominant_axis()` negates signed 16-bit values into signed 16-bit
locals. The magnitude of `-32768` is `32768`, which is not representable there,
so extreme negative motion can select the wrong axis.

- Code: `users/noah/lib/pointing/modes/pd_mode_arrow.c`
- Missing enforcement: `tests/host/pd_mode_handlers_test.c`

The comparison must widen to `int32_t` before negation while preserving the
existing strict-greater-than and tie behavior.

## Prior Finding Status

| Prior finding | Status | Evidence and preservation requirement |
| --- | --- | --- |
| Finding 05: VIA split persistence | partially resolved | Software and target gates pass in Review 10; the physical disconnect, power-cycle, reconnect, and role-swap matrix remains pending and is independent of this work. |
| Finding 08: synthetic key ownership | resolved | Discrete pointing output continues through the ownership-safe synthetic dispatch contract; capped or dropped debt must not bypass it. |
| Finding 11: dragscroll stall recovery | resolved | Review 11 closed the shared DRAGSCROLL/PINCH lifecycle. Finding 10 must not alter its thresholds, expiry, or report policy. |
| Finding 10: pointing backlog bounds | partially resolved | The 4/32 bounded implementation, diagnostics, extreme-value fix, tests, docs, full host suite, target build, and explicit 1,240/1,280 B stack paths pass. Flashed elapsed-time/feel measurement remains. |

## Reconciliation Note

The findings above are the audit-time baseline. The current tree resolves the
unbounded loop, overflow, reset, scheduling, observability, and configuration
enforcement defects. Finding 10 remains partially resolved only because the
planned on-device worst-case duration and feel measurement has not been run.

## Areas That Are Solid

- Arrow, volume, brightness, and zoom already share one accumulator/emitter
  helper, so one bounded policy can cover every discrete mode.
- Direction reversal already clears old debt before accepting new motion.
- Each mode has a complete reset callback, and the registry invokes it on mode
  transition.
- The pinned QMK `pointing_device_task()` calls the userspace handler after each
  successful sensor poll, including zero-motion reports. Backlog can therefore
  drain without a second scan scheduler.
- Dispatch already uses the settled, ownership-safe synthetic tap path.

## Intended Structure

Use one compile-time policy for all discrete pointing modes:

1. allow at most `NOAH_PD_MODE_MAX_TAPS_PER_TICK` taps in one handler call;
2. retain at most `NOAH_PD_MODE_MAX_BACKLOG_TAPS` whole taps per active axis;
3. retain the exact sub-threshold residual when whole-tap debt saturates;
4. discard only whole taps beyond the cap and count them with saturating
   diagnostics;
5. drain retained debt on later zero-motion calls;
6. clear debt, direction, and per-activation diagnostics on reset;
7. calculate arrow magnitudes in `int32_t`.

The initial policy is four taps per handler call and 32 retained whole taps.
That converts the former report-magnitude-dependent loop into a four-iteration
maximum while bounding the output tail to eight zero-motion polls. No pointing
threshold, DPI, keycode, dominant-axis tie rule, or dragscroll behavior changes.

## Verification Strategy

- Start with regressions for maximum positive/negative reports and
  `INT16_MIN` dominance; preserve their initial failure as red evidence.
- Cover exact thresholds, zero-motion draining, saturation accounting,
  sustained maximum input, reversal, reset, and ownership-balanced dispatch.
- Compile-fail zero/overflowing thresholds and invalid budget/cap
  configurations.
- Run all targeted pointing and ownership tests, feature gates, the complete
  host suite, an ordinary target build, and the fresh reviewed-path stack gate.

## Current Architecture Assessment

Finding 10 is **partially resolved**. The current tree has a compile-time work
budget, bounded exact-residual backlog, saturating overload diagnostics,
zero-report draining, reset/reversal invalidation, and correct extreme-value
axis comparison. The software and linked target evidence is complete; only the
flashed elapsed-time/feel check remains before this review can close.

## Recommended Next Refactor Sequence

1. Flash the current 4/32 build and measure worst-case handler duration.
2. Exercise sustained arrow, zoom, volume, and brightness motion for backlog
   feel and confirm an eight-poll tail is acceptable.
3. Close this review if the policy is accepted, or tune only the documented
   limits and rerun all Finding 10 gates.

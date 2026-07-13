# Finding 10: Bound Pointing-Mode Tap Backlogs and Handle INT16_MIN

## Plan metadata

- **Severity:** Should-fix (P1 main-loop starvation and extreme-input correctness risk)
- **Status:** Planned; axis conversion still emits unbounded loops
- **Affected surfaces:** arrow, volume, brightness, and zoom pointing modes; synthetic action dispatch; extended mouse reports; mode reset/transition
- **Primary files:** [pd_mode_handler_common.h](../users/noah/lib/pointing/modes/pd_mode_handler_common.h), [pd_mode_arrow.c](../users/noah/lib/pointing/modes/pd_mode_arrow.c), [pd_mode_volume.c](../users/noah/lib/pointing/modes/pd_mode_volume.c), [pd_mode_brightness.c](../users/noah/lib/pointing/modes/pd_mode_brightness.c), [pd_mode_zoom.c](../users/noah/lib/pointing/modes/pd_mode_zoom.c)
- **Prerequisites:** Reuse the ownership-safe synthetic tap contract from [Finding 08](08-synthetic-key-ownership.md) if it has landed; do not block on [Finding 07](07-nonblocking-macro-playback.md)'s macro-specific scheduler
- **Recommended phase:** Phase 4, preferably after the smaller dragscroll expiry correction in [Finding 11](11-dragscroll-stall-recovery.md)

## Problem statement

pd_mode_axis_emit converts accumulated motion to key taps with while loops. The amount of work in one pointing report is proportional to report magnitude. This firmware enables signed 16-bit extended reports, so one maximum report can cause roughly 820 arrow taps, 547 volume/brightness taps, or 410 zoom taps before control returns to the keyboard loop.

Each tap traverses the synthetic action lifecycle and may include QMK tap settling. A malformed, accumulated, or extreme sensor report can therefore monopolize the main loop and amplify into watchdog, input-latency, split, and RGB failures.

The arrow dominant-axis calculation also negates an int16_t directly. Negating INT16_MIN cannot be represented in int16_t; the value remains negative on the target representation, so axis comparison can select or drop the wrong direction.

## Current evidence and failure scenario

- users/noah/config.h:105-107 enables MOUSE_EXTENDED_REPORT and WHEEL_EXTENDED_REPORT.
- users/noah/lib/pointing/modes/pd_mode_handler_common.h:30-47 accumulates a signed delta and drains it with two unbounded while loops.
- users/noah/lib/pointing/modes/pd_mode_handler_common.h:50-52 applies the same helper to vertical modes.
- users/noah/lib/pointing/modes/pd_mode_arrow.c:16-21 uses thresholds 40/50.
- users/noah/lib/pointing/modes/pd_mode_volume.c:11-18 and pd_mode_brightness.c:11-18 use threshold 60.
- users/noah/lib/pointing/modes/pd_mode_zoom.c:11-18 uses threshold 80.
- users/noah/lib/pointing/modes/pd_mode_arrow.c:49-57 stores absolute values in int16_t after direct negation.

Failure:

1. Feed an active arrow mode a report with x = -32768.
2. Dominant-axis absolute conversion overflows int16_t and may fail to choose X.
3. If the axis is selected, the common emitter can synchronously dispatch hundreds of taps.
4. Matrix scanning and all other main-loop work wait for that burst to finish.
5. Repeated large reports can also grow the int32_t accumulator until it overflows unless backlog is bounded.

## Required invariants

1. One pointing handler invocation emits no more than a configured global tap budget.
2. Excess work remains as a bounded backlog or is dropped according to an explicit saturation policy.
3. Accumulator arithmetic cannot overflow int32_t for any sequence of legal reports.
4. Reversing direction clears the prior-direction backlog before accumulating the new direction, preserving current behavior.
5. Backlog can drain on later zero-delta ticks; it does not require new motion.
6. Mode reset/switch clears accumulator, direction, and backlog.
7. Every threshold is compile-time proven nonzero and safe for backlog-cap multiplication.
8. Dominant-axis magnitude is calculated in int32_t and handles -32768 exactly.
9. The frozen mouse report behavior remains unchanged.
10. Synthetic tap ownership remains balanced even when work is capped or dropped.

## Chosen backlog policy

Use two explicit bounds:

- NOAH_PD_MODE_MAX_TAPS_PER_TICK: maximum taps emitted by the active mode during one handler invocation;
- NOAH_PD_MODE_MAX_BACKLOG_TAPS: maximum whole-tap debt retained per axis.

Recommended initial values are 4 taps per tick and 32 taps of backlog, subject to target timing measurement. Preserve the sub-threshold residual in addition to the capped whole-tap debt.

When same-direction input exceeds the backlog cap, saturate at the cap and count the discarded motion. This drops the oldest indistinguishable excess while preserving direction and bounded latency. On direction reversal, clear old debt/residual before accepting new input, matching today's reset-on-reversal behavior.

Only one mode is active, so the per-tick budget is shared by the active handler. Arrow uses only its selected dominant axis.

## Scope and non-goals

In scope:

- bounded accumulation and emission in the common helper;
- zero-delta backlog draining;
- saturation diagnostics;
- compile-time threshold guards;
- int32_t dominant-axis magnitude;
- extreme report and recovery tests.

Not in scope:

- changing mode keycodes, thresholds, DPI, or authored mappings;
- converting pointing taps to a separate asynchronous macro engine;
- guaranteeing lossless output for arbitrarily large or sustained input;
- changing dragscroll gesture policy, covered by [Finding 11](11-dragscroll-stall-recovery.md).

## Detailed implementation plan

### Step 1: Make accumulation saturating

1. Promote delta to int32_t before arithmetic.
2. Compute the maximum representable accumulator from threshold, backlog-tap cap, and residual using checked/wider arithmetic.
3. Saturate additions to the positive/negative bound instead of allowing signed overflow.
4. Return or record whether saturation occurred and how many whole taps were discarded.
5. Preserve direction reversal reset before the saturating add.

Add static assertions per mode:

- threshold > 0;
- backlog cap > 0;
- threshold times backlog cap plus residual fits int32_t;
- tap budget > 0 and no larger than the backlog cap unless intentionally documented.

### Step 2: Replace unbounded while loops with a budget

Change the helper to consume a budget pointer or return emissions used. For each invocation:

1. add/saturate current delta;
2. while budget remains and one threshold is available, dispatch one tap and reduce the accumulator;
3. stop immediately when the budget reaches zero;
4. leave remaining debt in the accumulator for a later call.

The loop is now bounded by a small compile-time constant. Keep positive and negative paths mutually exclusive after direction normalization.

### Step 3: Define zero-motion service and mode ordering

Verify the pointing task calls the active handler on zero reports. If the pinned QMK path can skip handlers when the report is unchanged, expose a small pending-work query and drain from the existing pointing/runtime scan hook.

Do not drain from two places in the same loop. Add a test that an initial maximum report followed by zero reports drains at no more than the per-tick budget until empty.

On mode reset or transition, clear all debt so output from an old mode cannot appear in the new mode.

### Step 4: Fix dominant-axis magnitude

In arrow_update_dominant_axis:

1. cast dx and dy to int32_t;
2. negate only the widened negative value;
3. compare int32_t magnitudes;
4. preserve the current tie behavior and last-axis selection.

Cover x/y values of INT16_MIN, INT16_MAX, zero, equal magnitudes, and one-unit differences.

### Step 5: Measure and tune the budget

Instrument the host dispatch count and, on target, measure a worst-case handler tick for 1, 2, 4, and 8 taps. Select the lowest value that keeps intended scrolling/key-repeat usable while bounding scan latency. Record the chosen value and rationale in the review note/config comments.

## Test plan

Add cases to pd_mode_handlers_test.c for:

- max positive and negative reports in each mode;
- no invocation exceeding the configured budget;
- exact zero-delta draining across subsequent ticks;
- saturation at the backlog cap and a deterministic dropped-motion count;
- sustained input never overflowing or growing beyond the cap;
- reversal discarding old-direction debt before emitting the new direction;
- reset/mode switch clearing debt;
- threshold boundary values;
- arrow dx/dy = INT16_MIN in every dominance combination;
- ownership balance after capped and saturated output.

Run:

~~~sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_pd_mode_tests.sh
sh tests/host/run_pd_runtime_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_pointer_layer_policy_tests.sh
sh tests/host/run_owned_keycode_tests.sh
sh tests/host/run_feature_gate_compile_tests.sh
~~~

Closure requires:

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

Expose per-mode or aggregate counters for saturated reports, discarded whole taps, maximum backlog, and maximum taps emitted in one tick. Keep them under existing diagnostics and avoid console output per report.

Record target worst-case handler duration and confirm the configured budget keeps the loop within the runtime/watchdog expectation.

## Risks, tradeoffs, and fallback

- **Perceived lag:** retained backlog can continue after motion stops. The 32-tap cap bounds tail latency; tune with measurement.
- **Dropped intent:** saturation is lossy by design. It is safer than unbounded latency and must be observable.
- **Zero-report scheduling assumption:** prove the pinned QMK behavior or add one explicit drain seam.
- **Budget scope:** separate budgets per axis could double the bound. Use one active-mode budget.
- **Threshold misconfiguration:** static assertions must turn zero/overflowing values into compile failures.
- **Fallback:** start with a conservative budget of one if target timings are unavailable; raise only after measurement.

## Documentation and review-note updates

Document the bounded/coalescing behavior and configured limits in users/noah/config.h comments and any user-facing pointing-mode guide. Update the active runtime review with measured tick duration, saturation policy, and tests; open a new sortable review if the relevant review is closed.

## Acceptance checklist

- [ ] One tick cannot emit more than the configured tap budget.
- [ ] Backlog and arithmetic are bounded under sustained max reports.
- [ ] Zero reports drain pending work deterministically.
- [ ] Reversal and reset discard obsolete debt.
- [ ] INT16_MIN dominant-axis cases are correct.
- [ ] Threshold and multiplication guards compile-fail invalid configs.
- [ ] Target timing and backlog policy are recorded.
- [ ] Targeted tests, full host suite, and firmware compile pass.

## Next action

Add the INT16_MIN regression and max-report dispatch-count tests first. Then change the common helper to accept a budget and saturating bound, initially using one tap per tick until target measurements justify the final configured value.

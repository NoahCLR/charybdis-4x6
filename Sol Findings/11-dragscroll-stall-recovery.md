# Finding 11: Expire Dragscroll State Before Accumulating Post-Stall Motion

## Plan metadata

- **Severity:** Should-fix (P1 gesture-state correctness; low implementation risk)
- **Status:** Planned; new motion still refreshes timestamps before expiry checks
- **Affected surfaces:** dragscroll and pinch modes, axis lock, residual motion buffers, timer sampling, rate limiting
- **Primary files:** [pd_mode_dragscroll.c](../users/noah/lib/pointing/modes/pd_mode_dragscroll.c), [pd_mode_handlers_test.c](../tests/host/pd_mode_handlers_test.c), [config.h](../users/noah/config.h)
- **Prerequisites:** None; coordinate timer-helper style with [Finding 17](17-split-timer-sampling.md) if it lands first
- **Recommended phase:** Phase 4, before the broader pointing backlog work

## Problem statement

handle_dragscroll_mode adds a new report to the existing buffers and overwrites last_motion_time before it checks whether the previous gesture has expired. The first nonzero report after a long stall therefore makes the old gesture appear fresh.

With current configuration, a gap greater than the 55 ms lock timeout should release the prior axis, and a gap greater than the 80 ms buffer timeout should discard residual motion. Instead, the incoming report can inherit the old locked axis and residual buffer. This causes an unexpected scroll direction/step at the start of a new gesture.

The function also calls timer_read32 once and timer_elapsed32 multiple times. The repeated reads can disagree around boundaries and add avoidable timer work. All decisions in one handler invocation should use one sampled now value.

## Current evidence and failure scenario

- users/noah/lib/pointing/modes/pd_mode_dragscroll.c:64-70 stores buffers, last motion/scroll timestamps, and locked axis together.
- users/noah/lib/pointing/modes/pd_mode_dragscroll.c:153-181 considers a gesture active while motion age is at most NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS.
- users/noah/lib/pointing/modes/pd_mode_dragscroll.c:234-252 samples now, accumulates incoming motion, and immediately sets last_motion_time = now.
- users/noah/lib/pointing/modes/pd_mode_dragscroll.c:257-263 checks buffer expiry only after that timestamp refresh.
- users/noah/lib/pointing/modes/pd_mode_dragscroll.c:266-270 performs two more timer_elapsed32 reads for rate and motion age.
- users/noah/config.h:146-152 configures an 8 ms rate limit, 80 ms buffer expiry, and 55 ms lock timeout.

Failure:

1. Establish a horizontal dragscroll lock with nonzero horizontal residual.
2. Let 81 ms pass without a handler report.
3. Submit the first report of a new vertical gesture.
4. The function adds it to stale buffers and sets last_motion_time to now.
5. The buffer-expiry check sees age zero, and lock refresh sees the gesture as active.
6. Output can use the previous horizontal lock/residual instead of starting cleanly from the new vertical motion.

The same stale-lock issue occurs after 56-80 ms even when the buffer has not yet reached its separate expiry deadline.

## Required invariants

1. Prior lock age is evaluated before current motion can refresh last_motion_time.
2. A prior gap greater than the lock timeout clears locked_axis before the new report is classified.
3. A prior gap greater than the buffer expiry clears both buffers before new deltas are accumulated.
4. At exactly each timeout, preserve the existing inclusive behavior; expire at timeout + 1.
5. Current motion is accumulated only after prior-state expiry.
6. All timing decisions in one call use one timer_read32 sample and unsigned wrap-safe subtraction.
7. A fresh report after expiry can establish a new axis in the same call if it meets the normal start rules.
8. No-motion calls still expire stale lock/buffers and respect the rate limit.
9. reset_dragscroll_mode remains a complete state reset.
10. Pinch, which reuses the dragscroll handler, receives the same corrected lifecycle.

## Scope and non-goals

In scope:

- reorder expiry before accumulation;
- sample time once;
- separate prior motion age from current motion age;
- add boundary, first-report-after-gap, and wrap tests.

Not in scope:

- retuning thresholds, ratios, divisors, timeouts, or rate limit;
- changing horizontal/vertical direction or hi-res scroll;
- redesigning dragscroll buffering;
- folding this into the generic pointing backlog helper;
- changing auto-mouse ownership.

## Detailed implementation plan

### Step 1: Add explicit prior-state expiry

At function entry:

1. sample uint32_t now = timer_read32();
2. calculate prior_motion_age = now - last_motion_time with unsigned arithmetic;
3. determine whether prior gesture state exists from nonzero buffers or a non-NONE lock;
4. if prior_motion_age > NOAH_DRAGSCROLL_BUFFER_EXPIRE_MS, clear both buffers and locked axis;
5. otherwise, if prior_motion_age > NOAH_DRAGSCROLL_LOCK_TIMEOUT_MS, clear only locked axis.

Keep the two deadlines distinct. A 56-80 ms gap releases axis lock but may retain residual according to the existing configured buffer policy. An 81+ ms gap begins with no residual.

### Step 2: Accumulate the current report after expiry

After prior-state cleanup:

1. apply reverse-X/reverse-Y policy to the current report;
2. add deltas to the now-valid buffers;
3. if there is motion, set last_motion_time = now and use current motion age zero;
4. if there is no motion, retain prior_motion_age for lock refresh;
5. freeze x/y in the outgoing report as today.

Use int32_t additions already provided by the state. If Finding 10 introduces shared saturation helpers, do not silently apply its tap-backlog policy to dragscroll without a separate decision.

### Step 3: Use the same timestamp for rate and lock decisions

Replace timer_elapsed32 calls with:

- scroll_age = now - last_scroll_time;
- motion_age = had_motion ? 0 : prior_motion_age.

Evaluate rate limiting against scroll_age. Pass motion_age to dragscroll_refresh_axis_lock. When a scroll step emits, store last_scroll_time = now.

This makes one call internally consistent and removes the repeated timer reads.

### Step 4: Preserve reset and mode-reuse behavior

Confirm reset_dragscroll_mode clears timestamps, lock, and buffers. Exercise both DRAGSCROLL and PINCH manifest paths because both call handle_dragscroll_mode. Do not add mode-specific stale state outside dragscroll_state.

## Test plan

Extend pd_mode_handlers_test.c with:

- horizontal lock, 56 ms gap, first vertical report: old lock is cleared before classification;
- horizontal residual, 81 ms gap, first vertical report: old residual is cleared before accumulation;
- first fresh report after expiry can immediately establish/emit on its new axis;
- exactly 55 ms vs 56 ms lock boundary;
- exactly 80 ms vs 81 ms buffer boundary;
- no-motion call expires lock and buffers;
- rate-limit boundary using the same sampled now;
- timer wrap from near UINT32_MAX to a small value;
- one timer_read32 call and no timer_elapsed32 calls per handler invocation;
- reset and pinch reuse.

Run:

~~~sh
sh tests/host/run_pd_mode_handlers_tests.sh
sh tests/host/run_pd_mode_tests.sh
sh tests/host/run_pd_runtime_tests.sh
sh tests/host/run_pd_mode_key_runtime_integration_tests.sh
sh tests/host/run_pointer_layer_policy_tests.sh
~~~

If timer/config guards or headers change, run:

~~~sh
sh tests/host/run_feature_gate_compile_tests.sh
~~~

Closure requires:

~~~sh
sh tests/host/run_all_host_tests.sh
qmk compile -kb bastardkb/charybdis/4x6 -km noah
~~~

## Observability and measurements

The regression is deterministic enough for host tests. Optional diagnostics may count lock-expiry and buffer-expiry events, but no per-report logging is needed. Add a timer-call-count assertion to the host harness and record the reduction from one read plus multiple elapsed calls to one read per invocation.

## Risks, tradeoffs, and fallback

- **Boundary change:** preserve the existing greater-than comparisons; tests must lock exact-time behavior.
- **Residual policy between 55 and 80 ms:** clearing only lock is intentional under current configuration. If UX testing wants a fully fresh gesture at 56 ms, treat that as a separate tuning decision.
- **Zero timestamp after reset:** only expire when prior state exists, avoiding meaningless age decisions for an empty state.
- **Timer wrap:** direct unsigned subtraction is safe; signed deadline comparisons are not needed here.
- **Fallback:** if a shared timer helper from Finding 17 is already available, use it, but keep expiry-before-accumulation explicit in this function.

## Documentation and review-note updates

Update code comments to explain the two-stage expiry and single-sample timing. User-facing docs need changes only if the gesture behavior is documented in detail. Record regression tests and timer-call measurement in the active runtime review; if it is closed, use the next sortable review folder.

## Acceptance checklist

- [ ] Old lock expires before the first post-timeout report is classified.
- [ ] Old residual expires before the first post-buffer-timeout report is accumulated.
- [ ] Exact timeout boundaries preserve current semantics.
- [ ] A fresh report can establish a new axis immediately.
- [ ] One timer sample drives all decisions in a handler call.
- [ ] Wrap, no-motion, reset, and pinch reuse are tested.
- [ ] Targeted tests, full host suite, and firmware compile pass.
- [ ] Review notes describe the landed two-stage expiry.

## Next action

Add the 56 ms stale-lock and 81 ms stale-buffer regression tests first. Then reorder handle_dragscroll_mode around a single now sample without changing ratios, thresholds, or timeout values.

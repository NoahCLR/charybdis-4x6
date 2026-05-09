# Runtime Loop Performance Review

This review was opened because the previous review folder,
`review/2026-05-05-review-01/`, is closed and must remain an immutable
snapshot. This thread covers a different architecture topic: reducing runtime
loop overhead after observed mouse/report-rate regressions.

## Review Scope

The requested behavior change is intentionally narrow:

- Keep the RP2040 watchdog restart path, but remove stage-specific reboot
  diagnostics.
- Keep only a simple `HSV(0,0,150)` white boot indicator after every
  runtime init.
- Remove per-scan watchdog scratch writes from diagnostic scopes.
- Reduce split runtime sync key-feedback work on idle scans without changing
  the packets, heartbeats, or remote render behavior visible to the user.
- Keep combo RGB sync immediate, including idle-to-active combo feedback.
- Reduce idle key-runtime scan work and normal pointer-report overhead without
  changing authored key behavior or pointing mode semantics.
- Reduce normal pointer-report overhead in the idle-noise filter and QMK
  pointing throttle path.
- Reduce fixed master main-loop overhead from local matrix scan timing and
  watchdog refresh cadence.
- Remove inert diagnostic scope calls from continuous scan, pointer, RGB, and
  housekeeping paths while preserving watchdog heartbeat behavior.
- Reduce idle housekeeping and split-sync helper work that does not affect
  rendered state.
- Keep held-repeat rates anchored to the requested cadence even when the scan
  loop has small timing drift.

Out of scope:

- Changing authored keymap behavior.
- Changing RGB layer, combo, key-feedback, automouse, or pointing mode visual
  semantics after the boot indicator expires.
- Changing split RPC packet formats.
- Reworking the broader key runtime scan architecture beyond idle gating.

## Current Design

Implementation status: landed and verified across the 2026-05-08 watchdog /
split-sync pass and the 2026-05-09 idle-scan / pointer hot-path pass with the
targeted host runners, full host suite, and noah firmware compile listed in
`progress.md`.

### Runtime Diagnostics

`users/noah/lib/state/diagnostics/runtime_diag.c` is now a minimal restart
watchdog and boot-indicator surface. `noah_runtime_diag_post_init()` enables
the RP2040 hardware watchdog and starts the indicator every time the runtime
initializes. `noah_runtime_diag_heartbeat()` still runs from housekeeping, but
it refreshes the hardware watchdog on a loop-count cadence instead of writing
the watchdog register on every pass. The first heartbeat after init refreshes
the watchdog immediately, then subsequent updates run every
`NOAH_RUNTIME_DIAG_WATCHDOG_HEARTBEAT_DIVISOR` housekeeping passes. RGB renders
the boot indicator by converting `HSV(0,0,150)` with the same `hsv_to_rgb()`
path used by authored RGB config, then applying that full-board override for
`NOAH_RUNTIME_DIAG_INDICATOR_MS`.

The public diagnostic scope and watchdog query functions remain present for
existing call sites and host tests, but they no longer write RP2040 watchdog
scratch registers or report a latched reboot stage. Scope enter/leave are
inert compatibility hooks. Continuous runtime paths no longer call those inert
scope hooks; watchdog refresh still runs from `noah_runtime_diag_heartbeat()`
in housekeeping.

### Matrix Scan Timing

`users/noah/config.h` now owns a Charybdis-specific `MATRIX_IO_DELAY` of 10 us.
The upstream QMK default is 30 us after every selected column; on this ROW2COL
4x6 half that means six fixed waits per local scan. The override keeps an
explicit settle delay for diode/matrix stability while trimming roughly 120 us
of fixed delay from every main-loop pass on the master half.

### Split Runtime Sync

`users/noah/lib/split/runtime_sync.c` still owns the split RPC packet formats
and send cadence. Base runtime state is cheap and continues to be built every
sync tick. Combo feedback packets are also built every sync tick so combo RGB
feedback can move from idle to active immediately; the existing packet memcmp
still prevents unchanged combo packets from being sent before heartbeat.
Split sync now asks `combo_feedback_bitmaps()` for underlay and overlay maps in
one call, so the combo-origin cache is walked once per combo packet instead of
once per layer. `runtime_sync.c` keeps a weak fallback that calls the older
underlay/overlay hooks so isolated host runners and minimal link surfaces do
not need to link the full key feedback implementation.

PD owner bitmaps are only snapshotted when local owner sides are non-empty.
Owner-side snapshots read direct local/remote state instead of building a full
PD mode snapshot.

The heavier key-feedback semantic and branch packets are built only when one
of these conditions is true:

- The sync is forced.
- The relevant surface is dirty.
- The last sent packet was active, so transition-to-idle remains visible.
- The idle heartbeat is due, so a rebooted/rejoined half can recover.

Dirty state lives in `users/noah/lib/split/runtime_sync_dirty.c`. This keeps
dirty notifications linkable in isolated key-runtime host tests that do not
link the full split RPC implementation.

Dirty notifications are emitted from the surfaces that can alter remote RGB
truth:

- Combo physical-origin observation marks combo sync dirty.
- Key-runtime transition plans mark key-feedback sync dirty when they contain
  projected effects.
- Active press or pending multi-tap scan state marks key-feedback sync dirty
  while feedback can still change with time.
- Feedback pulse arming/projection marks key-feedback sync dirty.

### Key Runtime Scan

`users/noah/lib/key/runtime/scan.c` now treats active press tokens and pending
multi-tap series as the gate for timer-driven core scan work. When both counts
are zero, idle scans skip transition-plan allocation, timer-driven core
refresh, scan tracing, and empty plan execution. Deferred release dispatches
remain independently gated by the pending-release queue count, so queued
release actions still drain even when there is no active press or multi-tap
state.

`users/noah/lib/key/runtime/queue/pending_release_queue.c` also avoids the
deferred-release blocker scan when the pending-release queue is empty, and only
checks blocker state when there are active press tokens that could actually
block a drain.

### Pointing Runtime

`users/noah/lib/pointing/runtime/pd_runtime.c` no longer snapshots the full PD
mode display state or performs a registry lookup for normal mouse reports when
no local PD mode is active. The pointing task reads only the local active mode,
uses the existing idle-noise policy, and looks up a mode handler only when the
local active mode is nonzero. Idle-noise absolute-motion calculation now avoids
building a temporary delta array on every report.

Normal movement now exits the idle-noise filter before querying long-idle
timers. The filter only checks `last_input_activity_elapsed()` and
`last_matrix_activity_elapsed()` for tiny motion that could actually be
suppressed. `POINTING_DEVICE_TASK_THROTTLE_MS` is set to `0` so QMK skips its
1 ms throttle timer gate; the firmware loop remains the practical polling
limit.

### Held Repeat

`users/noah/lib/key/ownership/held_repeat.c` tracks an active binding count.
Idle housekeeping returns before reading the timer or scanning the board-sized
repeat table. Active repeat ticks read the timer once per housekeeping pass and
use unsigned elapsed arithmetic for each binding.

Repeat scheduling is phase-preserving: when a binding is due, the stored fire
time advances by the configured interval instead of snapping to `now`. This
restores the old 100 Hz click-spam behavior where small scan-loop drift does
not lower the average repeat rate. A single tick may emit up to four catch-up
taps; if the firmware was stalled beyond that cap, the scheduler emits the
bounded catch-up batch and re-anchors to `now` instead of replaying an
unbounded backlog.

## Contracts

- Runtime diagnostics owns only minimal watchdog restart behavior: enable once
  at post-init and update from housekeeping on the configured loop-count
  cadence.
- The first watchdog heartbeat after runtime init must refresh the watchdog;
  later idle heartbeats must skip most per-loop watchdog writes.
- The supported visible behavior is an `HSV(0,0,150)` white boot indicator
  after every runtime initialization.
- Diagnostic watchdog query APIs are compatibility APIs and must report idle /
  false / zero unless a future implementation deliberately reintroduces real
  reboot diagnostics.
- Split sync packet formats remain unchanged.
- Split sync still sends forced packets and heartbeat packets even when state is
  clean.
- Combo feedback maps are rebuilt every sync tick so idle-to-active combo RGB
  feedback is immediate; unchanged combo packets must still avoid transport
  sends before heartbeat.
- Combo feedback underlay and overlay maps should be generated together for
  split sync.
- The combined combo feedback API must retain a weak fallback for isolated
  split-sync link surfaces.
- PD owner bitmaps should not be snapshotted on idle ticks with no owner side.
- Idle clean scans must not rebuild key-feedback semantic/branch maps before
  the heartbeat window.
- Continuous scan, pointing, RGB, post-init, and housekeeping paths must not
  call inert diagnostic scope hooks.
- Housekeeping must still refresh the runtime watchdog often enough for hard
  freezes to reset the RP2040 under the 750 ms watchdog timeout.
- `MATRIX_IO_DELAY` is a userspace-owned Charybdis hardware tuning knob and is
  expected to remain visible in the generated keymap overview.
- Dirty notifications must be available to key-runtime code even when a host
  test defines `SPLIT_TRANSACTION_IDS_USER` but does not link
  `runtime_sync.c`.
- Idle key-runtime scans must not advance timer-driven core state when there
  are no active press tokens, no pending multi-tap series, and no pending
  release dispatches.
- Pending release dispatches must drain even when key-runtime core scan work is
  skipped.
- Pointer reports without a local active PD mode must pass through unchanged
  and must ignore remote display-only PD mode state on the slave half.
- Normal pointer motion above the idle-noise threshold must not query long-idle
  timers.
- The pointing-device task should run whenever the main loop reaches it; do
  not add a throttle gate unless hardware measurements show the loop has
  exceeded the sensor/report budget.
- Idle held-repeat housekeeping must avoid timer reads and repeat-table scans.
- Active held-repeat scheduling must preserve the requested cadence across
  normal scan-loop drift, including 100 Hz click-spam repeats.
- Held-repeat catch-up must be bounded so long stalls do not replay an
  unbounded backlog in one housekeeping tick.

## Verification Coverage

Expected coverage for this thread:

- `run_runtime_diag_tests.sh` covers watchdog enable/update without reboot-stage
  diagnostics, skipped per-loop watchdog writes, and boot-indicator expiry.
- `run_rgb_layer_render_tests.sh` covers the full-white boot indicator render
  override.
- `run_split_runtime_sync_tests.sh` covers immediate combo feedback, idle
  key-feedback dirty gating, skipped idle PD owner bitmap snapshots, and
  heartbeat recovery behavior.
- Key-runtime integration runners cover dirty notifications from scan,
  transition, and feedback paths.
- `run_runtime_debug_tests.sh` covers idle key-runtime scan gating and pending
  release drains with no active core work.
- `run_pd_runtime_tests.sh` covers pointer pass-through, remote display-only
  state, idle-noise suppression, skipping long-idle checks for ordinary
  pointer motion, and active PD mode handler dispatch after the pointer
  hot-path change.
- `run_held_action_tests.sh` covers idle held-repeat housekeeping avoiding
  timer work, phase-preserving repeat cadence, and bounded catch-up after long
  stalls.
- `run_runtime_init_order_tests.sh` covers unchanged runtime call order after
  removing inert diagnostic scope wrappers.
- `run_rgb_layer_render_tests.sh` covers RGB render behavior after removing
  inert diagnostic scope wrappers.
- `run_feature_gate_compile_tests.sh` covers the new split dirty-state source
  in the userspace build surface.
- Closure requires `run_all_host_tests.sh` and the `qmk compile` firmware gate.

## Next Steps

1. Measure firmware loop/report rates on hardware.
2. If report-rate measurements still show a large regression, profile the next
   hottest scan-loop surfaces rather than adding more split-sync special cases.

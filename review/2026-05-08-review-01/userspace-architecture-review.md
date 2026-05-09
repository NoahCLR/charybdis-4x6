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
initializes. `noah_runtime_diag_heartbeat()` refreshes the watchdog from
housekeeping. RGB renders the boot indicator by converting `HSV(0,0,150)` with
the same `hsv_to_rgb()` path used by authored RGB config, then applying that
full-board override for `NOAH_RUNTIME_DIAG_INDICATOR_MS`.

The public diagnostic scope and watchdog query functions remain present for
existing call sites and host tests, but they no longer write RP2040 watchdog
scratch registers or report a latched reboot stage. Scope enter/leave are
inert compatibility hooks.

### Split Runtime Sync

`users/noah/lib/split/runtime_sync.c` still owns the split RPC packet formats
and send cadence. Base runtime state is cheap and continues to be built every
sync tick. Combo feedback packets are also built every sync tick so combo RGB
feedback can move from idle to active immediately; the existing packet memcmp
still prevents unchanged combo packets from being sent before heartbeat.

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

## Contracts

- Runtime diagnostics owns only minimal watchdog restart behavior: enable once
  at post-init and update from housekeeping.
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
- Idle clean scans must not rebuild key-feedback semantic/branch maps before
  the heartbeat window.
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

## Verification Coverage

Expected coverage for this thread:

- `run_runtime_diag_tests.sh` covers watchdog enable/update without reboot-stage
  diagnostics plus boot-indicator expiry.
- `run_rgb_layer_render_tests.sh` covers the full-white boot indicator render
  override.
- `run_split_runtime_sync_tests.sh` covers immediate combo feedback, idle
  key-feedback dirty gating, and heartbeat recovery behavior.
- Key-runtime integration runners cover dirty notifications from scan,
  transition, and feedback paths.
- `run_runtime_debug_tests.sh` covers idle key-runtime scan gating and pending
  release drains with no active core work.
- `run_pd_runtime_tests.sh` covers pointer pass-through, remote display-only
  state, idle-noise suppression, and active PD mode handler dispatch after the
  pointer hot-path change.
- `run_feature_gate_compile_tests.sh` covers the new split dirty-state source
  in the userspace build surface.
- Closure requires `run_all_host_tests.sh` and the `qmk compile` firmware gate.

## Next Steps

1. Measure firmware loop/report rates on hardware.
2. If report-rate measurements still show a large regression, profile the next
   hottest scan-loop surfaces rather than adding more split-sync special cases.

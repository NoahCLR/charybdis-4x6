# Runtime Loop Performance Review

This review was opened because the previous review folder,
`review/2026-05-05-review-01/`, is closed and must remain an immutable
snapshot. This thread covers a different architecture topic: reducing runtime
loop overhead after observed mouse/report-rate regressions.

## Review Scope

The requested behavior change is intentionally narrow:

- Keep the RP2040 watchdog restart path, but remove stage-specific reboot
  diagnostics.
- Keep only a simple 150-brightness white RGB boot indicator after every
  runtime init.
- Remove per-scan watchdog scratch writes from diagnostic scopes.
- Reduce split runtime sync work on idle scans without changing the packets,
  heartbeats, or remote render behavior visible to the user.

Out of scope:

- Changing authored keymap behavior.
- Changing RGB layer, combo, key-feedback, automouse, or pointing mode visual
  semantics after the boot indicator expires.
- Changing split RPC packet formats.
- Reworking the broader key runtime scan architecture.

## Current Design

Implementation status: landed and verified on 2026-05-08 with the targeted
host runners, full host suite, and noah firmware compile listed in
`progress.md`.

### Runtime Diagnostics

`users/noah/lib/state/diagnostics/runtime_diag.c` is now a minimal restart
watchdog and boot-indicator surface. `noah_runtime_diag_post_init()` enables
the RP2040 hardware watchdog and starts the indicator every time the runtime
initializes. `noah_runtime_diag_heartbeat()` refreshes the watchdog from
housekeeping. RGB renders the boot indicator as a full-board RGB(150,150,150)
override for `NOAH_RUNTIME_DIAG_INDICATOR_MS`, then normal scene rendering
resumes.

The public diagnostic scope and watchdog query functions remain present for
existing call sites and host tests, but they no longer write RP2040 watchdog
scratch registers or report a latched reboot stage. Scope enter/leave are
inert compatibility hooks.

### Split Runtime Sync

`users/noah/lib/split/runtime_sync.c` still owns the split RPC packet formats
and send cadence. Base runtime state is cheap and continues to be built every
sync tick. The heavier combo and key-feedback packets are built only when one
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

## Contracts

- Runtime diagnostics owns only minimal watchdog restart behavior: enable once
  at post-init and update from housekeeping.
- The supported visible behavior is a 150-brightness white boot indicator after
  every runtime initialization.
- Diagnostic watchdog query APIs are compatibility APIs and must report idle /
  false / zero unless a future implementation deliberately reintroduces real
  reboot diagnostics.
- Split sync packet formats remain unchanged.
- Split sync still sends forced packets and heartbeat packets even when state is
  clean, but idle clean scans must not rebuild combo/key-feedback maps before
  the heartbeat window.
- Dirty notifications must be available to key-runtime code even when a host
  test defines `SPLIT_TRANSACTION_IDS_USER` but does not link
  `runtime_sync.c`.

## Verification Coverage

Expected coverage for this thread:

- `run_runtime_diag_tests.sh` covers watchdog enable/update without reboot-stage
  diagnostics plus boot-indicator expiry.
- `run_rgb_layer_render_tests.sh` covers the full-white boot indicator render
  override.
- `run_split_runtime_sync_tests.sh` covers idle dirty gating and heartbeat
  recovery behavior.
- Key-runtime integration runners cover dirty notifications from scan,
  transition, and feedback paths.
- `run_feature_gate_compile_tests.sh` covers the new split dirty-state source
  in the userspace build surface.
- Closure requires `run_all_host_tests.sh` and the `qmk compile` firmware gate.

## Next Steps

1. Measure firmware loop/report rates on hardware.
2. If report-rate measurements still show a large regression, profile the next
   hottest scan-loop surfaces rather than adding more split-sync special cases.

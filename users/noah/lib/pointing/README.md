# Pointing

The pointing runtime and its one engine. How a report and a key cross it is
[pointer-report.md](../../../../docs/architecture/pointer-report.md) and
[key-press.md](../../../../docs/architecture/key-press.md#pointing-modes-where-keys-enter-the-pointing-runtime);
what a mode does is [pd-mode-domain-v1.md](../../../../docs/spec/pd-mode-domain-v1.md).

## Layout

- `defs/`: fixed identities. `pd_mode_manifest.h` lists the 32 pointing
  slots with their hold keycodes and the traits enum; `pd_mode_flags.h` holds mode ids
  and read-only queries for modules that must not pull in the pointing API
  (lighting, the key runtime); `pd_modes.h` is the full API.
- `modes/`: the one engine every pointing slot runs. `pd_mode_configured.c`
  holds the latched pointing slot, the directional path, button overrides and
  `emit_tap`; `pd_mode_dragscroll.c` is the scrolling path every scrolling
  pointing slot drives.
- `policy/`: `pd_mode_policy.h` answers trait questions about a mode, a key or
  a snapshot; `pointer_layer_policy.c` steers QMK's auto-mouse.
- `runtime/`: `pd_mode_state.c` (the command model), `pd_mode_lifecycle.c`
  (transition side effects, DPI), `pd_mode_registry.c` (traits from the
  effective slot, lifecycle hooks, keycode lookups), `pd_mode_snapshot.c`
  (the read-only view), `pd_runtime.c` (the QMK pointing and layer hooks),
  `pd_mode_key_runtime_bridge.c` (the one write into the reducer).

## State

`pd_mode_runtime_shared_state_t` (`runtime/pd_mode_runtime_shared_state_internal.h`)
is the one store: the local active and locked mode, the owner keys (one entry
per key position, every entry the active mode's), the slave half's mirrored
display state in two banks under a publication counter, and three flags
(synthetic auto-mouse anchor, DPI sync pending, auto-sniping layer).

- Local state changes only through `pd_mode_apply_command()`; each command
  returns a before-and-after snapshot. Its `split_sync_required` has no
  reader: split sync samples the state on its own tick.
- The display banks are written only by the split callback
  (`PD_MODE_COMMAND_REMOTE_SNAPSHOT`); a reader copies the bank the
  counter selects and retries if it moved
  (`pd_mode_remote_display_identity_snapshot`). A remote snapshot replays no
  local side effect: no engine, auto-mouse or DPI change. On the master half
  the display view is the local view.
- `pd_mode_snapshot()` and `pd_mode_snapshot_with_owner_bitmap()` are the
  readers for lighting and policy; take identity and owner keys from one call
  so they come from one publication.

## Traits

`pd_mode_effective_traits()` derives a mode's traits from its effective slot:
keep the pointer layer anchored or prefer typing (the slot's pointer-layer
byte), and for a scrolling slot that does not prefer typing, a lock that owns
QMK's auto-mouse toggle. The registry attaches the lock hooks only to that
last trait. `PD_MODE_TRAIT_ENABLE_DRAGSCROLL_BACKEND` is set for scrolling
slots and read nowhere.

## Host-test seams

`PD_MODE_PUBLISH_TEST_BACKEND` would expose `pd_mode_test_set_publish_seam`,
which runs a callback in the middle of a remote publication so a test can read
across it. No runner defines it, so no test uses the seam today.

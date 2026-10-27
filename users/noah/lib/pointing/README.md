# Pointing

The pointing runtime and its one engine. How a report and a key cross it is
[pointer-report.md](../../../../docs/architecture/pointer-report.md) and
[key-press.md](../../../../docs/architecture/key-press.md#pointing-modes-where-keys-enter-the-pointing-runtime);
what a mode does is
[pd-mode-domain-v1.md](../../../../docs/spec/pd-mode-domain-v1.md).

## Layout

- `defs/`: fixed identities. `pointing_mode_manifest.h` lists the 32 pointing
  slots with their hold keycodes and the traits enum; `pointing_mode_flags.h`
  holds mode ids and read-only queries for modules that must not pull in the
  pointing API (lighting, the key runtime); `pointing_modes.h` is the full
  API.
- `modes/`: the one engine every pointing slot runs.
  `pointing_mode_configured.c` holds the latched pointing slot, the directional
  path and `emit_tap`; `pointing_mode_dragscroll.c` is the
  scrolling path every scrolling pointing slot drives.
- `policy/`: `pointing_mode_policy.h` answers trait questions about a mode, a
  key or a snapshot; `pointer_layer_policy.c` steers QMK's auto-mouse.
- `runtime/`: `pointing_mode_state.c` (the command model),
  `pointing_mode_lifecycle.c` (transition side effects, DPI),
  `pointing_mode_registry.c` (traits from the effective slot, lifecycle hooks,
  keycode lookups), `pointing_mode_snapshot.c` (the read-only view),
  `pd_runtime.c` (the QMK pointing and layer hooks),
  `pointing_mode_key_runtime_bridge.c` (the one write into the reducer).

## State

`pointing_mode_runtime_shared_state_t`
(`runtime/pointing_mode_runtime_shared_state_internal.h`) is the one store:
the local active and locked mode and the active mode's source (its key or its
linked layers, D-F62), the owner keys (one entry per key position, every entry
the active mode's), the slave half's mirrored display state in two banks under
a publication counter, three flags (synthetic auto-mouse anchor, DPI sync
pending, an auto-sniping layer on), the CPI the DPI policy last wrote
while nothing else has written the sensor's since, and when its refresh
period began. The layers a key-started
mode's key turns on are `layer_ownership`'s, not this store's.

- Local state changes only through `pointing_mode_apply_command()`; each command
  returns a before-and-after snapshot. Split sync samples the state on its
  own tick.
- The display banks are written only by the split callback
  (`POINTING_MODE_COMMAND_REMOTE_SNAPSHOT`); a reader copies the bank the
  counter selects and retries if it moved
  (`pointing_mode_remote_display_identity_snapshot`). A remote snapshot replays
  no local side effect: no engine, auto-mouse or DPI change. On the master half
  the display view is the local view.
- `pointing_mode_snapshot()` and `pointing_mode_snapshot_with_owner_bitmap()`
  are the readers for lighting and policy; take identity and owner keys from one
  call so they come from one publication.

## Linked layers

`pointing_mode_layer_started_mode()` (with what ends a mode its layers
started), `pointing_mode_key_turns_on()` and `pointing_mode_link_layers()`
(every layer of the link; a running mode paints beneath the lowest that is on,
read by lighting on both halves) read a slot's link from its effective record,
leaving out the pointer layer and a default layer. The command
model applies them: the layer-state command while no key runs a mode, and the
end of every key source.

## Traits

`pointing_mode_effective_traits()` derives a mode's traits from its effective
slot: keep the pointer layer anchored or prefer typing (the slot's pointer-layer
byte), and for a scrolling slot that does not prefer typing, a lock that owns
QMK's auto-mouse toggle. The registry attaches the lock hooks only to that
last trait.

## Host-test seams

`POINTING_MODE_PUBLISH_TEST_BACKEND` would expose
`pointing_mode_test_set_publish_seam`, which runs a callback in the middle of a
remote publication so a test can read across it. No runner defines it, so no
test uses the seam today.

Copyright 2026 Noah

This userspace contains the shared runtime for the `noah` keymaps.

Layout data, combos, and board-specific hardware tuning stay in the keyboard
keymap. Reusable behavior engines, pointing-device logic, and RGB helpers live
under `users/noah/`.

## Architecture

- `noah_keymap.h`: shared layer ids and custom keycodes used by both the keymap and runtime modules
- `noah_runtime.h` / `noah.c`: shared QMK hook surface and the thin userspace adapter layer
- `lib/key/`: custom key behavior engine, multi-tap resolution, delayed-action replay, and held-action ownership
- `lib/pointing/`: pointing-device mode flags, state, registry, runtime hooks, policy, and per-mode handlers
- `lib/state/`: shared runtime state helpers such as split-sync packets and keyboard modifier snapshots
- `lib/rgb/`: RGB overlays driven by pointing and auto-mouse state

## Authoring Model

- `keyboards/.../keymaps/noah/keymap.c` owns authored key data: `key_behaviors[]`, macros, combos, and physical layers
- `users/noah/` owns reusable runtime behavior that should stay consistent across `noah` keymaps
- pd modes stay registered in one place via `lib/pointing/pd_mode_registry.c`, with motion/key handlers collected in `lib/pointing/pointing_device_mode_handlers.c`

## Maintenance Notes

- Add new pd modes by extending the flag list, registry row, handlers, key behavior, and RGB config; `docs/ADDING_PD_MODE.md` is the canonical workflow
- Keep `lib/key/key_runtime_internal.h` narrow: shared state and cross-file helpers only
- Prefer extending existing runtime modules over introducing one-off special cases in `keymap.c`

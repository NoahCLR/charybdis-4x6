# Adding a Pointing Mode

For ordinary mode creation, use **Charybdis Live → Pointing modes**. The
keyboard has eight fixed device-owned slots. Directional keys and shortcuts,
scrolling, modifier policies, button overrides, DPI and RGB are profile data;
editing them requires no C and no reflash. See [Pointer Modes](POINTER_MODES.md)
and the [PD-mode domain contract](architecture/pd-mode-domain-v1.md).

The first six slots hold the factory Dragscroll, Volume, Brightness, Zoom, Arrow
and Pinch presets. Slots 7 and 8 start disabled. Changing or duplicating a
preset does not require another source file. The authored fallback records live
in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/pd_config.c`.

A ninth slot or a new kind of motion is a firmware capability change. It needs a
new domain/version and capability contract, validation in firmware and the app,
portable-profile migration, runtime implementation, host tests, and hardware
acceptance. Preserve the deployed hold and lock action identities when changing
the registry. Shared engine code belongs under `users/noah/lib/pointing/`; the
keymap should continue to hold authored data rather than per-preset handlers.

The old six-mode compiled handler build and its readback bridges are retired.
Do not add a `pd_mode_<preset>.c` handler or reintroduce the old build flags.

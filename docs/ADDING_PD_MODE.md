# Adding A Pointing-Device Mode

Use this guide when adding a new trackball / pointing-device mode to the `noah` keymap.

This keymap already has a generic pd-mode runtime. Most new modes are incremental changes, but there are a few invariants that must stay in sync or the lock actions, split sync, and VIA conversion will drift.

## What Counts As A Pd Mode

A pd mode is a custom keycode that:

- can be held momentarily
- can optionally be locked with `LOCK_PD_MODE(...)`
- can transform trackball motion in `pointing_device_task_user()`
- can optionally intercept key events while active
- participates in split sync and RGB overlays

Current examples are `VOLUME_MODE`, `BRIGHTNESS_MODE`, `ARROW_MODE`, `ZOOM_MODE`, and `DRAGSCROLL`.

## Hard Constraints

These are the parts most likely to break if they are not updated together.

1. The pd-mode keycodes in `keymap_defs.h` must remain a dense contiguous range ending immediately before `PD_MODE_LOCK_BASE`.
2. `LOCK_PD_MODE(mode_keycode_)` assumes that dense range and computes offsets from `VOLUME_MODE`.
3. `PD_MODE_COUNT` must match the number of bit flags and the number of rows in `pd_modes[]`.
4. Pd-mode flags are stored in `uint8_t`, so the current architecture supports at most 8 modes without widening the flag types and split-sync packet.

## Files To Touch

- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap_defs.h`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/lib/pointing/pointing_device_modes.h`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/lib/pointing/pointing_device_modes.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/lib/pointing/pointing_device_mode_handlers.h`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/lib/pointing/pointing_device_mode_handlers.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`
- `via layouts/via_to_qmk_layout.py`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/VERIFICATION.md`

## Normal Add-Mode Procedure

### 1. Add The Keycode

In `keymap_defs.h`, add the new mode keycode inside the pd-mode block, before `PD_MODE_LOCK_BASE`.

Example:

```c
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRAGSCROLL,
    SCRUB_MODE,
    PD_MODE_LOCK_BASE,
```

Then update the enum math that currently uses the last pd-mode keycode. If `DRAGSCROLL` is no longer the last pd-mode keycode, replace that expression accordingly.

### 2. Add The Mode Flag

In `lib/pointing/pointing_device_modes.h`:

- add a new `PD_MODE_*` bit
- bump `PD_MODE_COUNT`
- update the `_Static_assert(...)` if the final pd-mode keycode changed

Example:

```c
#define PD_MODE_VOLUME      (1 << 0)
#define PD_MODE_ARROW       (1 << 1)
#define PD_MODE_DRAGSCROLL  (1 << 2)
#define PD_MODE_BRIGHTNESS  (1 << 3)
#define PD_MODE_ZOOM        (1 << 4)
#define PD_MODE_SCRUB       (1 << 5)
#define PD_MODE_COUNT 6
```

### 3. Add The Handler API

In `pointing_device_mode_handlers.h`, declare:

- `handle_<mode>_mode(...)`
- `reset_<mode>_mode(...)`
- optionally `handle_<mode>_mode_key(...)`

If the mode only transforms mouse reports, you usually need just the first two.

### 4. Implement The Handler

In `pointing_device_mode_handlers.c`, implement the behavior.

Typical pattern:

- keep per-mode accumulators as `static` file state
- translate trackball deltas into key taps or other actions
- return a zeroed `report_mouse_t` if the cursor should freeze while active
- clear all mode-local state in the reset function

Use a key handler only if the mode needs to reinterpret keyboard or mouse-button presses while active, like `ARROW_MODE`.

### 5. Register The Mode

In `pointing_device_modes.c`, add a row to `pd_modes[]`:

```c
{PD_MODE_SCRUB, SCRUB_MODE, LOCK_PD_MODE(SCRUB_MODE), handle_scrub_mode, NULL, reset_scrub_mode},
```

Fields are:

- `mode_flag`: the bitmask used internally
- `keycode`: the custom keycode that activates the mode
- `lock_action`: usually `LOCK_PD_MODE(...)`
- `handler`: trackball-motion handler, or `NULL` if handled elsewhere
- `key_handler`: optional event interceptor
- `reset`: cleanup on deactivation

### 6. Add Special Side Effects If Needed

Most modes do not need this.

If the mode toggles some firmware feature outside the normal handler path, add hardcoded side effects in `pointing_device_modes.c`. `DRAGSCROLL` is the example to copy: it enables and disables Charybdis dragscroll in `pd_mode_activate()`, `pd_mode_deactivate()`, `pd_mode_lock()`, and `pd_mode_unlock()`.

If your new mode behaves like volume, brightness, zoom, or arrow, you likely do not need extra branches there.

### 7. Put The Key On A Layer

Add the new keycode to a layer in `keymap.c`.

If you want the standard pd-mode behavior:

- hold = momentary mode
- tap = base-layer key at that position
- double tap = lock

add a `key_behaviors[]` row similar to the existing mode keys:

```c
{.keycode = SCRUB_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(SCRUB_MODE))}}},
```

Then place `SCRUB_MODE` on `LAYER_POINTER`, `LAYER_NAV`, or wherever it belongs physically.

### 8. Add RGB Color

In `rgb_config.h`, add a row to `pd_mode_colors[]` so the right half gets a distinct overlay while the mode is active.

Optional: add a `pd_mode_led_groups[]` entry if you want a specific LED subset highlighted.

### 9. Update VIA Conversion

If the mode can appear in your VIA export, update `via layouts/via_to_qmk_layout.py`.

For this repo, that usually means appending the new keycode name to `PD_MODE_KEYCODES` in the same order as the enum in `keymap_defs.h`.

If you forget this, the converter may emit stale `CUSTOM(...)` tokens or misname the new mode.

### 10. Update Verification

Add checks to `VERIFICATION.md` for:

- hold behavior
- double-tap lock and unlock
- interaction with other locked and unlocked pd modes
- interaction with auto-mouse if relevant
- any special key interception behavior

## How The Runtime Works

The important flow is:

1. `key_runtime.c` detects that a pressed keycode is a pd-mode key via `pd_mode_for_keycode(...)`.
2. `pd_mode_key_runtime.c` handles press, release, tap-vs-hold, and lock toggling.
3. `pointing_device_runtime.c` calls the first active mode handler in `pd_modes[]`.
4. `pointer_layer_policy.c` keeps the pointer layer alive while pd modes are active or locked.
5. `split_sync.c` mirrors pd-mode active and locked flags to the other half.
6. `rgb_runtime.c` renders the pd-mode overlay color.

This means most new modes fit the system without changing the generic runtime.

## Common Mistakes

- Adding the keycode outside the contiguous pd-mode enum range.
- Forgetting to bump `PD_MODE_COUNT`.
- Forgetting to add the new row to `pd_modes[]`.
- Forgetting the reset function, which leaves stale accumulators or modifiers behind.
- Forgetting `via_to_qmk_layout.py`, so regenerated layouts emit the wrong symbolic name.
- Adding a 9th mode without widening `uint8_t` flag storage and split sync.
- Creating a mode with special firmware side effects but not mirroring the dragscroll-style activation and cleanup hooks.

## Quick AI Prompt Packet

If you hand this task to an AI, give it these requirements:

1. Add a new pd mode named `<NAME>_MODE`.
2. Keep the pd-mode custom keycodes contiguous in `keymap_defs.h`.
3. Update `PD_MODE_COUNT`, the `PD_MODE_*` bit flags, and the `pd_modes[]` table.
4. Add handler and reset functions in `pointing_device_mode_handlers.c/.h`.
5. Add the key to the desired layer in `keymap.c` and give it double-tap lock behavior via `LOCK_PD_MODE(...)`.
6. Add an RGB overlay color in `rgb_config.h`.
7. Update `via layouts/via_to_qmk_layout.py` so VIA exports render the new symbolic keycode.
8. Extend `VERIFICATION.md` with lock, unlock, overlap, and auto-mouse checks.
9. Compile with `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.
10. Do not break the existing pd-mode invariants or the 8-mode flag limit.


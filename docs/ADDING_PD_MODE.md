# Adding A Pointing-Device Mode

Use this guide when adding a new pointing-device mode to the `noah` userspace.

This repo already has a generic pd-mode runtime. Most new modes should fit into
that runtime without changing the engine. The main risk is breaking one of the
shared invariants, so this guide is optimized around:

- exact files to edit
- the minimum safe change set
- cases where you do need extra runtime work
- a final compile and manual verification checklist

If you have not read them yet, also see:

- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md)
- [POINTER_MODES.md](./POINTER_MODES.md)

## Agent Contract

If you hand this task to an agent, give it this exact job:

1. Add a new custom keycode in `users/noah/noah_keymap.h`.
2. Keep the pd-mode keycodes as one dense contiguous range from `VOLUME_MODE`
   through the last pd-mode keycode immediately before `PD_MODE_LOCK_BASE`.
3. Update the mode flag list and `PD_MODE_COUNT` in `users/noah/lib/pointing/pd_mode_flags.h`.
4. Register the mode in `users/noah/lib/pointing/pd_mode_registry.c`.
5. Add handler/reset declarations in `users/noah/lib/pointing/pointing_device_mode_handlers.h` and implementations in `users/noah/lib/pointing/pointing_device_mode_handlers.c`, if the mode needs them.
6. Add authored key behavior and physical placement in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`.
7. Add an RGB color in `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`.
8. Update `via layouts/via_to_qmk_layout.py` if the keycode can appear in VIA exports.
9. Update user-facing docs if the mode changes real behavior in a meaningful way.
10. Compile with `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

Do not rewrite the generic runtime unless the new mode truly needs runtime
behavior that existing modes do not cover.

## What Counts As A Pd Mode

A pd mode is a custom keycode that:

- is activated by holding a key
- can optionally be locked with `LOCK_PD_MODE(...)`
- can transform trackball motion in the pointing-device pipeline
- can optionally intercept key events while active
- participates in split sync and RGB overlays

Current examples:

- `VOLUME_MODE`
- `BRIGHTNESS_MODE`
- `ARROW_MODE`
- `ZOOM_MODE`
- `DRAGSCROLL`
- `PINCH_MODE`

## Hard Invariants

These are the rules most likely to break the system if you miss one.

1. The pd-mode keycodes in `users/noah/noah_keymap.h` must stay contiguous:
   `VOLUME_MODE ... <last mode> ... PD_MODE_LOCK_BASE`.
2. `LOCK_PD_MODE(mode_keycode_)` computes offsets from `VOLUME_MODE`, so gaps in
   that keycode range will break lock actions.
3. `PD_MODE_COUNT` in `users/noah/lib/pointing/pd_mode_flags.h` must match:
   the number of mode flags, the number of rows in `pd_modes[]`, and the
   dense-keycode `_Static_assert` in
   `users/noah/lib/pointing/pointing_device_modes.h`.
4. Mode flags are currently stored in `uint8_t` values:
   `users/noah/lib/pointing/pd_mode_flags.h` and `users/noah/lib/state/runtime_shared_state.h`.
   That means the current design supports at most 8 modes.

If you add a 9th mode, you must widen the flag storage and the split-sync packet
before the new mode is safe.

## Files You Usually Touch

Normal add-mode work lives in these files:

- `users/noah/noah_keymap.h`
- `users/noah/lib/pointing/pd_mode_flags.h`
- `users/noah/lib/pointing/pointing_device_modes.h`
- `users/noah/lib/pointing/pd_mode_registry.c`
- `users/noah/lib/pointing/pointing_device_mode_handlers.h`
- `users/noah/lib/pointing/pointing_device_mode_handlers.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`
- `via layouts/via_to_qmk_layout.py`

Files you usually do not need to touch:

- `users/noah/lib/pointing/pd_mode_key_runtime.c`
- `users/noah/lib/pointing/pointing_device_runtime.c`
- `users/noah/lib/pointing/pointer_layer_policy.c`
- `users/noah/lib/state/runtime_shared_state.c`
- `users/noah/lib/rgb/rgb_runtime.c`

## Fastest Safe Path

Use this when the new mode behaves like a normal pd mode.

### 1. Add The Custom Keycode

Edit `users/noah/noah_keymap.h`.

Add the new mode keycode inside the existing pd-mode block, immediately before
`PD_MODE_LOCK_BASE`.

Example:

```c
    VOLUME_MODE,
    BRIGHTNESS_MODE,
    ARROW_MODE,
    ZOOM_MODE,
    DRAGSCROLL,
    PINCH_MODE,
    EXAMPLE_MODE,
    PD_MODE_LOCK_BASE,
    LAYER_LOCK_BASE     = PD_MODE_LOCK_BASE + (PD_MODE_LOCK_BASE - VOLUME_MODE),
```

Do not break the dense pd-mode keycode block between `VOLUME_MODE` and
`PD_MODE_LOCK_BASE`.

### 2. Add The Mode Flag

Edit `users/noah/lib/pointing/pd_mode_flags.h`.

- add a new `PD_MODE_*` bit
- bump `PD_MODE_COUNT`

Example:

```c
#define PD_MODE_VOLUME (1 << 0)
#define PD_MODE_ARROW (1 << 1)
#define PD_MODE_DRAGSCROLL (1 << 2)
#define PD_MODE_BRIGHTNESS (1 << 3)
#define PD_MODE_ZOOM (1 << 4)
#define PD_MODE_PINCH (1 << 5)
#define PD_MODE_EXAMPLE (1 << 6)
#define PD_MODE_COUNT 7
```

### 3. Verify The Static Assert

Edit `users/noah/lib/pointing/pointing_device_modes.h`.

The `_Static_assert(...)` in that header should continue to pass once the
keycode block and `PD_MODE_COUNT` are both updated.

### 4. Add Handler Declarations If Needed

Edit `users/noah/lib/pointing/pointing_device_mode_handlers.h`.

Most motion-transforming modes need:

- `handle_<name>_mode(...)`
- `reset_<name>_mode(void)`

Only add `handle_<name>_mode_key(...)` if the mode needs to intercept key
events while active, like `ARROW_MODE`.

### 5. Implement The Handler

Edit `users/noah/lib/pointing/pointing_device_mode_handlers.c`.

Typical pattern:

- keep per-mode state as `static` file-local variables
- accumulate `mouse_report.x` and/or `mouse_report.y`
- emit taps when the accumulated value crosses a threshold
- return a zeroed `report_mouse_t` if the cursor should freeze while active
- fully clear the mode-local state in the reset function

Minimal skeleton:

```c
static int32_t example_acc_y    = 0;
static int8_t  example_last_dir = 0;

report_mouse_t handle_example_mode(report_mouse_t mouse_report) {
    int16_t dy = mouse_report.y;

    if (dy != 0) {
        int8_t dir = (dy > 0) ? 1 : -1;
        if (example_last_dir != 0 && dir != example_last_dir) {
            example_acc_y = 0;
        }
        example_last_dir = dir;

        example_acc_y += dy;
        while (example_acc_y >= EXAMPLE_THRESHOLD) {
            tap_code16(KC_WHATEVER);
            example_acc_y -= EXAMPLE_THRESHOLD;
        }
        while (example_acc_y <= -EXAMPLE_THRESHOLD) {
            tap_code16(KC_WHATEVER_ELSE);
            example_acc_y += EXAMPLE_THRESHOLD;
        }
    }

    return (report_mouse_t){0};
}

void reset_example_mode(void) {
    example_acc_y    = 0;
    example_last_dir = 0;
}
```

### 6. Register The Mode

Edit `users/noah/lib/pointing/pd_mode_registry.c`.

Add a row to `pd_modes[]`:

```c
{PD_MODE_EXAMPLE, EXAMPLE_MODE, LOCK_PD_MODE(EXAMPLE_MODE), handle_example_mode, NULL, reset_example_mode},
```

Field meaning:

- `mode_flag`: internal bit flag
- `keycode`: custom keycode that activates the mode
- `lock_action`: usually `LOCK_PD_MODE(...)`, or `KC_NO` if not lockable
- `handler`: trackball-motion handler, or `NULL`
- `key_handler`: optional key-event interceptor
- `reset`: cleanup callback, or `NULL`

### 7. Add Authored Key Behavior

Edit `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`.

There are two separate jobs here:

- add a `key_behaviors[]` row if the key needs custom tap / double-tap behavior
- place the physical keycode on the desired layer

Important: the generic pd-mode runtime already gives you momentary hold
activation. Double-tap behavior is not automatic. If you want lock, mute, or a
second-tap alternate mode, you must author that explicitly in `key_behaviors[]`.

The standard pd-mode pattern is:

- hold: momentary mode
- first quick tap: base-layer key at that physical position
- quick double tap: lock the mode

Example:

```c
{.keycode = EXAMPLE_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(EXAMPLE_MODE))}}},
```

Then place `EXAMPLE_MODE` on `LAYER_POINTER`, `LAYER_NAV`, or another layer in
the physical `keymaps[][]` block.

Current examples in this repo:

- `ARROW_MODE`: double tap locks
- `DRAGSCROLL`: double tap locks
- `VOLUME_MODE`: double tap mutes instead of locking
- `PINCH_MODE`: second tap is custom and can branch into `ZOOM_MODE`
- `BRIGHTNESS_MODE`: no authored double-tap behavior right now

### 8. Add RGB Color

Edit `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`.

Add a new row to `pd_mode_colors[]`:

```c
{PD_MODE_EXAMPLE, {120, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},
```

Optional: add a matching `pd_mode_led_groups[]` entry if the mode wants a
specific LED subset highlighted.

### 9. Update VIA Conversion

Edit `via layouts/via_to_qmk_layout.py`.

If the new keycode can appear in VIA exports, append it to `PD_MODE_KEYCODES`
in the same order as the enum in `users/noah/noah_keymap.h`.

If you skip this, regenerated layouts may emit stale `CUSTOM(n)` tokens or map
the wrong symbolic keycode.

### 10. Update User Docs If The Mode Is Real

If the new mode is meant to be used, not just prototyped, also update:

- `docs/POINTER_MODES.md` for user-facing behavior
- `docs/INTERACTION_MODEL.md` if the mode introduces a new interaction pattern

## When You Need Extra Work

Most new modes only need the fast path above. Use the sections below only if
the behavior matches.

### Mode Needs Side Effects On Activate / Deactivate

Examples:

- enable a firmware feature while the mode is active
- hold a modifier while the mode is active
- keep auto-mouse alive while the mode is locked

Edit `users/noah/lib/pointing/pd_mode_registry.c`.

Current examples to copy:

- `DRAGSCROLL` toggles Charybdis dragscroll
- `PINCH_MODE` toggles dragscroll and also registers / unregisters a weak `GUI` mod
- locked scroll-like modes use the auto-mouse ownership helpers

If your mode behaves like `VOLUME_MODE`, `BRIGHTNESS_MODE`, or `ZOOM_MODE`, you
probably do not need extra branches here.

### Mode Needs Key Interception

If the mode repurposes keyboard or mouse-button events while active, add a
`key_handler` in `users/noah/lib/pointing/pointing_device_mode_handlers.c` and register it in `pd_modes[]`.

Copy `ARROW_MODE` if you need a template.

### Second-Tap Hold Should Enter Another Pd Mode

The runtime already supports this pattern.

Example from the current keymap:

```c
{
    .keycode = PINCH_MODE,
    .tap_counts =
        {
            [1] = {.tap = TAP_SENDS(MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
        },
},
```

The important rule is that the hold action on that second press must resolve to
another pd-mode keycode. The generic pd-mode key runtime will detect that and
switch to the alternate mode.

You do not need to edit `users/noah/lib/pointing/pd_mode_key_runtime.c` unless you are inventing a new runtime behavior that existing modes do not cover.

### Mode Is Not Lockable

Set the `lock_action` field in `pd_modes[]` to `KC_NO`, and do not add a
double-tap lock action in `key_behaviors[]`.

Only do this if the product behavior really calls for it.

## Runtime Flow

This is the actual control path for pd modes:

1. `users/noah/lib/key/key_runtime_process.c` routes custom key events. It
   calls `handled_key_lookup(...)` from `users/noah/lib/key/key_runtime.c`,
   which bundles both `key_behavior_lookup(...)` and `pd_mode_for_keycode(...)`
   for the current keycode.
2. `users/noah/lib/pointing/pd_mode_key_runtime.c` handles hold, release, tap, double-tap, lock toggling, and alternate-mode entry for pd-mode keys.
3. `users/noah/lib/pointing/pointing_device_runtime.c` calls the first active handler in `pd_modes[]`.
4. `users/noah/lib/pointing/pointer_layer_policy.c` keeps the configured auto-mouse target layer alive while modes are active or locked.
5. `users/noah/lib/state/runtime_shared_state.c` mirrors active and locked flags to the other half.
6. `users/noah/lib/rgb/rgb_runtime.c` renders the mode overlay on the right half.

That is why most new modes are mostly a data-registration job, not a runtime rewrite.

## Common Mistakes

- Adding the keycode outside the contiguous pd-mode range in `custom_keycodes`.
- Forgetting to bump `PD_MODE_COUNT`.
- Forgetting to keep the dense keycode block and `PD_MODE_COUNT` aligned, so
  the `_Static_assert(...)` fails.
- Adding the new flag but forgetting the `pd_modes[]` row.
- Forgetting the reset function, which leaves stale accumulators or modifiers behind.
- Adding side effects in activate / deactivate but forgetting the locked path.
- Forgetting the VIA mapping, so converted layouts emit bad `CUSTOM(...)` tokens.
- Adding a 9th mode without widening the `uint8_t` flag storage and split-sync packet.

## Definition Of Done

A new mode is done when all of the following are true:

- The keycode exists in `users/noah/noah_keymap.h` and the pd-mode range is still dense.
- `PD_MODE_COUNT`, the flag list, and the static assert all match.
- The mode exists in `pd_modes[]`.
- Any needed handler, key handler, and reset function exist.
- The key is physically placed in `keymaps[][]`.
- The authored tap / lock behavior in `key_behaviors[]` matches the intended UX.
- The mode has an RGB overlay color.
- VIA conversion is updated if relevant.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.

## Manual Verification

After compiling, verify the real behavior on hardware:

1. Hold the key and confirm the momentary mode works.
2. Release the key and confirm the mode fully clears.
3. If lockable, quick double tap to lock and repeat to unlock.
4. Confirm locking this mode clears other locked pd modes if that is intended.
5. Confirm holding this mode cancels earlier unlocked modes if that is intended.
6. Confirm any side effects activate and clean up correctly.
7. Confirm RGB shows the expected color while active or locked.
8. Confirm the slave half mirrors lock state and overlay state.
9. If the mode changes key handling, test every intercepted key path.
10. If the mode can appear in VIA, export and reconvert a layout once.

## Quick Diff Checklist

For a normal new mode, the minimum expected diff usually includes:

- `users/noah/noah_keymap.h`
- `users/noah/lib/pointing/pd_mode_flags.h`
- `users/noah/lib/pointing/pointing_device_modes.h`
- `users/noah/lib/pointing/pd_mode_registry.c`
- `users/noah/lib/pointing/pointing_device_mode_handlers.h`
- `users/noah/lib/pointing/pointing_device_mode_handlers.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`
- `keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.h`
- `via layouts/via_to_qmk_layout.py`, if relevant

If your diff reaches into the generic runtime, stop and justify why.

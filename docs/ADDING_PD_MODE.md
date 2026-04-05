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

1. Add a new manifest row in [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h).
2. Add handler/reset declarations in [`users/noah/lib/pointing/pd_mode_handlers.h`](../users/noah/lib/pointing/pd_mode_handlers.h) and implementations in [`users/noah/lib/pointing/pd_mode_handlers.c`](../users/noah/lib/pointing/pd_mode_handlers.c), if the mode needs them.
3. Add authored key behavior and physical placement in [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).
4. Add an RGB color in [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
5. Update user-facing docs if the mode changes real behavior in a meaningful way.
6. Compile with `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

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

1. Every shared pd mode must exist as exactly one row in [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h).
2. Shared pd-mode keycodes and lock keycodes are generated from that manifest.
   Do not hand-edit the generated pd-mode section in [`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h).
3. `LOCK_PD_MODE(mode_keycode_)` token-pastes to the generated `<MODE>_LOCK`
   keycode, so authored mode keycodes must use the manifest-generated symbolic names.
4. Mode flags and split sync now use `pd_mode_mask_t` / `uint16_t` storage:
   [`users/noah/lib/pointing/pd_mode_flags.h`](../users/noah/lib/pointing/pd_mode_flags.h) and [`users/noah/lib/state/split_runtime_sync.h`](../users/noah/lib/state/split_runtime_sync.h).
   The current design supports up to 16 modes.

If you add a 17th mode, you must widen the flag storage and the
[`split_runtime_sync`](../users/noah/lib/state/split_runtime_sync.c) packet
before the new mode is safe.

## Files You Usually Touch

Normal add-mode work lives in these files:

- [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h)
- [`users/noah/lib/pointing/pd_mode_handlers.h`](../users/noah/lib/pointing/pd_mode_handlers.h)
- [`users/noah/lib/pointing/pd_mode_handlers.c`](../users/noah/lib/pointing/pd_mode_handlers.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

Files you usually do not need to touch:

- [`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h)
- [`users/noah/lib/pointing/pd_mode_flags.h`](../users/noah/lib/pointing/pd_mode_flags.h)
- [`users/noah/lib/pointing/pd_modes.h`](../users/noah/lib/pointing/pd_modes.h)
- [`users/noah/lib/pointing/pd_mode_registry.c`](../users/noah/lib/pointing/pd_mode_registry.c)
- [`users/noah/lib/key/key_runtime_process.c`](../users/noah/lib/key/key_runtime_process.c)
- [`users/noah/lib/key/held_action.c`](../users/noah/lib/key/held_action.c)
- [`users/noah/lib/pointing/pd_mode_state.c`](../users/noah/lib/pointing/pd_mode_state.c)
- [`users/noah/lib/pointing/pd_runtime.c`](../users/noah/lib/pointing/pd_runtime.c)
- [`users/noah/lib/pointing/pointer_layer_policy.c`](../users/noah/lib/pointing/pointer_layer_policy.c)
- [`users/noah/lib/state/split_runtime_sync.c`](../users/noah/lib/state/split_runtime_sync.c)
- [`users/noah/lib/rgb/rgb_runtime.c`](../users/noah/lib/rgb/rgb_runtime.c)

## Fastest Safe Path

Use this when the new mode behaves like a normal pd mode.

### 1. Add The Manifest Row

Edit [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h).

Add the new mode as one manifest row. That single row generates:

- the shared mode keycode in [`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h)
- the shared lock keycode used by `LOCK_PD_MODE(...)`
- the `PD_MODE_*` flag
- the `pd_modes[]` registry row

Example:

```c
    M(EXAMPLE, EXAMPLE_MODE, EXAMPLE_MODE_LOCK, handle_example_mode, NULL, reset_example_mode, 0)
```

Field meaning:

- `EXAMPLE`: symbolic suffix used for the generated `PD_MODE_EXAMPLE` flag
- `EXAMPLE_MODE`: shared custom keycode
- `EXAMPLE_MODE_LOCK`: generated lock keycode used by `LOCK_PD_MODE(EXAMPLE_MODE)`
- `handle_example_mode`: optional motion handler
- `NULL`: optional key-event interceptor
- `reset_example_mode`: optional cleanup hook
- `0`: DPI override (`0` = use normal pointer DPI)

### 2. Verify The Generated Outputs

You should not need to manually edit [`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h),
[`users/noah/lib/pointing/pd_mode_flags.h`](../users/noah/lib/pointing/pd_mode_flags.h),
or [`users/noah/lib/pointing/pd_mode_registry.c`](../users/noah/lib/pointing/pd_mode_registry.c)
for a normal new mode. The manifest row should materialize those changes.

### 3. Add Handler Declarations If Needed

Edit [`users/noah/lib/pointing/pd_mode_handlers.h`](../users/noah/lib/pointing/pd_mode_handlers.h).

Most motion-transforming modes need:

- `handle_<name>_mode(...)`
- `reset_<name>_mode(void)`

Only add `handle_<name>_mode_key(...)` if the mode needs to intercept key
events while active, like `ARROW_MODE`.

### 4. Implement The Handler

Edit [`users/noah/lib/pointing/pd_mode_handlers.c`](../users/noah/lib/pointing/pd_mode_handlers.c).

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

### 5. Understand The Generated Registry Row

You do not register the mode by hand anymore. The manifest row expands into a
`pd_modes[]` entry in [`users/noah/lib/pointing/pd_mode_registry.c`](../users/noah/lib/pointing/pd_mode_registry.c):

```c
{PD_MODE_EXAMPLE, EXAMPLE_MODE, LOCK_PD_MODE(EXAMPLE_MODE), handle_example_mode, NULL, reset_example_mode, 0},
```

Field meaning:

- `mode_flag`: internal bit flag
- `keycode`: custom keycode that activates the mode
- `lock_action`: usually `LOCK_PD_MODE(...)`, or `KC_NO` if not lockable
- `handler`: trackball-motion handler, or `NULL`
- `key_handler`: optional key-event interceptor
- `reset`: cleanup callback, or `NULL`
- `dpi`: pointer CPI override while the mode is active (`0` = keep normal pointer DPI)

If the mode needs a custom DPI, thread that through from the keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
the same way the existing `PD_MODE_*_DPI` values are wired.

### 6. Add Authored Key Behavior

Edit [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

There are two separate jobs here:

- add a `key_behaviors[]` row if the key needs custom tap / double-tap behavior
- place the physical keycode on the desired layer

Important: the generic pd-mode runtime already gives you momentary hold
activation. Tap behavior is never implicit. If you want a single tap, double
tap, lock, mute, or a second-tap alternate mode, you must author that
explicitly in `key_behaviors[]`.

Common lockable patterns are:

- hold: momentary mode
- optional first quick tap: an explicit `[0].tap`
- quick double tap: lock the mode when `[1].tap = TAP_SENDS(LOCK_PD_MODE(...))`
- double-tap hold: lock the mode when `[1].hold = TAP_AT_HOLD_THRESHOLD(LOCK_PD_MODE(...))`

Example:

```c
{.keycode = EXAMPLE_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(LOCK_PD_MODE(EXAMPLE_MODE))}}},
```

Then place `EXAMPLE_MODE` on `LAYER_POINTER`, `LAYER_NAV`, or another layer in
the physical `keymaps[][]` block.

Current examples in this repo:

- `ARROW_MODE`: double-tap hold locks
- `DRAGSCROLL`: double tap locks
- `VOLUME_MODE`: double tap mutes instead of locking
- `PINCH_MODE`: second tap is custom and can branch into `ZOOM_MODE`
- `BRIGHTNESS_MODE`: no authored double-tap behavior right now

### 7. Add RGB Color

Edit [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).

Add a new row to `pd_mode_colors[]`:

```c
{PD_MODE_EXAMPLE, {120, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS}},
```

Optional: add a matching `pd_mode_led_groups[]` entry if the mode wants a
specific LED subset highlighted.

### 8. Update User Docs If The Mode Is Real

If the new mode is meant to be used, not just prototyped, also update:

- [POINTER_MODES.md](./POINTER_MODES.md) for user-facing behavior
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md) if the mode introduces a new interaction pattern

## When You Need Extra Work

Most new modes only need the fast path above. Use the sections below only if
the behavior matches.

### Mode Needs Side Effects On Activate / Deactivate

Examples:

- enable a firmware feature while the mode is active
- hold a modifier while the mode is active
- keep auto-mouse alive while the mode is locked

Edit [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h)
and, if needed, [`users/noah/lib/pointing/pd_mode_registry.c`](../users/noah/lib/pointing/pd_mode_registry.c).

Current examples to copy:

- `DRAGSCROLL` toggles Charybdis dragscroll
- `PINCH_MODE` toggles dragscroll and also registers / unregisters an owned real `GUI` mod
- locked scroll-like modes use the auto-mouse ownership helpers

If your mode behaves like `VOLUME_MODE`, `BRIGHTNESS_MODE`, or `ZOOM_MODE`, you
probably do not need extra branches here.

### Mode Needs Key Interception

If the mode repurposes keyboard or mouse-button events while active, add a
`key_handler` in [`users/noah/lib/pointing/pd_mode_handlers.c`](../users/noah/lib/pointing/pd_mode_handlers.c) and reference it from the manifest row.

Copy `ARROW_MODE` if you need a template.

### Second-Tap Hold Should Enter Another Pd Mode

The runtime already supports this pattern.

Example from the current keymap:

```c
{
    .keycode = PINCH_MODE,
    .tap_counts =
        {
            [1] = {.tap = TAP_SENDS(VIA_MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
        },
},
```

The important rule is that the hold action on that second press must resolve to
another pd-mode keycode. The generic held-action dispatch path will treat that
as another pd-mode key and switch to the alternate mode.

You do not need to edit the generic key-runtime or pd-mode runtime files unless
you are inventing a new runtime behavior that existing modes do not cover.

### Mode Is Not Lockable

Set the manifest row's lock-keycode field to `KC_NO`, and do not add a
double-tap lock action in `key_behaviors[]`.

Only do this if the product behavior really calls for it.

## Runtime Flow

This is the actual control path for pd modes:

1. [`users/noah/lib/key/key_runtime_process.c`](../users/noah/lib/key/key_runtime_process.c) orchestrates custom key events.
   Press and release resolution live in [`key_runtime_press.c`](../users/noah/lib/key/key_runtime_press.c) and
   [`key_runtime_release.c`](../users/noah/lib/key/key_runtime_release.c); preflight checks such as multi-tap flush and
   layer-interrupt flagging live in [`key_runtime_preflight.c`](../users/noah/lib/key/key_runtime_preflight.c).
2. [`users/noah/lib/key/held_action.c`](../users/noah/lib/key/held_action.c) manages per-key held-action ownership.
   Held pd-mode keycodes flow through [`users/noah/lib/action/action_lifecycle.c`](../users/noah/lib/action/action_lifecycle.c),
   which routes them to `pd_mode_handle_keycode_press()` and
   `pd_mode_handle_keycode_release()`.
3. [`users/noah/lib/pointing/pd_mode_state.c`](../users/noah/lib/pointing/pd_mode_state.c) owns active and locked mode state,
   plus lock exclusivity.
4. [`users/noah/lib/pointing/pd_mode_registry.c`](../users/noah/lib/pointing/pd_mode_registry.c) materializes the mode table
   from the manifest, including handlers, reset hooks, lock actions, and DPI behavior.
5. [`users/noah/lib/pointing/pd_runtime.c`](../users/noah/lib/pointing/pd_runtime.c) calls the first active
   handler in `pd_modes[]`.
6. [`users/noah/lib/pointing/pointer_layer_policy.c`](../users/noah/lib/pointing/pointer_layer_policy.c) keeps the configured
   auto-mouse target layer alive while modes are active or locked.
7. [`users/noah/lib/state/split_runtime_sync.c`](../users/noah/lib/state/split_runtime_sync.c) mirrors active and locked
   flags to the other half.
8. [`users/noah/lib/rgb/rgb_runtime.c`](../users/noah/lib/rgb/rgb_runtime.c) renders the mode overlay on the right
   half.

That is why most new modes are mostly a data-registration job, not a runtime rewrite.

## Common Mistakes

- Editing the generated pd-mode section in `custom_keycodes` instead of the manifest.
- Adding the new manifest row but forgetting the handler declaration or implementation it references.
- Forgetting the reset function, which leaves stale accumulators or modifiers behind.
- Adding side effects in activate / deactivate but forgetting the locked path.
- Adding a 17th mode without widening the `pd_mode_mask_t` storage and the
  [`split_runtime_sync`](../users/noah/lib/state/split_runtime_sync.c) packet.

## Definition Of Done

A new mode is done when all of the following are true:

- The manifest row exists in [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h).
- The generated keycode, flag, and registry row all exist.
- Any needed handler, key handler, and reset function exist.
- The key is physically placed in `keymaps[][]`.
- The authored tap / lock behavior in `key_behaviors[]` matches the intended UX.
- The mode has an RGB overlay color.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.

## Manual Verification

After compiling, verify the real behavior on hardware:

1. Hold the key and confirm the momentary mode works.
2. Release the key and confirm the mode fully clears.
3. If lockable, test the authored lock gesture and repeat it to unlock.
4. Confirm locking this mode clears other locked pd modes if that is intended.
5. Confirm holding this mode cancels earlier unlocked modes if that is intended.
6. Confirm any side effects activate and clean up correctly.
7. Confirm RGB shows the expected color while active or locked.
8. Confirm the slave half mirrors lock state and overlay state.
9. If the mode changes key handling, test every intercepted key path.
10. If the mode can appear in VIA, export and reconvert a layout once.

## Quick Diff Checklist

For a normal new mode, the minimum expected diff usually includes:

- [`users/noah/lib/pointing/pd_mode_manifest.h`](../users/noah/lib/pointing/pd_mode_manifest.h)
- [`users/noah/lib/pointing/pd_mode_handlers.h`](../users/noah/lib/pointing/pd_mode_handlers.h)
- [`users/noah/lib/pointing/pd_mode_handlers.c`](../users/noah/lib/pointing/pd_mode_handlers.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

If your diff reaches into the generic runtime, stop and justify why.

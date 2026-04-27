# Adding A Pointing-Device Mode

This is a maintainer-facing guide for adding a new pointing-device mode to the
`noah` userspace.

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

1. Add a new manifest row in [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h).
2. If the mode needs runtime behavior, add handler/reset declarations in [`users/noah/lib/pointing/modes/pd_mode_handlers.h`](../users/noah/lib/pointing/modes/pd_mode_handlers.h), implement them in a new per-mode file under [`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/), and wire that file into [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk).
3. Expose the mode through at least one physical path in [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c), either directly in `keymaps[][]` or indirectly from another key behavior or combo. Add a `key_behaviors[]` row only if the mode itself needs custom taps or higher-tap behavior.
4. Add an RGB color in [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
5. Update user-facing docs if the mode changes real behavior in a meaningful way, including [`README.md`](../README.md) when the shared capability summary changes.
6. Run the matching host tests and compile gates for the files you touched.
7. Finish with `sh tests/host/run_all_host_tests.sh` and then `qmk compile -kb bastardkb/charybdis/4x6 -km noah`.

Do not rewrite the generic runtime unless the new mode truly needs runtime
behavior that existing modes do not cover.

If a mode needs unusual activation, deactivation, lock, unlock, buffered-tap
replay policy, or concurrent keyboard-event masking policy that is not shared
policy, keep the normal manifest path intact
and add an optional lifecycle hook object in a mode-owned file under
[`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/), then
reference that object from the manifest row instead of introducing another
one-off manifest trait or registry switch edit. Only reach into
[`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c)
or [`users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`](../users/noah/lib/pointing/runtime/pd_mode_lifecycle.c)
when the shared activate / deactivate / lock / unlock policy itself needs to
change.

`users/noah/lib/pointing/` is organized by ownership:

- `defs/` for manifest-generated shared mode definitions
- `runtime/` for registry, lifecycle, state, and dispatch machinery
- `policy/` for layer-policy glue tied to pd-mode state
- `modes/` for mode-owned handlers and lifecycle hooks

## What Counts As A Pd Mode

A pd mode is a custom keycode that:

- is activated by holding a key
- can be placed directly in the keymap for default momentary behavior
- can optionally grow explicit tap / hold / multi-tap behavior through
  `key_behaviors[]`
- gets a generated `<MODE>_LOCK` keycode whether or not the keymap uses it
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

1. Every shared pd mode must exist as exactly one row in [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h).
2. Shared pd-mode keycodes and lock keycodes are generated from that manifest.
   Do not hand-edit the generated pd-mode section in [`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h).
3. Authored lock actions should use the generated `<MODE>_LOCK` keycode for
   that mode, so mode keycodes must keep using the manifest-generated symbolic names.
4. Mode flags use `pd_mode_mask_t`, and split sync mirrors the active and
   locked modes as `pd_mode_id_t` values:
   [`users/noah/lib/pointing/defs/pd_mode_flags.h`](../users/noah/lib/pointing/defs/pd_mode_flags.h) and [`users/noah/lib/state/runtime/split_runtime_sync.h`](../users/noah/lib/state/runtime/split_runtime_sync.h).
   The current `pd_mode_mask_t` storage caps `PD_MODE_COUNT` at 16.

If you add a 17th mode, you must widen the flag storage and the
split-sync assumptions before the new mode is safe.

## Files You Usually Touch

Normal add-mode work lives in these files:

- [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h)
- [`users/noah/lib/pointing/modes/pd_mode_handlers.h`](../users/noah/lib/pointing/modes/pd_mode_handlers.h)
- a new per-mode implementation file under [`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/) such as [`pd_mode_volume.c`](../users/noah/lib/pointing/modes/pd_mode_volume.c)
- [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

Files you usually do not need to touch:

- [`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h)
- [`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h)
- [`users/noah/lib/pointing/defs/pd_mode_flags.h`](../users/noah/lib/pointing/defs/pd_mode_flags.h)
- [`users/noah/lib/pointing/defs/pd_modes.h`](../users/noah/lib/pointing/defs/pd_modes.h)
- [`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c)
- [`users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`](../users/noah/lib/pointing/runtime/pd_mode_lifecycle.c)
- [`users/noah/lib/key/runtime/process.c`](../users/noah/lib/key/runtime/process.c)
- [`users/noah/lib/key/ownership/held_action.c`](../users/noah/lib/key/ownership/held_action.c)
- [`users/noah/lib/pointing/runtime/pd_mode_state.c`](../users/noah/lib/pointing/runtime/pd_mode_state.c)
- [`users/noah/lib/pointing/runtime/pd_runtime.c`](../users/noah/lib/pointing/runtime/pd_runtime.c)
- [`users/noah/lib/pointing/policy/pointer_layer_policy.c`](../users/noah/lib/pointing/policy/pointer_layer_policy.c)
- [`users/noah/lib/state/runtime/split_runtime_sync.c`](../users/noah/lib/state/runtime/split_runtime_sync.c)
- [`users/noah/lib/rgb/core/rgb_runtime.c`](../users/noah/lib/rgb/core/rgb_runtime.c)

## Fastest Safe Path

Use this when the new mode behaves like a normal pd mode.

### 1. Add The Manifest Row

Edit [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h).

Add the new mode as one manifest row. That single row generates:

- the shared mode keycode in [`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h)
- the shared lock keycode (`<MODE>_LOCK`)
- the `PD_MODE_*` flag
- the `pd_modes[]` registry row

Example:

```c
    PDM(EXAMPLE, EXAMPLE_MODE, handle_example_mode, NULL, reset_example_mode, 0, PD_MODE_TRAIT_NONE, NULL)
```

Field meaning:

- `EXAMPLE`: symbolic suffix used for the generated `PD_MODE_EXAMPLE` flag
- `EXAMPLE_MODE`: shared custom keycode
- `handle_example_mode`: optional motion handler
- `NULL`: optional key-event interceptor
- `reset_example_mode`: optional cleanup hook
- `0`: DPI override (`0` = use normal pointer DPI)
- `PD_MODE_TRAIT_NONE`: manifest traits consumed by shared policy; combine `PD_MODE_TRAIT_*` flags when the mode needs them
- `NULL`: optional lifecycle hook pointer (`NULL` = no custom activate / deactivate / lock / unlock side effects, buffered tap replay policy, or concurrent keyboard-event masking policy)

### 2. Verify The Generated Outputs

You should not need to manually edit [`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h),
[`users/noah/lib/pointing/defs/pd_mode_flags.h`](../users/noah/lib/pointing/defs/pd_mode_flags.h),
or [`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c)
for a normal new mode. The manifest row should materialize those changes.
Only unusual lifecycle hook objects need extra registry work, and even then the
mode definition row should own the selection.

### 3. Add Handler Declarations If Needed

Edit [`users/noah/lib/pointing/modes/pd_mode_handlers.h`](../users/noah/lib/pointing/modes/pd_mode_handlers.h).

Most motion-transforming modes need:

- `handle_<name>_mode(...)`
- `reset_<name>_mode(void)`

Only add `handle_<name>_mode_key(...)` if the mode needs to intercept key
events while active, like `ARROW_MODE`.

### 4. Implement The Handler

Create a new per-mode implementation file under
[`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/) and add it to
[`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk).

Copy the nearest existing mode file for structure. Modes such as
[`pd_mode_volume.c`](../users/noah/lib/pointing/modes/pd_mode_volume.c),
[`pd_mode_brightness.c`](../users/noah/lib/pointing/modes/pd_mode_brightness.c),
[`pd_mode_zoom.c`](../users/noah/lib/pointing/modes/pd_mode_zoom.c), and
[`pd_mode_arrow.c`](../users/noah/lib/pointing/modes/pd_mode_arrow.c) are now split
per translation unit instead of growing one central handlers file.

Typical pattern:

- keep per-mode state as `static` file-local variables
- accumulate `mouse_report.x` and/or `mouse_report.y`
- emit taps when the accumulated value crosses a threshold
- return a zeroed `report_mouse_t` if the cursor should freeze while active
- fully clear the mode-local state in the reset function

Many vertical threshold modes can reuse helpers from
[`pd_mode_handler_common.h`](../users/noah/lib/pointing/modes/pd_mode_handler_common.h),
but bespoke modes should still keep their own state in the per-mode file.
If a mode needs to send a synthetic QMK tap while temporarily ignoring a subset
of ambient keyboard modifiers, prefer
[`noah_emit_synthetic_qmk_tap_with_masked_keyboard_mods(...)`](../users/noah/lib/action/action_dispatch.h)
instead of open-coding `keyboard_mod_state_suspend()` / `keyboard_mod_state_apply()`
around the emit. That keeps fallback-hold settlement and mod filtering in the
shared output seam.

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
`pd_modes[]` entry in [`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c):

```c
{PD_MODE_EXAMPLE, EXAMPLE_MODE, EXAMPLE_MODE_LOCK, handle_example_mode, NULL, reset_example_mode, 0, PD_MODE_TRAIT_NONE, NULL},
```

Field meaning:

- `mode_flag`: internal bit flag
- `keycode`: custom keycode that activates the mode
- `lock_action`: generated `<MODE>_LOCK` keycode for that mode
- `handler`: trackball-motion handler, or `NULL`
- `key_handler`: optional key-event interceptor
- `reset`: cleanup callback, or `NULL`
- `dpi`: pointer CPI override while the mode is active (`0` = keep normal pointer DPI)
- `traits`: manifest-defined `PD_MODE_TRAIT_*` flags consumed by shared pointer policy and registry behavior
- `lifecycle`: optional activate / deactivate / lock / unlock side-effect hooks plus buffered tap replay and concurrent keyboard-event masking policy owned by this mode definition row

If the mode needs a custom DPI, thread that through from the keymap
[`config.h`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
the same way the existing `PD_MODE_*_DPI` values are wired.

### 6. Place The Key And Add Optional Authored Behavior

Edit [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).

There are two separate jobs here:

- make the mode reachable from at least one physical path
- add a `key_behaviors[]` row only if the mode keycode itself needs custom tap
  / double-tap / higher-tap behavior

Important: a plain pd-mode keycode already works as a default momentary hold.
Tap behavior is never implicit. If you want a single tap, double tap, lock,
mute, or a second-tap alternate mode, you must author that explicitly in
`key_behaviors[]`.

When a pd-mode key has a first-tap override and a later tap-count hold enters a
different pd mode, the runtime defers the first mode until the hold threshold.
That keeps stacked mode lifecycles from overlapping while the tap count is
still unresolved.

Common lockable patterns are:

- hold: momentary mode
- optional first quick tap: an explicit `[0].tap`
- quick double tap: lock the mode when `[1].tap = TAP_SENDS(EXAMPLE_MODE_LOCK)`
- double-tap hold: lock the mode when `[1].hold = TAP_AT_HOLD_THRESHOLD(EXAMPLE_MODE_LOCK)`

`TAP_SENDS(...)` actions are tap outcomes, so they settle on release or pending
tap-series expiry rather than on the press that first reaches that tap count.

Example:

```c
{.keycode = EXAMPLE_MODE, .tap_counts = {[1] = {.tap = TAP_SENDS(EXAMPLE_MODE_LOCK)}}},
```

That row only matters if `EXAMPLE_MODE` is reachable. You can expose it:

- directly by placing `EXAMPLE_MODE` on `LAYER_POINTER`, `LAYER_NAV`, or
  another layer in `keymaps[][]`
- indirectly by emitting `EXAMPLE_MODE` from another key behavior or combo

Current examples in this repo:

- `ARROW_MODE`: the current profile does not place a plain mode key or a
  dedicated `key_behaviors[]` row; arrow mode is exposed through
  `KC_RIGHT_ALT` tap lock instead
- `DRAGSCROLL`: single tap `.`, double-tap hold locks
- `VOLUME_MODE`: double tap mutes instead of locking
- `PINCH_MODE`: second tap is custom and can branch into `ZOOM_MODE`
- `BRIGHTNESS_MODE`: no authored double-tap behavior right now

### 7. Add RGB Color

Edit [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c).
The `pd_mode_colors[]` table is compiled when `RGB_PD_MODE_FEEDBACK_ENABLE` is
enabled in the active keymap config.

Add a new row to `pd_mode_colors[]`:

```c
{ .pointing_mode = PD_MODE_EXAMPLE, .color = HSV(120, 255, RGB_MATRIX_MAXIMUM_BRIGHTNESS), .locality = RGB_RIGHT_HALF },
```

Use `.locality` to choose where the overlay paints: `RGB_BOTH_HALVES`,
`RGB_LEFT_HALF`, `RGB_RIGHT_HALF`, `RGB_KEY_HALF`, or `RGB_KEYS_ONLY`.

Optional: add or uncomment a `pd_mode_led_groups_data` row with
`.led_group = RGB_LED_GROUP_*` if the mode wants a specific LED subset
highlighted.

### 8. Update User Docs If The Mode Is Real

If the new mode is meant to be used, not just prototyped, also update:

- [README.md](../README.md) if the shared mode roster or user-facing capability
  summary changed
- [POINTER_MODES.md](./POINTER_MODES.md) for user-facing behavior
- [INTERACTION_MODEL.md](./INTERACTION_MODEL.md) if the mode introduces a new shared interaction pattern
- [KEYMAP.md](./KEYMAP.md) if the current profile gives that mode concrete taps,
  gestures, or placement worth documenting

## When You Need Extra Work

Most new modes only need the fast path above. Use the sections below only if
the behavior matches.

### Mode Needs Side Effects On Activate / Deactivate Or Buffered Tap Replay

Examples:

- enable a firmware feature while the mode is active
- hold a modifier while the mode is active
- keep auto-mouse alive while the mode is locked
- hide a mode-owned real modifier from delayed single-tap replay so transparent taps resolve like the underlying key
- hide a mode-owned real modifier from concurrent keyboard-event processing so unrelated keys do not chord with the mode-owned modifier

Edit [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h)
and, if needed, [`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c)
or [`users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`](../users/noah/lib/pointing/runtime/pd_mode_lifecycle.c).

Prefer existing manifest traits first. If the behavior is genuinely new
shared policy, add a new `PD_MODE_TRAIT_*` flag and consume that trait
centrally instead of reintroducing per-mode identity checks.

If the behavior is mode-specific instead of shared policy, add an optional
lifecycle hook object in a mode-owned file under
[`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/) and
point the manifest row at it. Do not reintroduce a mode-selection switch in
the registry.

Current examples to copy:

- `DRAGSCROLL` uses the shared local dragscroll handler plus the lock-owned auto-mouse toggle helper; its ordinary held-key path relies on the normal QMK auto-mouse key tracking instead of a synthetic lifecycle anchor
- `PINCH_MODE` uses the same dragscroll handler and keeps its owned real `GUI` modifier lifecycle, buffered tap replay masking policy, and concurrent keyboard-event masking policy in [`pd_mode_pinch.c`](../users/noah/lib/pointing/modes/pd_mode_pinch.c)
- locked non-toggle modes that set `PD_MODE_TRAIT_KEEP_AUTO_MOUSE_ANCHORED` use the shared synthetic lifecycle anchor in [`pd_mode_lifecycle.c`](../users/noah/lib/pointing/runtime/pd_mode_lifecycle.c)
- locked scroll-like modes that set `PD_MODE_TRAIT_LOCK_OWNS_AUTO_MOUSE_TOGGLE` should not also add a synthetic tracker-style anchor

If a mode owns real modifiers while active and also supports buffered single
taps, prefer masking only the managed-only subset of those modifiers during the
buffered replay snapshot. That keeps transparent taps intuitive without
dropping a modifier the user is physically holding outside the mode itself.

If a mode owns real modifiers while active and unrelated keyboard events can
arrive while the mode is held, prefer masking only the managed-only subset of
those modifiers during concurrent keyboard-event processing as well. That keeps
mode-owned modifiers scoped to pointing behavior without stripping a real
modifier the user physically pressed.

If your mode behaves like `VOLUME_MODE`, `BRIGHTNESS_MODE`, or `ZOOM_MODE`, you
probably do not need extra branches here.

### Mode Needs Key Interception

If the mode repurposes keyboard or mouse-button events while active, add a
`key_handler` in the mode's own implementation file, as
[`pd_mode_arrow.c`](../users/noah/lib/pointing/modes/pd_mode_arrow.c) does, and
reference it from the manifest row.

Copy `ARROW_MODE` if you need a template.

### Second-Tap Hold Should Enter Another Pd Mode

The runtime already supports this pattern.

Example from the current keymap:

```c
{
    .keycode = PINCH_MODE,
    .tap_counts =
        {
            [0] = {.tap = TAP_SENDS(KC_TRNS)},
            [1] = {.tap = TAP_SENDS(VIA_MACRO_6), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)},
        },
},
```

The important rule is that the hold action on that second press must resolve to
another pd-mode keycode. The generic held-action dispatch path will treat that
as another pd-mode key and switch to the alternate mode.
The first tap does not need an explicit `PRESS_AND_HOLD_UNTIL_RELEASE(...)`
entry for containment; the stacked-pd runtime rule supplies thresholded first
hold behavior for this shape.

You do not need to edit the generic key-runtime or pd-mode runtime files unless
you are inventing a new runtime behavior that existing modes do not cover.

### Mode Should Not Expose A Lock Gesture

Every shared pd mode still gets a generated `<MODE>_LOCK` keycode from the
manifest.

If the intended behavior should not expose locking, do not bind that action on
a physical key and do not author a lock gesture in `key_behaviors[]`.

If a mode must be impossible to lock anywhere, that is a runtime design change,
not part of the normal add-mode path.

## Runtime Flow

This is the actual control path for pd modes:

1. [`users/noah/lib/key/runtime/process.c`](../users/noah/lib/key/runtime/process.c) orchestrates custom key events.
   Press and release wrappers live in [`press.c`](../users/noah/lib/key/runtime/press.c) and
   [`release.c`](../users/noah/lib/key/runtime/release.c), but the shared transition planning now lives in
   [`transition.c`](../users/noah/lib/key/runtime/transition.c). Preflight checks such as active-key interruption,
   pending-series retention, and layer-interrupt flagging live in [`preflight.c`](../users/noah/lib/key/runtime/preflight.c).
2. [`users/noah/lib/key/ownership/held_action.c`](../users/noah/lib/key/ownership/held_action.c) manages per-key held-action ownership.
   Held pd-mode keycodes flow through [`users/noah/lib/action/action_lifecycle.c`](../users/noah/lib/action/action_lifecycle.c),
   which routes them to `pd_mode_handle_keycode_press()` and
   `pd_mode_handle_keycode_release()`.
3. [`users/noah/lib/pointing/runtime/pd_mode_state.c`](../users/noah/lib/pointing/runtime/pd_mode_state.c) owns active and locked mode state,
   local-vs-display snapshots, and split-applied mirrored UI state.
4. [`users/noah/lib/pointing/runtime/pd_mode_registry.c`](../users/noah/lib/pointing/runtime/pd_mode_registry.c) materializes the mode table
   from the manifest, including handlers, reset hooks, traits, row-owned
   lifecycle hook selection from [`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/),
   buffered tap replay policy, lock actions, and DPI metadata.
5. [`users/noah/lib/pointing/runtime/pd_mode_lifecycle.c`](../users/noah/lib/pointing/runtime/pd_mode_lifecycle.c) owns activate / deactivate /
   lock / unlock transitions, exclusivity, shared auto-mouse policy, DPI
   application, and active-mode key interception.
6. [`users/noah/lib/pointing/runtime/pd_runtime.c`](../users/noah/lib/pointing/runtime/pd_runtime.c) calls the
   selected active mode's handler from `pd_modes[]`.
7. [`users/noah/lib/pointing/policy/pointer_layer_policy.c`](../users/noah/lib/pointing/policy/pointer_layer_policy.c) keeps the configured
   auto-mouse target layer alive while modes are active or locked.
8. [`users/noah/lib/state/runtime/split_runtime_sync.c`](../users/noah/lib/state/runtime/split_runtime_sync.c) mirrors active and locked
   mode ids, auto-mouse progress, combo feedback, key-feedback semantic and tap-branch state,
   and preview-layer state to the other half, and
   [`pd_mode_state.c`](../users/noah/lib/pointing/runtime/pd_mode_state.c)
   exposes the mirrored mode state as display-state queries for UI consumers.
9. [`users/noah/lib/rgb/core/rgb_runtime.c`](../users/noah/lib/rgb/core/rgb_runtime.c) orchestrates stage order, and
   [`users/noah/lib/rgb/stages/rgb_pd_mode_stage.c`](../users/noah/lib/rgb/stages/rgb_pd_mode_stage.c) renders the mode overlay on the right half.

That is why most new modes are mostly a data-registration job, not a runtime rewrite.

## Common Mistakes

- Editing the generated pd-mode section in `custom_keycodes` instead of the manifest.
- Adding the new manifest row but forgetting the handler declaration or implementation it references.
- Adding a new per-mode source file but forgetting to wire it into [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk).
- Adding a lifecycle hook object but forgetting to reference it from the manifest row.
- Forgetting the reset function, which leaves stale accumulators or modifiers behind.
- Adding side effects in activate / deactivate but forgetting the locked path.
- Adding a 17th mode without widening the `pd_mode_mask_t` storage and the
  [`split_runtime_sync`](../users/noah/lib/state/runtime/split_runtime_sync.c) packet.

## Definition Of Done

A new mode is done when all of the following are true:

- The manifest row exists in [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h).
- The generated keycode, flag, and registry row all exist.
- Any needed handler, key handler, and reset function exist, and any new
  per-mode source file is wired into [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk).
- The mode is reachable from at least one physical path, either directly in
  `keymaps[][]` or indirectly as an action emitted from another key behavior or
  combo.
- The authored tap / lock behavior in `key_behaviors[]` matches the intended UX.
- The mode has an RGB overlay color.
- The matching host tests and compile gates for the touched surfaces pass.
- `sh tests/host/run_all_host_tests.sh` passes.
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah` passes.

## Verification Workflow

For normal pd-mode work in this repo, the usual verification set is:

- `sh tests/host/run_pd_mode_tests.sh`
- `sh tests/host/run_pd_mode_handlers_tests.sh`
- `sh tests/host/run_pd_runtime_tests.sh`
- `sh tests/host/run_pointer_layer_policy_tests.sh`
- `sh tests/host/run_split_runtime_sync_tests.sh`

If you changed authored keymap or RGB data as part of exposing the mode, also run:

- `sh tests/host/run_real_profile_validation_tests.sh`

If you changed runtime wiring, source lists, or header boundaries while adding
the mode, also run:

- `sh tests/host/run_feature_gate_compile_tests.sh`

Before handing the work back, finish with:

- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

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

For a normal new motion-transforming mode, the minimum expected diff usually
includes:

- [`users/noah/lib/pointing/defs/pd_mode_manifest.h`](../users/noah/lib/pointing/defs/pd_mode_manifest.h)
- [`users/noah/lib/pointing/modes/pd_mode_handlers.h`](../users/noah/lib/pointing/modes/pd_mode_handlers.h)
- a new per-mode implementation file under [`users/noah/lib/pointing/modes/`](../users/noah/lib/pointing/modes/) such as [`pd_mode_volume.c`](../users/noah/lib/pointing/modes/pd_mode_volume.c)
- [`users/noah/source_manifest.mk`](../users/noah/source_manifest.mk)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c)
- [`keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c)

If your diff reaches into the generic runtime, stop and justify why.

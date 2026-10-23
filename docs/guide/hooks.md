# Hook overrides

The runtime implements QMK's `*_user` hooks as weak defaults in
[`users/noah/hooks.c`](../../users/noah/hooks.c). Defining the same hook
yourself replaces the default, and with it everything the runtime does
there, unless your hook calls the matching `noah_*` helper from
[`noah_runtime.h`](../../users/noah/noah_runtime.h).

Keymap files must not include `noah_runtime.h` (the host checks refuse it),
so an override that keeps the runtime lives in a C file of its own outside the
keymap folder, listed in
[`users/noah/source_manifest.mk`](../../users/noah/source_manifest.mk).
Treat it as firmware work, not profile authoring: a key of your own is a
[custom key](authoring.md#custom-keys) with a shortcut or behaviour, never a
`process_record_user()` case: `noah_process_record_user()` consumes custom
keys, so code after it never sees them.

## Chaining

Call the helper first, then add your own work:

```c
void matrix_scan_user(void) {
    noah_matrix_scan_user();
    // your scan work
}

layer_state_t layer_state_set_user(layer_state_t state) {
    state = noah_layer_state_set_user(state);
    // your layer logic
    return state;
}

bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = noah_rgb_matrix_indicators_advanced_user(led_min, led_max);
    // paint more LEDs
    return painted;
}
```

A filter hook stops when the helper says so. `process_record_user()` has one
more rule: whenever it returns `false`, it first calls
`noah_process_record_user_finalize(keycode, record, false)`, because QMK
then skips `post_process_record_user()`:

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!noah_process_record_user(keycode, record)) {
        noah_process_record_user_finalize(keycode, record, false);
        return false;
    }
    if (keycode == KC_F13) {
        if (record->event.pressed) {
            // your action
        }
        noah_process_record_user_finalize(keycode, record, false);
        return false;
    }
    return true;
}
```

## Replacing a default

To drop the runtime's work in a hook on purpose, say so in the override's
body, naming the helper it no longer calls and why:

```c
void pointing_device_init_user(void) {
    // Replaces noah_pointing_device_init_user(): this keyboard has no auto-mouse.
}
```

`sh tests/host/run_hook_override_tests.sh` fails, naming the file and hook,
when an override of a hook in `hooks.c` neither calls the helpers its default
calls (for `process_record_user()`, the finalize helper too) nor carries that
comment. Where in the override the helper is called is still yours to
get right.

## The hooks

| QMK hook | Helper | What breaks without it |
| --- | --- | --- |
| `keyboard_pre_init_user()` | `noah_keyboard_pre_init_user()` | the keyboard's name (it comes up under QMK's, then reconnects once), and forgetting the saved profile after a settings reset |
| `keyboard_post_init_user()` | `noah_keyboard_post_init_user()` | the saved profile, lighting and the link between the halves |
| `eeconfig_init_user()` | `noah_eeconfig_init_user()` | stopping a macro when the settings are reset |
| `process_record_admit_user()` | `noah_record_admission_admit()` | keys waiting behind an undecided key, and combo outputs' behaviours |
| `pre_process_record_user()` | `noah_pre_process_record_user()` | modifier tracking, each press's source layer, combo origins, and ignoring keys during uninterruptible playback |
| `process_record_user()` | `noah_process_record_user()` | behaviours, layer keys, pointing keys, custom keys and macros |
| `post_process_record_user()` | `noah_post_process_record_user()` | finishing each key: modifier masks, one-shot layers and the pointer layer |
| `matrix_scan_user()` | `noah_matrix_scan_user()` | saving, timed behaviours, macro playback, sending keys that waited, and keeping the halves in step |
| `matrix_slave_scan_user()` | `noah_matrix_slave_scan_user()` | the left half's share of saving (its flash writes); QMK never runs `matrix_scan_user()` there |
| `housekeeping_task_user()` | `noah_housekeeping_task_user()` | repeating holds, the watchdog, reconnecting after a rename and Ark's Restart |
| `suspend_power_down_user()` | `noah_suspend_power_down_user()` | the watchdog while the computer sleeps; without it the right half resets about 2 s after |
| `layer_state_set_user()` | `noah_layer_state_set_user()` | the pointer layer and sniping |
| `pointing_device_init_user()` | `noah_pointing_device_init_user()` | auto-mouse |
| `pointing_device_task_user()` | `noah_pointing_device_task_user()` | pointing modes |
| `is_mouse_record_user()` | `noah_is_mouse_record_user()` | which keys keep the pointer layer on |
| `rgb_matrix_indicators_advanced_user()` | `noah_rgb_matrix_indicators_advanced_user()` | all lighting above the background effect |

`process_record_admit_user()` is a hook of my BK fork, and its helper is
declared in `users/noah/lib/compat/qmk_record_admission.h`. A fork hook
named outside `lib/compat/` fails `run_compat_boundary_tests.sh`, so an
override of this one also goes on that check's allowlist, with its reason.

To keep the pointer layer on for a key of your own, set
`.keeps_auto_mouse_anchored = true` on its behaviour instead of overriding
`is_mouse_record_user()`.

Some QMK functions are defined outright, not weakly, and redefining one fails
to link: `raw_hid_receive()` (every message from Ark),
`keymap_key_to_keycode()` (the keymap comes from the profile), the combo
hooks (`combo_count()`, `combo_get()`, `get_combo_term()` and
`get_combo_must_*()`), `is_tap_record_user()` and `is_tap_keycode_user()`,
the report filters macro playback uses, and in the pair
`is_keyboard_master_impl()` (which half leads) and `is_keyboard_left_impl()`
(which half each file is).

`sh tests/host/run_hook_chaining_tests.sh` checks that the defaults chain;
`sh tests/host/run_hook_override_tests.sh` checks every override (above).

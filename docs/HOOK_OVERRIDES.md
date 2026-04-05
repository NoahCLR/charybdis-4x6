# Hook Overrides

The `noah` userspace ships weak default QMK hooks in
[`users/noah/hooks.c`](../users/noah/hooks.c).
That means a keymap can define the normal QMK hook names in `keymap.c`, and the
keymap-local definition will override the default userspace one.

If you still want the shared `noah` userspace behavior, call the matching
`noah_*` helper from
[`users/noah/noah_runtime.h`](../users/noah/noah_runtime.h).
`keymap.c` already gets those declarations through
[`users/noah/noah_keymap.h`](../users/noah/noah_keymap.h).

## Default Model

If you do nothing, the weak hooks in `users/noah/hooks.c` are used:

- `eeconfig_init_user()`
- `get_hold_on_other_key_press()`
- `process_record_user()`
- `matrix_scan_user()`
- `keyboard_post_init_user()`
- `layer_state_set_user()`
- `pointing_device_task_user()`
- `pointing_device_init_user()`
- `is_mouse_record_user()`
- `rgb_matrix_indicators_advanced_user()`

If you override one of those in your keymap and do not call the matching
`noah_*` helper, you fully replace the shared behavior for that hook.

## Override Patterns

### Void Hooks

Call the shared helper, then add your own logic:

```c
void matrix_scan_user(void) {
    noah_matrix_scan_user();
    // Keymap-specific scan work.
}
```

This pattern applies to:

- `eeconfig_init_user()` -> `noah_eeconfig_init_user()`
- `matrix_scan_user()` -> `noah_matrix_scan_user()`
- `keyboard_post_init_user()` -> `noah_keyboard_post_init_user()`
- `pointing_device_init_user()` -> `noah_pointing_device_init_user()`

### Filter Hooks

Let the shared helper short-circuit first. If it returns `false`, stop.

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!noah_process_record_user(keycode, record)) {
        return false;
    }

    switch (keycode) {
        case MY_CUSTOM_KEY:
            if (record->event.pressed) {
                // Custom action here.
            }
            return false;
        default:
            return true;
    }
}
```

This pattern applies to:

- `process_record_user()` -> `noah_process_record_user()`

For mouse-record classification, let the shared helper claim its keys first:

```c
bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_is_mouse_record_user(keycode, record)) {
        return true;
    }

    return keycode == MY_MOUSE_RELATED_KEY;
}
```

For hold-preference decisions, return `true` as soon as the shared helper wants
hold behavior:

```c
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    if (noah_get_hold_on_other_key_press(keycode, record)) {
        return true;
    }

    return keycode == MY_SPECIAL_MOD_TAP;
}
```

### State-Transform Hooks

Feed the current value through the shared helper, then continue from the
transformed result.

```c
layer_state_t layer_state_set_user(layer_state_t state) {
    state = noah_layer_state_set_user(state);
    // Additional layer logic here.
    return state;
}
```

This pattern applies to:

- `layer_state_set_user()` -> `noah_layer_state_set_user()`

### Mouse-Report Hooks

Transform the mouse report through the shared helper first, then modify the
result if needed.

```c
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    mouse_report = noah_pointing_device_task_user(mouse_report);
    // Additional report edits here.
    return mouse_report;
}
```

This pattern applies to:

- `pointing_device_task_user()` -> `noah_pointing_device_task_user()`

### RGB Hooks

Keep the shared indicators, then return whether anything has been painted so
far.

```c
bool rgb_matrix_indicators_advanced_user(uint8_t led_min, uint8_t led_max) {
    bool painted = noah_rgb_matrix_indicators_advanced_user(led_min, led_max);

    // Paint more LEDs here if needed.

    return painted;
}
```

This pattern applies to:

- `rgb_matrix_indicators_advanced_user()` -> `noah_rgb_matrix_indicators_advanced_user()`

## Available Overrides

| QMK hook | Shared helper | What the helper currently owns |
| --- | --- | --- |
| `eeconfig_init_user()` | `noah_eeconfig_init_user()` | VIA macro seeding after EEPROM init |
| `get_hold_on_other_key_press()` | `noah_get_hold_on_other_key_press()` | shared tap-hold exceptions |
| `process_record_user()` | `noah_process_record_user()` | key behavior engine, pointer-mode keys, macros, direct actions |
| `matrix_scan_user()` | `noah_matrix_scan_user()` | deferred VIA reseed checks, key runtime scanning, and split shared-state sync ticks |
| `keyboard_post_init_user()` | `noah_keyboard_post_init_user()` | deferred VIA reseed checks, RGB runtime init, and split shared-state RPC init |
| `layer_state_set_user()` | `noah_layer_state_set_user()` | pointer-layer policy and sniping state |
| `pointing_device_task_user()` | `noah_pointing_device_task_user()` | pointer-mode mouse-report transforms |
| `pointing_device_init_user()` | `noah_pointing_device_init_user()` | auto-mouse init |
| `is_mouse_record_user()` | `noah_is_mouse_record_user()` | pointer-layer mouse-key classification |
| `rgb_matrix_indicators_advanced_user()` | `noah_rgb_matrix_indicators_advanced_user()` | shared layer, auto-mouse, LED-group, key-feedback, and pointer-mode RGB rendering |

## Custom Keycodes

Keymap-local custom keycodes belong in
[`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
not in `users/noah/noah_keymap.h`:

```c
enum keymap_custom_keycodes {
    KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
    MY_CUSTOM_KEY,
    MY_OTHER_KEY,
};
```

Those keycodes can be handled in `process_record_user()` and also used in
`key_behaviors[]` actions such as `TAP_SENDS(...)`,
`TAP_AT_HOLD_THRESHOLD(...)`, and `PRESS_AND_HOLD_UNTIL_RELEASE(...)`.

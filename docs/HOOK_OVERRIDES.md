# Hook Overrides

This is a maintainer-facing doc about overriding shared userspace hooks without
accidentally dropping shared runtime behavior.

The `noah` userspace ships weak default QMK hooks in
[`users/noah/hooks.c`](../users/noah/hooks.c).
Any stronger definition of the normal QMK hook name will override the weak
userspace one.

If you still want the shared `noah` userspace behavior, call the matching
`noah_*` helper from
[`users/noah/noah_runtime.h`](../users/noah/noah_runtime.h).

One important process-chain detail:

- if your override returns `false` after calling
  `noah_process_record_user(...)`, call
  `noah_process_record_user_finalize(..., false)` before returning because QMK
  will skip `post_process_record_user()` on a final `false`

One important boundary in this repo:

- keymap-owned translation units under `keyboards/.../keymaps/noah/` are
  compile-gated away from including `noah_runtime.h`
- treat hook overrides as deliberate integration work, not ordinary keymap-data
  authoring in `keymap.c` or `rgb_config.c`

## Default Model

If you do nothing, the weak hooks in [`users/noah/hooks.c`](../users/noah/hooks.c) are used:

- `eeconfig_init_user()`
- `get_hold_on_other_key_press()`
- `pre_process_record_user()`
- `process_record_user()`
- `post_process_record_user()`
- `matrix_scan_user()`
- `housekeeping_task_user()`
- `keyboard_post_init_user()`
- `layer_state_set_user()`
- `pointing_device_task_user()`
- `pointing_device_init_user()`
- `is_mouse_record_user()`
- `rgb_matrix_indicators_advanced_user()`

If you override one of those in your keymap and do not call the matching
`noah_*` helper, you fully replace the shared behavior for that hook. The
exception is `get_hold_on_other_key_press()`: the weak default returns `false`
directly because the shared userspace has no hold-preference policy.

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
- `housekeeping_task_user()` -> `noah_housekeeping_task_user()`
- `keyboard_post_init_user()` -> `noah_keyboard_post_init_user()`
- `pointing_device_init_user()` -> `noah_pointing_device_init_user()`

### Filter Hooks

Let the shared helper short-circuit first. If it returns `false`, stop.

For pre-process filters, pass through the shared helper first:

```c
bool pre_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!noah_pre_process_record_user(keycode, record)) {
        return false;
    }

    return true;
}
```

For hold-preference decisions, define the QMK hook directly when you want custom
hold behavior:

```c
bool get_hold_on_other_key_press(uint16_t keycode, keyrecord_t *record) {
    return keycode == MY_SPECIAL_MOD_TAP;
}
```

There is no `noah_get_hold_on_other_key_press()` helper. The shared userspace
does not own hold-preference behavior.

For mouse-record classification, let the shared helper claim its keys first:

```c
bool is_mouse_record_user(uint16_t keycode, keyrecord_t *record) {
    if (noah_is_mouse_record_user(keycode, record)) {
        return true;
    }

    return keycode == MY_MOUSE_RELATED_KEY;
}
```

### Process-Record Hooks

The shared userspace splits record handling across three hooks:

- `pre_process_record_user()` tracks physical-key metadata before the main
  process path runs
- `process_record_user()` owns the main key-runtime, pd-mode, direct-action,
  and macro path
- `post_process_record_user()` finalizes process-return tracing and keyboard
  event modifier masking when the main process path returns `true`

If you override the main process hook, keep the shared return/finalize
contract intact:

```c
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    bool keep_processing = noah_process_record_user(keycode, record);

    if (!keep_processing) {
        noah_process_record_user_finalize(keycode, record, false);
        return false;
    }

    switch (keycode) {
        case MY_CUSTOM_KEY:
            if (record->event.pressed) {
                // Custom action here.
            }
            noah_process_record_user_finalize(keycode, record, false);
            return false;
        default:
            return true;
    }
}
```

If you also override the post-process hook, keep the shared finalize path:

```c
void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    noah_post_process_record_user(keycode, record);
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
| `get_hold_on_other_key_press()` | none | weak default returns `false`; define the QMK hook directly for keymap-specific hold preference |
| `pre_process_record_user()` | `noah_pre_process_record_user()` | physical-key tracking for keyboard modifier ownership before the main process path |
| `process_record_user()` | `noah_process_record_user()` | key behavior engine, pointer-mode keys, non-handled release cleanup, direct actions, macros, and process-entry tracing |
| `post_process_record_user()` | `noah_post_process_record_user()` | process-return finalization, including keyboard-event modifier-mask teardown and process trace result emission |
| `matrix_scan_user()` | `noah_matrix_scan_user()` | VIA macro default reseeding, key runtime scanning, and split shared-state sync ticks |
| `housekeeping_task_user()` | `noah_housekeeping_task_user()` | held-repeat ticking and runtime diagnostic heartbeat/watchdog servicing |
| `keyboard_post_init_user()` | `noah_keyboard_post_init_user()` | VIA macro default seeding, RGB runtime init, and split shared-state init |
| `layer_state_set_user()` | `noah_layer_state_set_user()` | pointer-layer policy, auto-sniping layer state, and queueing scan-time pd-mode DPI policy after layer-owned sniping changes |
| `pointing_device_task_user()` | `noah_pointing_device_task_user()` | pointer-mode mouse-report transforms and pointing idle-noise suppression |
| `pointing_device_init_user()` | `noah_pointing_device_init_user()` | auto-mouse init |
| `is_mouse_record_user()` | `noah_is_mouse_record_user()` | pointer-layer mouse-key classification |
| `rgb_matrix_indicators_advanced_user()` | `noah_rgb_matrix_indicators_advanced_user()` | shared layer, auto-mouse, LED-group, key-feedback, and pointer-mode RGB rendering |

Authored-profile validation is also wired into `users/noah/rules.mk`, so
invalid key behavior, keymap, hardcoded macro, or RGB authored data hard-fails
the firmware build before flashing.

## Verification

If you change hook wiring or override behavior, run:

- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Custom Keycodes

Keymap-local custom keycodes belong in
[`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
not in [`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h):

```c
enum keymap_custom_keycodes {
    KEYMAP_CUSTOM_KEYCODE_SENTINEL = NOAH_KEYMAP_SAFE_RANGE - 1,
    MY_CUSTOM_KEY,
    MY_OTHER_KEY,
};
```

Those keycodes can be handled in `process_record_user()` and also used in
`key_behaviors[]` actions such as `TAP_SENDS(...)`,
`TAP_AT_HOLD_THRESHOLD(...)`, `TAP_ON_RELEASE_AFTER_HOLD(...)`,
`REPEAT_WHILE_HELD(...)`, and
`PRESS_AND_HOLD_UNTIL_RELEASE(...)`.

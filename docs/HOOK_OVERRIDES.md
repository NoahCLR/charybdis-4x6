# Hook Overrides

This is a maintainer-facing doc about overriding shared userspace hooks without
accidentally dropping shared runtime behavior.

For the full hook lifecycle and ownership map, see
[architecture/runtime-flow.md](./architecture/runtime-flow.md) and
[architecture/README.md](./architecture/README.md).

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
- `process_record_admit_user()`, the BK fork's admission hook
- `pre_process_record_user()`
- `process_record_user()`
- `post_process_record_user()`
- `matrix_scan_user()`
- `matrix_slave_scan_user()` on split keyboards
- `housekeeping_task_user()`
- `suspend_power_down_user()`
- `keyboard_pre_init_user()`
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

On a split keyboard, QMK does not call `matrix_scan_user()` on the slave. If a
strong override replaces the slave hook, it must chain independently:

```c
void matrix_slave_scan_user(void) {
    noah_matrix_slave_scan_user();
    // Slave-specific scan work.
}
```

This pattern applies to:

- `eeconfig_init_user()` -> `noah_eeconfig_init_user()`
- `matrix_scan_user()` -> `noah_matrix_scan_user()`
- `matrix_slave_scan_user()` -> `noah_matrix_slave_scan_user()`
- `housekeeping_task_user()` -> `noah_housekeeping_task_user()`
- `suspend_power_down_user()` -> `noah_suspend_power_down_user()`
- `keyboard_pre_init_user()` -> `noah_keyboard_pre_init_user()`
- `keyboard_post_init_user()` -> `noah_keyboard_post_init_user()`
- `pointing_device_init_user()` -> `noah_pointing_device_init_user()`

`keyboard_pre_init_user()` runs before USB starts and serves the saved
keyboard name as the USB product string (D-F25). An override that skips the
helper enumerates under the built-in name, then restarts USB once after boot
to take up the saved one.

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

For mouse-record classification, a keymap in this repo does not need this hook
at all, and cannot use the chaining pattern anyway: chaining requires
`noah_is_mouse_record_user()` from `noah_runtime.h`, and keymap-owned
translation units are gated away from that header. Claim the key from its
authored `key_behaviors[]` row instead:

```c
{
    .keycode                   = MY_MOUSE_RELATED_KEY,
    .keeps_auto_mouse_anchored = true,
    .tap_counts = {...},
}
```

`pointer_layer_policy_is_mouse_record()` reads that flag alongside its own
rules, so the shared classification stays intact. See
[POINTER_MODES.md](./POINTER_MODES.md) for what the flag buys.

If a translation unit outside the keymap directory does need to override the
hook, let the shared helper claim its keys first:

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

Before them, at the top of QMK's `process_record`, the BK fork's
`process_record_admit_user()` decides whether a record runs now. Its shared
helper, `noah_record_admission_admit()`, holds records back behind an
undecided runtime-owned LT/MT/OSM key and attaches each combo output's
behaviour permission; an override that drops it breaks both. The helper is
declared in `users/noah/lib/compat/qmk_record_admission.h`, not
`noah_runtime.h`, so only a translation unit outside the keymap can chain it,
and it must ask the helper first:

```c
bool process_record_admit_user(keyrecord_t *record) {
    if (!noah_record_admission_admit(record)) {
        return false;
    }

    return true;
}
```

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
        case KC_F13:
            if (record->event.pressed) {
                // Keymap-specific action here.
            }
            noah_process_record_user_finalize(keycode, record, false);
            return false;
        default:
            return true;
    }
}
```

Custom keys never reach that `switch`: the shared path owns them, and consumes
one with neither a shortcut nor a behaviour row. Give a key of your own a
shortcut or a behaviour row instead; see
[Custom Keys](#custom-keys).

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
| `eeconfig_init_user()` | `noah_eeconfig_init_user()` | cancelling a running macro on EEPROM reset (the saved profiles are forgotten at boot, D-F29) |
| `get_hold_on_other_key_press()` | none | weak default returns `false`; define the QMK hook directly for keymap-specific hold preference |
| `pre_process_record_user()` | `noah_pre_process_record_user()` | physical-key tracking for keyboard modifier ownership before the main process path |
| `process_record_user()` | `noah_process_record_user()` | key behavior engine, pointer-mode keys, non-handled release cleanup, direct actions, macros, and process-entry tracing |
| `post_process_record_user()` | `noah_post_process_record_user()` | process-return finalization, including keyboard-event modifier-mask teardown and process trace result emission |
| `matrix_scan_user()` | `noah_matrix_scan_user()` | one scan-owned durable-I/O grant for the profile store, key runtime scanning, and split shared-state sync ticks |
| `matrix_slave_scan_user()` | `noah_matrix_slave_scan_user()` | the slave half's durable-I/O grant, including live-profile initialization and the profile split receiver mailbox; QMK never routes the slave through `matrix_scan_user()` |
| `housekeeping_task_user()` | `noah_housekeeping_task_user()` | held-repeat ticking, watchdog refresh, and runtime boot-indicator expiry |
| `suspend_power_down_user()` | `noah_suspend_power_down_user()` | watchdog refresh while the host has USB suspended; QMK's suspend loop never reaches `housekeeping_task()`, so an override that drops this chain lets the watchdog reset the master about 2 s after the host sleeps |
| `keyboard_post_init_user()` | `noah_keyboard_post_init_user()` | the settings apply hook, profile-store discovery setup, durable-I/O scheduler setup, RGB runtime init, and split shared-state init |
| `layer_state_set_user()` | `noah_layer_state_set_user()` | pointer-layer policy, auto-sniping layer state, and queueing scan-time pd-mode DPI policy after layer-owned sniping changes |
| `pointing_device_task_user()` | `noah_pointing_device_task_user()` | pointer-mode mouse-report transforms and pointing idle-noise suppression |
| `pointing_device_init_user()` | `noah_pointing_device_init_user()` | auto-mouse init |
| `is_mouse_record_user()` | `noah_is_mouse_record_user()` | pointer-layer mouse-key classification |
| `rgb_matrix_indicators_advanced_user()` | `noah_rgb_matrix_indicators_advanced_user()` | shared layer, auto-mouse, LED-group, key-feedback, and pointer-mode RGB rendering |

### Keyboard-level hooks defined outside `hooks.c`

These are not weak `*_user` hooks, so a keyboard-level definition replaces them
outright with no chaining helper and no compile gate to notice.

| QMK hook | Defined in | What it owns |
| --- | --- | --- |
| `raw_hid_receive()` | `users/noah/lib/compat/qmk_profile_channel.c` | every Raw HID report: Profile Wire dispatch and its reply (D-F31); replacing it cuts Charybdis Ark off the keyboard |
| `is_keyboard_master_impl()` | `users/noah/lib/compat/split_role.c` | forced split role under `FORCE_MASTER`/`FORCE_SLAVE`; empty otherwise |
| `is_keyboard_left_impl()` | `users/noah/lib/compat/qmk_physical_half.c` | flash-provisioned handedness under `NOAH_PHYSICAL_HALF=left/right`; empty for generic firmware |

Authored-profile validation is also wired into `users/noah/rules.mk`, so
invalid key behavior, keymap, or RGB authored data hard-fails
the firmware build before flashing.

## Verification

If you change hook wiring or override behavior, run:

- `sh tests/host/run_hook_chaining_tests.sh`
- `sh tests/host/run_runtime_init_order_tests.sh`
- `sh tests/host/run_feature_gate_compile_tests.sh`
- `sh tests/host/run_all_host_tests.sh`
- `qmk compile -kb bastardkb/charybdis/4x6 -km noah`

## Custom Keys

A key of your own is a custom key, not a new keycode or a
`process_record_user()` handler. The userspace reserves `CUSTOM_KEY_0` through
`CUSTOM_KEY_127` (`0x7f00`–`0x7f7f`) in
[`users/noah/noah_keymap_ids.h`](../users/noah/noah_keymap_ids.h). It gives
each userspace family one fixed, aligned block, so adding pointing modes or
layers never moves another keycode:

| Keycodes | Range | Reserved |
| --- | --- | --- |
| retired (custom keys before D-F14) | `0x7e40`–`0x7e7f` | inert |
| `PD_SLOT_n` (pointing hold) | `0x7e80`–`0x7e9f` | 32 |
| `PD_SLOT_n_LOCK` | `0x7ea0`–`0x7ebf` | 32 |
| `LOCK_LAYER(n)` | `0x7ec0`–`0x7edf` | 32 (16 used) |
| unassigned | `0x7ee0`–`0x7eff` | |
| `CUSTOM_KEY_0`–`127` | `0x7f00`–`0x7f7f` | 128 |
| unassigned | `0x7f80`–`0x7fff` | |

Name the key in the `CUSTOM_KEYS(KEY, SHORTCUT)` table in
[`keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c`](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c),
one row per slot (at most 32 bytes of UTF-8, `""` for an unused slot). A
`SHORTCUT` row also gives it a shortcut, its own action (D-F26): one key plus
any modifiers, left and right mixed, held while the custom key is held. Then
give it more as a `key_behaviors[]` row, which works on top of the shortcut:

```c
#define CUSTOM_KEYS(KEY, SHORTCUT)                                              \
    KEY(CUSTOM_KEY_0, "Right Thumb")                                            \
    KEY(CUSTOM_KEY_1, "My Key")                                                 \
    SHORTCUT(CUSTOM_KEY_2, "Hyper K", SHORTCUT_MODS(KC_LCTL, KC_RALT), KC_K)    \
    ...

{
    .keycode = CUSTOM_KEY_1,
    .tap_counts =
        {
            [0] = {.tap = TAP_SENDS(KC_MPLY), .hold = PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))},
        },
},
```

Place the key on a layer or make it a combo output. Without a shortcut or a
behaviour row it does nothing: the shared process path consumes its press, so
it never reaches a `process_record_user()` override. A behaviour step
(`TAP_SENDS(...)`, `PRESS_AND_HOLD_UNTIL_RELEASE(...)` and the other helpers)
on another key may send a custom key; it sends that key's shortcut only, never
its behaviour. Charybdis Ark edits the same keys on its **Custom keys** screen;
names and shortcuts it saves are stored on the keyboard, and a keyboard with
none stored reports those in `CUSTOM_KEYS`.

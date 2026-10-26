# Field classification

What every value in the authored profile files becomes: profile data a client
can change without a reflash, or something only a new build changes. The
authored files are the keymap folder's `keymap.c`, `rgb_config.c`,
`config.h` and `pd_config.c`
(`keyboards/bastardkb/charybdis/4x6/keymaps/noah/`). Each profile-data row
names the domain that holds it; the byte layouts are the domain files'
([Profile Wire](profile-wire-v1/README.md#layers-and-files),
[lighting](rgb-domain-v1.md), [pointing](pd-mode-domain-v1.md)). Where a
profile is stored and its ceilings are the
[device-resident profile](device-resident-profile.md)'s; what a backup holds
is the [portable profile](portable-profile-v1.md)'s.

## Classes

| Class | Meaning | Changed by |
| --- | --- | --- |
| Profile data | Held in a profile domain; the authored value is its compiled default | a live edit or an Apply |
| Capability | A feature the build includes or leaves out | a reflash |
| Ceiling | A capacity or safety bound live values must stay within | a reflash |
| Source-only | Exists only in the source: names, comments, code | a reflash |

Profile data cannot introduce a capability the build lacks: a profile may
switch off a lighting stage the build compiled, never switch on one it did not
([lighting](rgb-domain-v1.md)). A ceiling bounds live values and cannot be
raised live. The compiled defaults hold every authored profile-data value
(D-F12, [ADR](../adr/0012-one-complete-compiled-profile.md)); a profile that
omits a domain runs the compiled defaults' domain.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `folded_keyboard_profile_test.py`), `tests/fixtures/compiled_profile_pd_v2.fixture`, `tests/fixtures/folded_keyboard_profile.json`.

## `keymap.c`

| Authored | Class | Where it lives |
| --- | --- | --- |
| `keymaps[][][]` | profile data | keymap `0x60`, every layer of the bank |
| `layer_names[]` | profile data | settings `0x40`, layer names 0–15 |
| `CUSTOM_KEYS`: each row's name | profile data | settings `0x40`, custom-key names |
| `CUSTOM_KEYS`: a `SHORTCUT` row's modifiers and key | profile data | settings `0x40`, custom-key shortcuts (D-F26, [ADR](../adr/0026-custom-key-carries-a-shortcut.md)) |
| `MACROS`: each row's payload | profile data | macros `0x70`; generated in the bank encoding as `authored_macro_bank.h` |
| `MACROS`: each row's name | profile data | settings `0x40`, macro names (D-F43, [ADR](../adr/0043-macros-have-names-user-macros-retired.md)) |
| `COMBOS`: each row's inputs and output | profile data | combos `0x30`, a row |
| `COMBOS`: `COMBO_WINDOW`'s window; a `COMBO` row follows the default window | profile data | combos `0x30`, the row's window (`0` follows the default) |
| `key_behaviors[]`: `.keycode` | profile data | key behaviours `0x20`, the row's target |
| `key_behaviors[]`: `.tap_hold_term`, `.longer_hold_term`, `.multi_tap_term` | profile data | the row's thresholds (`0` follows the settings) |
| `key_behaviors[]`: `.keeps_auto_mouse_anchored` | profile data | the row's flag bit 0 |
| `key_behaviors[]`: `tap_counts[]` tap, hold and long hold, their hold style and repeat rate | profile data | the row's steps |
| The 128 custom-key slots, the 128 macro slots, the userspace keycode blocks | ceiling | the action-ABI digest ([blob and digests](profile-wire-v1/blob-and-digests.md#action-abi-digest)); D-F51, [ADR](../adr/0051-userspace-keycodes-in-fixed-blocks.md) |
| `LAYER_*`, `MACRO_n`, `CUSTOM_KEY_n` names in C | source-only | the profile stores numbers |

The compiled defaults enable every authored behaviour and combo on every
layer of the bank, with no placement switched off; disabling, the allowed
layers and the placement bitmaps exist only in profiles
([participation policy](participation-policy.md)).

Real-profile validation holds the authored file to its own rules: macro and
custom-key names are printable ASCII (`0x20`–`0x7e`), stricter than the
settings domain's UTF-8 names,
layer names and the keyboard name are UTF-8 of at most 32 bytes without C0
controls or DEL, every
keymap position is a keycode the keymap may hold, and `authored_macro_bank.h`
equals `MACROS()` encoded by the firmware's macro compiler.

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`, `folded_keyboard_profile_test.py`), `tests/host/run_real_profile_validation_tests.sh` (`real_profile_validation_test.c`).

## `rgb_config.c`

Every table is lighting `0x10` profile data when its stage is compiled:

| Authored | Lighting domain section | Stage compiled by |
| --- | --- | --- |
| `layer_colors[]`: layer, HSV, mode | layer colours | always, with RGB Matrix |
| `RGB_SAVED_LED_GROUP_TABLE`: reusable groups | reusable groups: id and LED bitmap | always |
| `RGB_LED_GROUP_TABLE` of layer group rows | layer group rows: selector, HSV, group | always |
| `automouse_fade_end_config`: mode, end HSV | auto-mouse fade | `RGB_AUTOMOUSE_GRADIENT_ENABLE` |
| `pointing_mode_colors[]`: slot, HSV, locality | pointing colours, one per pointing slot | `RGB_POINTING_MODE_FEEDBACK_ENABLE` |
| pointing group rows | pointing group rows: selector, HSV, group | `RGB_POINTING_MODE_FEEDBACK_ENABLE` |
| `combo_feedback_colors`: HSV, locality | combo feedback | `RGB_COMBO_FEEDBACK_ENABLE` |
| combo group rows | combo group rows: HSV, group | `RGB_COMBO_FEEDBACK_ENABLE` |
| `key_behavior_feedback_colors`: `RGB_TAP_BRANCH_COLORS` | tap-branch colours, one per tap count past the first | `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` |
| `key_behavior_feedback_colors`: committed, hold and long-hold colours, tap-commit mode, locality | key feedback | `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` |
| key-feedback group rows: semantic, HSV, group | key-feedback group rows | `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` |
| which stages run | the stage-enable mask; the compiled defaults enable every compiled stage | |

`RGB_LED_GROUP_*` macro names are source-only: the profile stores numeric
group ids and LED bitmaps. A new lighting value takes effect at the frame
after the profile holding it becomes active.

Held by: `tests/host/run_real_profile_validation_tests.sh` (`real_profile_validation_test.c`, the default and `no_rgb_feedback` variants), `tests/host/run_rgb_validation_tests.sh`, `tests/host/run_profile_rgb_v4_tests.sh`, `tests/host/run_profile_compiled_defaults_v1_tests.sh` (the compiled lighting domain's shape).

## `config.h`

| Authored | Class | Where it lives |
| --- | --- | --- |
| `NOAH_LAYER_BANK_COUNT` (16) | ceiling | the layer bank (D-F14, [ADR](../adr/0014-bigger-profiles.md)); part of the action-ABI digest |
| `TAPPING_TERM` | profile data | settings scalar 0; the combo domain's hold threshold default |
| `COMBO_TERM` | profile data | the combo domain's default window |
| `CUSTOM_TAP_HOLD_TERM`, `CUSTOM_LONGER_HOLD_TERM`, `CUSTOM_MULTI_TAP_TERM` | profile data | settings scalars 1–3 |
| `KEY_BEHAVIOR_MAX_TAP_COUNT` | ceiling | the tap depth ([key behaviours](profile-wire-v1/key-behavior-domain.md#ceilings)) |
| `CHARYBDIS_MINIMUM_DEFAULT_DPI`, `CHARYBDIS_MINIMUM_SNIPING_DPI` | profile data | settings scalars 18 and 19 |
| `CHARYBDIS_DEFAULT_DPI_CONFIG_STEP`, `CHARYBDIS_SNIPING_DPI_CONFIG_STEP` | capability | QMK's DPI ladders; the settings domain accepts 400–3,400 in steps of 200 and 100–400 in steps of 100 |
| `CHARYBDIS_DRAGSCROLL_DPI`, `POINTING_MODE_VOLUME_DPI`, `POINTING_MODE_BRIGHTNESS_DPI`, `POINTING_MODE_ZOOM_DPI`, `POINTING_MODE_ARROW_DPI` | profile data | pointing slots' DPI, through `pd_config.c` |
| `CHARYBDIS_AUTO_SNIPING_ENABLE` | capability | |
| `CHARYBDIS_AUTO_SNIPING_LAYER` | profile data | settings scalar 9, the one layer of the compiled auto-sniping layers |
| `POINTING_DEVICE_AUTO_MOUSE_ENABLE` | capability | |
| `AUTO_MOUSE_DEFAULT_LAYER`, `AUTO_MOUSE_TIME` | profile data | settings scalars 5 and 6 |
| `AUTO_MOUSE_DELAY`, `AUTO_MOUSE_THRESHOLD` | profile data | settings scalars 25 and 26; the keymap redefines both to read them |
| `RGB_MATRIX_DEFAULT_MODE`, `_HUE`, `_SAT`, `_VAL`, `_SPD` | profile data | settings scalars 21 and 22 |
| `RGB_MATRIX_MAXIMUM_BRIGHTNESS` | ceiling | QMK clamps brightness to it; GET `0x08` page 1 reports it (D-F38, [ADR](../adr/0038-keyboard-reports-its-brightness-limit.md)) |
| `RGB_MATRIX_LED_FLUSH_LIMIT` | ceiling | the shortest interval between LED updates, and the auto-mouse fade's step |
| `NOAH_RGB_IDLE_TIMEOUT_MS` | profile data | settings scalar 17; QMK's `RGB_MATRIX_TIMEOUT` is `0` in builds that save |
| `RGB_POINTING_MODE_FEEDBACK_ENABLE`, `RGB_COMBO_FEEDBACK_ENABLE`, `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`, `RGB_AUTOMOUSE_GRADIENT_ENABLE` | capability | the lighting stages the build compiles |
| `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS` | profile data | settings scalar 15 |
| `AUTOMOUSE_RGB_DEAD_TIME` | profile data | settings scalar 16 |
| `NOAH_DEFAULT_HOST_OS`, `NOAH_DEFAULT_HOST_LAYOUT`, `NOAH_DEFAULT_UNICODE_PLAYBACK`, `NOAH_DEFAULT_MACOS_ISO` | profile data | settings scalar 27 |
| `NOAH_KEYBOARD_NAME` | profile data | settings `0x40`, the keyboard name (D-F25, [ADR](../adr/0025-keyboard-name-is-usb-product-string.md)) |

Settings no authored macro sets take the fixed compiled defaults the
[settings domain](profile-wire-v1/settings-domain.md#scalars) lists: QMK's
`AUTO_MOUSE_DEBOUNCE` and lighting on and LED flags, the key options, the
default layers, and the participation masters and masks.

Held by: `tests/host/run_qmk_portable_profile_tests.sh` (`qmk_portable_profile_test.c`), `tests/host/run_profile_settings_v1_tests.sh` (`profile_settings_v1_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`).

## `pd_config.c`

| Authored | Class | Where it lives |
| --- | --- | --- |
| `noah_pd_defaults[]`: each slot's name, movement, pointer-layer policy, axis, DPI, held modifiers, thresholds, directional outputs, link, scroll tuning | profile data | pointing `0x50`; a disabled slot without a name is not stored |
| `NOAH_DRAGSCROLL_*` in `users/noah/config.h`, the thresholds in `pointing_mode_defaults.h` | profile data | the scrolling and directional slots' fields, through `noah_pd_defaults[]` |
| The 32 pointing slots | ceiling | part of the action-ABI digest (D-F09, [ADR](../adr/0009-thirty-two-pointing-slots-stored-sparsely.md)) |
| The scrolling and directional movement code, its per-scan output limits | capability | |

Keys reach a slot by its number, so a slot keeps its keys when its mode
changes (D-F50, [ADR](../adr/0050-pointing-keycodes-name-slots.md)).

Held by: `tests/host/run_profile_compiled_defaults_v1_tests.sh` (the compiled pointing domain equals the `presets` vector), `tests/fixtures/pd_mode_domain_v4.json`.

## Ceilings outside the authored files

The build fixes these; profile data stays within them and the capability
pages report the ones a client needs ([HID channel](profile-wire-v1/hid-channel.md#capability-pages--get-0x01)):

- the matrix (10 rows of 6, 60 positions), the 58 LEDs and their split;
- the profile's table sizes and the 130,048-byte blob
  ([device-resident profile](device-resident-profile.md));
- the repeat-rate limit, 1–100 Hz;
- the action vocabulary and schema versions, named by the action-ABI digest;
- QMK's EEPROM layout, which holds no profile data
  ([device-resident profile](device-resident-profile.md)).

Held by: `tests/host/run_profile_validator_v1_tests.sh` (`profile_validator_v1_test.c`), `tests/host/run_key_behavior_domain_v1_tests.sh` (`key_behavior_domain_v1_test.c`), `tests/host/run_profile_wire_v1_tests.sh` (`profile_wire_v1_test.c`).

## Source-only

Comments, whitespace and C macro names; helper code and QMK hooks; USB
descriptors other than the product string; the vendor and product ids; the
Raw HID report size; split transaction ids; pins, drivers, the sensor and the
matrix wiring. Changing any of them needs a source change and a build.

Held by: no test.

## How profile data names things

The profile stores what a value names, not the C spelling of it
([action encoding](profile-wire-v1/blob-and-digests.md#action-encoding)):
layers by index, pointing slots by slot number, macros by slot and custom
keys by number, each as a semantic action kind with a bounded operand, and
every other key as its QMK keycode. A QMK keycode means what it means in the
firmware whose action-ABI digest the profile was validated under. An unknown
action kind, a nonzero reserved bit, an operand past its kind's count or
another action-ABI digest refuses the whole candidate.

Held by: `tests/host/run_profile_blob_v1_tests.sh` (`profile_blob_v1_test.c`), `tests/host/run_profile_compiled_defaults_v1_tests.sh` (`profile_compiled_defaults_v1_test.c`).

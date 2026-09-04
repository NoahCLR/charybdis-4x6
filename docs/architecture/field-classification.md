# Live-Profile Field Classification

This matrix classifies every surface currently parsed by Profile Studio. It is
the starting scope boundary between live data, standard VIA state, compiled
ceilings, and executable firmware.

The first-grade product goal requires this to become a complete inventory of
every user-relevant value represented by the three authoring files, including
fields that Studio does not parse yet. A missing field is not implicitly out of
scope: it must be added here and classified as live data, runtime-applicable
policy, bounded structure, compiled capability/safety, or executable/source-only
behavior. The UI must explain every intentional non-live boundary.

## `keymap.c`

| Surface | Classification | Initial stage | Runtime or validation owner |
| --- | --- | ---: | --- |
| `key_behaviors[].keycode` | Milestone A behavior identity | 04 | profile behavior validator/provider |
| Row timing overrides | Milestone A behavior policy | 04 | profile behavior validator/provider |
| `keeps_auto_mouse_anchored` | Milestone A behavior policy | 04 | key runtime and auto-mouse bridge |
| Tap-count index and presence | Milestone A behavior policy | 04 | behavior validator/provider |
| Tap action | Milestone A behavior action | 04 | action-schema validator and key runtime |
| Hold/long-hold action | Milestone A behavior action | 04 | action-schema validator and key runtime |
| Hold mode and repeat rate | Milestone A behavior policy | 04 | behavior validator and lifecycle runtime |
| `keymaps[][]` | Standard VIA-owned state; C is compiled default | 06 | QMK dynamic keymap/VIA reconciliation |
| `VIA_MACROS` | Standard VIA-owned state; C is compiled default | 06 | QMK dynamic macro storage |
| `HARDCODED_MACROS` | Later custom live domain | 06 | macro provider and profile schema |
| `COMBOS` | Later live structural data | 07 | combo validator/provider |
| Logical layer names/order | Later fixed-capacity structure | 07 | whole-profile cross-reference validator |
| Custom-keycode enum and handler code | Executable firmware/action ABI | flash required | firmware build |

## `rgb_config.c`

Every row below is Milestone A data when its feature is compiled. A live
enable bit may disable a compiled stage, but live data cannot introduce a stage
that firmware did not compile.

| Surface | Semantic fields | Cache or lifecycle invalidation |
| --- | --- | --- |
| Layer colors | layer id, HSV, render mode | rebuild derived layer colors/maps at frame boundary |
| Layer LED groups | selector, inherit/literal HSV, reusable group id | rebuild inactive RGB cache |
| Auto-mouse fade | mode, end HSV | rebuild inactive automouse cache |
| PD-mode colors | stable mode id, HSV, locality | rebuild inactive PD cache |
| PD-mode LED groups | selector, inherit/literal HSV, group id | rebuild inactive PD cache |
| Combo feedback | HSV, locality | rebuild inactive combo cache |
| Combo LED groups | inherit/literal HSV, group id | rebuild inactive combo cache |
| Tap-branch feedback colors | ordered HSV list | rebuild inactive key-feedback cache |
| Tap-committed/hold/long-hold colors | HSV values | rebuild inactive key-feedback cache |
| Tap-commit policy | enum | strict activation boundary because key planning reads it |
| Key-feedback locality | enum | RGB frame boundary |
| Key-feedback LED groups | semantic, inherit/literal HSV, group id | rebuild inactive key-feedback cache |
| Reusable LED groups | stable profile-local id and 58-bit bitmap | rebuild every referring stage cache |
| Stage enable states | one bit per compiled RGB stage | RGB frame boundary unless field affects key planning |

Source macro names and formatting are source-only metadata. The device stores
profile-local numeric group ids and LED bitmaps, not C preprocessor names.

## `config.h`

### Later Live Policy

These values are parsed today and are candidates for Stage 06 after Milestone A:

- `TAPPING_TERM`, `COMBO_TERM`, `CUSTOM_TAP_HOLD_TERM`,
  `CUSTOM_LONGER_HOLD_TERM`, and `CUSTOM_MULTI_TAP_TERM`;
- normal and sniping DPI defaults and steps;
- pointing-mode DPI overrides;
- auto-sniping enable/layer;
- auto-mouse enable, layer, and timeout;
- default RGB effect, HSV value, and idle timeout;
- key-feedback flash period and auto-mouse RGB dead time.

Each requires an existing QMK setter, a userspace provider seam, or an explicit
fork compatibility wrapper. Until Stage 06 migrates a field, it remains a
compiled default.

### Compiled Capability Or Feature Inclusion

- `POINTING_DEVICE_AUTO_MOUSE_ENABLE`
- `RGB_PD_MODE_FEEDBACK_ENABLE`
- `RGB_COMBO_FEEDBACK_ENABLE`
- `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE`
- `RGB_AUTOMOUSE_GRADIENT_ENABLE`
- VIA, combo, RGB Matrix, pointing-device, and split feature gates

Milestone A adds runtime enable bits for compiled RGB feedback stages. Turning
on a feature omitted from the firmware still requires flashing.

### Compiled Capacity Ceiling

- `LAYER_COUNT` and `DYNAMIC_KEYMAP_LAYER_COUNT`
- `KEY_BEHAVIOR_MAX_TAP_COUNT`
- dynamic macro count
- RGB Matrix LED count and split geometry
- profile row, group, action, and payload limits

### Compiled Safety Or Resource Ceiling

- `RGB_MATRIX_MAXIMUM_BRIGHTNESS`
- `RGB_MATRIX_LED_FLUSH_LIMIT`
- wear-level backing size and EEPROM partition
- physical SRAM-bank, linked-section, allocator, stack, and regression-policy
  budgets
- repeat-rate maximum
- fixed action and schema compatibility rules

Safety ceilings can bound live values but cannot themselves be raised live.

### Source-Only Or Executable

- comments, labels, whitespace, macro names, and layer presentation names;
- C helper implementations and QMK hooks;
- USB descriptors, VID/PID, Raw HID report size, and split transaction ids;
- hardware pins, drivers, sensor type, and matrix geometry.

These require source changes and a firmware build.

## Canonical Identity Rules

- Standard QMK keycodes may use a 16-bit operand only when the stored
  action-ABI digest matches the connected firmware.
- Layer, PD-mode, VIA-macro, hardcoded-macro, and userspace-owned actions use
  semantic action kinds plus bounded operands. They are not persisted as raw
  custom-keycode enum values.
- PD identities come from the manifest's stable semantic id, not its current
  bit position.
- Layer references use logical layer ids validated against the candidate's
  complete layer set.
- Unknown action kinds, ids, reserved bits, or incompatible action-ABI digests
  reject the entire candidate.

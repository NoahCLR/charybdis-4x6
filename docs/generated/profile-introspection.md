<!-- Generated file. Do not edit by hand. -->
# Profile Introspection
This report is generated from the authored profile files [keymap.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c), [config.h](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h), and [rgb_config.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c). The renderer is board-specific to the Charybdis 4x6 and derives the current `LAYOUT()` slot order directly from [keymap.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).
## Summary

| Field | Value |
| --- | --- |
| `layer_count` | `5` |
| `layout_key_count` | `56` |
| `key_behavior_count` | `33` |
| `key_behavior_step_count` | `46` |
| `combo_count` | `2` |
| `via_macro_count` | `16` |
| `via_macro_non_empty_count` | `10` |
| `hardcoded_macro_count` | `16` |
| `hardcoded_macro_non_empty_count` | `0` |
| `keymap_custom_keycode_count` | `3` |
| `pd_mode_color_count` | `6` |

### Keymap Config

| Macro | Value |
| --- | --- |
| `TAPPING_TERM` | `200` |
| `COMBO_TERM` | `50` |
| `KEY_BEHAVIOR_MAX_TAP_COUNT` | `5` |
| `CUSTOM_TAP_HOLD_TERM` | `150` |
| `CUSTOM_LONGER_HOLD_TERM` | `400` |
| `CUSTOM_MULTI_TAP_TERM` | `150` |
| `CHARYBDIS_DRAGSCROLL_DPI` | `100` |
| `PD_MODE_VOLUME_DPI` | `0` |
| `PD_MODE_BRIGHTNESS_DPI` | `0` |
| `PD_MODE_ZOOM_DPI` | `400` |
| `PD_MODE_ARROW_DPI` | `400` |
| `CHARYBDIS_MINIMUM_DEFAULT_DPI` | `800` |
| `CHARYBDIS_DEFAULT_DPI_CONFIG_STEP` | `200` |
| `CHARYBDIS_MINIMUM_SNIPING_DPI` | `200` |
| `CHARYBDIS_SNIPING_DPI_CONFIG_STEP` | `100` |
| `CHARYBDIS_AUTO_SNIPING_LAYER` | `LAYER_NAV` |
| `AUTO_MOUSE_DEFAULT_LAYER` | `LAYER_POINTER` |
| `AUTO_MOUSE_TIME` | `1200` |
| `RGB_MATRIX_DEFAULT_MODE` | `RGB_MATRIX_SOLID_COLOR` |
| `RGB_MATRIX_DEFAULT_HUE` | `0` |
| `RGB_MATRIX_DEFAULT_SAT` | `255` |
| `RGB_MATRIX_MAXIMUM_BRIGHTNESS` | `200` |
| `RGB_MATRIX_DEFAULT_VAL` | `RGB_MATRIX_MAXIMUM_BRIGHTNESS` |
| `RGB_MATRIX_LED_FLUSH_LIMIT` | `32` |
| `RGB_MATRIX_TIMEOUT` | `900000` |
| `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS` | `200` |
| `AUTOMOUSE_RGB_DEAD_TIME` | `(AUTO_MOUSE_TIME/3)` |

### Authored Sources

| File | Authored Surface |
| --- | --- |
| [keymap.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | custom keycodes, macro tables, combos, key behaviors, and current `LAYOUT()` layer contents |
| [config.h](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing, RGB defaults, and keymap-facing feature config |
| [rgb_config.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pd-mode colors, and key-behavior feedback colors |

### Layer RGB Config

| Layer | RGB Matrix Render Mode | Authored HSV | Preview Color |
| --- | --- | --- | --- |
| `LAYER_BASE` | `ALL_KEYS` | `HSV(0, 0, 0)` | <img alt="LAYER_BASE preview color" src="./profile-introspection-assets/profile-color-swatch-ff0000.svg" width="96" height="28" /> |
| `LAYER_NUM` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(85, 255, 200)` | <img alt="LAYER_NUM preview color" src="./profile-introspection-assets/profile-color-swatch-00ff00.svg" width="96" height="28" /> |
| `LAYER_SYM` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(169, 255, 200)` | <img alt="LAYER_SYM preview color" src="./profile-introspection-assets/profile-color-swatch-0006ff.svg" width="96" height="28" /> |
| `LAYER_NAV` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(180, 255, 200)` | <img alt="LAYER_NAV preview color" src="./profile-introspection-assets/profile-color-swatch-3c00ff.svg" width="96" height="28" /> |
| `LAYER_POINTER` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(0, 0, 150)` | <img alt="LAYER_POINTER preview color" src="./profile-introspection-assets/profile-color-swatch-ffffff.svg" width="96" height="28" /> |

### Shared Keycode Surfaces

- Layers: `LAYER_BASE`, `LAYER_NUM`, `LAYER_SYM`, `LAYER_NAV`, `LAYER_POINTER`
- Keymap-local custom keycodes: `RIGHT_THUMB`, `LEFT_THUMB`, `CLICK_SPAM`
- PD color overlays: `PD_MODE_DRAGSCROLL`, `PD_MODE_VOLUME`, `PD_MODE_BRIGHTNESS`, `PD_MODE_ARROW`, `PD_MODE_PINCH`, `PD_MODE_ZOOM`

## Layer Images

These previews are generated as SVG image assets under [profile-introspection-assets/](profile-introspection-assets). The renderer uses the authored `layer_colors[]` config from [rgb_config.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) and the current `LAYOUT()` slot order from [keymap.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):

- `ALL_KEYS`: tint every physical key with the layer color
- `KEYS_MAPPED_ON_THIS_LAYER_ONLY`: tint only keys with an authored mapping on that layer; transparent `TRNS` positions stay neutral and explicitly labeled as passthrough keys
- `LAYER_BASE` falls back to the default RGB color from [config.h](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) when its authored layer color is `HSV(0, 0, 0)`

### `LAYER_BASE`

- RGB matrix render mode: `ALL_KEYS`
- Authored layer color: `HSV(0, 0, 0)`
- Preview color: <img alt="LAYER_BASE preview color" src="./profile-introspection-assets/profile-color-swatch-ff0000.svg" width="96" height="28" />

![LAYER_BASE](./profile-introspection-assets/profile-layer-LAYER_BASE.svg)

### `LAYER_NUM`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(85, 255, 200)`
- Preview color: <img alt="LAYER_NUM preview color" src="./profile-introspection-assets/profile-color-swatch-00ff00.svg" width="96" height="28" />

![LAYER_NUM](./profile-introspection-assets/profile-layer-LAYER_NUM.svg)

### `LAYER_SYM`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(169, 255, 200)`
- Preview color: <img alt="LAYER_SYM preview color" src="./profile-introspection-assets/profile-color-swatch-0006ff.svg" width="96" height="28" />

![LAYER_SYM](./profile-introspection-assets/profile-layer-LAYER_SYM.svg)

### `LAYER_NAV`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(180, 255, 200)`
- Preview color: <img alt="LAYER_NAV preview color" src="./profile-introspection-assets/profile-color-swatch-3c00ff.svg" width="96" height="28" />

![LAYER_NAV](./profile-introspection-assets/profile-layer-LAYER_NAV.svg)

### `LAYER_POINTER`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(0, 0, 150)`
- Preview color: <img alt="LAYER_POINTER preview color" src="./profile-introspection-assets/profile-color-swatch-ffffff.svg" width="96" height="28" />

![LAYER_POINTER](./profile-introspection-assets/profile-layer-LAYER_POINTER.svg)

## PD Mode Colors

These overlays come from `pd_mode_colors[]` in [rgb_config.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) and paint the right half while the matching pointing mode is active.

| Pointing Mode | Authored HSV | Preview Color |
| --- | --- | --- |
| `PD_MODE_DRAGSCROLL` | `HSV(21, 255, 200)` | <img alt="PD_MODE_DRAGSCROLL color" src="./profile-introspection-assets/profile-color-swatch-ff7e00.svg" width="96" height="28" /> |
| `PD_MODE_VOLUME` | `HSV(43, 255, 200)` | <img alt="PD_MODE_VOLUME color" src="./profile-introspection-assets/profile-color-swatch-fcff00.svg" width="96" height="28" /> |
| `PD_MODE_BRIGHTNESS` | `HSV(213, 255, 200)` | <img alt="PD_MODE_BRIGHTNESS color" src="./profile-introspection-assets/profile-color-swatch-ff00fc.svg" width="96" height="28" /> |
| `PD_MODE_ARROW` | `HSV(127, 255, 200)` | <img alt="PD_MODE_ARROW color" src="./profile-introspection-assets/profile-color-swatch-00fffc.svg" width="96" height="28" /> |
| `PD_MODE_PINCH` | `HSV(55, 255, 200)` | <img alt="PD_MODE_PINCH color" src="./profile-introspection-assets/profile-color-swatch-b4ff00.svg" width="96" height="28" /> |
| `PD_MODE_ZOOM` | `HSV(70, 255, 200)` | <img alt="PD_MODE_ZOOM color" src="./profile-introspection-assets/profile-color-swatch-5aff00.svg" width="96" height="28" /> |

## Key-Behavior Feedback LEDs

These colors come from `key_behavior_feedback_colors` in [rgb_config.c](../../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) and render last on top of the current layer and any pd-mode overlay.

| State | Meaning | Authored HSV | Preview Color |
| --- | --- | --- | --- |
| `Multi-tap pending` | Sequence still resolving the winning tap count. | `HSV(0, 0, 150)` | <img alt="Multi-tap pending color" src="./profile-introspection-assets/profile-color-swatch-ffffff.svg" width="96" height="28" /> |
| `Hold tier active` | Hold-tier pending, active, and commit-pulse feedback. | `HSV(18, 255, 200)` | <img alt="Hold tier active color" src="./profile-introspection-assets/profile-color-swatch-ff7e00.svg" width="96" height="28" /> |
| `Long-hold tier active` | Long-hold-tier active and commit-pulse feedback. | `HSV(148, 255, 200)` | <img alt="Long-hold tier active color" src="./profile-introspection-assets/profile-color-swatch-00fffc.svg" width="96" height="28" /> |

## Key Behavior Inventory

| Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- |
| `KC_1` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXLM)` | `-` | `defaults` |
| `KC_2` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_AT)` | `-` | `defaults` |
| `KC_3` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_HASH)` | `-` | `defaults` |
| `KC_4` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_DLR)` | `-` | `defaults` |
| `KC_5` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_PERC)` | `-` | `defaults` |
| `KC_6` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_CIRC)` | `-` | `defaults` |
| `KC_6` | `double` | `TAP_SENDS(KC_MPLY)` | `-` | `-` | `defaults` |
| `KC_7` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_AMPR)` | `-` | `defaults` |
| `KC_7` | `double` | `TAP_SENDS(KC_MNXT)` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `-` | `defaults` |
| `KC_8` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR)` | `-` | `defaults` |
| `KC_8` | `double` | `TAP_SENDS(KC_MPRV)` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `-` | `defaults` |
| `KC_9` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LPRN)` | `-` | `defaults` |
| `KC_0` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RPRN)` | `-` | `defaults` |
| `KC_MINS` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `defaults` |
| `KC_BSLS` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_PIPE)` | `-` | `defaults` |
| `KC_SCLN` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_COLN)` | `-` | `defaults` |
| `KC_QUOT` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)` | `-` | `defaults` |
| `KC_COMM` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)` | `-` | `defaults` |
| `KC_DOT` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)` | `-` | `defaults` |
| `KC_LBRC` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCBR)` | `-` | `defaults` |
| `KC_RBRC` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RCBR)` | `-` | `defaults` |
| `KC_ESC` | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `defaults` |
| `KC_ESC` | `double` | `TAP_SENDS(S(KC_GRV))` | `-` | `-` | `defaults` |
| `KC_LEFT_SHIFT` | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `defaults` |
| `KC_RIGHT_ALT` | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `defaults` |
| `KC_ENT` | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))` | `-` | `defaults` |
| `KC_LEFT_GUI` | `double` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LALT)` | `-` | `defaults` |
| `LT(LAYER_NAV,KC_SLSH)` | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))` | `-` | `tap_hold=100` |
| `KC_LEFT` | `single` | `-` | `TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT))` | `TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))` | `defaults` |
| `KC_RIGHT` | `single` | `-` | `TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT))` | `TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))` | `defaults` |
| `BRIGHTNESS_MODE` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `defaults` |
| `PINCH_MODE` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `defaults` |
| `PINCH_MODE` | `double` | `TAP_SENDS(VIA_MACRO_6)` | `PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)` | `-` | `defaults` |
| `VOLUME_MODE` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `defaults` |
| `VOLUME_MODE` | `double` | `TAP_SENDS(KC_MUTE)` | `-` | `-` | `defaults` |
| `DRAGSCROLL` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `defaults` |
| `DRAGSCROLL` | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_PD_MODE(DRAGSCROLL))` | `-` | `defaults` |
| `LEFT_THUMB` | `single` | `TAP_SENDS(LOCK_LAYER(LAYER_SYM))` | `PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))` | `-` | `defaults` |
| `LEFT_THUMB` | `double` | `TAP_SENDS(KC_MPLY)` | `TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE)` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))` | `defaults` |
| `LEFT_THUMB` | `triple` | `TAP_SENDS(KC_MNXT)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `defaults` |
| `LEFT_THUMB` | `quadruple` | `TAP_SENDS(KC_MPRV)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `defaults` |
| `RIGHT_THUMB` | `single` | `TAP_SENDS(LOCK_LAYER(LAYER_NAV))` | `PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))` | `-` | `tap_hold=100` |
| `RIGHT_THUMB` | `double` | `TAP_SENDS(KC_MPLY)` | `TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE)` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))` | `tap_hold=100` |
| `RIGHT_THUMB` | `triple` | `TAP_SENDS(KC_MNXT)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `tap_hold=100` |
| `RIGHT_THUMB` | `quadruple` | `TAP_SENDS(KC_MPRV)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `tap_hold=100` |
| `CLICK_SPAM` | `single` | `-` | `REPEAT_WHILE_HELD(MS_BTN1, 100Hz)` | `-` | `tap_hold=1` |

## Combos

| Inputs | Output |
| --- | --- |
| `KC_D`, `LT(LAYER_NAV,KC_F)` | `KC_TAB` |
| `MS_BTN1`, `MS_BTN2` | `CLICK_SPAM` |

### Combo Graph

```mermaid
flowchart LR
    combo_0["Combo 1"]
    combo_0_out["KC_TAB"]
    combo_0 --> combo_0_out
    combo_0_in_0["KC_D"]
    combo_0_in_0 --> combo_0
    combo_0_in_1["LT(LAYER_NAV,KC_F)"]
    combo_0_in_1 --> combo_0
    combo_1["Combo 2"]
    combo_1_out["CLICK_SPAM"]
    combo_1 --> combo_1_out
    combo_1_in_0["MS_BTN1"]
    combo_1_in_0 --> combo_1
    combo_1_in_1["MS_BTN2"]
    combo_1_in_1 --> combo_1
```

## Macro Inventory

### VIA Macros

| Slot | Payload | Usage |
| --- | --- | --- |
| `VIA_MACRO_0` | `{KC_LGUI,KC_SPC}` | `LAYER_NAV @ right[3,0]` |
| `VIA_MACRO_1` | `{KC_LALT,KC_SPC}` | `LAYER_NAV @ right[1,0]` |
| `VIA_MACRO_2` | `{KC_LALT,KC_LGUI,KC_SPC}` | `LAYER_NAV @ left[3,0]` |
| `VIA_MACRO_3` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_C}` | `LAYER_SYM @ right[2,3]` |
| `VIA_MACRO_4` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_X}` | `LAYER_SYM @ right[2,2]` |
| `VIA_MACRO_5` | `{KC_LCTL,KC_LGUI,KC_SPC}` | `LAYER_SYM @ right[2,0]` |
| `VIA_MACRO_6` | `{KC_LALT,KC_LGUI,KC_8}` | `PINCH_MODE double tap` |
| `VIA_MACRO_7` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_V}` | `LAYER_NAV @ left[0,3]` |
| `VIA_MACRO_8` | `{KC_LSFT,KC_LGUI,KC_V}` | `LAYER_SYM @ right[2,4]` |
| `VIA_MACRO_9` | `{KC_LSFT,KC_LGUI,KC_P}` | `LAYER_SYM @ right[2,5]` |

### Hardcoded Macros

No filled hardcoded macro slots.

## Generated Assets

- Generated layer images live under [profile-introspection-assets/](profile-introspection-assets) as `profile-layer-*.svg`
- Generated color swatches live under [profile-introspection-assets/](profile-introspection-assets) as `profile-color-swatch-*.svg`
- Regenerate the report, layer images, and swatches with `python3 tools/profile_introspect.py --write`; script: [tools/profile_introspect.py](../../tools/profile_introspect.py)
- Verify they are current with `python3 tools/profile_introspect.py --check`; script: [tools/profile_introspect.py](../../tools/profile_introspect.py)

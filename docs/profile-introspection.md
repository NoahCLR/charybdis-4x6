<!-- Generated file. Do not edit by hand. -->
# Profile Introspection
This report is generated from the authored profile files [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c), [config.h](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h), and [rgb_config.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c). The renderer is board-specific to the Charybdis 4x6 and derives the current `LAYOUT()` slot order directly from [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c).
PD mode names and bindings in this report stay in sync with the shared definitions in [pd_mode_manifest.h](../users/noah/lib/pointing/defs/pd_mode_manifest.h).
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
| `pd_mode_count` | `6` |
| `pd_mode_color_count` | `6` |

### Config Defines

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
| `CHARYBDIS_AUTO_SNIPING_ENABLE` | `defined` |
| `CHARYBDIS_AUTO_SNIPING_LAYER` | `LAYER_NAV` |
| `POINTING_DEVICE_AUTO_MOUSE_ENABLE` | `defined` |
| `AUTO_MOUSE_DEFAULT_LAYER` | `LAYER_POINTER` |
| `AUTO_MOUSE_TIME` | `1200` |
| `RGB_MATRIX_DEFAULT_MODE` | `RGB_MATRIX_SOLID_COLOR` |
| `RGB_MATRIX_DEFAULT_HUE` | `0` |
| `RGB_MATRIX_DEFAULT_SAT` | `255` |
| `RGB_MATRIX_MAXIMUM_BRIGHTNESS` | `200` |
| `RGB_MATRIX_DEFAULT_VAL` | `RGB_MATRIX_MAXIMUM_BRIGHTNESS` |
| `RGB_MATRIX_LED_FLUSH_LIMIT` | `32` |
| `RGB_MATRIX_TIMEOUT` | `900000` |
| `RGB_KEY_BEHAVIOR_FEEDBACK_ENABLE` | `defined` |
| `RGB_KEY_BEHAVIOR_FEEDBACK_FLASH_HALF_PERIOD_MS` | `200` |
| `RGB_AUTOMOUSE_GRADIENT_ENABLE` | `defined` |
| `AUTOMOUSE_RGB_DEAD_TIME` | `(AUTO_MOUSE_TIME/3)` |

### Authored Sources

| File | Authored Surface |
| --- | --- |
| [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) | custom keycodes, macro tables, combos, key behaviors, and current `LAYOUT()` layer contents |
| [config.h](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) | layer enum, timing, RGB defaults, and keymap-facing feature config |
| [rgb_config.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) | layer colors, pd-mode colors, and key-behavior feedback colors |

PD mode names and bindings in this report stay in sync with the shared definitions in [pd_mode_manifest.h](../users/noah/lib/pointing/defs/pd_mode_manifest.h).

### Layer RGB Config

| Layer | RGB Matrix Render Mode | Authored HSV | Preview Color |
| --- | --- | --- | --- |
| `LAYER_BASE` | `ALL_KEYS` | `HSV(0, 0, 0)` | <img alt="LAYER_BASE preview color" src="media/profile-introspection/profile-color-swatch-ff0000.svg" width="96" height="28" /> |
| `LAYER_NUM` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(85, 255, 200)` | <img alt="LAYER_NUM preview color" src="media/profile-introspection/profile-color-swatch-00ff00.svg" width="96" height="28" /> |
| `LAYER_SYM` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(169, 255, 200)` | <img alt="LAYER_SYM preview color" src="media/profile-introspection/profile-color-swatch-0006ff.svg" width="96" height="28" /> |
| `LAYER_NAV` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(180, 255, 200)` | <img alt="LAYER_NAV preview color" src="media/profile-introspection/profile-color-swatch-3c00ff.svg" width="96" height="28" /> |
| `LAYER_POINTER` | `KEYS_MAPPED_ON_THIS_LAYER_ONLY` | `HSV(0, 0, 150)` | <img alt="LAYER_POINTER preview color" src="media/profile-introspection/profile-color-swatch-ffffff.svg" width="96" height="28" /> |

### Shared Keycode Surfaces

- Layers: `LAYER_BASE`, `LAYER_NUM`, `LAYER_SYM`, `LAYER_NAV`, `LAYER_POINTER`
- Keymap-local custom keycodes: `RIGHT_THUMB`, `LEFT_THUMB`, `CLICK_SPAM`
- PD color overlays: `PD_MODE_DRAGSCROLL`, `PD_MODE_VOLUME`, `PD_MODE_BRIGHTNESS`, `PD_MODE_ARROW`, `PD_MODE_PINCH`, `PD_MODE_ZOOM`

## Key-Behavior Feedback LEDs

These colors come from `key_behavior_feedback_colors` in [rgb_config.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) and render last on top of the current layer and any pd-mode overlay.

| State | Meaning | Authored HSV | Preview Color |
| --- | --- | --- | --- |
| `Multi Tap Pending` | Neutral white while the engine is still resolving the active tap index. | `HSV(0, 0, 150)` | <img alt="Multi Tap Pending color" src="media/profile-introspection/profile-color-swatch-ffffff.svg" width="96" height="28" /> |
| `Hold Active` | Orange for authored hold-tier pending / active states and hold-tier commit pulses. | `HSV(18, 255, 200)` | <img alt="Hold Active color" src="media/profile-introspection/profile-color-swatch-ff7e00.svg" width="96" height="28" /> |
| `Long Hold Active` | Icy cyan for authored long-hold-tier active states and long-hold-tier commit pulses. | `HSV(148, 255, 200)` | <img alt="Long Hold Active color" src="media/profile-introspection/profile-color-swatch-00fffc.svg" width="96" height="28" /> |

## Layer Images

These previews are generated as SVG image assets under [docs/media/profile-introspection/](media/profile-introspection). The renderer uses the authored `layer_colors[]` config from [rgb_config.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c) and the current `LAYOUT()` slot order from [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c):

- `ALL_KEYS`: tint every physical key with the layer color
- `KEYS_MAPPED_ON_THIS_LAYER_ONLY`: tint only keys with an authored mapping on that layer; transparent `TRNS` positions stay neutral and explicitly labeled as passthrough keys
- `LAYER_BASE` falls back to the default RGB color from [config.h](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h) when its authored layer color is `HSV(0, 0, 0)`
- Keys with authored `key_behaviors[]` rows in [keymap.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/keymap.c) show activity dots derived from the authored key-behavior feedback colors in [rgb_config.c](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/rgb_config.c): white for authored tap or multi-tap handling, orange for authored hold tiers, and cyan for authored long-hold tiers
- Each layer section below also pulls in the authored key behaviors, pd-mode keys, and combos that are actually present on that layer

Timing legend for the layer-local behavior tables:

- `tap_hold(...)`, `long_hold(...)`, and `multi_tap(...)` use the default timings from [config.h](../keyboards/bastardkb/charybdis/4x6/keymaps/noah/config.h)
- `tap_hold=...`, `long_hold=...`, and `multi_tap=...` are custom timings authored on that key
- `release before tap_hold(...); otherwise normal hold` means the tap fires on a quick release; if you keep holding, the key keeps its normal hold behavior
- Timing is shown per tap count, so each row lists only the timings that matter for that behavior

### `LAYER_BASE`

- RGB matrix render mode: `ALL_KEYS`
- Authored layer color: `HSV(0, 0, 0)`
- Preview color: <img alt="LAYER_BASE preview color" src="media/profile-introspection/profile-color-swatch-ff0000.svg" width="96" height="28" />

![LAYER_BASE](media/profile-introspection/profile-layer-LAYER_BASE.svg)

#### Key Behaviors On This Layer

| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- | --- |
| `ESC` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `ESC` | `ESC` (`KC_ESC`) | `double` | `TAP_SENDS(S(KC_GRV))` | `-` | `-` | `multi_tap(150)` |
| `1` | `1` (`KC_1`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_EXLM)` | `-` | `tap_hold(150)` |
| `2` | `2` (`KC_2`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_AT)` | `-` | `tap_hold(150)` |
| `3` | `3` (`KC_3`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_HASH)` | `-` | `tap_hold(150)` |
| `4` | `4` (`KC_4`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_DLR)` | `-` | `tap_hold(150)` |
| `5` | `5` (`KC_5`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_PERC)` | `-` | `tap_hold(150)` |
| `6` | `6` (`KC_6`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_CIRC)` | `-` | `tap_hold(150), multi_tap(150)` |
| `6` | `6` (`KC_6`) | `double` | `TAP_SENDS(KC_MPLY)` | `-` | `-` | `multi_tap(150)` |
| `7` | `7` (`KC_7`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_AMPR)` | `-` | `tap_hold(150), multi_tap(150)` |
| `7` | `7` (`KC_7`) | `double` | `TAP_SENDS(KC_MNXT)` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `-` | `tap_hold(150), multi_tap(150)` |
| `8` | `8` (`KC_8`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_ASTR)` | `-` | `tap_hold(150), multi_tap(150)` |
| `8` | `8` (`KC_8`) | `double` | `TAP_SENDS(KC_MPRV)` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `-` | `tap_hold(150), multi_tap(150)` |
| `9` | `9` (`KC_9`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LPRN)` | `-` | `tap_hold(150)` |
| `0` | `0` (`KC_0`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RPRN)` | `-` | `tap_hold(150)` |
| `-` | `-` (`KC_MINS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `tap_hold(150)` |
| `\` | `\` (`KC_BSLS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_PIPE)` | `-` | `tap_hold(150)` |
| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `;` | `;` (`KC_SCLN`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_COLN)` | `-` | `tap_hold(150)` |
| `'` | `'` (`KC_QUOT`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)` | `-` | `tap_hold(150)` |
| `,` | `,` (`KC_COMM`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)` | `-` | `tap_hold(150)` |
| `.` | `.` (`KC_DOT`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)` | `-` | `tap_hold(150)` |
| `LT[NAV]/SLSH` | `LT[NAV]/SLSH` (`LT(LAYER_NAV,KC_SLSH)`) | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NAV))` | `-` | `tap_hold=100, multi_tap(150)` |
| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `LGUI` | `LGUI` (`KC_LEFT_GUI`) | `double` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LEFT_ALT)` | `-` | `tap_hold(150), multi_tap(150)` |
| `LTHUMB` | `LTHUMB` (`LEFT_THUMB`) | `single` | `TAP_SENDS(LOCK_LAYER(LAYER_SYM))` | `PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_SYM))` | `-` | `tap_hold(150), multi_tap(150)` |
| `LTHUMB` | `LTHUMB` (`LEFT_THUMB`) | `double` | `TAP_SENDS(KC_MPLY)` | `TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE)` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `LTHUMB` | `LTHUMB` (`LEFT_THUMB`) | `triple` | `TAP_SENDS(KC_MNXT)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `LTHUMB` | `LTHUMB` (`LEFT_THUMB`) | `quadruple` | `TAP_SENDS(KC_MPRV)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `RTHUMB` | `RTHUMB` (`RIGHT_THUMB`) | `single` | `TAP_SENDS(LOCK_LAYER(LAYER_NAV))` | `PRESS_AND_HOLD_UNTIL_RELEASE(MO(LAYER_NAV))` | `-` | `tap_hold=100, multi_tap(150)` |
| `RTHUMB` | `RTHUMB` (`RIGHT_THUMB`) | `double` | `TAP_SENDS(KC_MPLY)` | `TAP_ON_RELEASE_AFTER_HOLD(KC_ESCAPE)` | `TAP_AT_HOLD_THRESHOLD(LOCK_LAYER(LAYER_NUM))` | `tap_hold=100, long_hold(400), multi_tap(150)` |
| `RTHUMB` | `RTHUMB` (`RIGHT_THUMB`) | `triple` | `TAP_SENDS(KC_MNXT)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MNXT)` | `tap_hold=100, long_hold(400), multi_tap(150)` |
| `RTHUMB` | `RTHUMB` (`RIGHT_THUMB`) | `quadruple` | `TAP_SENDS(KC_MPRV)` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_MPRV)` | `tap_hold=100, long_hold(400), multi_tap(150)` |
| `ENT` | `ENT` (`KC_ENT`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(S(KC_ENT))` | `-` | `tap_hold(150)` |

#### PD Mode Keys On This Layer

No pd-mode keys are placed directly on this layer.

#### Combos Available On This Layer

| Inputs On This Layer | Output |
| --- | --- |
| `D` + `LT[NAV]/F` | `TAB` (`KC_TAB`) |

### `LAYER_NUM`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(85, 255, 200)`
- Preview color: <img alt="LAYER_NUM preview color" src="media/profile-introspection/profile-color-swatch-00ff00.svg" width="96" height="28" />

![LAYER_NUM](media/profile-introspection/profile-layer-LAYER_NUM.svg)

#### Key Behaviors On This Layer

| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- | --- |
| `ESC` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `ESC` | `ESC` (`KC_ESC`) | `double` | `TAP_SENDS(S(KC_GRV))` | `-` | `-` | `multi_tap(150)` |
| `-` | `-` (`KC_MINS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `tap_hold(150)` |
| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `,` | `,` (`KC_COMM`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LABK)` | `-` | `tap_hold(150)` |
| `.` | `.` (`KC_DOT`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RABK)` | `-` | `tap_hold(150)` |

#### PD Mode Keys On This Layer

No pd-mode keys are placed directly on this layer.

#### Combos Available On This Layer

No authored combos resolve entirely from keys on this layer.

### `LAYER_SYM`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(169, 255, 200)`
- Preview color: <img alt="LAYER_SYM preview color" src="media/profile-introspection/profile-color-swatch-0006ff.svg" width="96" height="28" />

![LAYER_SYM](media/profile-introspection/profile-layer-LAYER_SYM.svg)

#### Key Behaviors On This Layer

| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- | --- |
| `ESC x2` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `ESC x2` | `ESC` (`KC_ESC`) | `double` | `TAP_SENDS(S(KC_GRV))` | `-` | `-` | `multi_tap(150)` |
| `-` | `-` (`KC_MINS`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_UNDS)` | `-` | `tap_hold(150)` |
| `'` | `'` (`KC_QUOT`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_DQUO)` | `-` | `tap_hold(150)` |
| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `[` | `[` (`KC_LBRC`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_LCBR)` | `-` | `tap_hold(150)` |
| `]` | `]` (`KC_RBRC`) | `single` | `-` | `PRESS_AND_HOLD_UNTIL_RELEASE(KC_RCBR)` | `-` | `tap_hold(150)` |
| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |

#### PD Mode Keys On This Layer

No pd-mode keys are placed directly on this layer.

#### Combos Available On This Layer

No authored combos resolve entirely from keys on this layer.

### `LAYER_NAV`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(180, 255, 200)`
- Preview color: <img alt="LAYER_NAV preview color" src="media/profile-introspection/profile-color-swatch-3c00ff.svg" width="96" height="28" />

![LAYER_NAV](media/profile-introspection/profile-layer-LAYER_NAV.svg)

#### Key Behaviors On This Layer

| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- | --- |
| `LSFT` | `LSFT` (`KC_LEFT_SHIFT`) | `single` | `TAP_SENDS(KC_CAPS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `LEFT` | `LEFT` (`KC_LEFT`) | `single` | `-` | `TAP_ON_RELEASE_AFTER_HOLD(A(KC_LEFT))` | `TAP_AT_HOLD_THRESHOLD(G(KC_LEFT))` | `tap_hold(150), long_hold(400)` |
| `RIGHT` | `RIGHT` (`KC_RIGHT`) | `single` | `-` | `TAP_ON_RELEASE_AFTER_HOLD(A(KC_RIGHT))` | `TAP_AT_HOLD_THRESHOLD(G(KC_RIGHT))` | `tap_hold(150), long_hold(400)` |
| `ESC` | `ESC` (`KC_ESC`) | `single` | `-` | `-` | `TAP_AT_HOLD_THRESHOLD(LAG(KC_ESC))` | `tap_hold(150), long_hold(400), multi_tap(150)` |
| `ESC` | `ESC` (`KC_ESC`) | `double` | `TAP_SENDS(S(KC_GRV))` | `-` | `-` | `multi_tap(150)` |
| `DRAGSCROLL` | `DRAGSCROLL` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `multi_tap(150)` |
| `DRAGSCROLL` | `DRAGSCROLL` | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_PD_MODE(DRAGSCROLL))` | `-` | `tap_hold(150), multi_tap(150)` |
| `RALT` | `RALT` (`KC_RIGHT_ALT`) | `single` | `TAP_SENDS(LOCK_PD_MODE(ARROW_MODE))` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |

#### PD Mode Keys On This Layer

| Key On Layer | Mode Keycode | Pointing Mode | Authored HSV | Preview Color |
| --- | --- | --- | --- | --- |
| `DRAGSCROLL` | `DRAGSCROLL` | `PD_MODE_DRAGSCROLL` | `HSV(21, 255, 200)` | <img alt="PD_MODE_DRAGSCROLL color" src="media/profile-introspection/profile-color-swatch-ff7e00.svg" width="96" height="28" /> |

#### Combos Available On This Layer

| Inputs On This Layer | Output |
| --- | --- |
| `MS_BTN1` + `MS_BTN2` | `CLICK_SPAM` |

### `LAYER_POINTER`

- RGB matrix render mode: `KEYS_MAPPED_ON_THIS_LAYER_ONLY`
- Authored layer color: `HSV(0, 0, 150)`
- Preview color: <img alt="LAYER_POINTER preview color" src="media/profile-introspection/profile-color-swatch-ffffff.svg" width="96" height="28" />

![LAYER_POINTER](media/profile-introspection/profile-layer-LAYER_POINTER.svg)

#### Key Behaviors On This Layer

| Key On Layer | Behavior Keycode | Tap Count | Tap | Hold | Long Hold | Timing |
| --- | --- | --- | --- | --- | --- | --- |
| `BRIGHTNESS` | `BRIGHTNESS` (`BRIGHTNESS_MODE`) | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `release before tap_hold(150); otherwise normal hold` |
| `PINCH` | `PINCH` (`PINCH_MODE`) | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `multi_tap(150)` |
| `PINCH` | `PINCH` (`PINCH_MODE`) | `double` | `TAP_SENDS(VIA_MACRO_6)` | `PRESS_AND_HOLD_UNTIL_RELEASE(ZOOM_MODE)` | `-` | `tap_hold(150), multi_tap(150)` |
| `VOLUME` | `VOLUME` (`VOLUME_MODE`) | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `multi_tap(150)` |
| `VOLUME` | `VOLUME` (`VOLUME_MODE`) | `double` | `TAP_SENDS(KC_MUTE)` | `-` | `-` | `multi_tap(150)` |
| `DRAGSCROLL` | `DRAGSCROLL` | `single` | `TAP_SENDS(KC_TRNS)` | `-` | `-` | `multi_tap(150)` |
| `DRAGSCROLL` | `DRAGSCROLL` | `double` | `-` | `TAP_AT_HOLD_THRESHOLD(LOCK_PD_MODE(DRAGSCROLL))` | `-` | `tap_hold(150), multi_tap(150)` |

#### PD Mode Keys On This Layer

| Key On Layer | Mode Keycode | Pointing Mode | Authored HSV | Preview Color |
| --- | --- | --- | --- | --- |
| `DRAGSCROLL` | `DRAGSCROLL` | `PD_MODE_DRAGSCROLL` | `HSV(21, 255, 200)` | <img alt="PD_MODE_DRAGSCROLL color" src="media/profile-introspection/profile-color-swatch-ff7e00.svg" width="96" height="28" /> |
| `VOLUME` | `VOLUME` (`VOLUME_MODE`) | `PD_MODE_VOLUME` | `HSV(43, 255, 200)` | <img alt="PD_MODE_VOLUME color" src="media/profile-introspection/profile-color-swatch-fcff00.svg" width="96" height="28" /> |
| `BRIGHTNESS` | `BRIGHTNESS` (`BRIGHTNESS_MODE`) | `PD_MODE_BRIGHTNESS` | `HSV(213, 255, 200)` | <img alt="PD_MODE_BRIGHTNESS color" src="media/profile-introspection/profile-color-swatch-ff00fc.svg" width="96" height="28" /> |
| `PINCH` | `PINCH` (`PINCH_MODE`) | `PD_MODE_PINCH` | `HSV(55, 255, 200)` | <img alt="PD_MODE_PINCH color" src="media/profile-introspection/profile-color-swatch-b4ff00.svg" width="96" height="28" /> |

#### Combos Available On This Layer

| Inputs On This Layer | Output |
| --- | --- |
| `MS_BTN1` + `MS_BTN2` | `CLICK_SPAM` |

## Macro Inventory

### VIA Macros

| Slot | Payload | Usage |
| --- | --- | --- |
| `VIA_MACRO_0` | `{KC_LGUI,KC_SPC}` | `LAYER_NAV @ VIA0` |
| `VIA_MACRO_1` | `{KC_LALT,KC_SPC}` | `LAYER_NAV @ VIA1` |
| `VIA_MACRO_2` | `{KC_LALT,KC_LGUI,KC_SPC}` | `LAYER_NAV @ VIA2` |
| `VIA_MACRO_3` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_C}` | `LAYER_SYM @ VIA3` |
| `VIA_MACRO_4` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_X}` | `LAYER_SYM @ VIA4` |
| `VIA_MACRO_5` | `{KC_LCTL,KC_LGUI,KC_SPC}` | `LAYER_SYM @ VIA5` |
| `VIA_MACRO_6` | `{KC_LALT,KC_LGUI,KC_8}` | `PINCH_MODE double tap` |
| `VIA_MACRO_7` | `{KC_LCTL,KC_LALT,KC_LGUI,KC_V}` | `LAYER_NAV @ VIA7` |
| `VIA_MACRO_8` | `{KC_LSFT,KC_LGUI,KC_V}` | `LAYER_SYM @ VIA8` |
| `VIA_MACRO_9` | `{KC_LSFT,KC_LGUI,KC_P}` | `LAYER_SYM @ VIA9` |

### Hardcoded Macros

No filled hardcoded macro slots.

## Generated Assets

- Generated layer images live under [docs/media/profile-introspection/](media/profile-introspection) as `profile-layer-*.svg`
- Generated color swatches live under [docs/media/profile-introspection/](media/profile-introspection) as `profile-color-swatch-*.svg`
- Regenerate the report, layer images, and swatches with `python3 tools/profile_introspect.py --write`; script: [tools/profile_introspect.py](../tools/profile_introspect.py)
- Verify they are current with `python3 tools/profile_introspect.py --check`; script: [tools/profile_introspect.py](../tools/profile_introspect.py)

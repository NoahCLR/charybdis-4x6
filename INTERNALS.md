# Internals

Technical notes for developers working on this keymap. For feature overview, see [README.md](README.md). For visual flow diagrams, see [DIAGRAMS.md](DIAGRAMS.md).

## Quick Reference: Adding Features

Most features in this keymap are data-driven — you add one or two lines to a config table and the processing logic picks them up automatically. Here's where to go for each type of change.

### Add a new trackball mode

A mode intercepts trackball motion and converts it to keypresses (volume, arrows, etc.).

| Step | File | What to do |
|------|------|------------|
| 1 | `lib/pointing_device_modes.h` | Add a `#define PD_MODE_xxx (1 << N)` (next free bit) |
| 2 | `key_config.h` | Add a keycode to the `custom_keycodes` enum (e.g. `XXX_MODE`) |
| 3 | `lib/pointing_device_modes.h` | Write a `handle_xxx_mode()` function (see existing handlers for the pattern) |
| 4 | `lib/pointing_device_modes.h` | Add an entry to `pd_modes[]`: `{PD_MODE_xxx, XXX_MODE, handle_xxx_mode}` — array position = priority |
| 5 | `rgb_config.h` | Add a tagged color entry to `pd_mode_colors[]`: `{PD_MODE_xxx, {hue, sat, val}}` |
| 6 | `via_to_qmk_layout.py` | Add a `CUSTOM(N)` → `XXX_MODE` mapping to `REPLACEMENTS` |

Everything else is automatic: `process_record_user` activates/deactivates the mode on key press/release, `is_mouse_record_user` keeps the auto-mouse layer alive, `pointing_device_task_user` dispatches to your handler, and RGB paints the right-half overlay color.

### Add a double-tap / triple-tap action

A multi-tap gives a key an extra action on rapid double (or triple) tap.

| Step | File | What to do |
|------|------|------------|
| 1 | `key_config.h` | Add one line to `double_tap_keys[]`: `{keycode, action}` |
| 2 | `key_config.h` | (Optional) For triple-tap, also add to `triple_tap_keys[]`: `{keycode, action}` |

The key must already exist in the keymap (a regular key, `MO()`, or a mode key). Single-tap behavior is unaffected — it just adds a small delay while waiting for a potential second press. Triple-tap entries add one more deferral window on top of double-tap.

### Add a new tap/hold/longer-hold key

These keys send different keycodes based on hold duration (tap < 150ms, hold 150–400ms, longer hold > 400ms).

| Step | File | What to do |
|------|------|------------|
| 1 | `key_config.h` | Add one line to `hold_keys[]`: `{keycode, hold, immediate}` |
| 2 | `key_config.h` | (Optional) For a third-tier action, also add to `longer_hold_keys[]`: `{keycode, longer_hold, immediate}` |

Set `immediate` to `true` for keys that should fire the hold action at the threshold without waiting for release (most keys). Set to `false` for keys where you want to choose between hold and longer-hold on release (arrows).

### Add a new macro

A macro sends a key combination on a single keypress.

| Step | File | What to do |
|------|------|------------|
| 1 | `key_config.h` | Add a `case MACRO_N:` to the `macro_dispatch()` switch with a `SEND_STRING(...)` call |
| 2 | `key_config.h` | Make sure the keycode exists in `custom_keycodes` (MACRO_0–15 are pre-reserved) |
| 3 | `via_to_qmk_layout.py` | Macro mappings are auto-generated — no change needed unless using MACRO_16+ |

### Add a new layer

| Step | File | What to do |
|------|------|------------|
| 1 | `key_config.h` | Add the layer to the `charybdis_keymap_layers` enum (before `LAYER_COUNT`) |
| 2 | `key_config.h` | Add a `LAYOUT()` block in the `keymaps[]` array |
| 3 | `rgb_config.h` | Add an HSV entry to `layer_colors[]` at the new layer's index (`{0,0,0}` for no override) |
| 4 | `config.h` | Update `DYNAMIC_KEYMAP_LAYER_COUNT` if using VIA |
| 5 | `via_to_qmk_layout.py` | Add the layer name to `LAYER_NAMES` |

### Add a per-layer LED group highlight

Highlight specific LEDs with a different color when a layer is active (e.g. marking modifier keys).

| Step | File | What to do |
|------|------|------------|
| 1 | `rgb_config.h` | Define an LED index array: `static const uint8_t xxx_leds[] = {led1, led2, ...};` |
| 2 | `rgb_config.h` | Add one line to `layer_led_groups[]`: `{LAYER_xxx, {hue, sat, val}, xxx_leds, sizeof(xxx_leds)}` |

Use the LED Index Map in `rgb_config.h` to find the LED indices for each physical key position.

### Add a per-mode LED group highlight

Highlight specific LEDs with a different color when a pointing device mode is active.

| Step | File | What to do |
|------|------|------------|
| 1 | `rgb_config.h` | Define an LED index array (or reuse an existing one) |
| 2 | `rgb_config.h` | Add one line to `pd_mode_led_groups[]`: `{PD_MODE_xxx, {hue, sat, val}, xxx_leds, sizeof(xxx_leds)}` |

### Change a layer's color

Edit the corresponding entry in `layer_colors[]` in `rgb_config.h`. Colors are HSV values — see `hsv colors.jpg` for a quick hue reference. Changes take effect on next build with no other edits needed.

### Change a mode's overlay color

Edit the corresponding entry in `pd_mode_colors[]` in `rgb_config.h`. Entries are tagged by `mode_flag` so order doesn't matter.

## Custom Firmware Fork

This keymap depends on [NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk) (branch `qmk-latest`), which modifies two areas of the firmware:

### Charybdis dragscroll (`keyboards/bastardkb/charybdis/charybdis.c`)

- Configurable scroll tuning: rate limiting, axis snapping, step divisor, and max step size
- Dragscroll toggle now locks/unlocks the auto-mouse layer — toggling dragscroll on keeps the pointer layer active (no timeout), toggling it off restores normal auto-mouse behavior

### Auto-mouse timer (`quantum/pointing_device/pointing_device_auto_mouse.c/.h`)

- Added `auto_mouse_get_time_elapsed()` — exposes how long ago the auto-mouse layer was activated, used by the keymap to render the countdown gradient on the LEDs
- Not available in stock QMK

## Configuration (`config.h`)

All values use the `#undef` / `#define` pattern because QMK may have already defined them in parent config files.

### Split keyboard sync

- **`SERIAL_USART_TIMEOUT 5`** — QMK defaults to 20ms. At 230400 baud a transaction completes in <1ms; 5ms is generous headroom while keeping worst-case stalls below the cursor-jump threshold. See the [Serial Timeout](#split-serial-timeout-serial_usart_timeout) section below.
- **`SPLIT_LAYER_STATE_ENABLE`** — Syncs the active layer set to the slave half so it can show the correct RGB layer indicator colors.
- **`SPLIT_ACTIVITY_ENABLE`** — Syncs user activity timestamps so the RGB sleep timeout (15 min) triggers at the same time on both halves.

### RGB matrix

- **58 LEDs** split 29/29. The right (pointer) half has 2 dummy LED positions that still count toward the total.
- **Max brightness 200/255** (~78%) to stay within USB power budget.
- **Default effect: `RGB_MATRIX_SOLID_COLOR`** (red, full saturation) — layer indicators override this when a non-base layer is active.
- **30 FPS** (`LED_FLUSH_LIMIT 32`).
- **15-minute sleep** (`RGB_MATRIX_TIMEOUT 900000`).

### Pointing device

- **Auto-mouse timeout: 1200ms** (`AUTO_MOUSE_TIME`).
- **`SPLIT_TRANSACTION_IDS_USER PUT_PD_SYNC`** — Registers a custom split RPC transaction for syncing pointing device state from master to slave.
- **`MOUSE_EXTENDED_REPORT` / `WHEEL_EXTENDED_REPORT`** — 16-bit motion reports for higher precision at high DPI.
- **Hi-res scroll** — 120x multiplier (`POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER`). Each scroll unit = 1/120th of a notch.

### Dragscroll tuning

With hi-res scrolling enabled, scroll values are multiplied by 120 before reaching the OS. The dragscroll parameters control how trackball sensor counts translate into those scroll units:

| Parameter | Value | Effect |
|-----------|-------|--------|
| `CHARYBDIS_DRAGSCROLL_DPI` | 100 | Sensor CPI while scrolling (lower = finer control) |
| `CHARYBDIS_SCROLL_STEP_DIVISOR` | 8 | Sensor counts per hi-res scroll unit (higher = slower) |
| `CHARYBDIS_SCROLL_RATE_LIMIT_MS` | 8 | Min ms between scroll events (~125 Hz) |
| `CHARYBDIS_SCROLL_SNAP_RATIO` | 3 | Axis lock: dominant axis must be 3x the other to snap |
| `CHARYBDIS_SCROLL_BUFFER_EXPIRE_MS` | 80 | Discard buffered motion after 80ms idle |
| `CHARYBDIS_DRAGSCROLL_BUFFER_SIZE` | 0 | Dead zone in sensor counts (0 = immediate response) |
| `CHARYBDIS_DRAGSCROLL_REVERSE_Y` | (defined) | "Push forward = scroll up" |

### Custom tap/hold timing

These are NOT QMK's built-in `TAPPING_TERM` — they're checked manually in `process_record_user()`.

| Threshold | Value | Action |
|-----------|-------|--------|
| `CUSTOM_TAP_HOLD_TERM` | 150ms | Below this = tap (plain key) |
| Between 150–400ms | — | Hold (shifted variant) |
| `CUSTOM_LONGER_HOLD_TERM` | 400ms | Above this = longer hold (third action) |

### VIA

- `DYNAMIC_KEYMAP_LAYER_COUNT 5` — matches the 5 layers in the keymap enum (Base, Num, Lower, Raise, Pointer).

## Split Serial Timeout (`SERIAL_USART_TIMEOUT`)

QMK's split serial transport defaults to a 20ms receive timeout. Each scan cycle runs ~15+ transactions (matrix, timer sync, layer state, activity, RGB, pointing, mods, custom RPC, etc.). When any single transaction fails — due to noise, half-duplex collisions, or timing — the master blocks for the full 20ms timeout.

This is imperceptible for keypresses, but devastating for a trackball. The PMW33xx sensor accumulates motion during the stall, and when the next successful read finally happens, the firmware receives a huge delta, producing a visible cursor jump.

At 230400 baud, a successful transaction completes in <1ms. A 5ms timeout gives generous headroom while limiting worst-case stalls to well below the cursor-jump threshold. The default 20ms is only needed for unreliable connections (long cables, noisy environments).

## Split Role Override (`FORCE_MASTER` / `FORCE_SLAVE`)

By default, QMK determines master/slave by USB detection (`usb_bus_detected()`). When both halves have their own USB connection (e.g. for full LED brightness on each side when using a long cable), both detect USB and both claim master.

`FORCE_MASTER` and `FORCE_SLAVE` override `is_keyboard_master_impl()` (a weak function in `split_util.c`) to hardcode the role at compile time. The slave override also calls `usb_disconnect()` to prevent the slave's USB stack from staying active.

**Two ways to set the flag:**
- **`config.h`** — uncomment `#define FORCE_MASTER` or `#define FORCE_SLAVE` (under `#ifdef SPLIT_KEYBOARD`)
- **Command line** — `qmk compile ... -e FORCE_MASTER=yes` or `-e FORCE_SLAVE=yes` (handled by `rules.mk` via `OPT_DEFS`)

The `qmk-build` shell alias builds both firmwares in one go, outputting `charybdis_right.uf2` (master) and `charybdis_left.uf2` (slave).

`MASTER_RIGHT` is defined at the keyboard level (`4x6/config.h`) in the qmk repo — the right half (with the trackball) is always the master side. The `FORCE_*` flags only override role detection; handedness remains determined by `MASTER_RIGHT`.

If neither flag is set, the firmware falls through to QMK's default USB detection — no behavior change.

## LTO

`LTO_ENABLE = yes` in `rules.mk` — Link-time optimization reduces the binary size.

## Design Decisions

### Why not QMK's mod-tap?

QMK's built-in `MT()` maps a key to modifier+keycode on hold. It has two limitations that don't work for this keymap:

1. **Hold action is restricted to modifier combos.** `MT(MOD_LCTL, KC_A)` can send Ctrl on hold, but can't send an arbitrary keycode like `KC_EXLM` or `KC_MPLY`. This keymap needs number row keys to send shifted symbols on hold (e.g. hold `1` → `!`), which mod-tap can't express.

2. **No three-tier timing.** Mod-tap is tap-or-hold (one threshold). This keymap uses three tiers: tap (< 150ms), hold (150–400ms), longer hold (> 400ms). Arrow keys use all three: tap = arrow, hold = word jump (Alt+Arrow), longer hold = line jump (Cmd+Arrow). There's no way to do this with mod-tap.

3. **No immediate hold firing.** Mod-tap fires the held modifier either after TAPPING_TERM expires *and* the key is released, or when another key is pressed (with `HOLD_ON_OTHER_KEY_PRESS`). Neither option fires at the threshold moment while the key is still held. For a trackball keyboard, mode keys (VOLUME_MODE, etc.) need to activate the instant the hold threshold is reached so they're ready when you move the ball — waiting for release or another keypress adds perceptible lag.

### Why not QMK's tap dance?

QMK's built-in tap dance can handle multi-tap and tap-vs-hold, but the configuration model is heavier and doesn't compose with other features:

1. **Per-key boilerplate.** A simple "double-tap KC_6 → KC_MPLY" in QMK tap dance requires: (a) an enum entry, (b) a `tap_dance_actions[]` table entry, (c) using `TD(name)` in the keymap instead of the plain keycode. With advanced behavior (hold + double-tap on the same key), you also need a custom callback function with manual state/timer checks — per key. In this keymap, it's one row in `double_tap_keys[]` and the plain keycode stays in the LAYOUT array.

2. **Features don't stack.** In QMK, `TD()` and `MT()` are mutually exclusive on the same key — each consumes the keycode slot. If you want hold *and* double-tap on the same key, you must implement everything inside a single tap dance callback. This keymap's table system is composable: a key can independently appear in `hold_keys[]`, `double_tap_keys[]`, `triple_tap_keys[]`, and `longer_hold_keys[]`, and the processing logic handles all of them.

3. **Tap dance can't do immediate hold.** Tap dance's `on_dance_finished` callback fires when the tap dance resolves (after timeout or interrupt), not at a specific hold duration. Implementing "fire at 150ms while still held" requires manual timer logic inside a custom callback — at which point you've reimplemented what this keymap's `matrix_scan_user()` already does declaratively.

**What's the same:** Both QMK tap dance and this keymap's multi-tap delay the single-tap action while waiting for a possible second press. This is inherent to multi-tap detection — there's no way around it. The delay window (200ms `CUSTOM_DOUBLE_TAP_TERM`) is comparable to QMK's `TAPPING_TERM`. Keys that aren't in `double_tap_keys[]` are not affected and send immediately.

### Why not QMK's Auto Shift?

Auto Shift is the closest built-in match for "hold `1` → `!`" — it automatically sends the shifted variant of a key when held past a timeout. For the basic number-row use case it works out of the box with no per-key config. Where it falls short:

1. **Only sends the shifted variant by default.** Auto Shift applies the Shift modifier to the same keycode. To send a completely different keycode on hold (e.g. hold `6` → `KC_MPLY`), you need to override `autoshift_press_user()` with a per-key `switch` statement. This keymap's `hold_keys[]` table handles arbitrary keycodes declaratively — one row per key, no callback.

2. **Binary, not three-tier.** Auto Shift is tap-or-hold. There's no "hold longer for a third action" — you get the unshifted or the shifted key, nothing more. This keymap's arrow keys need three tiers (tap = arrow, hold = word jump, longer hold = line jump), which Auto Shift can't express.

3. **Doesn't compose with multi-tap.** Auto Shift owns the key's timing. Stacking double-tap detection on top would require intercepting Auto Shift's internal state, which isn't designed for that. The custom system handles hold and multi-tap in the same processing path.

4. **Delays every tap.** Auto Shift holds the keycode until you release the key or the timeout fires — even for plain taps. For keys not in `double_tap_keys[]`, this keymap sends the tap immediately on release with no waiting period.

**Where Auto Shift wins:** Zero-config shifted symbols on alpha keys. If all you need is "hold any letter to capitalize it," Auto Shift is simpler. This keymap doesn't use it because the number row, punctuation, and arrow keys need behaviors Auto Shift can't provide, and running two parallel hold-timing systems on the same keys would conflict.

### Why not both?

The custom system replaces mod-tap, tap dance, and Auto Shift entirely rather than layering on top of them. This is intentional: a single processing path in `process_record_user()` handles tap, hold, longer-hold, multi-tap, MO() layers, and mode activation uniformly. Mixing custom timing with QMK's built-in tap/hold state machines would create conflicting ownership over key events and timers.

## Key Behavior System

This keymap uses its own timing system in `process_record_user()` with independent, composable per-feature tables in `key_config.h`. A key can appear in multiple tables to combine behaviors (see [Design Decisions](#design-decisions) above for why).

### Tables

| Table | Purpose | Fields |
|-------|---------|--------|
| `hold_keys[]` | Tap vs hold (2-tier) | `keycode`, `hold`, `immediate` |
| `longer_hold_keys[]` | Third-tier action past 400ms | `keycode`, `longer_hold`, `immediate` |
| `double_tap_keys[]` | Action on rapid double-tap | `keycode`, `action` |
| `triple_tap_keys[]` | Action on rapid triple-tap | `keycode`, `action` |

### How it works

- **Tap** (< 150ms): Send the plain key (e.g. `1`)
- **Hold** (150–400ms): Send the hold variant (e.g. `!`)
- **Longer hold** (> 400ms): Send a third action (e.g. GUI+Arrow for line jump)
- **Double-tap**: Quick tap twice within 200ms → fires the double-tap action
- **Triple-tap**: Quick tap three times within 200ms windows → fires the triple-tap action (only if the key also has a `triple_tap_keys[]` entry)

**Immediate hold detection:** For most keys, `matrix_scan_user()` checks the hold timer every scan cycle (~1ms) and fires the hold variant as soon as the 150ms threshold is reached — no release needed. Arrow keys are excluded from this and keep the release-based behavior so the user can choose between the hold and longer-hold tiers.

State tracking uses three statics: `key_timer` (when the key was pressed), `key_active` (which key is held), and `key_hold_fired` (whether the hold action already sent). On release, if `key_hold_fired` is true, it's a no-op.

**MO() layer keys:** `MO(LAYER_LOWER)` and `MO(LAYER_RAISE)` are intercepted via `IS_QK_MOMENTARY()` — no custom keycodes needed. The layer activates immediately on press and deactivates on release (native MO behavior), while double-tap is handled alongside via `double_tap_keys[]`.

**Applies to:** Number row (1–0), punctuation (- = [ ] \ \` ; ' , .), Left/Right arrows, Enter, and MO() layer keys.

**Longer hold special cases:**
- Left/Right arrows: tap = arrow, hold = Alt+Arrow (word jump), longer hold = GUI+Arrow (line jump)
- All other keys: only in `hold_keys[]` (no longer-hold tier)

**Multi-tap note:** Adding a key to `double_tap_keys[]` introduces a small delay on single taps (waiting for potential second press). Adding a `triple_tap_keys[]` entry adds one more deferral window on top of that.

## Pointing Device Modes

The trackball mode system is a bitfield-based design in `lib/pointing_device_modes.h`. Each mode is a single bit in `pd_mode_flags`. When a mode is active, `pointing_device_task_user()` intercepts the trackball motion report and dispatches to the mode's handler function pointer (stored in `pd_modes[]`), which converts it to keypresses (volume, brightness, zoom, arrow) instead of cursor movement.

### Mode resolution

`pd_modes[]` is the single source of truth for mode config: each entry holds the flag, keycode, and handler function pointer. `pointing_device_task_user` dispatches to the first active mode's handler; modes with a `NULL` handler (dragscroll) are skipped — they're handled by the charybdis firmware. The RGB overlay iterates the same array, so priority order is consistent everywhere: dragscroll > volume > brightness > zoom > arrow.

### Mode key behavior

Mode keys (VOLUME_MODE, BRIGHTNESS_MODE, ZOOM_MODE, ARROW_MODE) are dual-purpose:
1. On press: start a timer and immediately activate the mode (so it's ready if you move the trackball)
2. On release: deactivate the mode. If held < 150ms, also send the base-layer key at that position (tap-through)

All mode keys are registered in `is_mouse_record_user()` so that holding them keeps the auto-mouse layer alive (prevents timeout while actively using a mode).

### Dragscroll special cases

**Momentary (`DRGSCRL`):** Dual-purpose like the other mode keys: tap sends the base-layer key, hold activates drag-scroll. If dragscroll was already toggled on (locked), pressing and releasing the momentary key unlocks it instead.

**Toggle-on-hold (`DRG_TOG_ON_HOLD`):** A dual-purpose key on the Pointer layer. Tap sends the base-layer key at that position; hold (> 150ms) enables dragscroll lock. When already locked, any press (tap or hold) unlocks it.

`tap_custom_bk_keycode()` simulates a full press+release of a Charybdis firmware keycode by calling `process_record_kb()` directly (not `_user`, to avoid infinite recursion).

## Layer State Management (`layer_state_set_user`)

Called by QMK whenever the active layer set changes.

1. **Sniping** — `charybdis_set_pointer_sniping_enabled()` mirrors LAYER_RAISE: on when RAISE is active, off otherwise.

2. **LAYER_POINTER stripping** — Since LAYER_POINTER (4) is above all other layers, it shadows their keys when stacked. The function strips it from the state when:
   - LAYER_RAISE is also active (always strip — sniping and pointer keys conflict)
   - Any other non-base layer is active AND nothing is holding LAYER_POINTER alive (no auto-mouse toggle, no held mouse key, no active drag scroll)

Auto-mouse stays enabled throughout — it may re-activate POINTER on the next trackball movement, but the strip catches it again. This avoids `set_auto_mouse_enable(false)` which destructively resets auto-mouse internals (key tracker, toggle, timers).

## RGB Indicator System

### Rendering pipeline

QMK renders the default RGB effect first, then calls `rgb_matrix_indicators_advanced_user()` in LED chunks (led_min to led_max). Any LEDs set by this function override the default effect for that frame. The function does two passes:

**Pass 1 — Layer color.** Iterates from the highest layer (`LAYER_COUNT - 1`) down to layer 1, checking `layer_state_cmp()`. The first active layer whose `layer_colors[]` entry is not `{0,0,0}` wins — its color is painted on both halves. Layers with `{0,0,0}` are skipped: LAYER_BASE falls through to the default RGB effect, and LAYER_POINTER falls through to the auto-mouse gradient. If no solid-color layer is active but `get_auto_mouse_layer()` is, the white→red countdown gradient renders instead.

This top-down iteration means higher layers always take priority. For example, LAYER_RAISE (layer 3) wins over LAYER_NUM (layer 1) when both are active. Colors are defined as HSV in `layer_colors[]` in `rgb_config.h` and pre-converted to RGB once at first render.

**Pass 2 — LED group highlights.** Iterates `layer_led_groups[]` and paints specific LEDs a different color when their associated layer is active. Then iterates `pd_mode_led_groups[]` for mode-specific LED highlights.

**Pass 3 — Mode overlay.** Iterates `pd_modes[]` and paints the first active mode's color from `pd_mode_colors[]` (defined in `rgb_config.h`) onto the right half. Then applies per-mode LED group highlights from `pd_mode_led_groups[]` on top. The helpers support targeting any half, both halves, or individual LEDs — the right-half-only choice is a UX decision, not a technical limitation.

**Return value:** Returns `layer_painted` (true if pass 1 set a color, false for LAYER_BASE). On LAYER_BASE with no mode active, the default RGB effect is preserved untouched. On LAYER_BASE with a mode active, pass 3 still paints the mode color over the default effect.

### Auto-mouse countdown gradient

Defined in `lib/rgb_automouse.h`. Interpolates linearly from white (HSV 0,0,150) to red (HSV 0,255,max) based on `auto_mouse_get_time_elapsed()`. The first 1/3 of the timeout (400ms) is "dead time" — color stays white to prevent distracting flicker during active trackball use.

The gradient also drives split sync: the master reads the timer once, uses it for both rendering and broadcasting to the slave via `pd_state_sync_elapsed()`.

HSV → RGB conversion is cached — only recomputed when the interpolated HSV actually changes, avoiding ~60 `hsv_to_rgb()` calls per second.

### Color precomputation

All layer and mode HSV colors are converted to RGB once at first render and cached in static variables, avoiding `hsv_to_rgb()` calls every frame.

## Split Sync Protocol

The master half (left) syncs pointing-device state to the slave half (right) so it can render correct LED colors. This uses QMK's split RPC transport with a single 3-byte packet:

```c
typedef struct __attribute__((packed)) {
    uint16_t elapsed;       // auto-mouse time elapsed, quantized to 50ms steps
    uint8_t  pd_mode_flags; // bitfield of active pointing device modes
} pd_sync_packet_t;
```

**Traffic optimization:** The elapsed time is quantized to 50ms steps and the packet is only sent when it differs from the last one. During active mouse movement, elapsed stays at 0, so no RPCs fire. RPCs only happen during the countdown after the user stops moving (~24 packets total over the 1200ms timeout).

**Sync triggers:**
- `pd_state_sync_elapsed()` — Called by `automouse_rgb_render()` with the already-read timer value, avoiding a redundant `auto_mouse_get_time_elapsed()` call.
- `pd_state_sync()` — Called by `process_record_user` on mode flag changes, reads the timer fresh.

**Slave-side handler:** `pd_sync_slave_rpc()` updates both `pd_sync_remote` (for RGB rendering) and `pd_mode_flags` (for mode overlay colors).

## RGB Helpers (`lib/rgb_helpers.h`)

Split-safe wrappers around `rgb_matrix_set_color()`. On a split keyboard, `rgb_matrix_indicators_advanced_user()` is called in chunks — each half only processes its own LEDs. The helpers clamp to the current chunk so callers can use global LED indices (0–57) without worrying about which half they're on.

| Function | What it does |
|----------|-------------|
| `rgb_set_led()` | Set a single LED by global index (no-op if outside chunk) |
| `rgb_set_led_color()` | Same but takes an `rgb_t` struct |
| `rgb_set_led_group()` | Color a list of non-contiguous LED indices |
| `rgb_fill_led_range()` | Color a continuous range, clamped to chunk |
| `rgb_set_left_half()` | Color LEDs 0–28 |
| `rgb_set_right_half()` | Color LEDs 29–57 |
| `rgb_set_both_halves()` | Color all LEDs |

The split boundary is fixed at LED index 29 (`RGB_LEFT_LED_COUNT`), matching `RGB_MATRIX_SPLIT {29, 29}`.

## RGB Color Configuration (`rgb_config.h`)

Pure color data — no logic. Contains layer indicator colors (`layer_colors[]`), mode overlay colors (`pd_mode_colors[]`), per-layer LED group highlights (`layer_led_groups[]`), per-mode LED group highlights (`pd_mode_led_groups[]`), and auto-mouse gradient endpoints. Also houses the LED Index Map and LED index arrays used by the group highlights. Mirrors the `key_config.h` pattern: config data only, rendering logic lives in `keymap.c`.

## VIA Layout Conversion

The `via layouts/via_to_qmk_layout.py` script converts VIA JSON exports into QMK `LAYOUT()` blocks. VIA uses a flat key index and its own token format (`CUSTOM(80)`, `KC_NO`, etc.) that doesn't match QMK's matrix order or keycode names.

When adding a custom keycode to the keymap, also add the VIA token mapping in the `REPLACEMENTS` dict at the top of the script. The dict translates VIA's `CUSTOM(N)` tokens to the corresponding enum name in `key_config.h`.

## Adding a New Trackball Mode

See the [quick reference](#add-a-new-trackball-mode) at the top for the step-by-step checklist. The full config lives in `lib/pointing_device_modes.h` — the handler function, `pd_modes[]` entry, and mode flag define are all in the same file.

`process_record_user`, `is_mouse_record_user`, `pointing_device_task_user`, and RGB rendering all iterate `pd_modes[]` automatically — no manual keycode lists or switch cases to update in `keymap.c`.

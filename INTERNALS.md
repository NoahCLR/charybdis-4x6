# Internals

Technical notes for developers working on this keymap. For feature overview, see [README.md](README.md). For visual flow diagrams, see [DIAGRAMS.md](DIAGRAMS.md).

## Custom Firmware Fork

This keymap depends on [NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk) (branch `bkb-master`), which modifies two areas of the firmware:

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

## Custom Tap / Hold / Longer-Hold System

This keymap does NOT use QMK's built-in mod-tap for the number row and punctuation. Instead, `process_record_user()` implements its own three-tier timing system:

- **Tap** (< 150ms): Send the plain key (e.g. `1`)
- **Hold** (150–400ms): Send the shifted variant (e.g. `!`)
- **Longer hold** (> 400ms): Send a third action (e.g. GUI+Arrow for line jump)

Key-down does NOT immediately register — the character is only sent on key-up, after measuring how long the key was held.

**Applies to:** Number row (1–0), punctuation (- = [ ] \ \` ; ' , .), Left/Right arrows, Enter.

**Longer hold special cases:**
- Left/Right arrows: tap = arrow, hold = Alt+Arrow (word jump), longer hold = GUI+Arrow (line jump)
- All other keys: longer hold falls back to the hold variant

## Pointing Device Modes

The trackball mode system is a bitfield-based design in `pointing_device_modes.h`. Each mode is a single bit in `pd_mode_flags`. When a mode is active, `pointing_device_task_user()` intercepts the trackball motion report and converts it to keypresses (volume, brightness, zoom, arrow) instead of cursor movement.

### Mode resolution

`pd_mode_priority[]` defines which mode wins when multiple are held simultaneously. Both the motion handler and the RGB overlay iterate this array, so the priority order is consistent everywhere: dragscroll > volume > brightness > zoom > arrow.

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

**Pass 1 — Layer color.** Uses `layer_state_cmp(layer_state, ...)` to check which layers are present in the layer state (not just the highest). The first match wins:

| Priority | Layer | Color | Notes |
|----------|-------|-------|-------|
| 1 | LAYER_RAISE | Purple | Both halves |
| 2 | LAYER_LOWER | Blue | Both halves |
| 3 | LAYER_NUM | Green | Both halves |
| 4 | LAYER_POINTER | White→red gradient | Both halves, animated countdown |
| — | LAYER_BASE | (none) | Default RGB effect shows through |

LAYER_RAISE is checked first because it and LAYER_POINTER can be active simultaneously (e.g. holding a mode key keeps LAYER_POINTER alive via the key tracker, then pressing MO(3) adds LAYER_RAISE). Without explicit priority, `get_highest_layer()` would return LAYER_POINTER (layer 4 > 3) and show the gradient instead of purple.

**Pass 2 — Mode overlay.** If any pointing device mode is active, the highest-priority mode's color is painted over LEDs. Currently uses `rgb_set_right_half()` to paint only the trackball side, but the helpers support targeting any half, both halves, or individual LEDs — the right-half-only choice is a UX decision, not a technical limitation. Priority order follows `pd_mode_priority[]`.

**Return value:** Returns `layer_painted` (true if pass 1 set a color, false for LAYER_BASE). On LAYER_BASE with no mode active, the default RGB effect is preserved untouched. On LAYER_BASE with a mode active, pass 2 still paints the mode color over the default effect.

### Auto-mouse countdown gradient

Defined in `rgb_automouse.h`. Interpolates linearly from white (HSV 0,0,150) to red (HSV 0,255,max) based on `auto_mouse_get_time_elapsed()`. The first 1/3 of the timeout (400ms) is "dead time" — color stays white to prevent distracting flicker during active trackball use.

The gradient also drives split sync: the master reads the timer once, uses it for both rendering and broadcasting to the slave via `pd_state_sync_elapsed()`.

HSV → RGB conversion is cached — only recomputed when the interpolated HSV actually changes, avoiding ~60 `hsv_to_rgb()` calls per second.

### Color precomputation

All layer and mode HSV colors are converted to RGB once at first render and cached in static variables, avoiding expensive `hsv_to_rgb()` calls every frame.

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

## RGB Helpers (`rgb_helpers.h`)

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

## VIA Layout Conversion

The `via layouts/via_to_qmk_layout.py` script converts VIA JSON exports into QMK `LAYOUT()` blocks. VIA uses a flat key index and its own token format (`CUSTOM(80)`, `KC_NO`, etc.) that doesn't match QMK's matrix order or keycode names.

When adding a custom keycode to the keymap, also add the VIA token mapping in the `REPLACEMENTS` dict at the top of the script. The dict translates VIA's `CUSTOM(N)` tokens to the corresponding enum name in `keymap.c`.

## Adding a New Trackball Mode

See the step-by-step guide in the header comment of `pointing_device_modes.h`. Summary:

1. Add a `PD_MODE_xxx` define (next free bit)
2. Add it to `pd_mode_priority[]`, update `PD_MODE_COUNT`
3. Add a custom keycode in `keymap.c`'s enum
4. Add the keycode to the tap/hold mode key handler in `process_record_user`
5. Add a handler case in `pointing_device_task_user`'s switch
6. Add an RGB color and `pd_mode_rgb[]` entry in the RGB section
7. Add the keycode to `is_mouse_record_user` (keeps auto-mouse alive)
8. Add the VIA token mapping in `via_to_qmk_layout.py`

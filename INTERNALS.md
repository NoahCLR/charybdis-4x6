# Internals

Technical notes for developers working on this keymap. For feature overview, see [README.md](README.md).

## Custom Firmware Fork

This keymap depends on [NoahCLR/bastardkb-qmk](https://github.com/NoahCLR/bastardkb-qmk) (branch `bkb-master`), which modifies two areas of the firmware:

### Charybdis dragscroll (`keyboards/bastardkb/charybdis/charybdis.c`)

- Configurable scroll tuning: rate limiting, axis snapping, step divisor, and max step size
- Dragscroll toggle now locks/unlocks the auto-mouse layer — toggling dragscroll on keeps the pointer layer active (no timeout), toggling it off restores normal auto-mouse behavior

### Auto-mouse timer (`quantum/pointing_device/pointing_device_auto_mouse.c/.h`)

- Added `auto_mouse_get_time_elapsed()` — exposes how long ago the auto-mouse layer was activated, used by the keymap to render the countdown gradient on the LEDs
- Not available in stock QMK

## RP2040 XIP Cache Aliasing

The RP2040 has a 16KB direct-mapped XIP (execute-in-place) flash cache. Each physical flash address maps to exactly one cache line with no associativity. When the firmware binary is large enough, two frequently-called functions can alias to the same cache line, causing pipeline stalls.

This matters because the pointing device pipeline runs at ~1000Hz. Cache misses in this path produce visible cursor jumps during fast trackball movement.

### The three-part fix (all three are required)

1. **LTO (`LTO_ENABLE = yes` in `rules.mk`)** — Link-time optimization reduces the binary by ~19KB and reshuffles code layout, reducing the chance of cache aliasing.

2. **`__attribute__((noinline))`** — Applied to mode handler functions in `pointing_device_modes.h` and sync functions in `split_sync.h`. Prevents the compiler from inlining small functions into the hot path, which would change code layout and reintroduce aliasing.

3. **`.time_critical` sections** — Places the two hottest callbacks in RAM, bypassing the XIP cache entirely:
   - `pointing_device_task_user()` in `keymap.c`
   - `rgb_matrix_indicators_advanced_user()` in `keymap.c`

Removing any one of these three can reintroduce cursor jumps.

## Split Sync Protocol

The master half (left) syncs pointing-device state to the slave half (right) so it can render correct LED colors. This uses QMK's split RPC transport with a single 3-byte packet:

```c
typedef struct __attribute__((packed)) {
    uint16_t elapsed;       // auto-mouse time elapsed, quantized to 50ms steps
    uint8_t  pd_mode_flags; // bitfield of active pointing device modes
} pd_sync_packet_t;
```

**Traffic optimization:** The elapsed time is quantized to 50ms steps and the packet is only sent when it differs from the last one. During active mouse movement, elapsed stays at 0, so no RPCs fire. RPCs only happen during the countdown after the user stops moving (~24 packets total over the 1200ms timeout).

## Adding a New Trackball Mode

See the step-by-step guide in the header comment of `pointing_device_modes.h`.

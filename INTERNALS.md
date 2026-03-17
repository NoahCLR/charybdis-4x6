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

## Split Serial Timeout (`SERIAL_USART_TIMEOUT`)

QMK's split serial transport defaults to a 20ms receive timeout (`SERIAL_USART_TIMEOUT`). Each scan cycle runs ~15+ transactions (matrix, timer sync, layer state, activity, RGB, pointing, mods, custom RPC, etc.). When any single transaction fails — due to noise, half-duplex collisions, or timing — the master blocks for the full 20ms timeout.

This is imperceptible for keypresses, but devastating for a trackball. The PMW33xx sensor accumulates motion during the stall, and when the next successful read finally happens, the firmware receives a huge delta, producing a visible cursor jump.

The fix is one line in `config.h`:

```c
#define SERIAL_USART_TIMEOUT 5
```

At 230400 baud, a successful transaction completes in <1ms. A 5ms timeout gives generous headroom while limiting worst-case stalls to well below the cursor-jump threshold. The default 20ms is only needed for unreliable connections (long cables, noisy environments).


### LTO

`LTO_ENABLE = yes` in `rules.mk` — Link-time optimization reduces the binary size.

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

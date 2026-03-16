# VIA support: enables runtime key remapping via the VIA desktop app.
VIA_ENABLE = yes

# High-resolution scroll: sends 120x multiplied scroll events for smooth scrolling.
POINTING_DEVICE_HIRES_SCROLL_ENABLE = yes

# Link-time optimization: reduces binary size and reshuffles code layout.
# REQUIRED: without LTO the binary is ~19KB larger, which causes XIP flash
# cache aliasing on the RP2040 (16KB direct-mapped cache).  The aliasing
# makes the ~1000Hz pointing device pipeline stall on cache misses,
# producing visible cursor jumps during fast trackball movement.
# Works together with __attribute__((noinline)) on handler functions and
# __attribute__((section(".time_critical.*"))) on the two hottest callbacks.
LTO_ENABLE = yes

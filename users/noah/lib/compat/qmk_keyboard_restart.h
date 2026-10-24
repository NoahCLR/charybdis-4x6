// ───────────────────────────────────────────────────────────────────────────
// QMK Keyboard Restart (D-F34)
// ───────────────────────────────────────────────────────────────────────────
//
// Restarts both halves when a client asks (Profile Wire SET 0x28): the way
// out of a stopped profile owner, and never automatic. The master half asks the
// other half over a split RPC of its own and waits for its acknowledgement or
// a short timeout; the Profile Wire reply goes out; then each half resets
// itself from housekeeping, between main-loop passes, so never inside a flash
// erase or program. The other half's RPC callback only notes the request.
// Nothing here reads the profile owner, so a restart works whatever state it
// is in.
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    // The split frame: kind, the confirmation (4), version, CRC-16 of the six
    // bytes before it. A request and its acknowledgement differ in kind only.
    NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE = 8u,
    NOAH_QMK_KEYBOARD_RESTART_REQUEST    = 0x01u,
    NOAH_QMK_KEYBOARD_RESTART_ACK        = 0x81u,
    NOAH_QMK_KEYBOARD_RESTART_VERSION    = 1u,
    // "REST", little-endian.
    NOAH_QMK_KEYBOARD_RESTART_CONFIRM = 0x54534552u,
    // The master half asks again every few milliseconds until the other half
    // acknowledges or this long has passed. A 4 KiB erase there (45 ms
    // typical) holds its split thread off for that long.
    NOAH_QMK_KEYBOARD_RESTART_SLAVE_WAIT_MS  = 250u,
    NOAH_QMK_KEYBOARD_RESTART_SLAVE_RETRY_MS = 5u,
    // Each half restarts this long after it took the request, so its own
    // answer goes out first: the Profile Wire reply on the master half, the
    // acknowledgement on the other.
    NOAH_QMK_KEYBOARD_RESTART_USB_DELAY_MS   = 50u,
    NOAH_QMK_KEYBOARD_RESTART_SLAVE_DELAY_MS = 20u,
};

void noah_qmk_keyboard_restart_encode(uint8_t kind, uint8_t frame[NOAH_QMK_KEYBOARD_RESTART_FRAME_SIZE]);
// Whether `frame` is exactly the frame of `kind`.
bool noah_qmk_keyboard_restart_frame_valid(const void *frame, uint8_t length, uint8_t kind);

// Master half, from the Raw HID handler: asks the other half to restart,
// waiting for its acknowledgement, and schedules this half's restart. True when
// the other half acknowledged. Asked again before the restart, it asks nothing
// and answers as the first time.
bool noah_qmk_keyboard_restart_request(void);
// Whether this half's restart is scheduled.
bool noah_qmk_keyboard_restart_pending(void);

// Housekeeping on both halves: registers the RPC, and restarts this half once
// its restart is due.
void noah_qmk_keyboard_restart_task(void);

#ifdef NOAH_QMK_KEYBOARD_RESTART_TEST
void noah_qmk_keyboard_restart_reset_for_test(void);
#endif

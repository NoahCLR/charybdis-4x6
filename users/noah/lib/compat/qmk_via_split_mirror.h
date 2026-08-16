// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split Write-Through Mirror
// ────────────────────────────────────────────────────────────────────────────
//
// Forwards each VIA storage command to the other half as it happens, so an edit
// lands on both halves immediately.
//
// This is deliberately independent of the durable reconciliation in
// qmk_via_split_sync.c. That layer exists to survive disconnects, power cycles
// and role swaps: it compares generations and digests and pushes whole regions
// as a session. It is the right tool for healing after an outage and the wrong
// tool for a single live keymap edit, because a stalled or deferred session
// leaves the halves visibly out of step -- slave RGB keeps rendering the old
// keycodes until the session completes.
//
// The two run side by side on separate transactions. The mirror keeps the
// halves in step during normal editing; reconciliation still owns durability
// and recovery. Neither can block the other.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdint.h>

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_via_split_mirror_init(void);
void noah_qmk_via_split_mirror_command(const uint8_t *data, uint8_t length);
#else
static inline void noah_qmk_via_split_mirror_init(void) {}
static inline void noah_qmk_via_split_mirror_command(const uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
}
#endif

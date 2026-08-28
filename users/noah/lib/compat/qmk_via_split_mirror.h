// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Split Write-Through Mirror
// ────────────────────────────────────────────────────────────────────────────
//
// Forwards each VIA storage command to the other half as it happens. The peer
// callback queues one bounded frame and its next scheduler grant applies it,
// so an edit normally lands on both halves within scan latency.
//
// This is deliberately independent of the durable reconciliation in
// qmk_via_split_sync.c. That layer exists to survive disconnects, power cycles
// and role swaps: it compares generations and digests and pushes whole regions
// as a session. It is the right tool for healing after an outage and the wrong
// tool for a single live keymap edit, because a stalled or deferred session
// leaves the halves visibly out of step -- slave RGB keeps rendering the old
// keycodes until the session completes.
//
// The two use separate transactions but share the scan-owned durable-I/O
// scheduler. The mirror keeps the halves in step during normal editing;
// reconciliation still owns durability and recovery.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#if defined(VIA_ENABLE) && defined(SPLIT_TRANSACTION_IDS_USER)
void noah_qmk_via_split_mirror_init(void);
void noah_qmk_via_split_mirror_command(const uint8_t *data, uint8_t length);
// Returns true only when one queued callback frame was consumed. All storage
// effects happen here, never in the split RPC callback.
bool noah_qmk_via_split_mirror_matrix_scan_step(void);
#else
static inline void noah_qmk_via_split_mirror_init(void) {}
static inline void noah_qmk_via_split_mirror_command(const uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
}
static inline bool noah_qmk_via_split_mirror_matrix_scan_step(void) {
    return false;
}
#endif

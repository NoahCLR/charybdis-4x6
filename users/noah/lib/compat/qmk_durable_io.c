// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_durable_io.h"

#include <stdbool.h>
#include <stdint.h>

#include "qmk_profile_flash_hooks.h"
#include "../profile/storage/profile_store_runtime_hooks.h"

// The profile store's step waits while its flash says so: a save needs a
// sector erased and the keyboard is busy, or the grant's flash time is spent
// (D-F29). It and the scan stay out of line in every build, so the flashed
// firmware has the call shape the reviewed stack paths name
// (tools/firmware_stack_budget*.json).
static __attribute__((noinline)) bool noah_qmk_durable_io_profile_step(void) {
    return noah_qmk_profile_flash_may_write() && noah_profile_store_runtime_matrix_scan_step();
}

void noah_qmk_durable_io_init(void) {}

__attribute__((noinline)) void noah_qmk_durable_io_matrix_scan(void) {
    // The profile store is the only durable owner left (D-F31). Flash pauses
    // are measured, and erases ahead of a save run, per grant (D-F29).
    noah_qmk_profile_flash_grant_begin();
    (void)noah_qmk_durable_io_profile_step();
}

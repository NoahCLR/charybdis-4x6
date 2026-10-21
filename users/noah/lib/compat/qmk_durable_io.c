// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Grant
// ──────────────────────────────────────────────────────────────────────────

#include QMK_KEYBOARD_H // IWYU pragma: keep (timer_read32)

#include "qmk_durable_io.h"

#include <stdbool.h>
#include <stdint.h>

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_qmk_durable_io_grant_t) <= NOAH_QMK_DURABLE_IO_STATE_BUDGET_32BIT, "one half's durable-I/O grant exceeds its 32-bit RAM policy");
#endif
_Static_assert(NOAH_QMK_DURABLE_IO_STEPS_MAX <= UINT8_MAX, "a grant counts its steps in a byte");

static uint32_t split_rpcs;

// The owner's clock (D-F32): the scan clock less every interval the step was
// not allowed to run, across the millisecond timer's wrap.
static uint32_t owner_now(const noah_qmk_durable_io_grant_t *grant, uint32_t now) {
    return now - grant->paused_total - (grant->paused ? now - grant->paused_since : 0u);
}

// Once per grant, before its first step: whether the step may not run. Out
// of line, as the step and the repeat are: inlined into the step, it widens
// the frame every reviewed storage path carries (memory-budgets.md, D-F32).
static __attribute__((noinline)) void noah_qmk_durable_io_note_paused(noah_qmk_durable_io_grant_t *grant, bool paused) {
    uint32_t now = timer_read32();

    if (paused && !grant->paused) {
        grant->paused_since = now;
        grant->paused       = true;
    } else if (!paused && grant->paused) {
        grant->paused_total += now - grant->paused_since;
        grant->paused = false;
    }
}

// The store's step waits while its flash says so: a save needs a sector
// erased and the keyboard is busy, or the grant's flash time is spent (D-F29).
static __attribute__((noinline)) bool noah_qmk_durable_io_profile_step(noah_qmk_durable_io_grant_t *grant, bool first_in_grant) {
    bool may_write = noah_profile_flash_store_may_write(grant->flash);

    if (first_in_grant) {
        noah_qmk_durable_io_note_paused(grant, !may_write);
    }
    if (!may_write) {
        return false;
    }
    // A repeated step visits only the subsystem whose step just worked.
    return noah_qmk_durable_io_store_step(grant->context, !first_in_grant, owner_now(grant, timer_read32()));
}

void noah_qmk_durable_io_init(noah_qmk_durable_io_grant_t *grant, noah_profile_flash_store_t *flash, void *context) {
    *grant = (noah_qmk_durable_io_grant_t){.flash = flash, .context = context};
}

void noah_qmk_durable_io_note_split_rpc(void) {
    split_rpcs++;
}

bool noah_qmk_durable_io_storage_paused(void *context) {
    const noah_qmk_durable_io_grant_t *grant = context;

    return grant->paused;
}

// A save copies and checks its bytes in small steps; one per scan made a 6 KB
// save take seconds of scans though the flash took 0.1 s (keyboard check,
// 2026-10-19). Local steps repeat within the grant: while the store has work
// in progress, never after an erase or a program (still at most one of either
// per grant), nor after a step that used the split link (the other half
// answers at its own pace), within the time budget and step limit.
static __attribute__((noinline)) bool noah_qmk_durable_io_repeat(noah_qmk_durable_io_grant_t *grant) {
    grant->steps++;
    return grant->steps < NOAH_QMK_DURABLE_IO_STEPS_MAX && split_rpcs == grant->split_rpcs && !noah_profile_flash_store_grant_flashed(grant->flash) && noah_qmk_durable_io_store_busy(grant->context) && (uint32_t)(noah_profile_flash_store_now_us(grant->flash) - grant->started_us) < NOAH_QMK_DURABLE_IO_STEP_BUDGET_US;
}

// Flash pauses are measured, and erases ahead of a save run, per grant; the
// matrix is inspected only when an erase is due (D-F29). Out of line, beside
// the step rather than above it: inlined into the scan, the flash store's
// grant start keeps a second pointer live across the scan's calls, which
// widens the frame every reviewed storage path carries.
static __attribute__((noinline)) bool noah_qmk_durable_io_begin(noah_qmk_durable_io_grant_t *grant) {
    noah_profile_flash_store_t *flash = grant->flash;

    if (!flash) {
        return false;
    }
    noah_profile_flash_store_grant_begin(flash, noah_profile_flash_store_erase_pending(flash) && noah_qmk_durable_io_keyboard_quiet(grant->context));
    grant->started_us = noah_profile_flash_store_now_us(flash);
    grant->split_rpcs = split_rpcs;
    grant->steps      = 0u;
    return true;
}

__attribute__((noinline)) void noah_qmk_durable_io_matrix_scan(noah_qmk_durable_io_grant_t *grant) {
    if (!noah_qmk_durable_io_begin(grant) || !noah_qmk_durable_io_profile_step(grant, true)) {
        return;
    }
    while (noah_qmk_durable_io_repeat(grant) && noah_qmk_durable_io_profile_step(grant, false)) {
    }
}

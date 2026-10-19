// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_durable_io.h"

#include <stdbool.h>
#include <stdint.h>

#include "qmk_profile_flash_hooks.h"
#include "../profile/storage/profile_store_runtime_hooks.h"

static uint32_t split_rpcs;
// The grant in progress: kept here rather than in the scan's frame, which
// every reviewed storage path carries (tools/firmware_stack_budget*.json).
static uint32_t grant_started_us;
static uint32_t grant_rpcs;
static uint8_t  grant_steps;

// The profile store's step waits while its flash says so: a save needs a
// sector erased and the keyboard is busy, or the grant's flash time is spent
// (D-F29). It and the scan stay out of line in every build, so the flashed
// firmware has the call shape the reviewed stack paths name
// (tools/firmware_stack_budget*.json).
static __attribute__((noinline)) bool noah_qmk_durable_io_profile_step(bool first_in_grant) {
    bool may_write = noah_qmk_profile_flash_may_write();

    // Time the step may not run is not the owner's time (D-F32), told once
    // per grant.
    if (first_in_grant) {
        noah_profile_store_runtime_note_storage_paused(!may_write);
    }
    if (!may_write) {
        return false;
    }
    // A repeated step visits only the subsystem whose step just worked.
    noah_profile_store_runtime_scan_again(!first_in_grant);
    return noah_profile_store_runtime_matrix_scan_step();
}

void noah_qmk_durable_io_init(void) {}

void noah_qmk_durable_io_note_split_rpc(void) {
    split_rpcs++;
}

// A save copies and checks its bytes in small steps; one per scan made a 6 KB
// save take seconds of scans though the flash took 0.1 s (keyboard check,
// 2026-10-19). Local steps repeat within the grant: while the store has work
// in progress, never after an erase or a program (still at most one of either
// per grant), nor after a step that used the split link (the other half
// answers at its own pace), within the time budget and step limit.
static __attribute__((noinline)) bool noah_qmk_durable_io_repeat(void) {
    grant_steps++;
    return grant_steps < NOAH_QMK_DURABLE_IO_STEPS_MAX && split_rpcs == grant_rpcs && !noah_qmk_profile_flash_grant_flashed() && noah_profile_store_runtime_storage_busy() && (uint32_t)(noah_qmk_profile_flash_now_us() - grant_started_us) < NOAH_QMK_DURABLE_IO_STEP_BUDGET_US;
}

__attribute__((noinline)) void noah_qmk_durable_io_matrix_scan(void) {
    // The profile store is the only durable owner left (D-F31). Flash pauses
    // are measured, and erases ahead of a save run, per grant (D-F29).
    noah_qmk_profile_flash_grant_begin();
    grant_started_us = noah_qmk_profile_flash_now_us();
    grant_rpcs       = split_rpcs;
    grant_steps      = 0u;
    if (!noah_qmk_durable_io_profile_step(true)) {
        return;
    }
    while (noah_qmk_durable_io_repeat() && noah_qmk_durable_io_profile_step(false)) {
    }
}

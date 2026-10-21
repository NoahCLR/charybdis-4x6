// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Grant
// ──────────────────────────────────────────────────────────────────────────
//
// A half's durable-I/O grant (D-F29 "Pacing", D-F32): once per matrix scan,
// on both halves, the profile store's turn at the half's slot flash. Every
// flash erase and program of the profile store is reached from a grant;
// split RPC callbacks only fill mailboxes.
//
// A grant, in order:
//  1. opens the flash store's grant: the previous grant's flash time is
//     closed and, only when a save needs a sector erased ahead, the half is
//     asked whether its keyboard is quiet; if so one sector is erased now;
//  2. before every step asks the flash store whether the writer may run
//     (may_write); before the first step it also notes, once per grant,
//     whether it may not: the owner's clock leaves such grants out, and the
//     other half's busy reply reads the pause;
//  3. gives a first step as a full round of the store's subsystems, then
//     repeats the subsystem that just worked while the store has local work
//     in progress and no step has erased or programmed, used the split link
//     or found nothing to do, for under NOAH_QMK_DURABLE_IO_STEP_BUDGET_US of
//     the flash's microsecond clock and NOAH_QMK_DURABLE_IO_STEPS_MAX steps.
//
// Not here: write permission, the 2 ms flash-time budget and the erase
// frontier (profile_flash_store.h); how many bytes one step moves (each store
// state machine's own quantum). The 1 ms repeat budget is not the flash
// store's flash-time budget, and neither is the owner's millisecond clock.
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../profile/storage/profile_flash_store.h"

enum {
    NOAH_QMK_DURABLE_IO_STEP_BUDGET_US = 1000u,
    NOAH_QMK_DURABLE_IO_STEPS_MAX      = 32u,
    // Regression policy for one half's grant state on 32-bit targets.
    NOAH_QMK_DURABLE_IO_STATE_BUDGET_32BIT = 28u,
};

// Storage for one half's grant; only this module reads or writes the fields.
// The firmware has one, a static of the profile store runtime; a host test
// may run several halves, never two grants at once.
typedef struct {
    noah_profile_flash_store_t *flash;   // NULL until init: grants do nothing
    void                       *context; // handed to the hooks below
    // The owner's clock (D-F32), in scan milliseconds: the time it was not
    // allowed to run.
    uint32_t paused_total;
    uint32_t paused_since;
    // The grant in progress, here rather than in the scan's frame, which
    // every reviewed storage path carries (tools/firmware_stack_budget*.json).
    uint32_t started_us;
    uint32_t split_rpcs;
    uint8_t  steps;
    // Whether the step may not run in the current grant. Written by the scan
    // once per grant; read from the split RPC callback on SlaveThread.
    volatile bool paused;
} noah_qmk_durable_io_grant_t;

// Binds `grant` to a half: its slot flash store and the context handed to
// the hooks. Starts the owner's clock unpaused. Init or scan context, before
// the half's first grant; until then a grant does nothing, since QMK scans
// the matrix during bootmagic, before keyboard_post_init.
void noah_qmk_durable_io_init(noah_qmk_durable_io_grant_t *grant, noah_profile_flash_store_t *flash, void *context);

// One grant for the half: once per matrix scan on both halves, from the scan
// only (never Raw HID or a split callback). Bounded: at most one sector erase
// or one page program, NOAH_QMK_DURABLE_IO_STEPS_MAX steps, repeats within
// NOAH_QMK_DURABLE_IO_STEP_BUDGET_US. Failures are the store's (a failed
// flash operation lets the writer run so the store learns of it); the grant
// itself cannot fail. Out of line in every build, as are its step, repeat and
// pause note: the reviewed stack paths name noah_qmk_durable_io_matrix_scan
// and noah_qmk_durable_io_profile_step (tools/firmware_stack_budget*.json).
void noah_qmk_durable_io_matrix_scan(noah_qmk_durable_io_grant_t *grant);

// A profile split RPC is being sent (the copy or the working copy, either
// direction): the grant in progress gives no further step. Scan context, from
// the sending step. One count per image, as there is one link.
void noah_qmk_durable_io_note_split_rpc(void);

// Whether the store's step may not run in the current grant: the profile
// owner's storage_paused callback, context = the grant. A volatile read with
// no flash or owner work, so the split RPC callback may call it on SlaveThread.
bool noah_qmk_durable_io_storage_paused(void *context);

// The half a grant runs on: one definition each per image. On the keyboard
// they are the profile store runtime's (owner and owner-free builds); on the
// host, a test's. `context` is the one given to init.
//
// One step of the store. `again` is set when the step repeats the subsystem
// that just worked; `now_ms` is the owner's clock. Returns whether it found
// work. Called only while the flash store allows its writer.
bool noah_qmk_durable_io_store_step(void *context, bool again, uint32_t now_ms);
// Whether the store has local work in progress that more steps in this grant
// can move on. No I/O.
bool noah_qmk_durable_io_store_busy(void *context);
// No key held and the matrix unchanged for the quiet time. The grant asks
// only when an erase is due, at most once per grant. The owner's config takes
// the same function as its keyboard_quiet.
bool noah_qmk_durable_io_keyboard_quiet(void *context);

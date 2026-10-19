// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────
#pragma once

// Nothing to initialize since the VIA store left (D-F31); kept so the boot
// order stays explicit.
void noah_qmk_durable_io_init(void);

#include <stdint.h>

enum {
    // A grant repeats the profile store's local steps for at most this long
    // and this many steps (D-F29, "Pacing").
    NOAH_QMK_DURABLE_IO_STEP_BUDGET_US = 1000u,
    NOAH_QMK_DURABLE_IO_STEPS_MAX      = 32u,
};

// Grants the profile store its steps for one scan. Split callbacks only
// populate mailboxes; every flash effect of the profile store is reached
// from here. One step, then more while the store has local work in progress,
// the grant has neither erased nor programmed, no step has used the split
// link and the time budget lasts.
void noah_qmk_durable_io_matrix_scan(void);

// Called by the profile's split RPCs (both directions of the profile and the
// working copy): a grant stops repeating steps once one has used the link.
void noah_qmk_durable_io_note_split_rpc(void);

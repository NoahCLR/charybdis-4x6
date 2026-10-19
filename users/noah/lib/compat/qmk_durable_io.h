// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────
#pragma once

// Nothing to initialize since the VIA store left (D-F31); kept so the boot
// order stays explicit.
void noah_qmk_durable_io_init(void);

// Grants the profile store one step. Split callbacks only populate mailboxes;
// every flash effect of the profile store is reached from here.
void noah_qmk_durable_io_matrix_scan(void);

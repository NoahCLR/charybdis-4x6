// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────
#pragma once

// Initializes deterministic round-robin ownership across boot discovery, the
// VIA write-through mirror, and durable VIA reconciliation.
void noah_qmk_durable_io_init(void);

// Runs at most one subsystem step. Split callbacks only populate mailboxes;
// every EEPROM/storage effect owned by these subsystems is reached from here.
void noah_qmk_durable_io_matrix_scan(void);

// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Runtime Hooks
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

void noah_profile_store_runtime_init(void);
void noah_profile_store_runtime_matrix_scan(void);
bool noah_profile_store_runtime_matrix_scan_step(void);
bool noah_profile_store_runtime_output_ready(void);

// Host logical VIA staging belongs to the owner's candidate: admitted only for
// that candidate's transaction and bound VIA identity, before its decision.
// An admitted frame the VIA layer then queues is reported as progress, which
// renews the candidate's lease. False without an owner.
bool noah_profile_store_runtime_logical_via_admit(uint16_t transaction_id, uint32_t generation, uint32_t digest);
void noah_profile_store_runtime_logical_via_progress(void);

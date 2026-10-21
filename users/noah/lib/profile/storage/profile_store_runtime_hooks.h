// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Runtime Hooks
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// keyboard_post_init: starts the store (with an owner, the owner) and binds
// this half's durable-I/O grant to its slot flash.
void noah_profile_store_runtime_init(void);
// This half's durable-I/O grant (qmk_durable_io.h): once per matrix scan on
// both halves, owner and owner-free builds.
void noah_profile_store_runtime_matrix_scan(void);

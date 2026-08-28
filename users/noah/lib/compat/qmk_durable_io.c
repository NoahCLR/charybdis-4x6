// ──────────────────────────────────────────────────────────────────────────
// QMK Scan-Owned Durable-I/O Scheduler
// ──────────────────────────────────────────────────────────────────────────

#include "qmk_durable_io.h"

#include <stdbool.h>
#include <stdint.h>

#include "qmk_via_split_mirror.h"
#include "qmk_via_split_sync.h"
#include "../profile/storage/profile_store_runtime_hooks.h"

typedef bool (*noah_qmk_durable_io_step_fn)(void);

enum {
    NOAH_QMK_DURABLE_IO_PROFILE_DISCOVERY = 0u,
    NOAH_QMK_DURABLE_IO_VIA_MIRROR,
    NOAH_QMK_DURABLE_IO_VIA_SYNC,
    NOAH_QMK_DURABLE_IO_STEP_COUNT,
};

static uint8_t noah_qmk_durable_io_next_step;

static uint8_t noah_qmk_durable_io_step_after(uint8_t step) {
    step++;
    return step == NOAH_QMK_DURABLE_IO_STEP_COUNT ? NOAH_QMK_DURABLE_IO_PROFILE_DISCOVERY : step;
}

void noah_qmk_durable_io_init(void) {
    noah_qmk_durable_io_next_step = NOAH_QMK_DURABLE_IO_PROFILE_DISCOVERY;
}

void noah_qmk_durable_io_matrix_scan(void) {
    static const noah_qmk_durable_io_step_fn steps[NOAH_QMK_DURABLE_IO_STEP_COUNT] = {
        noah_profile_store_runtime_matrix_scan_step,
        noah_qmk_via_split_mirror_matrix_scan_step,
        noah_qmk_via_split_sync_matrix_scan_step,
    };

    uint8_t index = noah_qmk_durable_io_next_step;

    for (uint8_t visited = 0u; visited < NOAH_QMK_DURABLE_IO_STEP_COUNT; visited++) {
        if (steps[index]()) {
            noah_qmk_durable_io_next_step = noah_qmk_durable_io_step_after(index);
            return;
        }
        index = noah_qmk_durable_io_step_after(index);
    }
}

_Static_assert(NOAH_QMK_DURABLE_IO_STEP_COUNT == 3u, "durable-I/O scheduler inventory changed without updating its round-robin contract");

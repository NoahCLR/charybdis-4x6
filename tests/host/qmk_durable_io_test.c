#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_durable_io.h"

enum {
    STEP_PROFILE = 0u,
    STEP_MIRROR,
    STEP_VIA_SYNC,
    STEP_COUNT,
    LOG_CAPACITY = 16u,
};

static bool    ready[STEP_COUNT];
static uint8_t log_entries[LOG_CAPACITY];
static uint8_t log_count;
static uint8_t consumed_count;

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

static bool record_step(uint8_t step) {
    check(log_count < LOG_CAPACITY, "scheduler log overflow");
    log_entries[log_count++] = step;
    if (!ready[step]) {
        return false;
    }
    ready[step] = false;
    consumed_count++;
    return true;
}

bool noah_profile_store_runtime_matrix_scan_step(void) {
    return record_step(STEP_PROFILE);
}

bool noah_qmk_via_split_mirror_matrix_scan_step(void) {
    return record_step(STEP_MIRROR);
}

bool noah_qmk_via_split_sync_matrix_scan_step(void) {
    return record_step(STEP_VIA_SYNC);
}

static void reset_log(void) {
    memset(log_entries, 0, sizeof(log_entries));
    log_count      = 0u;
    consumed_count = 0u;
}

static void test_idle_scan_visits_each_owner_once(void) {
    memset(ready, 0, sizeof(ready));
    reset_log();
    noah_qmk_durable_io_init();
    noah_qmk_durable_io_matrix_scan();

    check(log_count == 3u, "idle scan must query every owner");
    check(log_entries[0] == STEP_PROFILE && log_entries[1] == STEP_MIRROR && log_entries[2] == STEP_VIA_SYNC, "initial owner order must be deterministic");
    check(consumed_count == 0u, "idle scan must not consume a step");
}

static void test_ready_owners_rotate_and_only_one_consumes(void) {
    memset(ready, true, sizeof(ready));
    noah_qmk_durable_io_init();

    reset_log();
    noah_qmk_durable_io_matrix_scan();
    check(log_count == 1u && log_entries[0] == STEP_PROFILE && consumed_count == 1u, "profile discovery must receive the initial grant");

    ready[STEP_PROFILE] = true;
    reset_log();
    noah_qmk_durable_io_matrix_scan();
    check(log_count == 1u && log_entries[0] == STEP_MIRROR && consumed_count == 1u, "mirror must receive the next grant");

    ready[STEP_MIRROR] = true;
    reset_log();
    noah_qmk_durable_io_matrix_scan();
    check(log_count == 1u && log_entries[0] == STEP_VIA_SYNC && consumed_count == 1u, "VIA reconciliation must receive the next grant");

    ready[STEP_VIA_SYNC] = true;
    reset_log();
    noah_qmk_durable_io_matrix_scan();
    check(log_count == 1u && log_entries[0] == STEP_PROFILE && consumed_count == 1u, "round robin must wrap back to profile work");
}

static void test_scheduler_skips_idle_owner_without_double_consumption(void) {
    memset(ready, 0, sizeof(ready));
    ready[STEP_VIA_SYNC] = true;
    reset_log();
    noah_qmk_durable_io_init();
    noah_qmk_durable_io_matrix_scan();

    check(log_count == 3u, "scheduler must reach later ready owners");
    check(log_entries[0] == STEP_PROFILE && log_entries[1] == STEP_MIRROR && log_entries[2] == STEP_VIA_SYNC, "idle owners must be skipped in order");
    check(consumed_count == 1u, "one scan must consume at most one owner step");
}

int main(void) {
    test_idle_scan_visits_each_owner_once();
    test_ready_owners_rotate_and_only_one_consumes();
    test_scheduler_skips_idle_owner_without_double_consumption();
    puts("qmk durable-I/O scheduler tests passed");
    return 0;
}

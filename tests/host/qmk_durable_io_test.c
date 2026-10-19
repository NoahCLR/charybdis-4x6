#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/compat/qmk_durable_io.h"

// The profile store is the scheduler's only owner (D-F31): every scan opens
// one flash grant, then gives the store one step unless its flash says wait.

static unsigned grants;
static unsigned profile_steps;
static bool     flash_may_write = true;
static bool     grant_open;

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

void noah_qmk_profile_flash_grant_begin(void) {
    grant_open = true;
    grants++;
}

bool noah_qmk_profile_flash_may_write(void) {
    return flash_may_write;
}

bool noah_profile_store_runtime_matrix_scan_step(void) {
    check(grant_open, "the flash grant opens before the scan's profile step");
    check(flash_may_write, "the profile step runs only while its flash allows");
    grant_open = false;
    profile_steps++;
    return true;
}

static void test_every_scan_opens_one_grant_and_one_profile_step(void) {
    noah_qmk_durable_io_init();
    for (unsigned scan = 0u; scan < 4u; scan++) {
        noah_qmk_durable_io_matrix_scan();
        check(grants == scan + 1u, "one flash grant per scan");
        check(profile_steps == scan + 1u, "one profile step per scan");
    }
}

static void test_a_waiting_profile_step_does_not_run(void) {
    unsigned steps = profile_steps;

    flash_may_write = false;
    noah_qmk_durable_io_matrix_scan();
    check(profile_steps == steps, "a waiting profile step must not run");
    flash_may_write = true;
    noah_qmk_durable_io_matrix_scan();
    check(profile_steps == steps + 1u, "the profile step runs once its flash allows");
}

int main(void) {
    test_every_scan_opens_one_grant_and_one_profile_step();
    test_a_waiting_profile_step_does_not_run();
    puts("qmk durable-I/O scheduler tests passed");
    return 0;
}

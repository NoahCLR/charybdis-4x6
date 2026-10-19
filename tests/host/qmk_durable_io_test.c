#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/compat/qmk_durable_io.h"
#include "users/noah/lib/profile/storage/profile_paused_clock.h"

// The profile store is the scheduler's only owner (D-F31): every scan opens
// one flash grant and gives the store a step unless its flash says wait; while
// the store has local work in progress the grant repeats its steps, until one
// erases or programs, uses the split link or finds nothing to do, or the time
// budget runs out (D-F29, "Pacing").

static unsigned grants;
static unsigned profile_steps;
static bool     flash_may_write = true;
static bool     grant_open;
static unsigned paused_notes;
static bool     last_paused;
static bool     store_busy;
static unsigned step_flashes_at;   // the step that erases or programs (0: none)
static unsigned step_uses_link_at; // the step that sends a split RPC (0: none)
static unsigned step_idle_at;      // the step that finds nothing to do (0: none)
static unsigned steps_in_grant;
static bool     grant_flashed;
static uint32_t clock_us;
static uint32_t step_cost_us;

void noah_profile_store_runtime_note_storage_paused(bool paused) {
    paused_notes++;
    last_paused = paused;
}

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

void noah_qmk_profile_flash_grant_begin(void) {
    grant_open     = true;
    grant_flashed  = false;
    steps_in_grant = 0u;
    grants++;
}

bool noah_qmk_profile_flash_may_write(void) {
    return flash_may_write;
}

bool noah_qmk_profile_flash_grant_flashed(void) {
    return grant_flashed;
}

uint32_t noah_qmk_profile_flash_now_us(void) {
    return clock_us;
}

bool noah_profile_store_runtime_storage_busy(void) {
    return store_busy;
}

static bool     scan_again;
static unsigned again_steps;

void noah_profile_store_runtime_scan_again(bool again) {
    scan_again = again;
}

bool noah_profile_store_runtime_matrix_scan_step(void) {
    check(grant_open, "the flash grant opens before the scan's profile steps");
    check(flash_may_write, "the profile step runs only while its flash allows");
    check(scan_again == (steps_in_grant != 0u), "a grant's first step is a full scan; every later one repeats the subsystem that worked");
    if (scan_again) again_steps++;
    profile_steps++;
    steps_in_grant++;
    clock_us += step_cost_us;
    if (steps_in_grant == step_flashes_at) grant_flashed = true;
    if (steps_in_grant == step_uses_link_at) noah_qmk_durable_io_note_split_rpc();
    return steps_in_grant != step_idle_at;
}

static void reset(void) {
    grants = profile_steps = paused_notes = 0u;
    flash_may_write = true;
    store_busy      = false;
    step_flashes_at = step_uses_link_at = step_idle_at = 0u;
    step_cost_us    = 0u;
}

static unsigned steps_of_one_scan(void) {
    unsigned before = profile_steps;

    noah_qmk_durable_io_matrix_scan();
    return profile_steps - before;
}

static void test_an_idle_store_gets_one_grant_and_one_step_per_scan(void) {
    reset();
    noah_qmk_durable_io_init();
    for (unsigned scan = 0u; scan < 4u; scan++) {
        check(steps_of_one_scan() == 1u, "one profile step per scan with no storage work in progress");
        check(grants == scan + 1u, "one flash grant per scan");
    }
}

static void test_a_waiting_profile_step_does_not_run(void) {
    reset();
    store_busy      = true;
    flash_may_write = false;
    check(steps_of_one_scan() == 0u, "a waiting profile step must not run");
    check(last_paused && paused_notes == 1u, "a waiting step is told to the owner's clock, once per grant");
    flash_may_write = true;
    step_idle_at    = 5u;
    check(steps_of_one_scan() == 5u, "the profile steps run once its flash allows");
    check(!last_paused && paused_notes == 2u, "every grant tells the clock once, however many steps it gives");
}

// The slow part of a save was copying and checking bytes, one small step per
// scan (keyboard check, 2026-10-19): local steps now repeat within a grant.
static void test_local_storage_work_repeats_within_the_grant_up_to_its_limit(void) {
    reset();
    store_busy = true;
    again_steps = 0u;
    check(steps_of_one_scan() == NOAH_QMK_DURABLE_IO_STEPS_MAX, "local storage work repeats up to the step limit");
    check(grants == 1u, "in one grant");
    check(again_steps == NOAH_QMK_DURABLE_IO_STEPS_MAX - 1u, "every step after the first repeats the subsystem that worked");
}

static void test_an_erase_or_a_program_ends_the_grants_steps(void) {
    reset();
    store_busy      = true;
    step_flashes_at = 3u;
    check(steps_of_one_scan() == 3u, "no step follows one that erased or programmed: one of either per grant (D-F29)");
}

static void test_a_step_that_used_the_split_link_ends_the_grants_steps(void) {
    reset();
    store_busy        = true;
    step_uses_link_at = 2u;
    check(steps_of_one_scan() == 2u, "the other half answers at its own pace: no step follows one that used the link");
}

static void test_a_step_with_nothing_to_do_or_no_work_in_progress_ends_them(void) {
    reset();
    store_busy   = true;
    step_idle_at = 4u;
    check(steps_of_one_scan() == 4u, "a step that found nothing to do ends the grant's steps");
    store_busy   = false;
    step_idle_at = 0u;
    check(steps_of_one_scan() == 1u, "work no longer in progress ends them");
}

static void test_the_time_budget_ends_them(void) {
    reset();
    store_busy   = true;
    step_cost_us = 300u;
    clock_us     = UINT32_MAX - 500u; // across the counter's wrap
    check(steps_of_one_scan() == (NOAH_QMK_DURABLE_IO_STEP_BUDGET_US + 299u) / 300u, "steps stop once the grant's time budget is spent");
}

// The owner's clock does not count grants its step could not run (D-F32):
// a save waiting 20 s for a quiet keyboard is 20 s no timer of the owner sees.
static void test_the_owner_clock_stands_still_while_paused(void) {
    noah_profile_paused_clock_t clock = {0};

    noah_profile_paused_clock_note(&clock, false, 1000u);
    check(noah_profile_paused_clock_now(&clock, 1500u) == 1500u, "an unpaused clock is the scan clock");
    noah_profile_paused_clock_note(&clock, true, 2000u);
    check(noah_profile_paused_clock_now(&clock, 2000u) == 2000u && noah_profile_paused_clock_now(&clock, 22000u) == 2000u, "a paused clock stands still");
    noah_profile_paused_clock_note(&clock, true, 22000u);
    noah_profile_paused_clock_note(&clock, false, 22000u);
    check(noah_profile_paused_clock_now(&clock, 22500u) == 2500u, "it runs on from where it stopped");
    // Wrapping millisecond counters keep their differences.
    clock = (noah_profile_paused_clock_t){0};
    noah_profile_paused_clock_note(&clock, true, UINT32_MAX - 10u);
    noah_profile_paused_clock_note(&clock, false, 20u);
    check(noah_profile_paused_clock_now(&clock, 30u) == UINT32_MAX - 10u + 10u, "pauses across the counter's wrap");
}

int main(void) {
    test_an_idle_store_gets_one_grant_and_one_step_per_scan();
    test_a_waiting_profile_step_does_not_run();
    test_local_storage_work_repeats_within_the_grant_up_to_its_limit();
    test_an_erase_or_a_program_ends_the_grants_steps();
    test_a_step_that_used_the_split_link_ends_the_grants_steps();
    test_a_step_with_nothing_to_do_or_no_work_in_progress_ends_them();
    test_the_time_budget_ends_them();
    test_the_owner_clock_stands_still_while_paused();
    puts("qmk durable-I/O scheduler tests passed");
    return 0;
}

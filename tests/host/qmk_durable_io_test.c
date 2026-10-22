#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/compat/qmk_durable_io.h"

// A half's durable-I/O grant (D-F29 "Pacing", D-F32), run with the real flash
// store over a fake NOR chip and a scripted store: every scan opens one flash
// grant and gives the store a step unless its flash says wait; while the store
// has local work in progress the grant repeats its steps, until one erases or
// programs, uses the split link or finds nothing to do, or the time budget
// runs out. Grants the step could not run are not the owner's time.

enum {
    // An erase always spends a grant's flash time; a page program does not.
    CHIP_ERASE_US   = 2500u,
    CHIP_PROGRAM_US = 800u,
    // The scan's period on the millisecond timer.
    SCAN_MS = 10u,
    // A save over the header sector and two more.
    SAVE_LENGTH = NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + 2u * NOAH_PROFILE_STORAGE_SECTOR_SIZE,
};

_Static_assert((unsigned)CHIP_ERASE_US > (unsigned)NOAH_PROFILE_FLASH_GRANT_BUDGET_US && (unsigned)CHIP_PROGRAM_US < (unsigned)NOAH_PROFILE_FLASH_GRANT_BUDGET_US, "the chip's costs straddle the flash-time budget as the RP2040's do");

typedef struct {
    uint8_t  bytes[NOAH_PROFILE_STORAGE_REGION_SIZE];
    uint32_t clock_us;
    uint32_t erases;
    uint32_t programs;
} chip_t;

// One half: its grant, its slot flash and the store the grant steps. The
// script says, by a step's place in its grant (1-based; 0 never), when the
// store finds nothing to do, programs a page, starts a save or sends a split
// RPC.
typedef struct {
    noah_qmk_durable_io_grant_t grant;
    noah_profile_flash_store_t  flash;
    noah_profile_store_io_t     io;
    chip_t                      chip;
    bool                        busy;
    bool                        quiet;
    unsigned                    idle_at;
    unsigned                    program_at;
    unsigned                    save_at;
    unsigned                    link_at;
    uint32_t                    step_us;
    // What the grants gave.
    unsigned                    steps;
    unsigned                    grant_steps;
    unsigned                    repeated;
    unsigned                    quiet_calls;
    uint32_t                    now_ms;
    uint32_t                    next_page;
} half_t;

static uint32_t scan_ms;
static unsigned hook_calls;

static void check(bool condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "test failed: %s\n", message);
        exit(1);
    }
}

uint32_t timer_read32(void) {
    return scan_ms;
}

static bool chip_erase(void *context, uint32_t offset) {
    chip_t *chip = context;

    check(offset % NOAH_PROFILE_STORAGE_SECTOR_SIZE == 0u && offset < sizeof(chip->bytes), "erases whole sectors of the slot region");
    chip->clock_us += CHIP_ERASE_US;
    chip->erases++;
    memset(&chip->bytes[offset], 0xFF, NOAH_PROFILE_STORAGE_SECTOR_SIZE);
    return true;
}

static bool chip_program(void *context, uint32_t offset, const uint8_t *page) {
    chip_t *chip = context;

    check(offset % NOAH_PROFILE_STORAGE_PAGE_SIZE == 0u && offset < sizeof(chip->bytes), "programs whole pages of the slot region");
    chip->clock_us += CHIP_PROGRAM_US;
    chip->programs++;
    for (uint32_t index = 0u; index < NOAH_PROFILE_STORAGE_PAGE_SIZE; index++) {
        chip->bytes[offset + index] &= page[index];
    }
    return true;
}

static void chip_read(void *context, uint32_t offset, uint8_t *target, uint32_t length) {
    memcpy(target, &((chip_t *)context)->bytes[offset], length);
}

static uint32_t chip_now(void *context) {
    return ((chip_t *)context)->clock_us;
}

// A full page of the save's payload through the flash store's I/O, as a
// REUSE step copies one.
static void program_a_page(half_t *half) {
    uint8_t  bytes[NOAH_PROFILE_STORE_IO_CHUNK_MAX];
    uint32_t programs = half->chip.programs;

    memset(bytes, 0xA5, sizeof(bytes));
    for (uint32_t offset = 0u; offset < NOAH_PROFILE_STORAGE_PAGE_SIZE; offset += sizeof(bytes)) {
        check(half->io.write(half->io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE + half->next_page + offset, bytes, sizeof(bytes)), "the save's write lands");
    }
    half->next_page += NOAH_PROFILE_STORAGE_PAGE_SIZE;
    check(half->chip.programs == programs + 1u, "a full page goes to flash at once");
}

static void start_a_save(half_t *half) {
    check(half->io.reset(half->io.context, NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR, SAVE_LENGTH), "a save takes slot A");
    half->next_page = 0u;
}

bool noah_qmk_durable_io_store_step(void *context, bool again, uint32_t now_ms) {
    half_t *half = context;

    hook_calls++;
    check(noah_profile_flash_store_may_write(&half->flash), "a step runs only while the flash store allows its writer");
    check(!noah_profile_flash_store_grant_flashed(&half->flash), "no step follows an erase or a program in its grant, and every grant starts with its flash accounting reset");
    check(again == (half->grant_steps != 0u), "a grant's first step is a full round; every later one repeats the subsystem that worked");
    half->grant_steps++;
    half->steps++;
    half->repeated += again ? 1u : 0u;
    half->now_ms = now_ms;
    half->chip.clock_us += half->step_us;
    if (half->grant_steps == half->save_at) start_a_save(half);
    if (half->grant_steps == half->program_at) program_a_page(half);
    if (half->grant_steps == half->link_at) noah_qmk_durable_io_note_split_rpc();
    return half->grant_steps != half->idle_at;
}

bool noah_qmk_durable_io_store_busy(void *context) {
    hook_calls++;
    return ((half_t *)context)->busy;
}

bool noah_qmk_durable_io_keyboard_quiet(void *context) {
    half_t *half = context;

    hook_calls++;
    half->quiet_calls++;
    return half->quiet;
}

static half_t left_half;
static half_t right_half;

static half_t *fresh(half_t *half) {
    memset(half, 0, sizeof(*half));
    memset(half->chip.bytes, 0xFF, sizeof(half->chip.bytes));
    noah_profile_flash_store_init(&half->flash, (noah_profile_flash_driver_t){.erase_sector = chip_erase, .program_page = chip_program, .now_us = chip_now, .read = chip_read, .context = &half->chip});
    half->io    = noah_profile_flash_store_io(&half->flash);
    half->quiet = true;
    noah_qmk_durable_io_init(&half->grant, &half->flash, half);
    return half;
}

// One matrix scan's grant; returns the steps it gave.
static unsigned scan(half_t *half) {
    unsigned before = half->steps;

    half->grant_steps = 0u;
    noah_qmk_durable_io_matrix_scan(&half->grant);
    return half->steps - before;
}

static bool paused(half_t *half) {
    return noah_qmk_durable_io_storage_paused(&half->grant);
}

// A save with erased flash ahead of its writes: the quiet grant that erases
// the header sector gives no step; a busy keyboard asks no more after it.
static void a_save_ready_to_write(half_t *half) {
    start_a_save(half);
    half->quiet = true;
    check(scan(half) == 0u && half->chip.erases == 1u, "a quiet grant erases the save's header sector and gives no step");
    half->quiet = false;
}

// QMK scans the matrix during bootmagic, before keyboard_post_init binds the
// grant: such a grant touches nothing.
static void test_a_grant_before_init_does_nothing(void) {
    static noah_qmk_durable_io_grant_t unbound;

    hook_calls = 0u;
    noah_qmk_durable_io_matrix_scan(&unbound);
    check(hook_calls == 0u && !noah_qmk_durable_io_storage_paused(&unbound), "a grant before init asks nothing, steps nothing and pauses nothing");
}

static void test_an_idle_store_gets_one_grant_and_one_step_per_scan(void) {
    half_t *half = fresh(&left_half);

    for (unsigned grant = 0u; grant < 4096u; grant++) {
        check(scan(half) == 1u, "one step per scan with no storage work in progress");
    }
    check(half->repeated == 0u && !paused(half), "an idle grant's one step is a full round");
    check(half->quiet_calls == 0u && half->chip.erases == 0u && half->chip.programs == 0u, "idle grants never inspect the matrix");
}

// A save waiting for a quiet keyboard: no erase and no step, and the owner's
// clock does not count the wait (D-F32), however long the typing goes on.
static void test_a_save_waits_for_a_quiet_keyboard_and_its_wait_is_not_the_owners_time(void) {
    half_t  *half = fresh(&left_half);
    unsigned asked;

    half->busy    = true;
    half->idle_at = 1u;
    scan_ms       = 1000u;
    check(scan(half) == 1u && half->now_ms == 1000u, "an unpaused owner's clock is the scan clock");
    start_a_save(half);
    half->quiet = false;
    for (uint32_t elapsed = 0u; elapsed <= 20000u; elapsed += SCAN_MS) {
        scan_ms = 1010u + elapsed;
        asked   = half->quiet_calls;
        check(scan(half) == 0u, "the step waits while a save's erase waits for a quiet keyboard");
        check(half->quiet_calls == asked + 1u, "a grant with an erase due asks once whether the keyboard is quiet");
        check(paused(half), "and the other half's busy reply reads the pause");
    }
    check(half->chip.erases == 0u, "a busy keyboard is never paused for an erase");
    // Quiet: the erase runs, and that grant's step still waits.
    half->quiet = true;
    scan_ms     = 21020u;
    check(scan(half) == 0u && half->chip.erases == 1u && paused(half), "a quiet grant erases and gives no step");
    // The writer runs; no erase is due, so the keyboard is not asked.
    asked   = half->quiet_calls;
    scan_ms = 21030u;
    check(scan(half) == 1u && !paused(half), "the next grant steps");
    check(half->now_ms == 1010u, "the owner's clock runs on from where it stopped");
    check(half->quiet_calls == asked, "the keyboard is asked only while an erase is due");
    scan_ms = 21040u;
    check(scan(half) == 1u && half->now_ms == 1020u, "and keeps time with the scan clock");
}

static void test_local_storage_work_repeats_within_the_grant_up_to_its_limit(void) {
    half_t *half = fresh(&left_half);

    half->busy = true;
    check(scan(half) == NOAH_QMK_DURABLE_IO_STEPS_MAX, "local storage work repeats up to the step limit");
    check(half->repeated == NOAH_QMK_DURABLE_IO_STEPS_MAX - 1u, "every step after the first repeats the subsystem that worked");
    check(scan(half) == NOAH_QMK_DURABLE_IO_STEPS_MAX, "and so does the next grant's");
}

static void test_a_program_ends_the_grants_steps(void) {
    half_t *half = fresh(&left_half);

    a_save_ready_to_write(half);
    half->busy       = true;
    half->program_at = 3u;
    check(scan(half) == 3u && half->chip.programs == 1u, "no step follows one that programmed: one page per grant (D-F29)");
    // The next grant's accounting starts afresh (store_step checks it).
    check(scan(half) == 3u && half->chip.programs == 2u, "the next grant steps again");
    half->program_at = 1u;
    check(scan(half) == 1u && half->chip.programs == 3u, "even its first step ends a grant");
}

// The step that starts a save leaves no erased flash ahead: the grant gives
// no further step that would write unerased flash, and the pause is noted
// once, before the first step, so this grant was not paused.
static void test_a_step_that_starts_a_save_gets_no_unerased_follow_up(void) {
    half_t *half = fresh(&left_half);

    half->busy    = true;
    half->save_at = 1u;
    check(scan(half) == 1u, "may_write is asked before every step, not only the first");
    check(half->chip.erases == 0u && half->chip.programs == 0u, "a save's start touches no flash");
    check(!paused(half), "a grant whose first step ran is not a pause");
    half->save_at = 0u;
    check(scan(half) == 0u && half->chip.erases == 1u && paused(half), "the next quiet grant erases ahead, and its step waits");
    check(scan(half) == NOAH_QMK_DURABLE_IO_STEPS_MAX && !paused(half), "then the writer runs");
}

static void test_a_step_that_used_the_split_link_ends_the_grants_steps(void) {
    half_t *half = fresh(&left_half);

    half->busy    = true;
    half->link_at = 2u;
    check(scan(half) == 2u, "the other half answers at its own pace: no step follows one that used the link");
    half->link_at = 0u;
    check(scan(half) == NOAH_QMK_DURABLE_IO_STEPS_MAX, "a grant counts only its own RPCs");
}

static void test_a_step_with_nothing_to_do_or_no_work_in_progress_ends_them(void) {
    half_t *half = fresh(&left_half);

    half->busy    = true;
    half->idle_at = 4u;
    check(scan(half) == 4u, "a step that found nothing to do ends the grant's steps");
    half->busy    = false;
    half->idle_at = 0u;
    check(scan(half) == 1u, "work no longer in progress ends them");
}

static void test_the_time_budget_ends_them_across_the_flash_clocks_wrap(void) {
    half_t *half = fresh(&left_half);

    half->busy          = true;
    half->step_us       = 300u;
    half->chip.clock_us = UINT32_MAX - 500u;
    check(scan(half) == (NOAH_QMK_DURABLE_IO_STEP_BUDGET_US + 299u) / 300u, "steps stop once the grant's time budget is spent");
}

static void test_a_pause_across_the_millisecond_timers_wrap(void) {
    half_t *half = fresh(&left_half);

    half->busy    = true;
    half->idle_at = 1u;
    scan_ms       = (uint32_t)0u - 4u * SCAN_MS;
    check(scan(half) == 1u && half->now_ms == (uint32_t)0u - 4u * SCAN_MS, "the owner's clock before the pause");
    start_a_save(half);
    half->quiet = false;
    for (scan_ms = (uint32_t)0u - 3u * SCAN_MS; scan_ms != 2u * SCAN_MS; scan_ms += SCAN_MS) {
        check(scan(half) == 0u && paused(half), "paused across the wrap");
    }
    half->quiet = true;
    check(scan(half) == 0u && half->chip.erases == 1u, "the erase");
    scan_ms = 3u * SCAN_MS;
    check(scan(half) == 1u && half->now_ms == (uint32_t)0u - 3u * SCAN_MS, "a wrapping timer keeps its differences");
}

// Two halves in one test process: each grant has its own flash, clock and
// pause, and one half's split RPC ends no grant of the other.
static void test_two_grants_keep_their_own_clocks_and_pauses(void) {
    half_t *left  = fresh(&left_half);
    half_t *right = fresh(&right_half);

    left->busy = right->busy = true;
    left->idle_at = right->idle_at = 1u;
    scan_ms                        = 5000u;
    check(scan(left) == 1u && scan(right) == 1u, "both halves step");
    start_a_save(right);
    right->quiet = false;
    for (scan_ms = 5010u; scan_ms <= 9000u; scan_ms += SCAN_MS) {
        check(scan(left) == 1u && left->now_ms == scan_ms, "the running half keeps the scan clock");
        check(scan(right) == 0u && right->now_ms == 5000u, "the waiting half's clock stands still");
        check(!paused(left) && paused(right), "each half's pause is its own");
    }
    right->quiet = true;
    check(scan(right) == 0u && right->chip.erases == 1u && left->chip.erases == 0u, "each half erases its own flash");
    check(scan(right) == 1u && right->now_ms == 5010u && !paused(right), "the waiting half runs on from where it stopped");
    left->idle_at = right->idle_at = 0u;
    left->link_at                  = 1u;
    check(scan(left) == 1u && scan(right) == NOAH_QMK_DURABLE_IO_STEPS_MAX, "one half's split RPC ends only its own grant");
}

int main(void) {
    test_a_grant_before_init_does_nothing();
    test_an_idle_store_gets_one_grant_and_one_step_per_scan();
    test_a_save_waits_for_a_quiet_keyboard_and_its_wait_is_not_the_owners_time();
    test_local_storage_work_repeats_within_the_grant_up_to_its_limit();
    test_a_program_ends_the_grants_steps();
    test_a_step_that_starts_a_save_gets_no_unerased_follow_up();
    test_a_step_that_used_the_split_link_ends_the_grants_steps();
    test_a_step_with_nothing_to_do_or_no_work_in_progress_ends_them();
    test_the_time_budget_ends_them_across_the_flash_clocks_wrap();
    test_a_pause_across_the_millisecond_timers_wrap();
    test_two_grants_keep_their_own_clocks_and_pauses();
    puts("qmk durable-I/O grant tests passed");
    return 0;
}

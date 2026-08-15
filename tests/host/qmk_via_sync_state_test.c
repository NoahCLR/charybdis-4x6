#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "users/noah/lib/compat/qmk_via_sync_state.h"

static uint32_t stored_word;
static uint8_t  write_count;

static void test_fail(const char *expr, const char *file, int line) {
    fprintf(stderr, "test failed: %s (%s:%d)\n", expr, file, line);
    exit(1);
}

#define CHECK(expr)                               \
    do {                                          \
        if (!(expr)) {                            \
            test_fail(#expr, __FILE__, __LINE__); \
        }                                         \
    } while (0)

uint32_t eeconfig_read_user(void) {
    return stored_word;
}

void eeconfig_update_user(uint32_t word) {
    stored_word = word;
    write_count++;
}

static noah_qmk_via_sync_metadata_t decode_stored(void) {
    noah_qmk_via_sync_metadata_t metadata = {0};

    CHECK(noah_qmk_via_sync_metadata_decode(stored_word, &metadata));
    return metadata;
}

static void test_clean_boot_mutation_is_dirty_before_completion(void) {
    noah_qmk_via_sync_state_snapshot_t state;
    noah_qmk_via_sync_metadata_t       stored;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 7u});
    write_count = 0u;
    noah_qmk_via_sync_state_init();

    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.initialized);
    CHECK(!state.recovery_required);
    CHECK(state.metadata.generation == 7u);
    CHECK(!state.metadata.dirty);

    CHECK(noah_qmk_via_sync_state_begin_mutation());
    stored = decode_stored();
    CHECK(write_count == 1u);
    CHECK(stored.generation == 7u);
    CHECK(stored.dirty);

    CHECK(noah_qmk_via_sync_state_complete_mutation(true));
    stored = decode_stored();
    CHECK(write_count == 2u);
    CHECK(stored.generation == 8u);
    CHECK(!stored.dirty);
}

static void test_repeated_mutations_coalesce_one_dirty_write(void) {
    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 12u});
    write_count = 0u;
    noah_qmk_via_sync_state_init();

    CHECK(noah_qmk_via_sync_state_begin_mutation());
    CHECK(noah_qmk_via_sync_state_begin_mutation());
    CHECK(noah_qmk_via_sync_state_begin_mutation());
    CHECK(write_count == 1u);
}

static void test_readback_failure_keeps_dirty_generation(void) {
    noah_qmk_via_sync_metadata_t stored;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 3u});
    write_count = 0u;
    noah_qmk_via_sync_state_init();
    CHECK(noah_qmk_via_sync_state_begin_mutation());

    CHECK(!noah_qmk_via_sync_state_complete_mutation(false));
    stored = decode_stored();
    CHECK(write_count == 1u);
    CHECK(stored.generation == 3u);
    CHECK(stored.dirty);
}

static void test_dirty_or_unknown_boot_requires_recovery(void) {
    noah_qmk_via_sync_state_snapshot_t state;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 9u, .dirty = true});
    write_count = 0u;
    noah_qmk_via_sync_state_init();
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.initialized);
    CHECK(state.recovery_required);
    CHECK(!noah_qmk_via_sync_state_complete_mutation(true));
    CHECK(write_count == 0u);

    stored_word = UINT32_C(0xF0000001);
    noah_qmk_via_sync_state_init();
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(!state.initialized);
    CHECK(state.recovery_required);
    CHECK(noah_qmk_via_sync_state_begin_mutation());
    CHECK(!noah_qmk_via_sync_state_complete_mutation(true));
    CHECK(decode_stored().dirty);
}

static void test_reset_publishes_clean_only_after_defaults_commit(void) {
    noah_qmk_via_sync_state_snapshot_t state;

    stored_word = 0u;
    write_count = 0u;
    noah_qmk_via_sync_state_reset_after_defaults(false);
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.initialized);
    CHECK(state.recovery_required);
    CHECK(state.metadata.generation == 1u);
    CHECK(state.metadata.dirty);

    noah_qmk_via_sync_state_reset_after_defaults(true);
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(!state.recovery_required);
    CHECK(state.metadata.generation == 1u);
    CHECK(!state.metadata.dirty);
    CHECK(write_count == 2u);
}

static void test_reset_during_classified_mutation_stays_dirty_until_readback(void) {
    noah_qmk_via_sync_state_snapshot_t state;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 8u});
    write_count = 0u;
    noah_qmk_via_sync_state_init();
    CHECK(noah_qmk_via_sync_state_begin_mutation());

    noah_qmk_via_sync_state_reset_after_defaults(true);
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.metadata.generation == 1u);
    CHECK(state.metadata.dirty);
    CHECK(!state.recovery_required);
    CHECK(noah_qmk_via_sync_state_complete_mutation(true));
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.metadata.generation == 2u);
    CHECK(!state.metadata.dirty);
}

static void test_completion_wraps_without_publishing_zero(void) {
    noah_qmk_via_sync_metadata_t stored;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK});
    write_count = 0u;
    noah_qmk_via_sync_state_init();
    CHECK(noah_qmk_via_sync_state_begin_mutation());
    CHECK(noah_qmk_via_sync_state_complete_mutation(true));
    stored = decode_stored();
    CHECK(stored.generation == 1u);
    CHECK(!stored.dirty);
}

static void test_remote_apply_is_dirty_until_explicit_accept(void) {
    noah_qmk_via_sync_state_snapshot_t state;

    stored_word = noah_qmk_via_sync_metadata_encode((noah_qmk_via_sync_metadata_t){.generation = 4u});
    write_count = 0u;
    noah_qmk_via_sync_state_init();

    CHECK(noah_qmk_via_sync_state_begin_remote_apply(9u));
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.metadata.generation == 9u);
    CHECK(state.metadata.dirty);
    CHECK(state.recovery_required);

    CHECK(noah_qmk_via_sync_state_accept_remote(9u));
    state = noah_qmk_via_sync_state_snapshot();
    CHECK(state.metadata.generation == 9u);
    CHECK(!state.metadata.dirty);
    CHECK(!state.recovery_required);
    CHECK(write_count == 2u);
    CHECK(!noah_qmk_via_sync_state_begin_remote_apply(0u));
    CHECK(!noah_qmk_via_sync_state_accept_remote(NOAH_QMK_VIA_SYNC_METADATA_GENERATION_MASK + 1u));
}

int main(void) {
    test_clean_boot_mutation_is_dirty_before_completion();
    test_repeated_mutations_coalesce_one_dirty_write();
    test_readback_failure_keeps_dirty_generation();
    test_dirty_or_unknown_boot_requires_recovery();
    test_reset_publishes_clean_only_after_defaults_commit();
    test_reset_during_classified_mutation_stays_dirty_until_readback();
    test_completion_wraps_without_publishing_zero();
    test_remote_apply_is_dirty_until_explicit_accept();

    puts("qmk_via_sync_state host tests passed");
    return 0;
}

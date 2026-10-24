// Working copy (D-F32): splices into domains, staged above the profile,
// checked domain by domain and published in place at a safe boundary.

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/profile_working_copy.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    ACTION_ABI_DIGEST = 0x2468ACE0u,
    CAPACITY          = NOAH_PROFILE_BLOB_V1_MAX_SIZE,
    POSITIONS         = 60u,
    SESSION           = 0x5E55104Eu,
    KEYMAP_KEY        = 2u * 61u, // layer 1, position 1
};

static uint8_t  saved_bytes[CAPACITY];
static uint32_t saved_length;
static uint8_t  working_bytes[CAPACITY];

static noah_profile_validator_v1_compatibility_t rules;
static noah_active_profile_provider_t            provider;
static noah_active_profile_snapshot_t            saved_snapshot;
static noah_profile_working_copy_t               copy;
static uint32_t                                  boundary_reasons;

static struct {
    uint32_t count;
    uint8_t  changed_mask;
    uint8_t  kind;
    uint32_t revision;
    size_t   key_behaviors_base;
    uint8_t  domain_mask;
} invalidations;

static uint32_t safe_boundary(void *context) {
    (void)context;
    return boundary_reasons;
}

static void invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view) {
    (void)context;
    (void)publication;
    (void)previous;
    invalidations.count++;
    invalidations.changed_mask       = view->changed_domain_mask;
    invalidations.kind               = active.kind;
    invalidations.revision           = active.payload_digest;
    invalidations.key_behaviors_base = view->profile.key_behaviors.base_offset;
    invalidations.domain_mask        = view->profile.domain_mask;
}

static void load_fixture(const char *path) {
    FILE *file = fopen(path, "rb");

    assert(file);
    saved_length = (uint32_t)fread(saved_bytes, 1u, sizeof(saved_bytes), file);
    fclose(file);
    assert(saved_length > 64u);
}

static noah_profile_domain_range_t domain_range(const uint8_t *bytes, uint32_t length, uint8_t id) {
    noah_profile_reader_t       reader = noah_profile_reader_from_memory(bytes, length);
    noah_profile_domain_range_t range;

    assert(noah_profile_blob_v1_find_domain(&reader, 0u, length, id, &range));
    return range;
}

static noah_profile_validator_v1_profile_t validate_whole(const uint8_t *bytes, uint32_t length) {
    noah_profile_reader_t                   reader = noah_profile_reader_from_memory(bytes, length);
    noah_profile_validator_v1_t             validator;
    noah_profile_validator_v1_profile_t     profile;
    noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = NOAH_PROFILE_DOMAIN_MASK_ALL,
        .byte_length       = length,
        .crc32             = noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length)),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, length),
        .action_abi_digest = ACTION_ABI_DIGEST,
    };
    noah_profile_validator_v1_result_t result = noah_profile_validator_v1_begin(&validator, &reader, 0u, &declaration, &rules, NULL);

    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_STEP_READ_MAX, NULL);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(noah_profile_validator_v1_profile(&validator, &profile, NULL) == NOAH_PROFILE_VALIDATOR_V1_VALID);
    return profile;
}

static void setup(void) {
    static const noah_profile_reader_t *unused;
    noah_profile_validator_v1_profile_t profile;
    noah_profile_reader_t               reader = noah_profile_reader_from_memory(saved_bytes, saved_length);
    noah_active_profile_invalidator_t invalidator = {.callback = invalidate, .context = NULL};

    (void)unused;
    rules                     = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    rules.placement_positions = POSITIONS;
    profile                   = validate_whole(saved_bytes, saved_length);
    assert(noah_active_profile_snapshot_make_compiled(&profile, &reader, 0u, &saved_snapshot) == NOAH_ACTIVE_PROFILE_OK);
    assert(noah_active_profile_provider_init(&provider, &saved_snapshot, safe_boundary, NULL, &invalidator, 1u) == NOAH_ACTIVE_PROFILE_OK);
    memset(working_bytes, 0xA5, sizeof(working_bytes));
    noah_profile_working_copy_init(&copy, working_bytes, sizeof(working_bytes), &provider, &rules, &rules, SESSION);
    memset(&invalidations, 0, sizeof(invalidations));
    boundary_reasons = 0u;
}

static uint32_t crc_of(const uint8_t *bytes, uint32_t length) {
    return noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, length));
}

static void run_scans(void) {
    for (uint32_t scan = 0u; scan < 4096u && noah_profile_working_copy_busy(&copy); scan++) {
        (void)noah_profile_working_copy_scan(&copy);
    }
}

static noah_profile_working_result_t edit(uint16_t transaction, const noah_profile_working_splice_t *splices, uint8_t count, const uint8_t *inserted, uint32_t inserted_length) {
    noah_profile_working_result_t result = noah_profile_working_copy_begin(&copy, transaction, copy.session, copy.revision, count, inserted_length, 0u, false);

    if (result != NOAH_PROFILE_WORKING_OK) {
        return result;
    }
    for (uint8_t index = 0u; index < count; index++) {
        assert(noah_profile_working_copy_splice(&copy, transaction, index, &splices[index]) == NOAH_PROFILE_WORKING_OK);
    }
    for (uint32_t offset = 0u; offset < inserted_length; offset += 20u) {
        uint32_t chunk = inserted_length - offset < 20u ? inserted_length - offset : 20u;
        assert(noah_profile_working_copy_data(&copy, transaction, offset, inserted + offset, chunk) == NOAH_PROFILE_WORKING_OK);
    }
    result = noah_profile_working_copy_commit(&copy, transaction, crc_of(inserted, inserted_length));
    if (result != NOAH_PROFILE_WORKING_QUEUED) {
        return result;
    }
    run_scans();
    return (noah_profile_working_result_t)copy.last_result;
}

static void active_bytes(uint8_t *bytes, uint32_t *length) {
    noah_active_profile_snapshot_t active;

    assert(noah_active_profile_provider_copy_active(&provider, &active) == NOAH_ACTIVE_PROFILE_OK);
    *length = (uint32_t)active.profile.byte_length;
    assert(noah_active_profile_snapshot_read(&active, 0u, bytes, *length));
}

static void test_keymap_edit_publishes_in_place(void) {
    static uint8_t                expected[CAPACITY];
    static uint8_t                actual[CAPACITY];
    uint32_t                      length;
    noah_profile_domain_range_t   keymap = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_KEYMAP);
    const uint8_t                 key[2] = {0x04u, 0x00u}; // KC_A
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};
    noah_profile_working_status_t status;

    setup();
    assert(edit(1u, &splice, 1u, key, sizeof(key)) == NOAH_PROFILE_WORKING_OK);
    assert(copy.active && copy.revision == 1u);
    assert(invalidations.count == 1u && invalidations.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY && invalidations.changed_mask == NOAH_PROFILE_DOMAIN_MASK_KEYMAP && invalidations.revision == 1u);
    memcpy(expected, saved_bytes, saved_length);
    memcpy(expected + keymap.offset + KEYMAP_KEY, key, sizeof(key));
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, expected, length) == 0);
    assert(memcmp(working_bytes, expected, length) == 0);
    assert(noah_profile_working_copy_status(&copy, &status) && status.active && status.revision == 1u && status.session == SESSION && status.last_transaction_id == 1u && status.last_result == NOAH_PROFILE_WORKING_OK);

    // The saved profile is untouched and is what Revert returns to.
    assert(noah_profile_working_copy_revert(&copy, 2u, SESSION, 1u) == NOAH_PROFILE_WORKING_QUEUED);
    run_scans();
    assert(!copy.active && copy.revision == 2u && copy.last_result == NOAH_PROFILE_WORKING_OK);
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0);
    assert(invalidations.kind == NOAH_ACTIVE_PROFILE_KIND_COMPILED_DEFAULTS && invalidations.changed_mask == NOAH_PROFILE_DOMAIN_MASK_ALL);
    // Nothing to revert: no publication.
    invalidations.count = 0u;
    assert(noah_profile_working_copy_revert(&copy, 3u, SESSION, 2u) == NOAH_PROFILE_WORKING_OK && invalidations.count == 0u && copy.revision == 2u);
    puts("keymap edit publishes in place; revert returns to the saved profile");
}

static void test_stale_and_refused_edits_change_nothing(void) {
    static uint8_t                actual[CAPACITY];
    static uint8_t                before[CAPACITY];
    uint32_t                      length;
    uint32_t                      before_length;
    const uint8_t                 key[3] = {0x04u, 0x00u, 0x05u};
    noah_profile_working_splice_t grow   = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 3u};
    noah_profile_working_splice_t ok     = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};

    setup();
    assert(edit(1u, &ok, 1u, key, 2u) == NOAH_PROFILE_WORKING_OK);
    active_bytes(before, &before_length);
    invalidations.count = 0u;
    // A stale revision is refused at once.
    assert(noah_profile_working_copy_begin(&copy, 2u, SESSION, 0u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_STALE);
    assert(noah_profile_working_copy_begin(&copy, 2u, SESSION + 1u, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_STALE);
    // A keymap one byte too long fails its domain check: nothing changes.
    assert(edit(3u, &grow, 1u, key, sizeof(key)) == NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
    assert(copy.revision == 1u && invalidations.count == 0u);
    active_bytes(actual, &length);
    assert(length == before_length && memcmp(actual, before, length) == 0);
    // A compound edit is all or nothing: a valid keymap cell with a bad one.
    {
        noah_profile_working_splice_t pair[2] = {
            {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 0u, .remove = 2u, .insert = 2u},
            {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 3u},
        };
        const uint8_t bytes[5] = {0x05u, 0x00u, 0x06u, 0x00u, 0x07u};
        assert(edit(4u, pair, 2u, bytes, sizeof(bytes)) == NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
        active_bytes(actual, &length);
        assert(length == before_length && memcmp(actual, before, length) == 0 && copy.revision == 1u);
    }
    // Overlapping or misordered splices are refused before any check.
    {
        noah_profile_working_splice_t pair[2] = {
            {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 4u, .remove = 4u, .insert = 2u},
            {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = 6u, .remove = 2u, .insert = 2u},
        };
        const uint8_t bytes[4] = {0};
        assert(edit(5u, pair, 2u, bytes, sizeof(bytes)) == NOAH_PROFILE_WORKING_INVALID_SPLICE);
    }
    // Retries: the same chunk again is fine, a different one is not, and a
    // wrong checksum ends the edit.
    assert(noah_profile_working_copy_begin(&copy, 6u, SESSION, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_begin(&copy, 6u, SESSION, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_begin(&copy, 7u, SESSION, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_BUSY);
    assert(noah_profile_working_copy_splice(&copy, 6u, 0u, &ok) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_splice(&copy, 6u, 0u, &ok) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_data(&copy, 6u, 0u, key, 2u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_data(&copy, 6u, 0u, key, 2u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_data(&copy, 6u, 0u, key + 1u, 2u) == NOAH_PROFILE_WORKING_OUT_OF_ORDER);
    assert(noah_profile_working_copy_commit(&copy, 6u, crc_of(key, 2u) ^ 1u) == NOAH_PROFILE_WORKING_CHECKSUM_MISMATCH);
    assert(!noah_profile_working_copy_busy(&copy) && copy.revision == 1u);
    // Too large to stage above the profile.
    assert(noah_profile_working_copy_begin(&copy, 8u, SESSION, 1u, 1u, CAPACITY - saved_length + 1u, 0u, false) == NOAH_PROFILE_WORKING_CAPACITY_EXCEEDED);
    // Abort before publication leaves nothing behind.
    assert(noah_profile_working_copy_begin(&copy, 9u, SESSION, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_abort(&copy, 9u) == NOAH_PROFILE_WORKING_OK && !noah_profile_working_copy_busy(&copy) && copy.last_result == NOAH_PROFILE_WORKING_ABORTED);
    puts("stale, invalid, overlapping and aborted edits change nothing");
}

static void test_edit_waits_for_a_safe_boundary(void) {
    const uint8_t                 key[2] = {0x04u, 0x00u};
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};

    setup();
    boundary_reasons = 1u;
    assert(noah_profile_working_copy_begin(&copy, 1u, SESSION, 0u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_splice(&copy, 1u, 0u, &splice) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_data(&copy, 1u, 0u, key, 2u) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_commit(&copy, 1u, crc_of(key, 2u)) == NOAH_PROFILE_WORKING_QUEUED);
    for (uint32_t scan = 0u; scan < 200u; scan++) {
        (void)noah_profile_working_copy_scan(&copy);
    }
    assert(copy.phase == NOAH_PROFILE_WORKING_PHASE_WAITING_SAFE && !copy.active && invalidations.count == 0u && copy.revision == 0u);
    boundary_reasons = 0u;
    run_scans();
    assert(copy.active && copy.revision == 1u && invalidations.count == 1u);
    puts("an edit waits for the safe boundary before it changes anything");
}

static void test_create_and_drop_move_later_domains(void) {
    static uint8_t                actual[CAPACITY];
    static uint8_t                expected[CAPACITY];
    uint32_t                      length;
    noah_profile_domain_range_t   rgb        = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_RGB);
    noah_profile_domain_range_t   behaviours = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_KEY_BEHAVIORS);
    noah_profile_working_splice_t drop       = {.domain_id = NOAH_PROFILE_DOMAIN_V1_RGB, .offset = 0u, .remove = rgb.length, .flags = NOAH_PROFILE_WORKING_SPLICE_DROP};
    noah_profile_working_splice_t create     = {.domain_id = NOAH_PROFILE_DOMAIN_V1_RGB, .insert = rgb.length, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    noah_active_profile_snapshot_t active;
    noah_key_behavior_row_v1_view_t row_before;
    noah_key_behavior_row_v1_view_t row_after;
    noah_profile_codec_v1_error_t codec_error;

    setup();
    assert(noah_active_profile_provider_copy_active(&provider, &active) == NOAH_ACTIVE_PROFILE_OK);
    assert(noah_key_behavior_domain_v1_row_at(&active.profile.key_behaviors, 0u, &row_before, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    // Dropping the first domain moves the behaviours view down by its size
    // and header, and the behaviours still read the same rows.
    assert(edit(1u, &drop, 1u, NULL, 0u) == NOAH_PROFILE_WORKING_OK);
    memcpy(expected, saved_bytes, NOAH_PROFILE_BLOB_V1_HEADER_SIZE);
    expected[6] = (uint8_t)(saved_bytes[6] - 1u);
    memcpy(expected + NOAH_PROFILE_BLOB_V1_HEADER_SIZE, saved_bytes + rgb.offset + rgb.length, saved_length - rgb.offset - rgb.length);
    active_bytes(actual, &length);
    assert(length == saved_length - rgb.length - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE && memcmp(actual, expected, length) == 0);
    assert(invalidations.domain_mask == (NOAH_PROFILE_DOMAIN_MASK_ALL & ~NOAH_PROFILE_DOMAIN_MASK_RGB) && invalidations.key_behaviors_base == behaviours.offset - rgb.length - NOAH_PROFILE_BLOB_V1_DOMAIN_HEADER_SIZE);
    assert(noah_active_profile_provider_copy_active(&provider, &active) == NOAH_ACTIVE_PROFILE_OK);
    assert(noah_key_behavior_domain_v1_row_at(&active.profile.key_behaviors, 0u, &row_after, &codec_error) == NOAH_PROFILE_CODEC_V1_OK && memcmp(&row_before, &row_after, sizeof(row_after)) == 0);
    // Creating it again restores the saved bytes exactly.
    assert(edit(2u, &create, 1u, saved_bytes + rgb.offset, rgb.length) == NOAH_PROFILE_WORKING_OK);
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0 && copy.revision == 2u);
    assert(invalidations.key_behaviors_base == behaviours.offset);
    // The pointing domain is required and cannot be dropped.
    {
        noah_profile_domain_range_t   pd       = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_PD);
        noah_profile_working_splice_t drop_pd  = {.domain_id = NOAH_PROFILE_DOMAIN_V1_PD, .remove = pd.length, .flags = NOAH_PROFILE_WORKING_SPLICE_DROP};
        noah_profile_working_splice_t again    = create;
        assert(edit(3u, &drop_pd, 1u, NULL, 0u) == NOAH_PROFILE_WORKING_INVALID_SPLICE);
        assert(edit(4u, &again, 1u, saved_bytes + rgb.offset, rgb.length) == NOAH_PROFILE_WORKING_INVALID_SPLICE);
    }
    puts("creating and dropping a domain moves the later domains and their views");
}

static void test_resizing_macro_edit_and_working_reads(void) {
    static uint8_t                actual[CAPACITY];
    uint32_t                      length;
    noah_profile_domain_range_t   macros = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_MACROS);
    // Macro 0 gains a leading "x"; then it is taken out again.
    noah_profile_working_splice_t grow   = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .insert = 1u};
    noah_profile_working_splice_t shrink = {.domain_id = NOAH_PROFILE_DOMAIN_V1_MACROS, .offset = 0u, .remove = 1u};
    const uint8_t                 x      = 'x';
    uint8_t                       chunk[25];

    setup();
    assert(edit(1u, &grow, 1u, &x, 1u) == NOAH_PROFILE_WORKING_OK);
    active_bytes(actual, &length);
    assert(length == saved_length + 1u);
    assert(domain_range(actual, length, NOAH_PROFILE_DOMAIN_V1_MACROS).length == macros.length + 1u && actual[macros.offset] == 'x');
    assert(memcmp(actual + macros.offset + 1u, saved_bytes + macros.offset, macros.length) == 0);
    assert(noah_profile_working_copy_read(&copy, 0u, chunk, sizeof(chunk)) && memcmp(chunk, actual, sizeof(chunk)) == 0);
    assert(!noah_profile_working_copy_read(&copy, length - 1u, chunk, 2u));
    assert(edit(2u, &shrink, 1u, NULL, 0u) == NOAH_PROFILE_WORKING_OK);
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0);
    puts("a resizing edit keeps the envelope exact; reads follow the active profile");
}

static void test_replace_needs_the_saved_profile(void) {
    static uint8_t                replacement[CAPACITY];
    static uint8_t                actual[CAPACITY];
    uint32_t                      length;
    noah_profile_domain_range_t   keymap = domain_range(saved_bytes, saved_length, NOAH_PROFILE_DOMAIN_V1_KEYMAP);
    const uint8_t                 key[2] = {0x04u, 0x00u};
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};

    setup();
    memcpy(replacement, saved_bytes, saved_length);
    replacement[keymap.offset] ^= 0x01u;
    assert(noah_profile_working_copy_begin(&copy, 1u, SESSION, 0u, 0u, saved_length, NOAH_PROFILE_WORKING_EDIT_REPLACE, false) == NOAH_PROFILE_WORKING_OK);
    for (uint32_t offset = 0u; offset < saved_length; offset += 20u) {
        uint32_t chunk = saved_length - offset < 20u ? saved_length - offset : 20u;
        assert(noah_profile_working_copy_data(&copy, 1u, offset, replacement + offset, chunk) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_working_copy_commit(&copy, 1u, crc_of(replacement, saved_length)) == NOAH_PROFILE_WORKING_QUEUED);
    run_scans();
    assert(copy.last_result == NOAH_PROFILE_WORKING_OK && copy.active && invalidations.changed_mask == NOAH_PROFILE_DOMAIN_MASK_ALL);
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, replacement, length) == 0);
    // With unsaved edits a replacement is refused; a splice is not.
    assert(noah_profile_working_copy_begin(&copy, 2u, SESSION, 1u, 0u, saved_length, NOAH_PROFILE_WORKING_EDIT_REPLACE, false) == NOAH_PROFILE_WORKING_NOT_CLEAN);
    assert(edit(3u, &splice, 1u, key, sizeof(key)) == NOAH_PROFILE_WORKING_OK && copy.revision == 2u);
    // A replacement that is not a valid profile changes nothing.
    assert(noah_profile_working_copy_revert(&copy, 4u, SESSION, 2u) == NOAH_PROFILE_WORKING_QUEUED);
    run_scans();
    replacement[0] = 'X';
    assert(noah_profile_working_copy_begin(&copy, 5u, SESSION, 3u, 0u, saved_length, NOAH_PROFILE_WORKING_EDIT_REPLACE, false) == NOAH_PROFILE_WORKING_OK);
    for (uint32_t offset = 0u; offset < saved_length; offset += 20u) {
        uint32_t chunk = saved_length - offset < 20u ? saved_length - offset : 20u;
        assert(noah_profile_working_copy_data(&copy, 5u, offset, replacement + offset, chunk) == NOAH_PROFILE_WORKING_OK);
    }
    assert(noah_profile_working_copy_commit(&copy, 5u, crc_of(replacement, saved_length)) == NOAH_PROFILE_WORKING_QUEUED);
    run_scans();
    assert(copy.last_result == NOAH_PROFILE_WORKING_VALIDATION_REJECTED && !copy.active && copy.revision == 3u);
    active_bytes(actual, &length);
    assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0);
    puts("a whole replacement needs the saved profile active and is checked whole");
}

static void test_replaced_by_a_save_or_apply(void) {
    const uint8_t                 key[2] = {0x04u, 0x00u};
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_KEYMAP, .offset = KEYMAP_KEY, .remove = 2u, .insert = 2u};

    setup();
    assert(edit(1u, &splice, 1u, key, sizeof(key)) == NOAH_PROFILE_WORKING_OK);
    assert(noah_profile_working_copy_begin(&copy, 2u, SESSION, 1u, 1u, 2u, 0u, false) == NOAH_PROFILE_WORKING_OK);
    // A save: same content, same revision; the edit in progress is abandoned.
    assert(noah_profile_working_copy_note_replaced(&copy, false));
    assert(!copy.active && copy.revision == 1u && copy.last_result == NOAH_PROFILE_WORKING_ABORTED && copy.last_transaction_id == 2u);
    // An Apply: new content, a new revision.
    assert(!noah_profile_working_copy_note_replaced(&copy, true) && copy.revision == 2u);
    // Another half's identity is taken only while nothing is edited.
    assert(noah_profile_working_copy_adopt(&copy, 7u, 9u) && copy.session == 7u && copy.revision == 9u);
    puts("a save or Apply ends the working copy; the revision says whether content changed");
}

// Every domain: replacing it with itself is accepted and republishes that
// domain alone; the same domain one byte short fails its own check and
// changes nothing (D-F32).
static void test_every_domain_is_checked_alone(void) {
    static uint8_t actual[CAPACITY];
    uint32_t       length;
    uint16_t       transaction = 1u;

    setup();
    for (uint8_t index = 0u; index < NOAH_PROFILE_DOMAIN_REGISTRY_COUNT; index++) {
        const noah_profile_domain_shape_t *shape = noah_profile_domain_at(index);
        noah_profile_domain_range_t        range = domain_range(saved_bytes, saved_length, shape->id);
        noah_profile_working_splice_t      same  = {.domain_id = shape->id, .offset = 0u, .remove = range.length, .insert = range.length};
        noah_profile_working_splice_t      short_by_one = {.domain_id = shape->id, .offset = range.length - 1u, .remove = 1u};
        uint32_t                           revision     = copy.revision;
        uint32_t                           count        = invalidations.count;

        assert(range.length != 0u);
        assert(edit(transaction++, &same, 1u, saved_bytes + range.offset, range.length) == NOAH_PROFILE_WORKING_OK);
        assert(copy.revision == revision + 1u && invalidations.count == count + 1u && invalidations.changed_mask == shape->mask);
        active_bytes(actual, &length);
        assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0);
        assert(edit(transaction++, &short_by_one, 1u, NULL, 0u) == NOAH_PROFILE_WORKING_VALIDATION_REJECTED);
        assert(copy.revision == revision + 1u && invalidations.count == count + 1u && copy.error.domain_id == shape->id);
        active_bytes(actual, &length);
        assert(length == saved_length && memcmp(actual, saved_bytes, length) == 0);
    }
    puts("each of the seven domains is checked on its own, accepted whole and refused one byte short");
}

int main(int argc, char **argv) {
    assert(argc == 2);
    load_fixture(argv[1]);
    test_keymap_edit_publishes_in_place();
    test_every_domain_is_checked_alone();
    test_stale_and_refused_edits_change_nothing();
    test_edit_waits_for_a_safe_boundary();
    test_create_and_drop_move_later_domains();
    test_resizing_macro_edit_and_working_reads();
    test_replace_needs_the_saved_profile();
    test_replaced_by_a_save_or_apply();
    return 0;
}

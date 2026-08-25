#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "users/noah/lib/profile/runtime/effective_profile_provider.h"
#include "users/noah/lib/profile/storage/profile_checksum.h"

enum {
    ACTION_ABI_DIGEST = 0x12345678u,
    REASON_KEYS_HELD  = 1u << 0,
    REASON_MACRO_BUSY = 1u << 3,
};

static const uint8_t canonical_empty_profile[] = {'N', 'L', 'P', '1', 1u, 0u, 0u, 1u};
static const uint8_t canonical_behavior_profile[] =
    "\x4e\x4c\x50\x31\x01\x00\x01\x01\x20\x01\x3a\x00\x02\x03\x00\x00"
    "\x20\x00\x01\x00\x34\x12\x96\x00\x90\x01\xaf\x00\x01\x02\x00\x02"
    "\x03\x19\x01\x00\x28\x00\x02\x05\x06\x00\x0a\x00\x02\x00\x03\x00"
    "\x03\x00\x12\x00\x04\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x01"
    "\x01\x01\x07\x00\x02\x00";

typedef struct {
    uint8_t prefix[5];
    uint8_t profile[sizeof(canonical_empty_profile)];
    uint8_t suffix[3];
} backing_t;

typedef struct {
    uint8_t prefix[5];
    uint8_t profile[sizeof(canonical_behavior_profile) - 1u];
    uint8_t suffix[3];
} behavior_backing_t;

typedef struct {
    uint8_t entries[32];
    uint8_t count;
} invalidation_log_t;

typedef struct {
    noah_effective_profile_provider_t *provider;
    invalidation_log_t                *log;
    uint8_t                            callback_id;
    uint32_t                           expected_publication;
    uint32_t                           expected_previous_generation;
    uint32_t                           expected_active_generation;
    bool                               saw_active_generation;
    bool                               saw_pending_clear;
    bool                               saw_reentrant_guard;
    bool                               saw_public_reader_guard;
    bool                               saw_callback_view;
} invalidation_context_t;

typedef struct {
    noah_effective_profile_provider_t *provider;
    uint32_t                           reason_mask;
    bool                               saw_cancel_guard;
    bool                               saw_poll_guard;
    bool                               saw_status_during_callback;
} safe_boundary_context_t;

static uint32_t safe_boundary_reason_mask;

static uint32_t safe_boundary(void *context) {
    uint32_t *reason_mask = context;

    return *reason_mask;
}

static uint32_t reentrant_safe_boundary(void *context) {
    safe_boundary_context_t          *boundary = context;
    noah_effective_profile_status_t   status;

    boundary->saw_cancel_guard = noah_effective_profile_provider_cancel_pending(boundary->provider) == NOAH_EFFECTIVE_PROFILE_REENTRANT;
    boundary->saw_poll_guard   = noah_effective_profile_provider_poll(boundary->provider) == NOAH_EFFECTIVE_PROFILE_REENTRANT;
    assert(noah_effective_profile_provider_status(boundary->provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    boundary->saw_status_during_callback = status.safe_boundary_evaluation_in_progress && status.has_pending;
    return boundary->reason_mask;
}

static void invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *callback_view) {
    invalidation_context_t            *invalidation = context;
    noah_effective_profile_snapshot_t  copied;
    noah_effective_profile_status_t    status;
    uint8_t                            magic[4];

    assert(publication_count == invalidation->expected_publication);
    assert(previous.generation == invalidation->expected_previous_generation);
    assert(active.generation == invalidation->expected_active_generation);
    assert(invalidation->log->count < sizeof(invalidation->log->entries));
    invalidation->log->entries[invalidation->log->count++] = invalidation->callback_id;
    assert(noah_effective_profile_provider_copy_active(invalidation->provider, &copied) == NOAH_EFFECTIVE_PROFILE_BUSY);
    assert(noah_effective_profile_provider_status(invalidation->provider, &status) == NOAH_EFFECTIVE_PROFILE_BUSY);
    invalidation->saw_public_reader_guard = true;
    invalidation->saw_callback_view       = callback_view && callback_view->identity.generation == active.generation && callback_view->identity.payload_digest == active.payload_digest && noah_effective_profile_snapshot_read(callback_view, 0u, magic, sizeof(magic)) && memcmp(magic, "NLP1", sizeof(magic)) == 0;
    invalidation->saw_active_generation   = invalidation->saw_callback_view;
    invalidation->saw_pending_clear       = !invalidation->provider->has_pending && invalidation->provider->publication_in_progress;
    invalidation->saw_reentrant_guard = noah_effective_profile_provider_request_compiled_fallback(invalidation->provider) == NOAH_EFFECTIVE_PROFILE_REENTRANT;
}

static noah_profile_validator_v1_profile_t validate_profile(const noah_profile_reader_t *reader, size_t base_offset, uint16_t byte_length, uint8_t domain_mask) {
    noah_profile_validator_v1_compatibility_t compatibility = noah_profile_validator_v1_default_compatibility(ACTION_ABI_DIGEST);
    noah_profile_validator_v1_declaration_t   declaration;
    noah_profile_validator_v1_error_t         error = noah_profile_validator_v1_no_error();
    noah_profile_validator_v1_t               validator;
    noah_profile_validator_v1_profile_t       profile;
    noah_profile_validator_v1_result_t        result;
    uint8_t                                   bytes[NOAH_PROFILE_BLOB_V1_MAX_SIZE];
    uint32_t                                  crc_state;

    assert(byte_length <= sizeof(bytes));
    assert(noah_profile_reader_read(reader, base_offset, bytes, byte_length));
    crc_state = noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, byte_length);
    declaration = (noah_profile_validator_v1_declaration_t){
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = domain_mask,
        .flags             = 0u,
        .byte_length       = byte_length,
        .crc32             = noah_profile_crc32_finish(crc_state),
        .digest            = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, bytes, byte_length),
        .action_abi_digest = ACTION_ABI_DIGEST,
    };
    compatibility.required_domain_mask = domain_mask;
    compatibility.allowed_domain_mask  = domain_mask;
    result = noah_profile_validator_v1_begin(&validator, reader, base_offset, &declaration, &compatibility, &error);
    while (result == NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        result = noah_profile_validator_v1_step(&validator, NOAH_PROFILE_VALIDATOR_V1_CHECKSUM_CHUNK_MAX, &error);
    }
    assert(result == NOAH_PROFILE_VALIDATOR_V1_VALID);
    assert(noah_profile_validator_v1_profile(&validator, &profile, &error) == NOAH_PROFILE_VALIDATOR_V1_VALID);
    return profile;
}

static void init_backing(backing_t *backing, uint8_t marker) {
    memset(backing, marker, sizeof(*backing));
    memcpy(backing->profile, canonical_empty_profile, sizeof(canonical_empty_profile));
}

static noah_effective_profile_snapshot_t make_compiled(backing_t *backing) {
    noah_profile_reader_t                reader = noah_profile_reader_from_memory((const uint8_t *)backing, sizeof(*backing));
    noah_profile_validator_v1_profile_t  profile = validate_profile(&reader, offsetof(backing_t, profile), sizeof(backing->profile), 0u);
    noah_effective_profile_snapshot_t    snapshot;

    assert(noah_effective_profile_snapshot_make_compiled(&profile, &reader, offsetof(backing_t, profile), &snapshot) == NOAH_EFFECTIVE_PROFILE_OK);
    return snapshot;
}

static noah_effective_profile_snapshot_t make_live(backing_t *backing, uint32_t generation, uint8_t origin, uint32_t compiled_default_digest) {
    noah_profile_reader_t                reader = noah_profile_reader_from_memory((const uint8_t *)backing, sizeof(*backing));
    noah_profile_validator_v1_profile_t  profile = validate_profile(&reader, offsetof(backing_t, profile), sizeof(backing->profile), 0u);
    noah_effective_profile_snapshot_t    snapshot;

    assert(noah_effective_profile_snapshot_make_validated(&profile, &reader, offsetof(backing_t, profile), generation, origin, compiled_default_digest, &snapshot) == NOAH_EFFECTIVE_PROFILE_OK);
    return snapshot;
}

static noah_effective_profile_snapshot_t make_live_behavior(behavior_backing_t *backing, uint32_t generation, uint8_t origin, uint32_t compiled_default_digest) {
    noah_profile_reader_t                reader = noah_profile_reader_from_memory((const uint8_t *)backing, sizeof(*backing));
    noah_profile_validator_v1_profile_t  profile = validate_profile(&reader, offsetof(behavior_backing_t, profile), sizeof(backing->profile), NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    noah_effective_profile_snapshot_t    snapshot;

    assert(noah_effective_profile_snapshot_make_validated(&profile, &reader, offsetof(behavior_backing_t, profile), generation, origin, compiled_default_digest, &snapshot) == NOAH_EFFECTIVE_PROFILE_OK);
    return snapshot;
}

static noah_effective_profile_backing_t make_backing(void *bytes, size_t length) {
    return (noah_effective_profile_backing_t){
        .reader      = noah_profile_reader_from_memory(bytes, length),
        .base_offset = 0u,
        .byte_length = length,
    };
}

static void expect_active(const noah_effective_profile_provider_t *provider, noah_effective_profile_kind_t kind, uint32_t generation, uint8_t origin, uint32_t digest) {
    noah_effective_profile_status_t status;

    assert(noah_effective_profile_provider_status(provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.active.kind == (uint8_t)kind);
    assert(status.active.generation == generation);
    assert(status.active.origin == origin);
    assert(status.active.payload_digest == digest);
}

static void test_initial_compiled_snapshot_and_fail_closed_copies(void) {
    backing_t                           compiled_backing;
    noah_effective_profile_snapshot_t   compiled;
    noah_effective_profile_snapshot_t   copied;
    noah_effective_profile_snapshot_t   copied_again;
    noah_effective_profile_provider_t   provider;
    noah_effective_profile_status_t     status;
    uint8_t                             bytes[sizeof(canonical_empty_profile)];

    init_backing(&compiled_backing, 0x11u);
    compiled = make_compiled(&compiled_backing);
    assert(noah_effective_profile_provider_init(&provider, &compiled, NULL, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS, 0u, NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED, compiled.profile.digest);

    assert(noah_effective_profile_provider_copy_active(&provider, &copied) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(copied.reader.context == &compiled_backing);
    assert(noah_effective_profile_snapshot_read(&copied, 0u, bytes, sizeof(bytes)));
    assert(memcmp(bytes, canonical_empty_profile, sizeof(bytes)) == 0);
    assert(!noah_effective_profile_snapshot_read(&copied, sizeof(bytes), bytes, 1u));

    copied.identity.generation = 99u;
    copied.reader.context      = NULL;
    copied.profile.digest      = 0u;
    assert(noah_effective_profile_provider_copy_active(&provider, &copied_again) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(copied_again.identity.generation == 0u);
    assert(copied_again.reader.context == &compiled_backing);
    assert(copied_again.profile.digest == compiled.profile.digest);

    provider.publication_sequence = 1u;
    assert(noah_effective_profile_provider_copy_active(&provider, &copied) == NOAH_EFFECTIVE_PROFILE_BUSY);
    assert(copied.identity.generation == 99u);
    memset(&status, 0xa5, sizeof(status));
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_BUSY);
    assert(status.active.generation == 0xa5a5a5a5u);
    provider.publication_sequence = 2u;
}

static void test_nested_domain_views_must_stay_inside_blob(void) {
    backing_t                          compiled_backing;
    behavior_backing_t                 live_backing;
    noah_profile_reader_t              reader;
    noah_profile_validator_v1_profile_t profile;
    noah_effective_profile_snapshot_t  snapshot;

    init_backing(&compiled_backing, 0x18u);
    memset(&live_backing, 0x19u, sizeof(live_backing));
    memcpy(live_backing.profile, canonical_behavior_profile, sizeof(live_backing.profile));
    reader  = noah_profile_reader_from_memory((const uint8_t *)&live_backing, sizeof(live_backing));
    profile = validate_profile(&reader, offsetof(behavior_backing_t, profile), sizeof(live_backing.profile), NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);

    profile.key_behaviors.base_offset = 0u;
    assert(noah_effective_profile_snapshot_make_validated(&profile, &reader, offsetof(behavior_backing_t, profile), 1u, 0u, make_compiled(&compiled_backing).identity.payload_digest, &snapshot) == NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT);

    profile = validate_profile(&reader, offsetof(behavior_backing_t, profile), sizeof(live_backing.profile), NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS);
    profile.key_behaviors.base_offset = offsetof(behavior_backing_t, profile) + sizeof(live_backing.profile);
    profile.key_behaviors.byte_length = 1u;
    assert(noah_effective_profile_snapshot_make_validated(&profile, &reader, offsetof(behavior_backing_t, profile), 1u, 0u, make_compiled(&compiled_backing).identity.payload_digest, &snapshot) == NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT);
}

static void test_safe_boundary_reentrancy_and_missing_behavior_predicate(void) {
    backing_t                           compiled_backing;
    backing_t                           live_backing;
    behavior_backing_t                  behavior_backing;
    noah_effective_profile_snapshot_t   compiled;
    noah_effective_profile_snapshot_t   live;
    noah_effective_profile_snapshot_t   behavior;
    noah_effective_profile_provider_t   provider;
    noah_effective_profile_status_t     status;
    safe_boundary_context_t             boundary;

    init_backing(&compiled_backing, 0x1au);
    init_backing(&live_backing, 0x1bu);
    memset(&behavior_backing, 0x1cu, sizeof(behavior_backing));
    memcpy(behavior_backing.profile, canonical_behavior_profile, sizeof(behavior_backing.profile));
    compiled = make_compiled(&compiled_backing);
    live     = make_live(&live_backing, 2u, 0u, compiled.identity.payload_digest);
    behavior = make_live_behavior(&behavior_backing, 3u, 1u, compiled.identity.payload_digest);

    memset(&boundary, 0, sizeof(boundary));
    boundary.provider    = &provider;
    boundary.reason_mask = REASON_KEYS_HELD;
    assert(noah_effective_profile_provider_init(&provider, &compiled, reentrant_safe_boundary, &boundary, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &live) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING);
    assert(boundary.saw_cancel_guard);
    assert(boundary.saw_poll_guard);
    assert(boundary.saw_status_during_callback);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.has_pending);
    assert(!status.safe_boundary_evaluation_in_progress);
    boundary.reason_mask = 0u;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);

    assert(noah_effective_profile_provider_init(&provider, &compiled, NULL, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &behavior) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_SAFE_BOUNDARY_REQUIRED);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.has_pending);
    assert(status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS);
    assert(noah_effective_profile_provider_cancel_pending(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
}

static void test_safe_wait_and_exactly_once_ordered_publication(void) {
    backing_t                           compiled_backing;
    backing_t                           live_backing;
    noah_effective_profile_snapshot_t   compiled;
    noah_effective_profile_snapshot_t   live;
    noah_effective_profile_snapshot_t   copied;
    noah_effective_profile_provider_t   provider;
    noah_effective_profile_status_t     status;
    invalidation_context_t              contexts[3];
    noah_effective_profile_invalidator_t invalidators[3];
    invalidation_log_t                  log;
    size_t                              index;

    init_backing(&compiled_backing, 0x21u);
    init_backing(&live_backing, 0x22u);
    compiled = make_compiled(&compiled_backing);
    live     = make_live(&live_backing, 7u, 1u, compiled.identity.payload_digest);
    memset(contexts, 0, sizeof(contexts));
    memset(&log, 0, sizeof(log));
    for (index = 0u; index < 3u; index++) {
        contexts[index].provider             = &provider;
        contexts[index].log                  = &log;
        contexts[index].callback_id          = (uint8_t)(index + 1u);
        contexts[index].expected_publication = 1u;
        contexts[index].expected_previous_generation = 0u;
        contexts[index].expected_active_generation   = 7u;
        invalidators[index] = (noah_effective_profile_invalidator_t){
            .callback = invalidate,
            .context  = &contexts[index],
        };
    }
    safe_boundary_reason_mask = REASON_KEYS_HELD | REASON_MACRO_BUSY;
    assert(noah_effective_profile_provider_init(&provider, &compiled, safe_boundary, &safe_boundary_reason_mask, invalidators, 3u) == NOAH_EFFECTIVE_PROFILE_OK);

    // The provider owns a copy of its callback table and staged snapshot.
    assert(noah_effective_profile_provider_request_validated(&provider, &live) == NOAH_EFFECTIVE_PROFILE_OK);
    invalidators[0].callback = NULL;
    live.identity.generation = 91u;
    live.reader.context      = NULL;
    live.profile.digest      = 0u;

    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.has_pending);
    assert(status.pending.generation == 7u);
    assert(status.pending.origin == 1u);
    assert(status.pending.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE);
    assert(status.pending.payload_digest == compiled.identity.payload_digest);
    assert(status.pending.compiled_default_digest == compiled.identity.payload_digest);
    assert(status.pending.action_abi_digest == ACTION_ABI_DIGEST);

    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.safe_boundary_reason_mask == (REASON_KEYS_HELD | REASON_MACRO_BUSY));
    assert(status.publication_count == 0u);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS, 0u, NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED, compiled.identity.payload_digest);

    safe_boundary_reason_mask = 0u;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_NO_PENDING);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, 7u, 1u, compiled.identity.payload_digest);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(!status.has_pending);
    assert(status.publication_count == 1u);
    assert(status.safe_boundary_reason_mask == 0u);
    assert(status.rollback_available);
    assert(log.count == 3u);
    assert(log.entries[0] == 1u && log.entries[1] == 2u && log.entries[2] == 3u);
    for (index = 0u; index < 3u; index++) {
        assert(contexts[index].saw_active_generation);
        assert(contexts[index].saw_pending_clear);
        assert(contexts[index].saw_reentrant_guard);
        assert(contexts[index].saw_public_reader_guard);
        assert(contexts[index].saw_callback_view);
    }
    assert(noah_effective_profile_provider_copy_active(&provider, &copied) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(copied.reader.context == &live_backing);
}

static void test_rollback_fallback_and_retained_snapshot_immutability(void) {
    backing_t                          compiled_backing;
    behavior_backing_t                 first_backing;
    behavior_backing_t                 second_backing;
    noah_effective_profile_snapshot_t  compiled;
    noah_effective_profile_snapshot_t  first;
    noah_effective_profile_snapshot_t  second;
    noah_effective_profile_snapshot_t  retained_first;
    noah_effective_profile_snapshot_t  active;
    noah_effective_profile_provider_t  provider;
    noah_effective_profile_status_t    status;
    noah_key_behavior_row_v1_view_t    row;
    noah_profile_codec_v1_error_t      codec_error;
    uint8_t                            bytes[sizeof(canonical_behavior_profile) - 1u];

    init_backing(&compiled_backing, 0x31u);
    memset(&first_backing, 0x32u, sizeof(first_backing));
    memset(&second_backing, 0x33u, sizeof(second_backing));
    memcpy(first_backing.profile, canonical_behavior_profile, sizeof(first_backing.profile));
    memcpy(second_backing.profile, canonical_behavior_profile, sizeof(second_backing.profile));
    compiled = make_compiled(&compiled_backing);
    first    = make_live_behavior(&first_backing, 10u, 0u, compiled.identity.payload_digest);
    second   = make_live_behavior(&second_backing, 11u, 1u, compiled.identity.payload_digest);
    safe_boundary_reason_mask = 0u;
    assert(noah_effective_profile_provider_init(&provider, &compiled, safe_boundary, &safe_boundary_reason_mask, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_ROLLBACK);

    assert(noah_effective_profile_provider_request_validated(&provider, &first) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_copy_active(&provider, &retained_first) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, 11u, 1u, second.identity.payload_digest);

    // A copied interaction view is a value, not an active-provider pointer.
    assert(retained_first.identity.generation == 10u);
    assert(retained_first.reader.context == &first_backing);
    assert(retained_first.profile.key_behaviors.reader.context == &first_backing);
    assert(noah_key_behavior_domain_v1_row_at(&retained_first.profile.key_behaviors, 0u, &row, &codec_error) == NOAH_PROFILE_CODEC_V1_OK);
    assert(row.target.kind == NOAH_PROFILE_ACTION_V1_QMK_KEYCODE && row.target.operand == 0x1234u);
    assert(noah_effective_profile_snapshot_read(&retained_first, 0u, bytes, sizeof(bytes)));
    assert(memcmp(bytes, canonical_behavior_profile, sizeof(bytes)) == 0);
    retained_first.identity.generation = 500u;
    retained_first.reader.context      = NULL;
    assert(noah_effective_profile_provider_copy_active(&provider, &active) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(active.identity.generation == 11u);
    assert(active.reader.context == &second_backing);
    assert(active.profile.key_behaviors.reader.context == &second_backing);

    safe_boundary_reason_mask = REASON_KEYS_HELD;
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_WAITING);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, 11u, 1u, second.identity.payload_digest);
    safe_boundary_reason_mask = 0u;
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, 10u, 0u, first.identity.payload_digest);

    assert(noah_effective_profile_provider_request_compiled_fallback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS, 0u, NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED, compiled.identity.payload_digest);
    assert(noah_effective_profile_provider_request_compiled_fallback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_CHANGE);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.publication_count == 4u);
}

static void test_rejection_busy_and_cancel_contracts(void) {
    backing_t                          compiled_backing;
    backing_t                          live_backing;
    noah_effective_profile_snapshot_t  compiled;
    noah_effective_profile_snapshot_t  first;
    noah_effective_profile_snapshot_t  second;
    noah_effective_profile_snapshot_t  invalid;
    noah_effective_profile_provider_t  provider;

    init_backing(&compiled_backing, 0x41u);
    init_backing(&live_backing, 0x42u);
    compiled = make_compiled(&compiled_backing);
    first    = make_live(&live_backing, 4u, 0u, compiled.identity.payload_digest);
    second   = make_live(&live_backing, 5u, 0u, compiled.identity.payload_digest);
    assert(noah_effective_profile_provider_init(&provider, &compiled, NULL, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);

    invalid = first;
    invalid.identity.compiled_default_digest ^= 1u;
    assert(noah_effective_profile_provider_request_validated(&provider, &invalid) == NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT);
    invalid = first;
    invalid.identity.action_abi_digest ^= 1u;
    invalid.profile.action_abi_digest = invalid.identity.action_abi_digest;
    assert(noah_effective_profile_provider_request_validated(&provider, &invalid) == NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT);

    assert(noah_effective_profile_provider_request_validated(&provider, &first) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_BUSY);
    assert(noah_effective_profile_provider_cancel_pending(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_cancel_pending(&provider) == NOAH_EFFECTIVE_PROFILE_NO_PENDING);
    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_request_validated(&provider, &first) == NOAH_EFFECTIVE_PROFILE_STALE_GENERATION);
}

static void test_discard_rollback_before_backing_reuse(void) {
    backing_t                          compiled_backing;
    behavior_backing_t                 first_backing;
    behavior_backing_t                 second_backing;
    noah_effective_profile_snapshot_t  compiled;
    noah_effective_profile_snapshot_t  first;
    noah_effective_profile_snapshot_t  second;
    noah_effective_profile_snapshot_t  active;
    noah_effective_profile_provider_t  provider;
    noah_effective_profile_status_t    status;

    init_backing(&compiled_backing, 0x51u);
    memset(&first_backing, 0x52u, sizeof(first_backing));
    memset(&second_backing, 0x53u, sizeof(second_backing));
    memcpy(first_backing.profile, canonical_behavior_profile, sizeof(first_backing.profile));
    memcpy(second_backing.profile, canonical_behavior_profile, sizeof(second_backing.profile));
    compiled = make_compiled(&compiled_backing);
    first    = make_live_behavior(&first_backing, 20u, 0u, compiled.identity.payload_digest);
    second   = make_live_behavior(&second_backing, 21u, 1u, compiled.identity.payload_digest);
    safe_boundary_reason_mask = 0u;
    assert(noah_effective_profile_provider_init(&provider, &compiled, safe_boundary, &safe_boundary_reason_mask, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_validated(&provider, &first) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);

    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.rollback_available);
    assert(status.active.generation == 21u);
    assert(noah_effective_profile_provider_discard_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(!status.rollback_available);
    assert(status.active.generation == 21u);
    assert(status.publication_count == 2u);
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_ROLLBACK);
    assert(noah_effective_profile_provider_copy_active(&provider, &active) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(active.identity.generation == 21u);
    assert(active.reader.context == &second_backing);
    assert(noah_effective_profile_provider_discard_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_CHANGE);

    // A staged operation blocks backing reuse until it is published or
    // cancelled, even when a rollback snapshot exists.
    assert(noah_effective_profile_provider_request_compiled_fallback(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_discard_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_BUSY);
    assert(noah_effective_profile_provider_cancel_pending(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    expect_active(&provider, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, 21u, 1u, second.identity.payload_digest);
}

static void test_backing_reuse_requires_explicit_active_aware_reservation(void) {
    backing_t                          compiled_backing;
    backing_t                          first_backing;
    backing_t                          second_backing;
    noah_effective_profile_snapshot_t  compiled;
    noah_effective_profile_snapshot_t  first;
    noah_effective_profile_snapshot_t  second;
    noah_effective_profile_snapshot_t  same_active_backing;
    noah_effective_profile_snapshot_t  compiled_as_live;
    noah_effective_profile_backing_t   compiled_range;
    noah_effective_profile_backing_t   first_range;
    noah_effective_profile_backing_t   second_range;
    noah_effective_profile_provider_t  provider;
    noah_effective_profile_status_t    status;

    init_backing(&compiled_backing, 0x61u);
    init_backing(&first_backing, 0x62u);
    init_backing(&second_backing, 0x63u);
    compiled            = make_compiled(&compiled_backing);
    first               = make_live(&first_backing, 30u, 0u, compiled.identity.payload_digest);
    second              = make_live(&second_backing, 31u, 1u, compiled.identity.payload_digest);
    same_active_backing = make_live(&second_backing, 32u, 1u, compiled.identity.payload_digest);
    assert(noah_effective_profile_snapshot_make_validated(&compiled.profile, &compiled.reader, compiled.base_offset, 33u, 0u, compiled.identity.payload_digest, &compiled_as_live) == NOAH_EFFECTIVE_PROFILE_OK);
    compiled_range      = make_backing(&compiled_backing, sizeof(compiled_backing));
    first_range         = make_backing(&first_backing, sizeof(first_backing));
    second_range        = make_backing(&second_backing, sizeof(second_backing));

    assert(noah_effective_profile_provider_init(&provider, &compiled, NULL, NULL, NULL, 0u) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &compiled_range) == NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED);
    assert(noah_effective_profile_provider_request_validated(&provider, &first) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &first_range) == NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED);

    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &second_range) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.backing_reuse_in_progress);
    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS);
    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &second_range) == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS);
    assert(noah_effective_profile_provider_end_backing_reuse(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_end_backing_reuse(&provider) == NOAH_EFFECTIVE_PROFILE_NO_BACKING_REUSE);

    assert(noah_effective_profile_provider_request_validated(&provider, &second) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_poll(&provider) == NOAH_EFFECTIVE_PROFILE_PUBLISHED);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.rollback_available);
    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &first_range) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_status(&provider, &status) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(status.backing_reuse_in_progress);
    assert(!status.rollback_available);
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS);
    assert(noah_effective_profile_provider_end_backing_reuse(&provider) == NOAH_EFFECTIVE_PROFILE_OK);
    assert(noah_effective_profile_provider_request_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_ROLLBACK);

    // Removing a rollback option never releases the active generation's
    // backing, and bypassing the reservation protocol cannot stage over it.
    assert(noah_effective_profile_provider_discard_rollback(&provider) == NOAH_EFFECTIVE_PROFILE_NO_CHANGE);
    assert(noah_effective_profile_provider_begin_backing_reuse(&provider, &second_range) == NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED);
    assert(noah_effective_profile_provider_request_validated(&provider, &same_active_backing) == NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED);
    assert(noah_effective_profile_provider_request_validated(&provider, &compiled_as_live) == NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED);
}

int main(void) {
    test_initial_compiled_snapshot_and_fail_closed_copies();
    test_nested_domain_views_must_stay_inside_blob();
    test_safe_boundary_reentrancy_and_missing_behavior_predicate();
    test_safe_wait_and_exactly_once_ordered_publication();
    test_rollback_fallback_and_retained_snapshot_immutability();
    test_rejection_busy_and_cancel_contracts();
    test_discard_rollback_before_backing_reuse();
    test_backing_reuse_requires_explicit_active_aware_reservation();
    puts("effective profile provider tests passed");
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Generation-Owned Effective Profile Provider
// ────────────────────────────────────────────────────────────────────────────

#include "effective_profile_provider.h"

#include <limits.h>
#include <string.h>

static bool reader_equal(const noah_profile_reader_t *left, const noah_profile_reader_t *right) {
    return left && right && left->read == right->read && left->context == right->context && left->length == right->length;
}

static bool reader_range_valid(const noah_profile_reader_t *reader, size_t base_offset, size_t length) {
    return reader && reader->read && base_offset <= reader->length && length <= reader->length - base_offset;
}

static bool range_contained(size_t outer_offset, size_t outer_length, size_t inner_offset, size_t inner_length) {
    return inner_offset >= outer_offset && inner_offset - outer_offset <= outer_length && inner_length <= outer_length - (inner_offset - outer_offset);
}

static bool ranges_overlap(size_t left_offset, size_t left_length, size_t right_offset, size_t right_length) {
    return left_length != 0u && right_length != 0u && left_offset < right_offset + right_length && right_offset < left_offset + left_length;
}

static uint8_t domain_count(uint8_t mask) {
    uint8_t count = 0u;

    while (mask != 0u) {
        count = (uint8_t)(count + (mask & 1u));
        mask >>= 1u;
    }
    return count;
}

static bool profile_view_valid(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset) {
    if (!profile || !reader_range_valid(reader, base_offset, profile ? profile->byte_length : 0u) || profile->byte_length < NOAH_PROFILE_BLOB_V1_HEADER_SIZE || profile->byte_length > NOAH_PROFILE_BLOB_V1_MAX_SIZE || (profile->domain_mask & (uint8_t)~NOAH_PROFILE_VALIDATOR_V1_KNOWN_DOMAINS) != 0u || profile->domain_count != domain_count(profile->domain_mask)) {
        return false;
    }
    if ((profile->domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) != 0u && (!reader_equal(reader, &profile->rgb.reader) || !reader_range_valid(reader, profile->rgb.base_offset, profile->rgb.byte_length) || !range_contained(base_offset, profile->byte_length, profile->rgb.base_offset, profile->rgb.byte_length))) {
        return false;
    }
    if ((profile->domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS) != 0u && (!reader_equal(reader, &profile->key_behaviors.reader) || !reader_range_valid(reader, profile->key_behaviors.base_offset, profile->key_behaviors.byte_length) || !range_contained(base_offset, profile->byte_length, profile->key_behaviors.base_offset, profile->key_behaviors.byte_length))) {
        return false;
    }
    if ((profile->domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS) && (profile->combos.row_count > 32u || !range_contained(base_offset, profile->byte_length, base_offset + profile->combos.payload_offset, 4u + (size_t)profile->combos.row_count * 28u))) return false;
    return true;
}

static bool identity_equal(const noah_effective_profile_identity_t *left, const noah_effective_profile_identity_t *right) {
    return left && right && left->generation == right->generation && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest && left->origin == right->origin && left->kind == right->kind;
}

static bool snapshot_valid(const noah_effective_profile_snapshot_t *snapshot) {
    if (!snapshot || !profile_view_valid(&snapshot->profile, &snapshot->reader, snapshot->base_offset) || snapshot->identity.payload_crc32 != snapshot->profile.crc32 || snapshot->identity.payload_digest != snapshot->profile.digest || snapshot->identity.action_abi_digest != snapshot->profile.action_abi_digest) {
        return false;
    }
    if (snapshot->identity.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS) {
        return snapshot->identity.generation == 0u && snapshot->identity.origin == NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED && snapshot->identity.compiled_default_digest == snapshot->profile.digest;
    }
    return snapshot->identity.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE && snapshot->identity.generation != 0u && snapshot->identity.origin <= 1u;
}

static noah_effective_profile_result_t make_snapshot(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset, uint32_t generation, uint8_t origin, uint32_t compiled_default_digest, noah_effective_profile_kind_t kind, noah_effective_profile_snapshot_t *snapshot) {
    noah_effective_profile_snapshot_t result;

    if (!snapshot) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (!profile_view_valid(profile, reader, base_offset)) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT;
    }
    memset(&result, 0, sizeof(result));
    result.identity = (noah_effective_profile_identity_t){
        .generation              = generation,
        .payload_crc32           = profile->crc32,
        .payload_digest          = profile->digest,
        .compiled_default_digest = compiled_default_digest,
        .action_abi_digest       = profile->action_abi_digest,
        .origin                  = origin,
        .kind                    = (uint8_t)kind,
    };
    result.reader      = *reader;
    result.base_offset = base_offset;
    result.profile     = *profile;

    // Every decoded domain view uses the copied generation reader rather than
    // retaining a pointer to an input snapshot or validator object.
    if ((result.profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_RGB) != 0u) {
        result.profile.rgb.reader = result.reader;
    }
    if ((result.profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS) != 0u) {
        result.profile.key_behaviors.reader = result.reader;
    }
    if (!snapshot_valid(&result)) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT;
    }
    *snapshot = result;
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_snapshot_make_compiled(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset, noah_effective_profile_snapshot_t *snapshot) {
    return make_snapshot(profile, reader, base_offset, 0u, NOAH_EFFECTIVE_PROFILE_ORIGIN_COMPILED, profile ? profile->digest : 0u, NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS, snapshot);
}

noah_effective_profile_result_t noah_effective_profile_snapshot_make_validated(const noah_profile_validator_v1_profile_t *profile, const noah_profile_reader_t *reader, size_t base_offset, uint32_t generation, uint8_t origin, uint32_t compiled_default_digest, noah_effective_profile_snapshot_t *snapshot) {
    return make_snapshot(profile, reader, base_offset, generation, origin, compiled_default_digest, NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE, snapshot);
}

bool noah_effective_profile_snapshot_read(const noah_effective_profile_snapshot_t *snapshot, size_t offset, uint8_t *target, size_t length) {
    if (!snapshot || (!target && length != 0u) || offset > snapshot->profile.byte_length || length > snapshot->profile.byte_length - offset || snapshot->base_offset > SIZE_MAX - offset) {
        return false;
    }
    return noah_profile_reader_read(&snapshot->reader, snapshot->base_offset + offset, target, length);
}

static void update_begin(noah_effective_profile_provider_t *provider) {
    noah_runtime_publication_begin(&provider->publication_sequence);
}

static void update_end(noah_effective_profile_provider_t *provider) {
    noah_runtime_publication_end(&provider->publication_sequence);
}

noah_effective_profile_result_t noah_effective_profile_provider_init(noah_effective_profile_provider_t *provider, const noah_effective_profile_snapshot_t *compiled_defaults, noah_effective_profile_safe_boundary_fn safe_boundary, void *safe_boundary_context, const noah_effective_profile_invalidator_t *invalidators, size_t invalidator_count) {
    size_t index;

    if (!provider || !compiled_defaults || !snapshot_valid(compiled_defaults) || compiled_defaults->identity.kind != NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS || invalidator_count > NOAH_EFFECTIVE_PROFILE_MAX_INVALIDATORS || (!invalidators && invalidator_count != 0u)) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    for (index = 0u; index < invalidator_count; index++) {
        if (!invalidators[index].callback) {
            return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
        }
    }
    memset(provider, 0, sizeof(*provider));
    provider->active_banks[0]       = *compiled_defaults;
    provider->active_banks[1]       = *compiled_defaults;
    provider->compiled_defaults     = *compiled_defaults;
    provider->safe_boundary         = safe_boundary;
    provider->safe_boundary_context = safe_boundary_context;
    provider->invalidator_count     = (uint8_t)invalidator_count;
    for (index = 0u; index < invalidator_count; index++) {
        provider->invalidators[index] = invalidators[index];
    }
    provider->initialized = true;
    return NOAH_EFFECTIVE_PROFILE_OK;
}

static noah_effective_profile_result_t mutation_guard(const noah_effective_profile_provider_t *provider) {
    if (!provider || !provider->initialized) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    if (provider->publication_in_progress || provider->safe_boundary_evaluation_in_progress) {
        return NOAH_EFFECTIVE_PROFILE_REENTRANT;
    }
    return provider->backing_reuse_in_progress ? NOAH_EFFECTIVE_PROFILE_BACKING_REUSE_IN_PROGRESS : NOAH_EFFECTIVE_PROFILE_OK;
}

static noah_effective_profile_backing_t snapshot_backing(const noah_effective_profile_snapshot_t *snapshot) {
    return (noah_effective_profile_backing_t){
        .reader      = snapshot->reader,
        .base_offset = snapshot->base_offset,
        .byte_length = snapshot->profile.byte_length,
    };
}

static bool backing_valid(const noah_effective_profile_backing_t *backing) {
    return backing && backing->byte_length != 0u && reader_range_valid(&backing->reader, backing->base_offset, backing->byte_length);
}

static bool backing_overlaps(const noah_effective_profile_backing_t *left, const noah_effective_profile_backing_t *right) {
    return backing_valid(left) && backing_valid(right) && reader_equal(&left->reader, &right->reader) && ranges_overlap(left->base_offset, left->byte_length, right->base_offset, right->byte_length);
}

static bool snapshot_overlaps_backing(const noah_effective_profile_snapshot_t *snapshot, const noah_effective_profile_backing_t *backing) {
    noah_effective_profile_backing_t borrowed = snapshot_backing(snapshot);

    return snapshot_valid(snapshot) && backing_overlaps(&borrowed, backing);
}

static noah_effective_profile_result_t request_snapshot(noah_effective_profile_provider_t *provider, const noah_effective_profile_snapshot_t *snapshot) {
    noah_effective_profile_result_t guard = mutation_guard(provider);

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (!snapshot || !snapshot_valid(snapshot)) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT;
    }
    if (identity_equal(&provider->active_banks[provider->active_index].identity, &snapshot->identity) || (provider->has_pending && identity_equal(&provider->pending.identity, &snapshot->identity))) {
        return NOAH_EFFECTIVE_PROFILE_NO_CHANGE;
    }
    if (provider->has_pending) {
        return NOAH_EFFECTIVE_PROFILE_BUSY;
    }
    update_begin(provider);
    provider->pending                   = *snapshot;
    provider->has_pending               = true;
    provider->safe_boundary_reason_mask = 0u;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_provider_request_validated(noah_effective_profile_provider_t *provider, const noah_effective_profile_snapshot_t *snapshot) {
    noah_effective_profile_result_t          result;
    const noah_effective_profile_identity_t *active;
    noah_effective_profile_backing_t         candidate_backing;
    noah_effective_profile_backing_t         active_backing;
    noah_effective_profile_backing_t         compiled_backing;

    result = mutation_guard(provider);
    if (result != NOAH_EFFECTIVE_PROFILE_OK) {
        return result;
    }
    if (!snapshot || !snapshot_valid(snapshot) || snapshot->identity.kind != NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT;
    }
    active = &provider->active_banks[provider->active_index].identity;
    if (snapshot->identity.compiled_default_digest != provider->compiled_defaults.identity.payload_digest || snapshot->identity.action_abi_digest != provider->compiled_defaults.identity.action_abi_digest) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_SNAPSHOT;
    }
    if (identity_equal(active, &snapshot->identity) || (provider->has_pending && identity_equal(&provider->pending.identity, &snapshot->identity))) {
        return NOAH_EFFECTIVE_PROFILE_NO_CHANGE;
    }
    if (!identity_equal(active, &snapshot->identity) && snapshot->identity.generation <= active->generation) {
        return NOAH_EFFECTIVE_PROFILE_STALE_GENERATION;
    }
    candidate_backing = snapshot_backing(snapshot);
    active_backing    = snapshot_backing(&provider->active_banks[provider->active_index]);
    compiled_backing  = snapshot_backing(&provider->compiled_defaults);
    if (backing_overlaps(&candidate_backing, &active_backing) || backing_overlaps(&candidate_backing, &compiled_backing)) {
        return NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED;
    }
    if (provider->rollback_available && snapshot_overlaps_backing(&provider->active_banks[(uint8_t)(provider->active_index ^ 1u)], &candidate_backing)) {
        return NOAH_EFFECTIVE_PROFILE_BUSY;
    }
    return request_snapshot(provider, snapshot);
}

noah_effective_profile_result_t noah_effective_profile_provider_request_rollback(noah_effective_profile_provider_t *provider) {
    noah_effective_profile_result_t guard = mutation_guard(provider);

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (provider->has_pending) {
        return NOAH_EFFECTIVE_PROFILE_BUSY;
    }
    if (!provider->rollback_available) {
        return NOAH_EFFECTIVE_PROFILE_NO_ROLLBACK;
    }
    return request_snapshot(provider, &provider->active_banks[(uint8_t)(provider->active_index ^ 1u)]);
}

noah_effective_profile_result_t noah_effective_profile_provider_request_compiled_fallback(noah_effective_profile_provider_t *provider) {
    noah_effective_profile_result_t guard = mutation_guard(provider);

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    return request_snapshot(provider, &provider->compiled_defaults);
}

noah_effective_profile_result_t noah_effective_profile_provider_cancel_pending(noah_effective_profile_provider_t *provider) {
    noah_effective_profile_result_t guard = mutation_guard(provider);

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (!provider->has_pending) {
        return NOAH_EFFECTIVE_PROFILE_NO_PENDING;
    }
    update_begin(provider);
    memset(&provider->pending, 0, sizeof(provider->pending));
    provider->has_pending               = false;
    provider->safe_boundary_reason_mask = 0u;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_provider_discard_rollback(noah_effective_profile_provider_t *provider) {
    noah_effective_profile_result_t guard = mutation_guard(provider);
    uint8_t                         rollback_index;

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (provider->has_pending) {
        return NOAH_EFFECTIVE_PROFILE_BUSY;
    }
    if (!provider->rollback_available) {
        return NOAH_EFFECTIVE_PROFILE_NO_CHANGE;
    }
    rollback_index = (uint8_t)(provider->active_index ^ 1u);
    update_begin(provider);
    memset(&provider->active_banks[rollback_index], 0, sizeof(provider->active_banks[rollback_index]));
    provider->rollback_available = false;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_provider_begin_backing_reuse(noah_effective_profile_provider_t *provider, const noah_effective_profile_backing_t *backing) {
    noah_effective_profile_result_t guard = mutation_guard(provider);
    uint8_t                         rollback_index;

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (!backing_valid(backing)) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    if (snapshot_overlaps_backing(&provider->compiled_defaults, backing) || snapshot_overlaps_backing(&provider->active_banks[provider->active_index], backing)) {
        return NOAH_EFFECTIVE_PROFILE_ACTIVE_BACKING_PINNED;
    }
    if (provider->has_pending && snapshot_overlaps_backing(&provider->pending, backing)) {
        return NOAH_EFFECTIVE_PROFILE_BUSY;
    }

    rollback_index = (uint8_t)(provider->active_index ^ 1u);
    update_begin(provider);
    if (provider->rollback_available && snapshot_overlaps_backing(&provider->active_banks[rollback_index], backing)) {
        memset(&provider->active_banks[rollback_index], 0, sizeof(provider->active_banks[rollback_index]));
        provider->rollback_available = false;
    }
    provider->reuse_backing             = *backing;
    provider->backing_reuse_in_progress = true;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_provider_end_backing_reuse(noah_effective_profile_provider_t *provider) {
    if (!provider || !provider->initialized) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    if (provider->publication_in_progress || provider->safe_boundary_evaluation_in_progress) {
        return NOAH_EFFECTIVE_PROFILE_REENTRANT;
    }
    if (!provider->backing_reuse_in_progress) {
        return NOAH_EFFECTIVE_PROFILE_NO_BACKING_REUSE;
    }
    update_begin(provider);
    memset(&provider->reuse_backing, 0, sizeof(provider->reuse_backing));
    provider->backing_reuse_in_progress = false;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_OK;
}

noah_effective_profile_result_t noah_effective_profile_provider_poll(noah_effective_profile_provider_t *provider) {
    noah_effective_profile_identity_t previous;
    noah_effective_profile_identity_t active;
    noah_effective_profile_snapshot_t pending;
    uint32_t                          reason_mask;
    uint8_t                           next_index;
    uint8_t                           invalidator_index;

    noah_effective_profile_result_t guard = mutation_guard(provider);

    if (guard != NOAH_EFFECTIVE_PROFILE_OK) {
        return guard;
    }
    if (!provider->has_pending) {
        return NOAH_EFFECTIVE_PROFILE_NO_PENDING;
    }
    if (!provider->safe_boundary && ((provider->pending.profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_KEY_BEHAVIORS) || ((provider->pending.profile.domain_mask | provider->active_banks[provider->active_index].profile.domain_mask) & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_COMBOS))) {
        return NOAH_EFFECTIVE_PROFILE_SAFE_BOUNDARY_REQUIRED;
    }
    update_begin(provider);
    provider->safe_boundary_evaluation_in_progress = true;
    update_end(provider);
    reason_mask = provider->safe_boundary ? provider->safe_boundary(provider->safe_boundary_context) : 0u;
    update_begin(provider);
    provider->safe_boundary_evaluation_in_progress = false;
    update_end(provider);
    if (reason_mask != 0u) {
        update_begin(provider);
        provider->safe_boundary_reason_mask = reason_mask;
        update_end(provider);
        return NOAH_EFFECTIVE_PROFILE_WAITING;
    }

    pending    = provider->pending;
    previous   = provider->active_banks[provider->active_index].identity;
    next_index = (uint8_t)(provider->active_index ^ 1u);

    update_begin(provider);
    provider->publication_in_progress  = true;
    provider->active_banks[next_index] = pending;
    provider->active_index             = next_index;
    memset(&provider->pending, 0, sizeof(provider->pending));
    provider->has_pending               = false;
    provider->rollback_available        = true;
    provider->safe_boundary_reason_mask = 0u;
    if (provider->publication_count != UINT32_MAX) {
        provider->publication_count++;
    }
    active = provider->active_banks[provider->active_index].identity;

    for (invalidator_index = 0u; invalidator_index < provider->invalidator_count; invalidator_index++) {
        provider->invalidators[invalidator_index].callback(provider->invalidators[invalidator_index].context, provider->publication_count, previous, active, &provider->active_banks[provider->active_index]);
    }

    provider->publication_in_progress = false;
    update_end(provider);
    return NOAH_EFFECTIVE_PROFILE_PUBLISHED;
}

noah_effective_profile_result_t noah_effective_profile_provider_copy_active(const noah_effective_profile_provider_t *provider, noah_effective_profile_snapshot_t *snapshot) {
    uint8_t attempt;

    if (!provider || !provider->initialized || !snapshot) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                           observed = noah_runtime_publication_observe(&provider->publication_sequence);
        noah_effective_profile_snapshot_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        copied = provider->active_banks[provider->active_index];
        if (noah_runtime_publication_settled(&provider->publication_sequence, observed)) {
            *snapshot = copied;
            return NOAH_EFFECTIVE_PROFILE_OK;
        }
    }
    return NOAH_EFFECTIVE_PROFILE_BUSY;
}

noah_effective_profile_result_t noah_effective_profile_provider_status(const noah_effective_profile_provider_t *provider, noah_effective_profile_status_t *status) {
    uint8_t attempt;

    if (!provider || !provider->initialized || !status) {
        return NOAH_EFFECTIVE_PROFILE_INVALID_ARGUMENT;
    }
    for (attempt = 0u; attempt < NOAH_RUNTIME_PUBLICATION_READ_ATTEMPTS; attempt++) {
        uint8_t                         observed = noah_runtime_publication_observe(&provider->publication_sequence);
        noah_effective_profile_status_t copied;

        if (noah_runtime_publication_in_flight(observed)) {
            continue;
        }
        memset(&copied, 0, sizeof(copied));
        copied.active                               = provider->active_banks[provider->active_index].identity;
        copied.has_pending                          = provider->has_pending;
        copied.rollback_available                   = provider->rollback_available;
        copied.publication_in_progress              = provider->publication_in_progress;
        copied.safe_boundary_evaluation_in_progress = provider->safe_boundary_evaluation_in_progress;
        copied.backing_reuse_in_progress            = provider->backing_reuse_in_progress;
        copied.safe_boundary_reason_mask            = provider->safe_boundary_reason_mask;
        copied.publication_count                    = provider->publication_count;
        if (provider->has_pending) {
            copied.pending = provider->pending.identity;
        }
        if (noah_runtime_publication_settled(&provider->publication_sequence, observed)) {
            *status = copied;
            return NOAH_EFFECTIVE_PROFILE_OK;
        }
    }
    return NOAH_EFFECTIVE_PROFILE_BUSY;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_effective_profile_provider_t) <= NOAH_EFFECTIVE_PROFILE_PROVIDER_STATE_BUDGET_32BIT, "effective profile provider exceeds its 32-bit state policy");
#endif

// ──────────────────────────────────────────────────────────────────────────
// Single Live-Profile Runtime Owner
// ──────────────────────────────────────────────────────────────────────────

#include "profile_owner.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "../storage/profile_storage_layout.h"

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
static uint32_t noah_profile_owner_ready_refresh_count;
#endif

enum {
    OWNER_SCHEDULE_HOST = 0u,
    OWNER_SCHEDULE_SPLIT,
    OWNER_SCHEDULE_PEER_ACTIVATION,
    OWNER_SCHEDULE_COUNT,
};

static bool scan_running(noah_profile_owner_t *owner, bool master, uint32_t now_ms, bool activating_boot);
static bool request_host_precommit_cancel(noah_profile_owner_t *owner, noah_profile_candidate_v1_error_id_t reason);

static noah_profile_candidate_store_backend_t *candidate_backend(noah_profile_owner_t *owner) {
    return owner ? &owner->staging.candidate_backend : NULL;
}

static bool no_peer_observer(void *context, uint8_t *unresolved_count) {
    (void)context;
    if (!unresolved_count) {
        return false;
    }
    *unresolved_count = 0u;
    return true;
}

static bool elapsed_at_least(uint32_t now, uint32_t since, uint32_t interval) {
    return (uint32_t)(now - since) >= interval;
}

static bool host_state_is_precommit(noah_profile_candidate_v1_state_t state) {
    return state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING || state == NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE || state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING || state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED || state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED || state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER;
}

static bool host_mailbox_is_matching_abort(const noah_profile_owner_t *owner) {
    return owner && owner->host_transaction.mailbox.pending && owner->host_transaction.mailbox.command.operation == NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT && owner->host_transaction.mailbox.command.transaction_id == owner->host_transaction.status.transaction_id;
}

static bool descriptor_equal(const noah_profile_split_descriptor_t *left, const noah_profile_split_descriptor_t *right) {
    return left && right && left->generation == right->generation && left->payload_crc32 == right->payload_crc32 && left->payload_digest == right->payload_digest && left->compiled_default_digest == right->compiled_default_digest && left->action_abi_digest == right->action_abi_digest && left->payload_length == right->payload_length && left->schema_major == right->schema_major && left->schema_minor == right->schema_minor && left->domain_mask == right->domain_mask && left->profile_flags == right->profile_flags && left->origin_half == right->origin_half && left->readable == right->readable && left->has_profile == right->has_profile;
}

static noah_profile_split_descriptor_t descriptor_from_candidate(const noah_profile_store_candidate_t *candidate) {
    if (!candidate) {
        return (noah_profile_split_descriptor_t){0};
    }
    return (noah_profile_split_descriptor_t){
        .generation              = candidate->generation,
        .payload_crc32           = candidate->payload_crc32,
        .payload_digest          = candidate->payload_digest,
        .compiled_default_digest = candidate->compiled_default_digest,
        .action_abi_digest       = candidate->action_abi_digest,
        .payload_length          = candidate->payload_length,
        .schema_major            = candidate->schema_major,
        .schema_minor            = candidate->schema_minor,
        .domain_mask             = candidate->domain_mask,
        .profile_flags           = candidate->flags,
        .origin_half             = candidate->origin_half,
        .readable                = true,
        .has_profile             = true,
    };
}

static bool host_staged_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_owner_t           *owner = context;
    noah_profile_store_candidate_t  candidate;

    if (!owner || !descriptor || !owner->host_barrier_descriptor_known || !descriptor_equal(descriptor, &owner->host_barrier_descriptor) || !noah_profile_candidate_store_backend_staged_candidate(candidate_backend(owner), &candidate) || !descriptor_equal(descriptor, &(noah_profile_split_descriptor_t){
            .generation              = candidate.generation,
            .payload_crc32           = candidate.payload_crc32,
            .payload_digest          = candidate.payload_digest,
            .compiled_default_digest = candidate.compiled_default_digest,
            .action_abi_digest       = candidate.action_abi_digest,
            .payload_length          = candidate.payload_length,
            .schema_major            = candidate.schema_major,
            .schema_minor            = candidate.schema_minor,
            .domain_mask             = candidate.domain_mask,
            .profile_flags           = candidate.flags,
            .origin_half             = candidate.origin_half,
            .readable                = true,
            .has_profile             = true,
        })) {
        return false;
    }
    return noah_profile_candidate_store_backend_staged_read(candidate_backend(owner), &candidate, offset, bytes, length);
}

static bool owner_peer_observer(void *context, uint8_t *unresolved_count) {
    noah_profile_owner_t                  *owner = context;
    noah_profile_split_authority_status_t  authority;

    if (!owner || !unresolved_count || !owner->descriptor_readable || !owner->split_initialized || !noah_profile_split_authority_status(&owner->reconciler.authority, &authority)) {
        return false;
    }
    *unresolved_count = !authority.transfer_pending && descriptor_equal(&authority.local, &owner->committed_descriptor) && descriptor_equal(&authority.peer, &owner->committed_descriptor) && (authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED) ? 0u : 1u;
    return true;
}

static void reset_host_barrier(noah_profile_owner_t *owner) {
    if (!owner) {
        return;
    }
    memset(&owner->host_barrier_descriptor, 0, sizeof(owner->host_barrier_descriptor));
    owner->host_barrier_descriptor_known       = false;
    owner->host_barrier_started                = false;
    owner->host_barrier_local_published        = false;
    owner->host_barrier_peer_commit_authorized = false;
    owner->host_cancel_reason                  = NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE;
    owner->host_cancel_pending                 = false;
    owner->host_barrier_progress_offset        = 0u;
}

static bool host_session_cleanup_needed(const noah_profile_owner_t *owner) {
    return owner && (owner->host_activity_known || owner->host_barrier_descriptor_known || owner->host_barrier_started || owner->host_barrier_local_published || owner->host_barrier_peer_commit_authorized || owner->host_cancel_pending || owner->host_cancel_reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE || owner->host_barrier_progress_offset != 0u);
}

static bool peer_can_supersede_host(const noah_profile_owner_t *owner, const noah_profile_split_descriptor_t *peer) {
    return owner && peer && peer->readable && peer->has_profile && noah_profile_split_descriptor_valid(peer) && peer->schema_major == NOAH_PROFILE_STORE_SCHEMA_MAJOR && peer->schema_minor == NOAH_PROFILE_STORE_SCHEMA_MINOR && (peer->domain_mask & (uint8_t)~owner->compatibility.allowed_domain_mask) == 0u && peer->compiled_default_digest == owner->compiled.metadata.digest && peer->action_abi_digest == owner->compiled.metadata.action_abi_digest && owner->store.prepare_active && peer->generation >= owner->store.candidate.generation;
}

static bool supersede_host_precommit(noah_profile_owner_t *owner) {
    noah_profile_split_authority_status_t authority;
    noah_profile_candidate_expire_result_t result;

    if (!owner || !owner->split_initialized || noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_HOST || !host_state_is_precommit(owner->host_transaction.status.state) || !noah_profile_split_authority_status(&owner->reconciler.authority, &authority) || !peer_can_supersede_host(owner, &authority.peer)) {
        return false;
    }
    if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_barrier_started) {
        return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED);
    }
    result = noah_profile_candidate_transaction_supersede_precommit(&owner->host_transaction);
    if (result == NOAH_PROFILE_CANDIDATE_EXPIRE_DONE) {
        owner->host_activity_known = false;
        return true;
    }
    if (result == NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR) {
        owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        return true;
    }
    return false;
}

static bool record_payload_start(noah_profile_slot_t slot, uint16_t *address) {
    if (!address) {
        return false;
    }
    if (slot == NOAH_PROFILE_SLOT_A) {
        *address = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    if (slot == NOAH_PROFILE_SLOT_B) {
        *address = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    return false;
}

static bool record_matches_descriptor(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && descriptor->readable && descriptor->has_profile && record->slot != NOAH_PROFILE_SLOT_NONE && record->schema_major == descriptor->schema_major && record->schema_minor == descriptor->schema_minor && record->domain_mask == descriptor->domain_mask && record->flags == descriptor->profile_flags && record->payload_length == descriptor->payload_length && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half && record->payload_crc32 == descriptor->payload_crc32 && record->payload_digest == descriptor->payload_digest && record->compiled_default_digest == descriptor->compiled_default_digest && record->action_abi_digest == descriptor->action_abi_digest;
}

static bool local_descriptor(void *context, noah_profile_split_descriptor_t *descriptor) {
    noah_profile_owner_t *owner = context;

    if (!owner || !descriptor || !owner->descriptor_readable) {
        return false;
    }
    *descriptor = owner->committed_descriptor;
    return noah_profile_split_descriptor_valid(descriptor);
}

static void publish_compiled_descriptor(noah_profile_owner_t *owner) {
    owner->committed_descriptor = (noah_profile_split_descriptor_t){
        .compiled_default_digest = owner->compiled.metadata.digest,
        .action_abi_digest       = owner->compiled.metadata.action_abi_digest,
        .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .readable                = true,
    };
    owner->descriptor_readable = true;
}

static bool publish_validated_descriptor(noah_profile_owner_t *owner) {
    noah_profile_store_record_t record;

    if (!owner || !noah_profile_candidate_store_backend_committed(candidate_backend(owner), &record)) {
        return false;
    }
    owner->committed_descriptor = (noah_profile_split_descriptor_t){
        .generation              = record.generation,
        .payload_crc32           = record.payload_crc32,
        .payload_digest          = record.payload_digest,
        .compiled_default_digest = record.compiled_default_digest,
        .action_abi_digest       = record.action_abi_digest,
        .payload_length          = record.payload_length,
        .schema_major            = record.schema_major,
        .schema_minor            = record.schema_minor,
        .domain_mask             = record.domain_mask,
        .profile_flags           = record.flags,
        .origin_half             = record.origin_half,
        .readable                = true,
        .has_profile             = true,
    };
    owner->descriptor_readable = noah_profile_split_descriptor_valid(&owner->committed_descriptor);
    return owner->descriptor_readable;
}

static bool local_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint16_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_owner_t              *owner = context;
    const noah_profile_store_record_t *record;
    uint16_t                           payload_start;

    if (!owner || !descriptor || !bytes || length == 0u || !owner->store.io.read) {
        return false;
    }
    record = owner->store.committed.slot == NOAH_PROFILE_SLOT_NONE ? NULL : &owner->store.committed;
    if (!record_matches_descriptor(record, descriptor) || !record_payload_start(record->slot, &payload_start) || (uint32_t)offset + length > record->payload_length) {
        return false;
    }
    return owner->store.io.read(owner->store.io.context, (uint16_t)(payload_start + offset), bytes, length);
}

static void fail_integration(noah_profile_owner_t *owner) {
    if (!owner) {
        return;
    }
    if (owner->runtimes_installed) {
        noah_effective_key_behavior_runtime_uninstall(&owner->key_behaviors);
        noah_effective_rgb_runtime_uninstall(&owner->rgb);
        owner->runtimes_installed = false;
    }
    owner->state = NOAH_PROFILE_OWNER_INTEGRATION_ERROR;
}

static bool initialize_runtime_graph(noah_profile_owner_t *owner) {
    noah_effective_profile_invalidator_t invalidators[2];
    noah_profile_candidate_backend_t     host_backend;
    noah_profile_candidate_compatibility_t host_compatibility;
    noah_profile_split_reconciler_config_t split_config;
    noah_profile_activation_peer_observer_fn observer;
    void *observer_context;

    if (!owner || noah_profile_validator_v1_profile(&owner->staging.compiled_validator, &owner->compiled_profile, NULL) != NOAH_PROFILE_VALIDATOR_V1_VALID || noah_effective_profile_snapshot_make_compiled(&owner->compiled_profile, &owner->compiled_reader, 0u, &owner->compiled_snapshot) != NOAH_EFFECTIVE_PROFILE_OK) {
        return false;
    }

    noah_effective_key_behavior_runtime_init(&owner->key_behaviors);
    noah_effective_rgb_runtime_init(&owner->rgb);
    invalidators[0] = (noah_effective_profile_invalidator_t){
        .callback = noah_effective_key_behavior_runtime_invalidate,
        .context  = &owner->key_behaviors,
    };
    invalidators[1] = (noah_effective_profile_invalidator_t){
        .callback = noah_effective_rgb_runtime_invalidate,
        .context  = &owner->rgb,
    };
    observer         = owner->config.peer_required ? owner_peer_observer : no_peer_observer;
    observer_context = owner->config.peer_required ? owner : NULL;
    noah_profile_activation_policy_init(&owner->activation_policy, observer, observer_context);
    if (noah_effective_profile_provider_init(&owner->provider, &owner->compiled_snapshot, noah_profile_activation_policy_safe_boundary, &owner->activation_policy, invalidators, 2u) != NOAH_EFFECTIVE_PROFILE_OK) {
        return false;
    }
    noah_effective_key_behavior_runtime_invalidate(&owner->key_behaviors, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    noah_effective_rgb_runtime_invalidate(&owner->rgb, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);

    noah_profile_store_init(&owner->store, owner->config.store_io,
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = owner->compiled.metadata.digest,
                                .action_abi_digest       = owner->compiled.metadata.action_abi_digest,
                            });
    noah_profile_candidate_store_backend_init(candidate_backend(owner), &owner->store, &owner->provider, &owner->compatibility, owner->compiled.metadata.digest, owner->config.origin_half);
    if (!candidate_backend(owner)->reuse_guard_installed) {
        return false;
    }
    host_backend       = noah_profile_candidate_store_backend_interface(candidate_backend(owner));
    host_compatibility = (noah_profile_candidate_compatibility_t){
        .schema_major          = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
        .schema_minor          = NOAH_PROFILE_STORE_SCHEMA_MINOR,
        .supported_domain_mask = owner->compatibility.allowed_domain_mask,
        .max_payload_length    = owner->compatibility.max_blob_size,
        .action_abi_digest     = owner->compatibility.action_abi_digest,
    };
    noah_profile_candidate_transaction_init(&owner->host_transaction, &host_backend, &host_compatibility);
    if (!noah_profile_candidate_transaction_require_split_authorization(&owner->host_transaction, owner->config.peer_required)) {
        return false;
    }
    noah_profile_peer_store_backend_init(&owner->peer_store, candidate_backend(owner));

    if (owner->config.peer_required) {
        split_config = (noah_profile_split_reconciler_config_t){
            .local_context    = owner,
            .local_descriptor = local_descriptor,
            .local_read       = local_read,
            .transport_context = owner->config.split_transport_context,
            .exchange          = owner->config.split_exchange,
            .peer_store        = &owner->peer_store,
        };
        noah_profile_split_reconciler_init(&owner->reconciler, &split_config);
        owner->split_initialized = noah_profile_split_reconciler_authority(&owner->reconciler) != NULL;
        if (!owner->split_initialized) {
            return false;
        }
    }
    if (!noah_effective_key_behavior_runtime_install(&owner->key_behaviors)) {
        return false;
    }
    if (!noah_effective_rgb_runtime_install(&owner->rgb)) {
        noah_effective_key_behavior_runtime_uninstall(&owner->key_behaviors);
        return false;
    }
    owner->runtimes_installed = true;
    owner->discovery_result   = noah_profile_store_boot_select_begin(&owner->store);
    if (owner->discovery_result != NOAH_PROFILE_STORE_IN_PROGRESS) {
        return false;
    }
    owner->state = NOAH_PROFILE_OWNER_DISCOVERING;
    return true;
}

bool noah_profile_owner_init(noah_profile_owner_t *owner, const noah_profile_owner_config_t *config) {
    noah_profile_validator_v1_result_t result;

    if (!owner || !config || !config->store_io.read || !config->store_io.write || config->origin_half > 1u || (config->peer_required && !config->split_exchange)) {
        return false;
    }
    memset(owner, 0, sizeof(*owner));
    owner->config           = *config;
    owner->discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    if (noah_profile_compiled_v1_open(&owner->compiled, NULL) != NOAH_PROFILE_COMPILED_V1_OK || !noah_profile_compiled_v1_compatibility(&owner->compiled, &owner->compatibility)) {
        owner->state = NOAH_PROFILE_OWNER_COMPILED_ERROR;
        return false;
    }
    owner->compiled_reader      = noah_profile_compiled_v1_reader(&owner->compiled);
    owner->compiled_declaration = (noah_profile_validator_v1_declaration_t){
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = owner->compiled.metadata.domain_mask,
        .flags             = 0u,
        .byte_length       = owner->compiled.metadata.byte_length,
        .crc32             = owner->compiled.metadata.crc32,
        .digest            = owner->compiled.metadata.digest,
        .action_abi_digest = owner->compiled.metadata.action_abi_digest,
    };
    result = noah_profile_validator_v1_begin(&owner->staging.compiled_validator, &owner->compiled_reader, 0u, &owner->compiled_declaration, &owner->compatibility, NULL);
    if (result != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        owner->state = NOAH_PROFILE_OWNER_COMPILED_ERROR;
        return false;
    }
    owner->state = NOAH_PROFILE_OWNER_VALIDATING_COMPILED;
    return true;
}

static bool scan_discovery(noah_profile_owner_t *owner) {
    noah_profile_store_record_t selected;

    owner->discovery_result = noah_profile_store_boot_select_step(&owner->store, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, &selected);
    if (owner->discovery_result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return true;
    }
    if (owner->discovery_result == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE) {
        publish_compiled_descriptor(owner);
        owner->state = NOAH_PROFILE_OWNER_READY_COMPILED;
        return true;
    }
    if (owner->discovery_result == NOAH_PROFILE_STORE_GENERATION_CONFLICT) {
        owner->state = NOAH_PROFILE_OWNER_GENERATION_CONFLICT;
        return true;
    }
    if (owner->discovery_result != NOAH_PROFILE_STORE_OK) {
        owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        return true;
    }
    owner->adoption_error = noah_profile_candidate_v1_no_error();
    switch (noah_profile_candidate_store_backend_adopt_committed_begin(candidate_backend(owner), &selected, &owner->adoption_error)) {
        case NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS:
            owner->state = NOAH_PROFILE_OWNER_ADOPTING_COMMITTED;
            break;
        case NOAH_PROFILE_CANDIDATE_BACKEND_VALID:
            owner->state = publish_validated_descriptor(owner) ? (owner->config.peer_required ? NOAH_PROFILE_OWNER_RECONCILING_COMMITTED : NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED) : NOAH_PROFILE_OWNER_STORAGE_ERROR;
            break;
        default:
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
            break;
    }
    return true;
}

static bool scan_adoption(noah_profile_owner_t *owner) {
    noah_profile_candidate_backend_result_t result = noah_profile_candidate_store_backend_adopt_committed_step(candidate_backend(owner), NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, &owner->adoption_error);

    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        return true;
    }
    owner->state = result == NOAH_PROFILE_CANDIDATE_BACKEND_VALID && publish_validated_descriptor(owner) ? (owner->config.peer_required ? NOAH_PROFILE_OWNER_RECONCILING_COMMITTED : NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED) : NOAH_PROFILE_OWNER_STORAGE_ERROR;
    return true;
}

static bool boot_peer_converged(const noah_profile_owner_t *owner) {
    uint8_t unresolved_count = 1u;

    return owner && owner_peer_observer((void *)owner, &unresolved_count) && unresolved_count == 0u;
}

static bool scan_boot_reconciliation(noah_profile_owner_t *owner, bool master, uint32_t now_ms) {
    noah_profile_storage_admission_owner_t admission = noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner));
    bool                                   worked;

    if (admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST) {
        fail_integration(owner);
        return true;
    }
    if (admission == NOAH_PROFILE_STORAGE_ADMISSION_PEER) {
        return scan_running(owner, master, now_ms, false);
    }
    if (boot_peer_converged(owner)) {
        owner->boot_activation_started = false;
        owner->state                   = NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED;
        return true;
    }

    worked = noah_profile_split_reconciler_scan_mode(&owner->reconciler, master, now_ms, NOAH_PROFILE_SPLIT_RECONCILE_FULL);
    if (worked && noah_profile_peer_store_backend_state(&owner->peer_store) == NOAH_PROFILE_PEER_STORE_COMMITTED) {
        if (!publish_validated_descriptor(owner)) {
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        }
    }
    return worked;
}

static bool scan_activation(noah_profile_owner_t *owner) {
    noah_profile_candidate_backend_result_t result;

    if (!owner->boot_activation_started) {
        result = noah_profile_candidate_store_backend_activation_begin(candidate_backend(owner), NOAH_PROFILE_STORAGE_ADMISSION_NONE);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK || result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            owner->boot_activation_started = true;
        } else {
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        }
        return true;
    }
    result = noah_profile_candidate_store_backend_activation_step(candidate_backend(owner), NOAH_PROFILE_STORAGE_ADMISSION_NONE);
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK && owner->boot_activation_started && noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) == NOAH_PROFILE_STORAGE_ADMISSION_NONE) {
        noah_effective_profile_status_t status;
        owner->state = noah_effective_profile_provider_status(&owner->provider, &status) == NOAH_EFFECTIVE_PROFILE_OK && status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_COMPILED_DEFAULTS ? NOAH_PROFILE_OWNER_READY_COMPILED : NOAH_PROFILE_OWNER_READY_VALIDATED;
    } else if (result != NOAH_PROFILE_CANDIDATE_BACKEND_OK && result != NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
    }
    return true;
}

static bool scan_peer_activation(noah_profile_owner_t *owner) {
    noah_profile_candidate_backend_result_t result;

    if (noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_PEER || noah_profile_peer_store_backend_state(&owner->peer_store) != NOAH_PROFILE_PEER_STORE_COMMITTED) {
        owner->peer_activation_started = false;
        return false;
    }
    if (!owner->peer_activation_started) {
        result = noah_profile_candidate_store_backend_activation_begin(candidate_backend(owner), NOAH_PROFILE_STORAGE_ADMISSION_PEER);
        if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK || result == NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
            owner->peer_activation_started = true;
        }
    } else {
        result = noah_profile_candidate_store_backend_activation_step(candidate_backend(owner), NOAH_PROFILE_STORAGE_ADMISSION_PEER);
    }
    if (result == NOAH_PROFILE_CANDIDATE_BACKEND_OK && noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) == NOAH_PROFILE_STORAGE_ADMISSION_NONE) {
        owner->peer_activation_started = false;
        owner->state                   = NOAH_PROFILE_OWNER_READY_VALIDATED;
    } else if (result != NOAH_PROFILE_CANDIDATE_BACKEND_OK && result != NOAH_PROFILE_CANDIDATE_BACKEND_IN_PROGRESS) {
        owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
    }
    return true;
}

static void refresh_ready_state(noah_profile_owner_t *owner) {
    noah_effective_profile_status_t status;

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
    noah_profile_owner_ready_refresh_count++;
#endif
    if (!owner || noah_effective_profile_provider_status(&owner->provider, &status) != NOAH_EFFECTIVE_PROFILE_OK) {
        return;
    }
    owner->state = status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE ? NOAH_PROFILE_OWNER_READY_VALIDATED : NOAH_PROFILE_OWNER_READY_COMPILED;
}

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
uint32_t noah_profile_owner_test_ready_refresh_count(void) {
    return noah_profile_owner_ready_refresh_count;
}
#endif

static noah_profile_candidate_expire_result_t finish_host_precommit_cancel(noah_profile_owner_t *owner) {
    if (!owner || !owner->host_cancel_pending || owner->reconciler.prepared_push_active) {
        return NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING;
    }
    switch (owner->host_cancel_reason) {
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE:
            if (!host_mailbox_is_matching_abort(owner) || !noah_profile_candidate_transaction_scan(&owner->host_transaction)) {
                return NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING;
            }
            return owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_HOST ? NOAH_PROFILE_CANDIDATE_EXPIRE_DONE : NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR;
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT:
            return noah_profile_candidate_transaction_expire_precommit(&owner->host_transaction);
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED:
            return noah_profile_candidate_transaction_supersede_precommit(&owner->host_transaction);
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED:
            return noah_profile_candidate_transaction_yield_precommit(&owner->host_transaction);
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE:
            return noah_profile_candidate_transaction_cleanup_failed_precommit(&owner->host_transaction);
        default:
            return NOAH_PROFILE_CANDIDATE_EXPIRE_NOTHING;
    }
}

static bool advance_host_precommit_cancel(noah_profile_owner_t *owner) {
    noah_profile_candidate_expire_result_t result;
    noah_profile_candidate_v1_error_id_t   reason;

    if (!owner || !owner->host_cancel_pending) {
        return false;
    }
    result = finish_host_precommit_cancel(owner);
    if (result == NOAH_PROFILE_CANDIDATE_EXPIRE_DONE) {
        reason = owner->host_cancel_reason;
        reset_host_barrier(owner);
        owner->host_activity_known = false;
        if (reason == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE) {
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        } else {
            refresh_ready_state(owner);
        }
        return true;
    }
    if (result == NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR) {
        owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        return true;
    }
    return false;
}

static bool request_host_precommit_cancel(noah_profile_owner_t *owner, noah_profile_candidate_v1_error_id_t reason) {
    if (!owner || (reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE)) {
        return false;
    }
    if (reason == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE && !host_mailbox_is_matching_abort(owner)) {
        return false;
    }
    if (!owner->host_cancel_pending) {
        if (owner->host_barrier_started && owner->host_barrier_descriptor_known && !noah_profile_split_reconciler_prepared_push_cancel(&owner->reconciler, &owner->host_barrier_descriptor)) {
            return false;
        }
        owner->host_cancel_reason  = reason;
        owner->host_cancel_pending = true;
    } else if (owner->host_cancel_reason != reason) {
        return false;
    }
    if (advance_host_precommit_cancel(owner)) {
        return true;
    }
    // Scheduling or awaiting the bounded peer ABORT is useful work even when
    // the local backend cannot be released in this grant.
    return true;
}

static bool provisional_peer_wins(const noah_profile_owner_t *owner, const noah_profile_split_descriptor_t *local, const noah_profile_split_descriptor_t *peer, bool *conflict) {
    noah_profile_split_authority_state_t comparison;

    if (conflict) {
        *conflict = false;
    }
    if (!owner || !local || !peer || !noah_profile_split_descriptor_valid(local) || !noah_profile_split_descriptor_valid(peer) || !local->has_profile || !peer->has_profile) {
        return false;
    }
    comparison = noah_profile_split_authority_compare(local, peer);
    if (comparison == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER) {
        return true;
    }
    if (comparison == NOAH_PROFILE_SPLIT_AUTHORITY_LOCAL_NEWER) {
        return false;
    }
    if (comparison == NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT) {
        return peer->origin_half < local->origin_half;
    }
    if (conflict && (comparison == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE || comparison == NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE || (comparison == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED && peer->origin_half == local->origin_half))) {
        *conflict = true;
    }
    return false;
}

static bool begin_or_advance_host_barrier(noah_profile_owner_t *owner) {
    noah_profile_store_candidate_t   candidate;
    noah_profile_split_descriptor_t  descriptor;
    noah_profile_split_descriptor_t  prepared;
    noah_profile_split_descriptor_t  provisional_peer;
    bool                             conflict = false;

    if (!owner || !owner->split_initialized || owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER || !noah_profile_candidate_store_backend_staged_candidate(candidate_backend(owner), &candidate)) {
        return false;
    }
    descriptor = descriptor_from_candidate(&candidate);
    if (!noah_profile_split_descriptor_valid(&descriptor)) {
        fail_integration(owner);
        return true;
    }
    if (!owner->host_barrier_descriptor_known) {
        owner->host_barrier_descriptor       = descriptor;
        owner->host_barrier_descriptor_known = true;
    } else if (!descriptor_equal(&owner->host_barrier_descriptor, &descriptor)) {
        fail_integration(owner);
        return true;
    }
    if (noah_profile_split_reconciler_provisional_peer_descriptor(&owner->reconciler, &provisional_peer)) {
        if (provisional_peer_wins(owner, &owner->host_barrier_descriptor, &provisional_peer, &conflict)) {
            return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED);
        }
        if (conflict) {
            (void)request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED);
            owner->state = NOAH_PROFILE_OWNER_CONCURRENT_COMMIT;
            return true;
        }
    }
    if (!owner->host_barrier_started) {
        if (!noah_profile_split_reconciler_prepared_push_begin(&owner->reconciler, &owner->host_barrier_descriptor, owner, host_staged_read)) {
            return false;
        }
        owner->host_barrier_started = true;
        return true;
    }
    if (!noah_profile_split_reconciler_prepared_push_ready(&owner->reconciler, &prepared)) {
        return false;
    }
    if (!descriptor_equal(&prepared, &owner->host_barrier_descriptor)) {
        fail_integration(owner);
    } else if (!noah_profile_candidate_transaction_authorize_commit(&owner->host_transaction)) {
        if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE) {
            return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
        }
        fail_integration(owner);
    }
    return true;
}

static void note_host_barrier_progress(noah_profile_owner_t *owner, uint32_t now_ms) {
    noah_profile_split_reconciler_status_t status;

    if (!owner || !owner->host_barrier_started || owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER || !noah_profile_split_reconciler_status(&owner->reconciler, &status) || status.transfer_offset == owner->host_barrier_progress_offset) {
        return;
    }
    owner->host_barrier_progress_offset = status.transfer_offset;
    owner->host_last_activity_at        = now_ms;
    owner->host_activity_known          = true;
}

static bool host_barrier_exact_convergence(const noah_profile_owner_t *owner) {
    noah_profile_split_authority_status_t authority;

    return owner && owner->host_barrier_descriptor_known && owner->descriptor_readable && descriptor_equal(&owner->committed_descriptor, &owner->host_barrier_descriptor) && noah_profile_split_authority_status(&owner->reconciler.authority, &authority) && !authority.transfer_pending && authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED && descriptor_equal(&authority.local, &owner->host_barrier_descriptor) && descriptor_equal(&authority.peer, &owner->host_barrier_descriptor);
}

static bool fail_host_postcommit_authority(noah_profile_owner_t *owner, noah_profile_candidate_v1_error_id_t error, noah_profile_owner_state_t state) {
    if (!owner || !noah_profile_candidate_transaction_fail_postcommit(&owner->host_transaction, error)) {
        return false;
    }
    owner->state = state;
    return true;
}

static bool advance_host_postcommit_barrier(noah_profile_owner_t *owner) {
    if (!owner || owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER || !owner->host_barrier_descriptor_known) {
        return false;
    }
    if (!owner->host_barrier_local_published) {
        if (!publish_validated_descriptor(owner) || !descriptor_equal(&owner->committed_descriptor, &owner->host_barrier_descriptor) || !noah_profile_split_reconciler_refresh_authority(&owner->reconciler)) {
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
        } else {
            owner->host_barrier_local_published = true;
        }
        return true;
    }
    if (!owner->host_barrier_peer_commit_authorized) {
        if (noah_profile_split_reconciler_prepared_push_authorize_commit(&owner->reconciler, &owner->host_barrier_descriptor)) {
            owner->host_barrier_peer_commit_authorized = true;
            return true;
        }
        return false;
    }
    if (!host_barrier_exact_convergence(owner)) {
        noah_profile_split_authority_status_t  authority;
        noah_profile_split_reconciler_status_t reconciler;

        if (noah_profile_split_authority_status(&owner->reconciler.authority, &authority) && descriptor_equal(&authority.local, &owner->host_barrier_descriptor)) {
            if (authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_PEER_NEWER) {
                return fail_host_postcommit_authority(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_POSTCOMMIT_AUTHORITY_LOST, NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST);
            }
            if (authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_CONCURRENT_COMMIT || authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_CORRUPT_SAME_TUPLE || authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_INCOMPATIBLE) {
                return fail_host_postcommit_authority(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_COMMIT_CONFLICT, NOAH_PROFILE_OWNER_CONCURRENT_COMMIT);
            }
        }
        if (noah_profile_split_reconciler_status(&owner->reconciler, &reconciler) && reconciler.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED) {
            if (reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_CONFLICT || reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_CORRUPT || reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_INCOMPATIBLE) {
                return fail_host_postcommit_authority(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_COMMIT_CONFLICT, NOAH_PROFILE_OWNER_CONCURRENT_COMMIT);
            }
            if (reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_STORAGE_ERROR || reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_VALIDATION_ERROR || reconciler.last_status == NOAH_PROFILE_SPLIT_V1_STATUS_DIGEST_MISMATCH) {
                return fail_host_postcommit_authority(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_POSTCOMMIT_AUTHORITY_LOST, NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST);
            }
        }
        return false;
    }
    if (!noah_profile_candidate_transaction_authorize_activation(&owner->host_transaction)) {
        fail_integration(owner);
    }
    return true;
}

static bool scan_running(noah_profile_owner_t *owner, bool master, uint32_t now_ms, bool activating_boot) {
    noah_profile_storage_admission_owner_t admission = noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner));
    uint32_t                               host_timeout_ms;

    if (!activating_boot && owner->host_cancel_pending && advance_host_precommit_cancel(owner)) {
        return true;
    }
    if (!activating_boot && supersede_host_precommit(owner)) {
        return true;
    }

    host_timeout_ms = owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_barrier_started ? NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS : NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS;
    if (!activating_boot && !owner->host_cancel_pending && admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST && owner->host_activity_known && !owner->host_transaction.mailbox.pending && elapsed_at_least(now_ms, owner->host_last_activity_at, host_timeout_ms)) {
        if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_barrier_started) {
            return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT);
        }
        noah_profile_candidate_expire_result_t expired = noah_profile_candidate_transaction_expire_precommit(&owner->host_transaction);

        if (expired == NOAH_PROFILE_CANDIDATE_EXPIRE_DONE) {
            owner->host_activity_known = false;
            return true;
        }
        if (expired == NOAH_PROFILE_CANDIDATE_EXPIRE_BACKEND_ERROR) {
            owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
            return true;
        }
    }

    for (uint8_t visited = 0u; visited < OWNER_SCHEDULE_COUNT; visited++) {
        uint8_t current = owner->scheduler_cursor;
        bool    worked  = false;

        owner->scheduler_cursor = (uint8_t)((owner->scheduler_cursor + 1u) % OWNER_SCHEDULE_COUNT);
        if (current == OWNER_SCHEDULE_HOST) {
            if (activating_boot) {
                worked = scan_activation(owner);
            } else if (admission != NOAH_PROFILE_STORAGE_ADMISSION_PEER) {
                if (owner->host_cancel_pending) {
                    worked = false;
                } else if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_transaction.mailbox.pending) {
                    worked = host_mailbox_is_matching_abort(owner) ? request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE) : noah_profile_candidate_transaction_scan(&owner->host_transaction);
                } else if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER) {
                    worked = begin_or_advance_host_barrier(owner);
                } else if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER && owner->host_transaction.mailbox.pending) {
                    worked = noah_profile_candidate_transaction_scan(&owner->host_transaction);
                } else if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER) {
                    worked = advance_host_postcommit_barrier(owner);
                } else {
                    worked = noah_profile_candidate_transaction_scan(&owner->host_transaction);
                }
                if (worked) {
                    owner->host_last_activity_at = now_ms;
                    owner->host_activity_known   = true;
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_DURABILITY_UNKNOWN) {
                    owner->state = NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN;
                    return true;
                }
                if (worked && owner->host_barrier_started && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE) {
                    return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED) {
                    owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
                    return true;
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING && !owner->config.peer_required) {
                    (void)publish_validated_descriptor(owner);
                }
                if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_HOST && host_session_cleanup_needed(owner)) {
                    owner->host_activity_known = false;
                    reset_host_barrier(owner);
                    refresh_ready_state(owner);
                }
            }
        } else if (current == OWNER_SCHEDULE_SPLIT) {
            noah_profile_split_reconcile_mode_t mode = activating_boot || admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST ? NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY : NOAH_PROFILE_SPLIT_RECONCILE_FULL;
            worked = owner->split_initialized && noah_profile_split_reconciler_scan_mode(&owner->reconciler, master, now_ms, mode);
            if (worked && !activating_boot) {
                note_host_barrier_progress(owner, now_ms);
            }
            if (worked && noah_profile_peer_store_backend_state(&owner->peer_store) == NOAH_PROFILE_PEER_STORE_COMMITTED) {
                if (!publish_validated_descriptor(owner) || !noah_profile_split_reconciler_refresh_authority(&owner->reconciler)) {
                    owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
                }
            }
            if (!activating_boot && supersede_host_precommit(owner)) {
                return true;
            }
        } else if (!activating_boot && admission != NOAH_PROFILE_STORAGE_ADMISSION_HOST) {
            worked = scan_peer_activation(owner);
        }
        if (worked) {
            return true;
        }
    }
    return false;
}

bool noah_profile_owner_scan(noah_profile_owner_t *owner, bool master, uint32_t now_ms) {
    noah_profile_validator_v1_result_t validator_result;

    if (!owner) {
        return false;
    }
    switch (owner->state) {
        case NOAH_PROFILE_OWNER_VALIDATING_COMPILED:
            validator_result = noah_profile_validator_v1_step(&owner->staging.compiled_validator, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, NULL);
            if (validator_result == NOAH_PROFILE_VALIDATOR_V1_VALID) {
                if (!initialize_runtime_graph(owner)) {
                    fail_integration(owner);
                }
            } else if (validator_result != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
                owner->state = NOAH_PROFILE_OWNER_COMPILED_ERROR;
            }
            return true;
        case NOAH_PROFILE_OWNER_DISCOVERING:
            return scan_discovery(owner);
        case NOAH_PROFILE_OWNER_ADOPTING_COMMITTED:
            return scan_adoption(owner);
        case NOAH_PROFILE_OWNER_RECONCILING_COMMITTED:
            return scan_boot_reconciliation(owner, master, now_ms);
        case NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED:
            return scan_running(owner, master, now_ms, true);
        case NOAH_PROFILE_OWNER_READY_COMPILED:
        case NOAH_PROFILE_OWNER_READY_VALIDATED:
            return scan_running(owner, master, now_ms, false);
        default:
            return false;
    }
}

bool noah_profile_owner_receive(noah_profile_owner_t *owner, uint8_t *frame, size_t length) {
    return owner && (owner->state == NOAH_PROFILE_OWNER_READY_COMPILED || owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED) && noah_profile_candidate_transaction_receive(&owner->host_transaction, frame, length);
}

noah_profile_owner_state_t noah_profile_owner_state(const noah_profile_owner_t *owner) {
    return owner ? owner->state : NOAH_PROFILE_OWNER_UNINITIALIZED;
}

noah_profile_store_result_t noah_profile_owner_discovery_result(const noah_profile_owner_t *owner) {
    return owner ? owner->discovery_result : NOAH_PROFILE_STORE_INVALID_ARGUMENT;
}

const noah_profile_store_t *noah_profile_owner_store(const noah_profile_owner_t *owner) {
    return owner ? &owner->store : NULL;
}

const noah_profile_store_record_t *noah_profile_owner_committed(const noah_profile_owner_t *owner) {
    return owner && owner->descriptor_readable && owner->committed_descriptor.has_profile && record_matches_descriptor(&owner->store.committed, &owner->committed_descriptor) ? &owner->store.committed : NULL;
}

noah_profile_candidate_transaction_t *noah_profile_owner_host_transaction(noah_profile_owner_t *owner) {
    return owner && (owner->state == NOAH_PROFILE_OWNER_READY_COMPILED || owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED) ? &owner->host_transaction : NULL;
}

noah_profile_split_reconciler_t *noah_profile_owner_split_reconciler(noah_profile_owner_t *owner) {
    return owner && owner->split_initialized ? &owner->reconciler : NULL;
}

bool noah_profile_owner_status(const noah_profile_owner_t *owner, noah_profile_owner_status_t *status) {
    noah_effective_profile_status_t      provider;
    noah_profile_split_authority_status_t authority;
    const noah_profile_store_record_t    *committed;

    if (!owner || !status || owner->state == NOAH_PROFILE_OWNER_UNINITIALIZED) {
        return false;
    }
    memset(status, 0, sizeof(*status));
    status->candidate.error          = noah_profile_candidate_v1_no_error();
    status->owner_state              = owner->state;
    status->compiled_default_digest  = owner->compiled.metadata.digest;
    status->action_abi_digest        = owner->compiled.metadata.action_abi_digest;
    status->supported_domain_mask    = owner->compatibility.allowed_domain_mask;

    if (owner->runtimes_installed && noah_effective_profile_provider_status(&owner->provider, &provider) == NOAH_EFFECTIVE_PROFILE_OK) {
        status->provider_known            = true;
        status->active                    = provider.active;
        status->has_pending               = provider.has_pending;
        status->safe_boundary_reason_mask = provider.safe_boundary_reason_mask;
        if (provider.has_pending) {
            status->pending = provider.pending;
        }
    }
    if (owner->runtimes_installed) {
        noah_profile_candidate_transaction_status(&owner->host_transaction, &status->candidate);
        status->candidate_pending              = owner->host_transaction.has_candidate || owner->host_transaction.mailbox.pending;
        status->last_committed_transaction_id = owner->host_transaction.last_committed_transaction_id;
    }
    committed = noah_profile_owner_committed(owner);
    if (committed) {
        status->committed     = owner->committed_descriptor;
        status->has_committed = true;
    }
    if (owner->split_initialized && noah_profile_split_authority_status(&owner->reconciler.authority, &authority)) {
        status->authority_state  = authority.state;
        status->peer             = authority.peer;
        status->peer_known       = authority.peer.readable;
        status->transfer_pending = authority.transfer_pending;
        status->peer_converged   = !authority.transfer_pending && (authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED);
    }
    return true;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_owner_t) <= NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT, "live-profile owner state exceeded its 32-bit engineering regression policy");
#endif

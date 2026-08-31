// ──────────────────────────────────────────────────────────────────────────
// Single Live-Profile Runtime Owner
// ──────────────────────────────────────────────────────────────────────────

#include "profile_owner.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "../storage/profile_storage_layout.h"

enum {
    OWNER_SCHEDULE_HOST = 0u,
    OWNER_SCHEDULE_SPLIT,
    OWNER_SCHEDULE_PEER_ACTIVATION,
    OWNER_SCHEDULE_COUNT,
};

static bool scan_running(noah_profile_owner_t *owner, bool master, uint32_t now_ms, bool activating_boot);

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
    observer         = owner->config.peer_required ? noah_profile_split_authority_peer_observer : no_peer_observer;
    observer_context = owner->config.peer_required ? (void *)&owner->reconciler.authority : NULL;
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

    return owner && owner->split_initialized && noah_profile_split_authority_peer_observer((void *)&owner->reconciler.authority, &unresolved_count) && unresolved_count == 0u;
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

    if (!owner || noah_effective_profile_provider_status(&owner->provider, &status) != NOAH_EFFECTIVE_PROFILE_OK) {
        return;
    }
    owner->state = status.active.kind == NOAH_EFFECTIVE_PROFILE_KIND_VALIDATED_PROFILE ? NOAH_PROFILE_OWNER_READY_VALIDATED : NOAH_PROFILE_OWNER_READY_COMPILED;
}

static bool scan_running(noah_profile_owner_t *owner, bool master, uint32_t now_ms, bool activating_boot) {
    noah_profile_storage_admission_owner_t admission = noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner));

    if (!activating_boot && admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST && owner->host_activity_known && !owner->host_transaction.mailbox.pending && elapsed_at_least(now_ms, owner->host_last_activity_at, NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS)) {
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
                worked = noah_profile_candidate_transaction_scan(&owner->host_transaction);
                if (worked) {
                    owner->host_last_activity_at = now_ms;
                    owner->host_activity_known   = true;
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_DURABILITY_UNKNOWN) {
                    owner->state = NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN;
                    return true;
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_ACTIVATION_FAILED) {
                    owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
                    return true;
                }
                if (worked && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING) {
                    (void)publish_validated_descriptor(owner);
                }
                if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE && noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_HOST) {
                    owner->host_activity_known = false;
                    refresh_ready_state(owner);
                }
            }
        } else if (current == OWNER_SCHEDULE_SPLIT) {
            noah_profile_split_reconcile_mode_t mode = activating_boot || admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST ? NOAH_PROFILE_SPLIT_RECONCILE_CONVERGENCE_ONLY : NOAH_PROFILE_SPLIT_RECONCILE_FULL;
            worked = owner->split_initialized && noah_profile_split_reconciler_scan_mode(&owner->reconciler, master, now_ms, mode);
            if (worked && noah_profile_peer_store_backend_state(&owner->peer_store) == NOAH_PROFILE_PEER_STORE_COMMITTED) {
                (void)publish_validated_descriptor(owner);
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

const noah_profile_store_record_t *noah_profile_owner_committed(const noah_profile_owner_t *owner) {
    return owner && owner->descriptor_readable && owner->committed_descriptor.has_profile && record_matches_descriptor(&owner->store.committed, &owner->committed_descriptor) ? &owner->store.committed : NULL;
}

noah_profile_candidate_transaction_t *noah_profile_owner_host_transaction(noah_profile_owner_t *owner) {
    return owner && (owner->state == NOAH_PROFILE_OWNER_READY_COMPILED || owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED) ? &owner->host_transaction : NULL;
}

noah_profile_split_reconciler_t *noah_profile_owner_split_reconciler(noah_profile_owner_t *owner) {
    return owner && owner->split_initialized ? &owner->reconciler : NULL;
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_owner_t) <= NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT, "live-profile owner state exceeded its 32-bit engineering regression policy");
#endif

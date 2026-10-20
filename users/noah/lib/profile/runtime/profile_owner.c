#include "effective_pd_runtime.h"
// ──────────────────────────────────────────────────────────────────────────
// Single Live-Profile Runtime Owner
// ──────────────────────────────────────────────────────────────────────────

#include "profile_owner.h"
#include "profile_action_placement_v1.h"
#include "effective_settings_runtime.h"
#include "effective_keymap_macros_runtime.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "../storage/profile_checksum.h"
#include "../storage/profile_storage_layout.h"

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
static uint32_t noah_profile_owner_ready_refresh_count;
static uint32_t noah_profile_owner_profile_held_count;
#endif

enum {
    OWNER_SCHEDULE_HOST = 0u,
    OWNER_SCHEDULE_SPLIT,
    OWNER_SCHEDULE_PEER_ACTIVATION,
    OWNER_SCHEDULE_WORKING,
    OWNER_SCHEDULE_COUNT,
    // Save reads the working copy this much per scan for its checksums, and
    // copies it into the candidate this much per REUSE.
    OWNER_SAVE_CHECKSUM_BYTES = 1024u,
    OWNER_SAVE_REUSE_BYTES    = NOAH_PROFILE_CANDIDATE_V1_REUSE_MAX,
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

static bool local_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, uint8_t *bytes, uint8_t length);

static bool host_staged_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_owner_t          *owner = context;
    noah_profile_store_candidate_t candidate;

    if (!owner || !descriptor || !owner->host_barrier_descriptor_known || !descriptor_equal(descriptor, &owner->host_barrier_descriptor)) {
        return false;
    }
    if (!noah_profile_candidate_store_backend_staged_candidate(candidate_backend(owner), &candidate)) {
        // After this half's commit marker the staged candidate is the
        // committed record. A peer that lost its prepared copy, for example
        // because it rebooted, is sent the copy again from that record.
        return local_read(context, descriptor, offset, bytes, length);
    }
    if (!descriptor_equal(descriptor, &(noah_profile_split_descriptor_t){
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

// The ranges the host filled with REUSE, which the peer can copy from its
// own active profile. Only while the candidate is still staged: after this
// half's commit marker a copy is sent again from the committed record, as
// bytes.
static bool host_reuse_plan(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, noah_profile_split_reuse_range_t *range) {
    noah_profile_owner_t                      *owner = context;
    noah_profile_store_candidate_t             candidate;
    noah_profile_split_descriptor_t            staged;
    noah_profile_candidate_v1_source_t         source;
    noah_profile_candidate_store_reuse_range_t reused;

    if (!owner || !descriptor || !range || !owner->host_barrier_descriptor_known || !descriptor_equal(descriptor, &owner->host_barrier_descriptor) || !noah_profile_candidate_store_backend_staged_candidate(candidate_backend(owner), &candidate)) {
        return false;
    }
    staged = descriptor_from_candidate(&candidate);
    if (!descriptor_equal(descriptor, &staged) || !noah_profile_candidate_store_backend_staged_reuse(candidate_backend(owner), &candidate, offset, &source, &reused)) {
        return false;
    }
    *range = (noah_profile_split_reuse_range_t){
        .source        = {.generation = source.generation, .digest = source.digest, .crc32 = source.crc32, .kind = source.kind, .origin = source.origin},
        .offset        = reused.offset,
        .source_offset = reused.source_offset,
        .length        = reused.length,
    };
    return true;
}

static bool owner_peer_observer(void *context, uint8_t *unresolved_count) {
    noah_profile_owner_t                 *owner = context;
    noah_profile_split_authority_status_t authority;

    if (!owner || !unresolved_count || !owner->descriptor_readable || !owner->split_initialized) {
        return false;
    }
    if (owner->host_barrier_peer_confirmed && owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING && owner->host_barrier_descriptor_known && descriptor_equal(&owner->committed_descriptor, &owner->host_barrier_descriptor)) {
        // This Apply already saw the peer durably commit exactly this record
        // before activation was authorized. Activating needs no second, live
        // observation of the peer, so a link lost meanwhile cannot hold it.
        *unresolved_count = 0u;
        return true;
    }
    if (!noah_profile_split_authority_status(&owner->reconciler.authority, &authority)) {
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
    owner->host_barrier_waiting_boundary       = false;
    owner->host_barrier_peer_confirmed         = false;
    owner->host_cancel_reason                  = NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE;
    owner->host_cancel_pending                 = false;
    owner->host_barrier_progress_offset        = 0u;
    owner->host_barrier_progress_phase         = 0u;
}

static bool host_session_cleanup_needed(const noah_profile_owner_t *owner) {
    return owner && (owner->host_activity_known || owner->host_barrier_descriptor_known || owner->host_barrier_started || owner->host_barrier_local_published || owner->host_barrier_peer_commit_authorized || owner->host_cancel_pending || owner->host_cancel_reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE || owner->host_barrier_progress_offset != 0u);
}

static bool peer_can_supersede_host(const noah_profile_owner_t *owner, const noah_profile_split_descriptor_t *peer) {
    return owner && peer && peer->readable && peer->has_profile && noah_profile_split_descriptor_valid(peer) && peer->schema_major == NOAH_PROFILE_STORE_SCHEMA_MAJOR && peer->schema_minor == NOAH_PROFILE_STORE_SCHEMA_MINOR && (peer->domain_mask & (uint8_t)~owner->compatibility.allowed_domain_mask) == 0u && peer->compiled_default_digest == owner->compiled.metadata.digest && peer->action_abi_digest == owner->compiled.metadata.action_abi_digest && owner->store.prepare_active && peer->generation >= owner->store.candidate.generation;
}

static bool supersede_host_precommit(noah_profile_owner_t *owner) {
    noah_profile_split_authority_status_t authority;

    if (!owner || !owner->split_initialized || noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner)) != NOAH_PROFILE_STORAGE_ADMISSION_HOST || !host_state_is_precommit(owner->host_transaction.status.state) || !noah_profile_split_authority_status(&owner->reconciler.authority, &authority) || !peer_can_supersede_host(owner, &authority.peer)) {
        return false;
    }
    return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED);
}

static bool record_payload_start(noah_profile_slot_t slot, noah_profile_storage_address_t *address) {
    if (!address) {
        return false;
    }
    if (slot == NOAH_PROFILE_SLOT_A) {
        *address = (NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    if (slot == NOAH_PROFILE_SLOT_B) {
        *address = (NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
        return true;
    }
    return false;
}

static bool record_matches_descriptor(const noah_profile_store_record_t *record, const noah_profile_split_descriptor_t *descriptor) {
    return record && descriptor && descriptor->readable && descriptor->has_profile && record->slot != NOAH_PROFILE_SLOT_NONE && record->schema_major == descriptor->schema_major && record->schema_minor == descriptor->schema_minor && record->domain_mask == descriptor->domain_mask && record->flags == descriptor->profile_flags && record->payload_length == descriptor->payload_length && record->generation == descriptor->generation && record->origin_half == descriptor->origin_half && record->payload_crc32 == descriptor->payload_crc32 && record->payload_digest == descriptor->payload_digest && record->compiled_default_digest == descriptor->compiled_default_digest && record->action_abi_digest == descriptor->action_abi_digest && record->format_version == NOAH_PROFILE_STORE_FORMAT_VERSION;
}

static bool local_descriptor(void *context, noah_profile_split_descriptor_t *descriptor) {
    noah_profile_owner_t *owner = context;

    if (!owner || !descriptor || !owner->descriptor_readable) {
        return false;
    }
    // Publication establishes validity; the readable descriptor is immutable
    // until the next publication. The reconciler validates changed contents.
    *descriptor = owner->committed_descriptor;
    return true;
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

static bool local_read(void *context, const noah_profile_split_descriptor_t *descriptor, uint32_t offset, uint8_t *bytes, uint8_t length) {
    noah_profile_owner_t              *owner = context;
    const noah_profile_store_record_t *record;
    noah_profile_storage_address_t  payload_start;

    if (!owner || !descriptor || !bytes || length == 0u || !owner->store.io.read) {
        return false;
    }
    record = owner->store.committed.slot == NOAH_PROFILE_SLOT_NONE ? NULL : &owner->store.committed;
    if (!record_matches_descriptor(record, descriptor) || !record_payload_start(record->slot, &payload_start) || offset > record->payload_length || length > record->payload_length - offset) {
        return false;
    }
    return noah_profile_store_read_span(&owner->store, (payload_start + offset), bytes, length);
}

static void fail_integration(noah_profile_owner_t *owner) {
    if (!owner) {
        return;
    }
    if (owner->runtimes_installed) {
        noah_effective_key_behavior_runtime_uninstall(&owner->key_behaviors);
        noah_effective_rgb_runtime_uninstall(&owner->rgb);
#ifdef COMBO_ENABLE
        noah_effective_combo_runtime_uninstall(&owner->combos);
#endif
        owner->runtimes_installed = false;
    }
    owner->state = NOAH_PROFILE_OWNER_INTEGRATION_ERROR;
}

// One invalidator per runtime that follows the active profile.
enum {
    OWNER_INVALIDATOR_COUNT = 3u
#ifdef COMBO_ENABLE
                              + 1u
#endif
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
                              + 1u
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
                              + 1u
#endif
};
_Static_assert((unsigned)OWNER_INVALIDATOR_COUNT <= (unsigned)NOAH_EFFECTIVE_PROFILE_MAX_INVALIDATORS, "the provider holds every runtime's invalidator");

// Boot only: kept out of line so its locals do not widen the durable I/O
// step's frame on every scan.
static __attribute__((noinline)) bool initialize_runtime_graph(noah_profile_owner_t *owner) {
    noah_effective_profile_invalidator_t     invalidators[OWNER_INVALIDATOR_COUNT];
    uint8_t invalidator_count = 0;
    noah_profile_candidate_backend_t         host_backend;
    noah_profile_candidate_compatibility_t   host_compatibility;
    noah_profile_split_reconciler_config_t   split_config;
    noah_profile_activation_peer_observer_fn observer;
    void                                    *observer_context;

    // The provider does not exist until provider_init below. Borrow its pending
    // view for boot validation, then copy it into the independent snapshot
    // before provider_init clears the provider. No extra permanent profile view
    // or large automatic workspace is needed for this boot-only operation.
    if (!owner || noah_profile_validator_v1_profile(&owner->staging.compiled_validator, &owner->provider.pending.profile, NULL) != NOAH_PROFILE_VALIDATOR_V1_VALID || noah_effective_profile_snapshot_make_compiled(&owner->provider.pending.profile, &owner->compiled_reader, 0u, &owner->compiled_snapshot) != NOAH_EFFECTIVE_PROFILE_OK) {
        return false;
    }

    noah_effective_key_behavior_runtime_init(&owner->key_behaviors);
    noah_effective_rgb_runtime_init(&owner->rgb);
#ifdef COMBO_ENABLE
    noah_effective_combo_runtime_init(&owner->combos);
#endif
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){.callback = noah_effective_keymap_macros_invalidate, .context = NULL};
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){
        .callback = noah_effective_key_behavior_runtime_invalidate,
        .context  = &owner->key_behaviors,
    };
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){
        .callback = noah_effective_rgb_runtime_invalidate,
        .context  = &owner->rgb,
    };
#ifdef COMBO_ENABLE
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){.callback = noah_effective_combo_runtime_invalidate, .context = &owner->combos};
#endif
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){.callback = noah_effective_settings_invalidate, .context = &owner->compiled_snapshot};
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
    invalidators[invalidator_count++] = (noah_effective_profile_invalidator_t){.callback = noah_effective_pd_invalidate, .context = NULL};
#endif
    observer         = owner->config.peer_required ? owner_peer_observer : no_peer_observer;
    observer_context = owner->config.peer_required ? owner : NULL;
    noah_profile_activation_policy_init(&owner->activation_policy, observer, observer_context);
    if (noah_effective_profile_provider_init(&owner->provider, &owner->compiled_snapshot, noah_profile_activation_policy_safe_boundary, &owner->activation_policy, invalidators, invalidator_count
                                             ) != NOAH_EFFECTIVE_PROFILE_OK) {
        return false;
    }
    noah_effective_keymap_macros_invalidate(NULL, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    noah_effective_key_behavior_runtime_invalidate(&owner->key_behaviors, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    noah_effective_rgb_runtime_invalidate(&owner->rgb, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    if (!owner->rgb.banks[owner->rgb.active_index].valid) return false;
#ifdef COMBO_ENABLE
    owner->combos.compiled_defaults = &owner->compiled_snapshot;
    noah_effective_combo_runtime_invalidate(&owner->combos, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    if (!owner->combos.valid) return false;
#endif
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    noah_effective_settings_boot(true);
    noah_effective_settings_invalidate(&owner->compiled_snapshot, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    noah_effective_settings_boot(false);
    if (!noah_effective_settings_length()) return false;
#endif
#ifdef NOAH_PD_PROFILE_ENABLE
    noah_effective_pd_invalidate(NULL, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
    if (!noah_effective_pd_ready()) return false;
#endif

    noah_profile_store_init(&owner->store, owner->config.store_io,
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = owner->compiled.metadata.digest,
                                .action_abi_digest       = owner->compiled.metadata.action_abi_digest,
                            });
    noah_profile_candidate_store_backend_init(candidate_backend(owner), &owner->store, &owner->provider, &owner->compatibility, owner->compiled.metadata.digest, owner->config.origin_half);
    // A candidate the host asks the keyboard to save is also held to where its
    // actions are placed; the compiled defaults and a committed record are not.
    owner->candidate_runtime = (noah_profile_validator_v1_runtime_t){
        .combo_to_native     = owner->compatibility.runtime ? owner->compatibility.runtime->combo_to_native : NULL,
        .placement_supported = noah_profile_action_placement_v1_supported,
    };
    noah_profile_candidate_store_backend_hold_candidates_to(candidate_backend(owner), &owner->candidate_runtime);
    if (!candidate_backend(owner)->reuse_guard_installed) {
        return false;
    }
    // Live edits (D-F32) are held to a host candidate's rules; what the other
    // half sends, and Save, to a saved record's.
    owner->edit_compatibility         = owner->compatibility;
    owner->edit_compatibility.runtime = &owner->candidate_runtime;
    if (owner->config.working_bytes && owner->config.working_capacity) {
        noah_profile_working_copy_init(&owner->working, owner->config.working_bytes, owner->config.working_capacity, &owner->provider, &owner->edit_compatibility, &owner->compatibility, owner->config.working_session);
        noah_profile_working_sync_init(&owner->working_sync, &owner->working, owner->config.working_exchange, owner->config.working_transport_context, &owner->config.edit_mode_storage);
        owner->working_ready = true;
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
            .local_context       = owner,
            .local_descriptor    = local_descriptor,
            .local_read          = local_read,
            .transport_context   = owner->config.split_transport_context,
            .exchange            = owner->config.split_exchange,
            .peer_store          = &owner->peer_store,
            .prepared_reuse_plan = host_reuse_plan,
            .storage_paused         = owner->config.storage_paused,
            .storage_paused_context = owner->config.storage_paused_context,
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
#ifdef COMBO_ENABLE
    if (!noah_effective_combo_runtime_install(&owner->combos)) {
        noah_effective_key_behavior_runtime_uninstall(&owner->key_behaviors);
        noah_effective_rgb_runtime_uninstall(&owner->rgb);
        return false;
    }
#endif
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
    owner->compiled_reader = noah_profile_compiled_v1_reader(&owner->compiled);
    // begin copies this declaration; no second lifetime-long copy is needed.
    const noah_profile_validator_v1_declaration_t declaration = {
        .schema_major      = NOAH_PROFILE_BLOB_V1_SCHEMA_MAJOR,
        .schema_minor      = NOAH_PROFILE_BLOB_V1_SCHEMA_MINOR,
        .domain_mask       = owner->compiled.metadata.domain_mask,
        .flags             = 0u,
        .byte_length       = owner->compiled.metadata.byte_length,
        .crc32             = owner->compiled.metadata.crc32,
        .digest            = owner->compiled.metadata.digest,
        .action_abi_digest = owner->compiled.metadata.action_abi_digest,
    };
    result = noah_profile_validator_v1_begin(&owner->staging.compiled_validator, &owner->compiled_reader, 0u, &declaration, &owner->compatibility, NULL);
    if (result != NOAH_PROFILE_VALIDATOR_V1_IN_PROGRESS) {
        owner->state = NOAH_PROFILE_OWNER_COMPILED_ERROR;
        return false;
    }
    owner->state = NOAH_PROFILE_OWNER_VALIDATING_COMPILED;
    return true;
}

// Nothing saved: the compiled defaults' settings are what QMK runs (D-F32),
// whatever its EEPROM held. Boot-only and out of line: its by-value
// identities would otherwise widen every scan's storage frame.
static __attribute__((noinline)) void project_compiled_settings(noah_profile_owner_t *owner) {
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    noah_effective_settings_invalidate(&owner->compiled_snapshot, 0u, owner->compiled_snapshot.identity, owner->compiled_snapshot.identity, &owner->compiled_snapshot);
#else
    (void)owner;
#endif
}

static bool scan_discovery(noah_profile_owner_t *owner) {
    noah_profile_store_record_t selected;

    owner->discovery_result = noah_profile_store_boot_select_step(&owner->store, NOAH_PROFILE_CANDIDATE_SCAN_BYTE_BUDGET, &selected);
    if (owner->discovery_result == NOAH_PROFILE_STORE_IN_PROGRESS) {
        return true;
    }
    if (owner->discovery_result == NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE) {
        publish_compiled_descriptor(owner);
        project_compiled_settings(owner);
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

// A copy taken from the other half is published as the step that committed
// it returns, while this half still holds its storage for that copy. The
// peer store stays COMMITTED after the copy activates (it answers a sender's
// retry of the finished copy), so its state alone is no sign of a new copy:
// a later Apply or Save holds the storage for its own candidate, and
// publishing then stopped the owner with STORAGE_ERROR, frozen until a
// restart (keyboard check, 2026-10-20). Out of line: it runs in the scan's
// shared frame.
static __attribute__((noinline)) bool took_peer_copy(const noah_profile_owner_t *owner) {
    return noah_profile_candidate_store_backend_admission_owner(&owner->staging.candidate_backend) == NOAH_PROFILE_STORAGE_ADMISSION_PEER && noah_profile_peer_store_backend_state(&owner->peer_store) == NOAH_PROFILE_PEER_STORE_COMMITTED;
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
    if (worked && took_peer_copy(owner)) {
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
        owner->state = noah_effective_profile_provider_status(&owner->provider, &status) == NOAH_EFFECTIVE_PROFILE_OK && status.active.generation == 0u ? NOAH_PROFILE_OWNER_READY_COMPILED : NOAH_PROFILE_OWNER_READY_VALIDATED;
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
    // A working copy over a saved profile still runs that saved generation.
    owner->state = status.active.generation != 0u ? NOAH_PROFILE_OWNER_READY_VALIDATED : NOAH_PROFILE_OWNER_READY_COMPILED;
}

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
uint32_t noah_profile_owner_test_ready_refresh_count(void) {
    return noah_profile_owner_ready_refresh_count;
}

uint32_t noah_profile_owner_test_profile_held_count(void) {
    return noah_profile_owner_profile_held_count;
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
        case NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_TRANSFER_FAILED:
            return noah_profile_candidate_transaction_peer_failed_precommit(&owner->host_transaction);
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
    if (!owner || (reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_SUPERSEDED && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_PREPARE_YIELDED && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_TRANSFER_FAILED && reason != NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE)) {
        return false;
    }
    if (reason == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE && !host_mailbox_is_matching_abort(owner)) {
        return false;
    }
    if (!host_state_is_precommit(owner->host_transaction.status.state)) {
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
    noah_profile_store_candidate_t  candidate;
    noah_profile_split_descriptor_t descriptor;
    noah_profile_split_descriptor_t prepared;
    noah_profile_split_descriptor_t provisional_peer;
    bool                            conflict = false;

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
        bool began = noah_profile_split_reconciler_prepared_push_begin(&owner->reconciler, &owner->host_barrier_descriptor, owner, host_staged_read);
        if (!began) {
            return false;
        }
        owner->host_barrier_started = true;
        return true;
    }
    {
        // The copy stopped for good (its storage retries are spent, or the
        // peer answered a terminal error): end the Apply now with that reason
        // instead of waiting out the no-progress timeout.
        noah_profile_split_reconciler_status_t split;
        if (noah_profile_split_reconciler_status(&owner->reconciler, &split) && split.prepared_push_active && split.state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED) {
            return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_PEER_TRANSFER_FAILED);
        }
    }
    if (!noah_profile_split_reconciler_prepared_push_ready(&owner->reconciler, &prepared)) {
        return false;
    }
    owner->host_barrier_waiting_boundary = false;
    if (!descriptor_equal(&prepared, &owner->host_barrier_descriptor)) {
        fail_integration(owner);
    } else if ((noah_profile_activation_policy_safe_boundary(&owner->activation_policy) &
                ~NOAH_PROFILE_ACTIVATION_REASON_PEER) != 0u) {
        // Waiting here, before anything is durable, lets a held key or locked
        // layer end the Apply cleanly on timeout. Activation waits for the
        // same boundary.
        owner->host_barrier_waiting_boundary = true;
        return false;
    } else if (!noah_profile_candidate_transaction_authorize_commit(&owner->host_transaction)) {
        if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED && owner->host_transaction.status.error.code == NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE) {
            return request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_STORAGE_FAILURE);
        }
        fail_integration(owner);
    }
    return true;
}

static void note_host_barrier_progress(noah_profile_owner_t *owner, uint32_t now_ms) {
    uint32_t offset;
    uint8_t  phase;

    // The other half waiting for a quiet keyboard before it can erase is not
    // a stalled copy (D-F32): its BUSY_STORAGE_PAUSED keeps the save's
    // deadlines, as its own clock stands still meanwhile.
    if (owner && owner->reconciler.busy_streak != 0u && owner->reconciler.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_STORAGE_PAUSED && owner->host_activity_known) {
        owner->host_last_activity_at = now_ms;
        return;
    }
    if (!owner || !owner->host_barrier_started || owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER || !noah_profile_split_reconciler_progress(&owner->reconciler, &offset, &phase) || (offset == owner->host_barrier_progress_offset && phase <= owner->host_barrier_progress_phase)) {
        return;
    }
    owner->host_barrier_progress_offset = offset;
    owner->host_barrier_progress_phase  = phase;
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
        // Once the peer has been seen holding exactly this commit, losing
        // sight of it again (a lost link, a peer reboot) does not undo that
        // confirmation. Conflicts above still end the Apply.
        if (!owner->host_barrier_peer_confirmed) {
            return false;
        }
    } else {
        owner->host_barrier_peer_confirmed = true;
    }
    // One store (D-F31): once both halves hold the commit, the new profile
    // activates at the next safe boundary. Nothing else has to catch up.
    if (!noah_profile_candidate_transaction_authorize_activation(&owner->host_transaction)) {
        fail_integration(owner);
    }
    return true;
}

static bool save_step(noah_profile_owner_t *owner);
static bool automatic_step(noah_profile_owner_t *owner);

// The working copy's own scan grant (D-F32): Save, then the edit, then the
// other half. Out of line, as is everything it calls into, so the scan's
// shared frame (every durable path runs through scan_running) does not grow.
static __attribute__((noinline)) bool scan_working(noah_profile_owner_t *owner, bool master, uint32_t now_ms) {
    if (!owner->working_ready) {
        return false;
    }
    if (owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE && save_step(owner)) {
        return true;
    }
    // An edit the host stopped sending: aborted, so Apply, Save, Revert and
    // the other half are not held by it forever.
    if (master && owner->working.phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING && elapsed_at_least(now_ms, owner->edit_activity_at, NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS)) {
        (void)noah_profile_working_copy_abort(&owner->working, owner->working.transaction_id);
        return true;
    }
    if (noah_profile_working_copy_scan(&owner->working)) {
        return true;
    }
    if (noah_profile_working_sync_store_mode(&owner->working_sync, now_ms)) {
        return true;
    }
    if (master && automatic_step(owner)) {
        return true;
    }
    if (!owner->config.peer_required) {
        return false;
    }
    return master ? noah_profile_working_sync_master_scan(&owner->working_sync, now_ms) : noah_profile_working_sync_peer_scan(&owner->working_sync);
}

static __attribute__((noinline)) void observe_working(noah_profile_owner_t *owner) {
    (void)noah_profile_working_copy_observe(&owner->working, owner->save.phase == NOAH_PROFILE_OWNER_SAVE_WAIT);
    if (owner->auto_save.observed_revision != owner->working.revision || !owner->working.active || noah_profile_working_copy_busy(&owner->working) || !owner->config.keyboard_quiet || !owner->config.keyboard_quiet(owner->config.keyboard_quiet_context)) {
        owner->auto_save.quiet_since = owner->now_ms;
    }
    if (owner->auto_save.observed_revision != owner->working.revision) {
        // Only a published client edit disqualifies an adjustment-only copy;
        // a refused proposal must not strand the keyboard's earlier edits.
        if (!owner->auto_save.edit_transaction) owner->auto_save.apply_adjustments_only = false;
        owner->auto_save.observed_revision = owner->working.revision;
        owner->auto_save.attempted = false;
    }
    if (!owner->working.active && !owner->auto_save.edit_transaction) {
        owner->auto_save.apply_adjustments_only = false;
    }
}

static bool scan_running(noah_profile_owner_t *owner, bool master, uint32_t now_ms, bool activating_boot) {
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
    noah_effective_settings_boot(activating_boot);
#endif
    // Anything that published since the last scan (an Apply or a save
    // activating, the other half's newer profile) ended any working copy.
    if (owner->working_ready) {
        observe_working(owner);
    }
    noah_profile_storage_admission_owner_t admission = noah_profile_candidate_store_backend_admission_owner(candidate_backend(owner));
    uint32_t                               host_timeout_ms;

    if (!activating_boot && owner->host_cancel_pending && advance_host_precommit_cancel(owner)) {
        return true;
    }
    if (!activating_boot && supersede_host_precommit(owner)) {
        return true;
    }

    host_timeout_ms = owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_barrier_started ? NOAH_PROFILE_OWNER_HOST_BARRIER_NO_PROGRESS_MS : NOAH_PROFILE_OWNER_HOST_TIMEOUT_MS;
    // Only the decision is exempt from the lease: from COMMITTING on nothing
    // expires, and the peer keeps the target recovery copy.
    if (!activating_boot && !owner->host_cancel_pending && admission == NOAH_PROFILE_STORAGE_ADMISSION_HOST && owner->host_activity_known && !owner->host_transaction.mailbox.pending && elapsed_at_least(now_ms, owner->host_last_activity_at, host_timeout_ms) && host_state_is_precommit(owner->host_transaction.status.state) && request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_TIMEOUT)) {
        return true;
    }

    // A repeated step tries the subsystem whose step last worked first, then,
    // if it has nothing, the ordinary round.
    uint8_t visits = (uint8_t)OWNER_SCHEDULE_COUNT + (owner->scan_again ? 1u : 0u);

    for (uint8_t visited = 0u; visited < visits; visited++) {
        uint8_t current;
        bool    worked = false;

        if (owner->scan_again) {
            owner->scan_again = false;
            current           = owner->scheduler_last;
        } else {
            current                 = owner->scheduler_cursor;
            owner->scheduler_cursor = (uint8_t)((owner->scheduler_cursor + 1u) % OWNER_SCHEDULE_COUNT);
        }
        if (current == OWNER_SCHEDULE_HOST) {
            if (activating_boot) {
                worked = scan_activation(owner);
            } else if (admission != NOAH_PROFILE_STORAGE_ADMISSION_PEER) {
                if (owner->host_cancel_pending) {
                    worked = false;
                } else if (host_mailbox_is_matching_abort(owner) && host_state_is_precommit(owner->host_transaction.status.state)) {
                    worked = request_host_precommit_cancel(owner, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
                } else if (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER && owner->host_transaction.mailbox.pending) {
                    worked = noah_profile_candidate_transaction_scan(&owner->host_transaction);
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
            worked                                   = owner->split_initialized && noah_profile_split_reconciler_scan_mode(&owner->reconciler, master, now_ms, mode);
            if (worked && !activating_boot) {
                note_host_barrier_progress(owner, now_ms);
            }
            if (worked && took_peer_copy(owner)) {
                if (!publish_validated_descriptor(owner) || !noah_profile_split_reconciler_refresh_authority(&owner->reconciler)) {
                    owner->state = NOAH_PROFILE_OWNER_STORAGE_ERROR;
                }
            }
            if (!activating_boot && supersede_host_precommit(owner)) {
                return true;
            }
        } else if (current == OWNER_SCHEDULE_PEER_ACTIVATION) {
            worked = !activating_boot && admission != NOAH_PROFILE_STORAGE_ADMISSION_HOST && scan_peer_activation(owner);
        } else {
            worked = !activating_boot && scan_working(owner, master, now_ms);
        }
        if (worked) {
            owner->scheduler_last = current;
            return true;
        }
    }
    return false;
}

void noah_profile_owner_scan_again(noah_profile_owner_t *owner, bool again) {
    if (owner) {
        owner->scan_again = again;
    }
}

bool noah_profile_owner_storage_busy(const noah_profile_owner_t *owner) {
    noah_profile_peer_store_state_t peer;

    if (!owner) {
        return false;
    }
    switch (owner->state) {
        case NOAH_PROFILE_OWNER_VALIDATING_COMPILED:
        case NOAH_PROFILE_OWNER_DISCOVERING:
        case NOAH_PROFILE_OWNER_ADOPTING_COMMITTED:
        case NOAH_PROFILE_OWNER_RECONCILING_COMMITTED:
        case NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED:
            return true;
        case NOAH_PROFILE_OWNER_READY_COMPILED:
        case NOAH_PROFILE_OWNER_READY_VALIDATED:
            break;
        default:
            return false;
    }
    switch (owner->host_transaction.status.state) {
        case NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_COMMITTING:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_ACTIVATING:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER:
        case NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER:
            return true;
        default:
            break;
    }
    peer = noah_profile_peer_store_backend_state(&owner->peer_store);
    if (peer == NOAH_PROFILE_PEER_STORE_RECEIVING || peer == NOAH_PROFILE_PEER_STORE_VALIDATING || peer == NOAH_PROFILE_PEER_STORE_PREPARING || peer == NOAH_PROFILE_PEER_STORE_COMMITTING) {
        return true;
    }
    return owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE || owner->working.phase == NOAH_PROFILE_WORKING_PHASE_LOADING || owner->working.phase == NOAH_PROFILE_WORKING_PHASE_VALIDATING;
}

bool noah_profile_owner_scan(noah_profile_owner_t *owner, bool master, uint32_t now_ms) {
    noah_profile_validator_v1_result_t validator_result;

    if (!owner) {
        return false;
    }
    owner->now_ms = now_ms;
    owner->master = master;
#ifdef NOAH_PD_PROFILE_ENABLE
    if (owner->runtimes_installed && !noah_effective_pd_ready()) {
        owner->state = NOAH_PROFILE_OWNER_INTEGRATION_ERROR;
        return false;
    }
#endif
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

// Candidate frames that would start or feed a candidate while an edit or a
// save holds the working copy are answered BUSY; ABORT and reads are not.
// A new Apply (a BEGIN naming a transaction other than the one being run) is
// refused while a keyboard adjustment awaits publication: published later, it
// would overwrite what the Apply sets. An Apply already admitted carries on,
// and adjustments that arrive during it publish after it lands.
static bool apply_begin_behind_adjustment(const noah_profile_owner_t *owner, const uint8_t *frame, size_t length) {
    return owner->auto_save.pending != 0u && frame && length == NOAH_PROFILE_WIRE_V1_REPORT_SIZE && frame[0] == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET && frame[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL && frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN && (uint16_t)(frame[3] | frame[4] << 8u) != owner->host_transaction.status.transaction_id;
}

static bool candidate_held_by_working_copy(const noah_profile_owner_t *owner, const uint8_t *frame, size_t length) {
    return owner->working_ready && frame && length == NOAH_PROFILE_WIRE_V1_REPORT_SIZE && frame[0] == NOAH_PROFILE_CANDIDATE_V1_COMMAND_SET && frame[1] == NOAH_PROFILE_WIRE_V1_CUSTOM_CHANNEL && (frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_BEGIN || frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_CHUNK || frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_VALIDATE || frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_COMMIT || frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_REUSE || frame[2] == NOAH_PROFILE_CANDIDATE_V1_VALUE_STREAM_CHUNK) && (owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE || noah_profile_working_copy_busy(&owner->working));
}

bool noah_profile_owner_receive(noah_profile_owner_t *owner, uint8_t *frame, size_t length) {
    if (owner && (owner->state == NOAH_PROFILE_OWNER_READY_COMPILED || owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED) && (candidate_held_by_working_copy(owner, frame, length) || (owner->working_ready && apply_begin_behind_adjustment(owner, frame, length)))) {
        noah_profile_candidate_v1_encode_ack(frame, NOAH_PROFILE_CANDIDATE_V1_ADMISSION_BUSY, NOAH_PROFILE_CANDIDATE_V1_ERROR_MAILBOX_BUSY, NOAH_PROFILE_CANDIDATE_V1_LOCATION_NONE_U8);
        return true;
    }
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
    noah_effective_profile_status_t       provider;
    noah_profile_split_authority_status_t  authority;
    noah_profile_split_reconciler_status_t reconciler;
    const noah_profile_store_record_t     *committed;

    if (!owner || !status || owner->state == NOAH_PROFILE_OWNER_UNINITIALIZED) {
        return false;
    }
    memset(status, 0, sizeof(*status));
    status->candidate.error         = noah_profile_candidate_v1_no_error();
    status->owner_state             = owner->state;
    status->compiled_default_digest = owner->compiled.metadata.digest;
    status->action_abi_digest       = owner->compiled.metadata.action_abi_digest;
    status->supported_domain_mask   = owner->compatibility.allowed_domain_mask;

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
        status->candidate_pending             = owner->host_transaction.has_candidate || owner->host_transaction.mailbox.pending;
        status->last_committed_transaction_id = owner->host_transaction.last_committed_transaction_id;
    }
    committed = noah_profile_owner_committed(owner);
    if (committed) {
        status->committed     = owner->committed_descriptor;
        status->has_committed = true;
    }
    status->stored_earlier_format = !committed && owner->store.earlier_format;
    if (owner->split_initialized && noah_profile_split_authority_status(&owner->reconciler.authority, &authority)) {
        status->authority_state  = authority.state;
        status->peer             = authority.peer;
        status->peer_known       = authority.peer.readable;
        status->transfer_pending = authority.transfer_pending;
        status->peer_converged   = !authority.transfer_pending && (authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMPILED_CONVERGED || authority.state == NOAH_PROFILE_SPLIT_AUTHORITY_COMMITTED_CONVERGED);
    }
    if (owner->split_initialized && noah_profile_split_reconciler_status(&owner->reconciler, &reconciler)) {
        status->peer_cleanup_pending = reconciler.peer_cleanup_pending;
    }
    return true;
}

bool noah_profile_owner_usb_restart_safe(noah_profile_owner_t *owner) {
    noah_profile_owner_status_t status;

    if (!owner || (owner->state != NOAH_PROFILE_OWNER_READY_COMPILED && owner->state != NOAH_PROFILE_OWNER_READY_VALIDATED)) {
        return false;
    }
    if (owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || owner->host_transaction.has_candidate || owner->host_transaction.mailbox.pending || host_session_cleanup_needed(owner)) {
        return false;
    }
    if (!noah_profile_owner_status(owner, &status) || status.has_pending || status.transfer_pending || status.peer_cleanup_pending || (owner->config.peer_required && !status.peer_converged)) {
        return false;
    }
    // A USB restart keeps the working copy, but not an edit or save mid-way.
    if (owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE || noah_profile_working_copy_busy(&owner->working)) {
        return false;
    }
    return noah_profile_activation_policy_safe_boundary(&owner->activation_policy) == 0u;
}

static noah_profile_candidate_v1_peer_phase_t peer_phase(const noah_profile_split_reconciler_status_t *status) {
    if (status->state == NOAH_PROFILE_SPLIT_RECONCILER_STOPPED) {
        return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_STOPPED;
    }
    if (!status->prepared_push_active) {
        return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_IDLE;
    }
    switch (status->state) {
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_BEGIN:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_BEGINNING;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_READ:
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_SEND:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_SENDING;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_DURABLE:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_PREPARING;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_PREPARED:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_PREPARED;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_COMMIT:
        case NOAH_PROFILE_SPLIT_RECONCILER_VERIFY:
        case NOAH_PROFILE_SPLIT_RECONCILER_CONVERGED:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_COMMITTING;
        case NOAH_PROFILE_SPLIT_RECONCILER_PUSH_ABORT:
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_ABORTING;
        default:
            // A lost exchange drops the reconciler back to DISCOVER while the
            // push stays active; it resumes where it left off.
            return NOAH_PROFILE_CANDIDATE_V1_PEER_PHASE_WAITING;
    }
}

bool noah_profile_owner_peer_transfer(const noah_profile_owner_t *owner, noah_profile_candidate_v1_peer_status_t *peer) {
    noah_profile_split_reconciler_status_t status;

    if (!owner || !peer || owner->state == NOAH_PROFILE_OWNER_UNINITIALIZED || !owner->split_initialized || !noah_profile_split_reconciler_status(&owner->reconciler, &status)) {
        return false;
    }
    *peer = (noah_profile_candidate_v1_peer_status_t){
        .phase                   = peer_phase(&status),
        .last_status             = (uint8_t)status.last_status,
        .flags                   = (uint8_t)((status.peer_cleanup_pending ? NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_CLEANUP_PENDING : 0u) | (status.master ? NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_MASTER : 0u) | (owner->host_barrier_waiting_boundary && (owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_PREPARING_PEER || owner->host_transaction.status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_CONVERGING_PEER) ? NOAH_PROFILE_CANDIDATE_V1_PEER_FLAG_WAITING_SAFE_BOUNDARY : 0u)),
        .transfer_offset         = status.transfer_offset,
        .transfer_length         = status.transfer_length,
        .retry_count             = status.retry_count,
        .transport_failure_count = status.transport_failure_count,
        .busy_streak             = status.busy_streak,
        // Copying is internal split progress. Keep the host status vocabulary
        // compatible with clients that know only reasons 0..7.
        .busy_reason      = status.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_COPYING || status.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_STORAGE_PAUSED ? NOAH_PROFILE_SPLIT_V1_BUSY_STORE_WORKING : status.last_busy_reason,
        .busy_store_state = status.last_busy_store_state,
        .busy_owner       = status.last_busy_owner,
        .busy_admission   = status.last_busy_admission,
        // Diagnostic counters stay out of the scan path's normal status
        // workspace. This owner query runs in the same main context.
        .reused_bytes = status.reused_bytes,
        .reuse_exchanges = owner->reconciler.reuse_exchanges,
        .source_unavailable_replies = owner->reconciler.source_unavailable_replies,
        .prepared_scans = owner->reconciler.prepared_scans,
        .prepared_exchanges = owner->reconciler.prepared_exchanges,
        .reuse_unavailable = owner->reconciler.reuse_unavailable,
    };
    return true;
}

// ── Live edits (D-F32) ──────────────────────────────────────────────────────

static bool owner_ready(const noah_profile_owner_t *owner) {
    return owner && owner->working_ready && (owner->state == NOAH_PROFILE_OWNER_READY_COMPILED || owner->state == NOAH_PROFILE_OWNER_READY_VALIDATED);
}

// Something else holds the profile: an Apply's candidate (or its cleanup),
// a save, or a publication waiting for its boundary.
static bool profile_held(const noah_profile_owner_t *owner) {
    noah_effective_profile_status_t provider;

#ifdef NOAH_PROFILE_OWNER_TEST_DIAGNOSTICS
    noah_profile_owner_profile_held_count++;
#endif
    // A publication the working copy has not yet looked at (it does at the
    // next scan) may have replaced it.
    return !owner_ready(owner) || owner->host_transaction.status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || owner->host_transaction.has_candidate || owner->host_transaction.mailbox.pending || owner->host_cancel_pending || noah_profile_candidate_store_backend_admission_owner(&owner->staging.candidate_backend) != NOAH_PROFILE_STORAGE_ADMISSION_NONE || owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE || noah_effective_profile_provider_status(&owner->provider, &provider) != NOAH_EFFECTIVE_PROFILE_OK || provider.has_pending || provider.publication_count != owner->working.publication_count;
}

// The other half does not yet hold the same saved profile and working copy.
static bool halves_apart(const noah_profile_owner_t *owner) {
    uint8_t unresolved = 1u;

    return owner->config.peer_required && (!noah_profile_working_sync_converged(&owner->working_sync, owner->now_ms) || !owner_peer_observer((void *)owner, &unresolved) || unresolved != 0u);
}

// Each frame the edit accepts restarts its lease, on the owner's clock.
static noah_profile_working_result_t note_edit_activity(noah_profile_owner_t *owner, noah_profile_working_result_t result) {
    if (result == NOAH_PROFILE_WORKING_OK) {
        owner->edit_activity_at = owner->now_ms;
    }
    return result;
}

noah_profile_working_result_t noah_profile_owner_edit_begin(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags) {
    if (!owner_ready(owner)) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    // A repeated BEGIN of the edit already admitted is answered as before.
    if ((profile_held(owner) || halves_apart(owner) || owner->auto_save.pending || owner->auto_save.edit_transaction) && !(!owner->auto_save.edit_transaction && owner->working.phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING && owner->working.transaction_id == transaction_id)) {
        return session != owner->working.session || revision != owner->working.revision ? NOAH_PROFILE_WORKING_STALE : NOAH_PROFILE_WORKING_BUSY;
    }
    return note_edit_activity(owner, noah_profile_working_copy_begin(&owner->working, transaction_id, session, revision, splice_count, insert_total, flags, false));
}

noah_profile_working_result_t noah_profile_owner_edit_splice(noah_profile_owner_t *owner, uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice) {
    if (owner && owner->auto_save.edit_transaction) return NOAH_PROFILE_WORKING_BUSY;
    return owner_ready(owner) ? note_edit_activity(owner, noah_profile_working_copy_splice(&owner->working, transaction_id, index, splice)) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_owner_edit_data(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    if (owner && owner->auto_save.edit_transaction) return NOAH_PROFILE_WORKING_BUSY;
    return owner_ready(owner) ? note_edit_activity(owner, noah_profile_working_copy_data(&owner->working, transaction_id, offset, bytes, count)) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_owner_edit_commit(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t crc32) {
    if (owner && owner->auto_save.edit_transaction) return NOAH_PROFILE_WORKING_BUSY;
    noah_profile_working_result_t result = owner_ready(owner) ? noah_profile_working_copy_commit(&owner->working, transaction_id, crc32) : NOAH_PROFILE_WORKING_INVALID_STATE;
    return result;
}

noah_profile_working_result_t noah_profile_owner_edit_abort(noah_profile_owner_t *owner, uint16_t transaction_id) {
    if (owner && owner->auto_save.edit_transaction) return NOAH_PROFILE_WORKING_BUSY;
    return owner_ready(owner) ? noah_profile_working_copy_abort(&owner->working, transaction_id) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_owner_revert(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision) {
    if (!owner_ready(owner)) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    // Revert never waits for the other half: it follows (profile_working_sync.c).
    if (profile_held(owner) || owner->auto_save.pending || owner->auto_save.edit_transaction) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    // An edit still being sent is abandoned: Revert is the way out of one.
    if (owner->working.phase == NOAH_PROFILE_WORKING_PHASE_RECEIVING) {
        (void)noah_profile_working_copy_abort(&owner->working, owner->working.transaction_id);
    }
    return noah_profile_working_copy_revert(&owner->working, transaction_id, session, revision);
}

static void finish_save(noah_profile_owner_t *owner, noah_profile_working_result_t result, noah_profile_candidate_v1_error_id_t error) {
    noah_profile_candidate_store_backend_hold_candidates_to(candidate_backend(owner), &owner->candidate_runtime);
    owner->save.last_automatic = owner->save.automatic;
    if (owner->save.automatic && result == NOAH_PROFILE_WORKING_OK) {
        owner->save.keyboard_saves++;
        owner->save.keyboard_revision = owner->working.revision;
        owner->save.keyboard_generation = owner->store.committed.generation;
    }
    owner->save.automatic = false;
    owner->save.last_transaction_id = owner->save.transaction_id;
    owner->save.last_result         = (uint8_t)result;
    owner->save.last_error          = (uint8_t)error;
    owner->save.phase               = NOAH_PROFILE_OWNER_SAVE_IDLE;
}

noah_profile_working_result_t noah_profile_owner_save(noah_profile_owner_t *owner, uint16_t transaction_id, uint32_t session, uint32_t revision) {
    noah_effective_profile_snapshot_t active;

    if (!owner_ready(owner) || transaction_id == 0u) {
        return owner_ready(owner) ? NOAH_PROFILE_WORKING_INVALID_ARGUMENT : NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    if (owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE) {
        return !owner->save.automatic && owner->save.transaction_id == transaction_id ? NOAH_PROFILE_WORKING_QUEUED : NOAH_PROFILE_WORKING_BUSY;
    }
    if (session != owner->working.session || revision != owner->working.revision) {
        return NOAH_PROFILE_WORKING_STALE;
    }
    if (profile_held(owner) || halves_apart(owner) || noah_profile_working_copy_busy(&owner->working) || owner->auto_save.pending || owner->auto_save.edit_transaction) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    owner->save.automatic = false;
    owner->save.transaction_id = transaction_id;
    if (!owner->working.active) {
        // Nothing unsaved: no new generation.
        finish_save(owner, NOAH_PROFILE_WORKING_OK, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
        return NOAH_PROFILE_WORKING_OK;
    }
    if (noah_effective_profile_provider_copy_active(&owner->provider, &active) != NOAH_EFFECTIVE_PROFILE_OK || active.identity.kind != NOAH_EFFECTIVE_PROFILE_KIND_WORKING_COPY || active.base_offset != 0u) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    owner->save.phase       = NOAH_PROFILE_OWNER_SAVE_CHECKSUM;
    owner->save.offset      = 0u;
    owner->save.length      = (uint32_t)active.profile.byte_length;
    owner->save.domain_mask = active.profile.domain_mask;
    owner->save.crc32       = NOAH_PROFILE_CRC32_INITIAL;
    owner->save.digest      = NOAH_PROFILE_FNV1A_INITIAL;
    // Its unedited domains came from a saved profile, which is held to the
    // rules of a saved record, not of a host candidate; edits already were.
    noah_profile_candidate_store_backend_hold_candidates_to(candidate_backend(owner), NULL);
    return NOAH_PROFILE_WORKING_QUEUED;
}

static bool fail_save(noah_profile_owner_t *owner) {
    noah_profile_candidate_v1_error_id_t error = owner->host_transaction.status.error.code;

    // A candidate that never reached its commit is let go, as an abandoned
    // Apply would be.
    if (owner->host_transaction.has_candidate && host_state_is_precommit(owner->host_transaction.status.state) && !owner->host_transaction.mailbox.pending) {
        noah_profile_candidate_v1_command_t abort = {.operation = NOAH_PROFILE_CANDIDATE_V1_OPERATION_ABORT, .transaction_id = owner->save.transaction_id};

        (void)noah_profile_candidate_transaction_submit(&owner->host_transaction, &abort);
    }
    finish_save(owner, error == NOAH_PROFILE_CANDIDATE_V1_ERROR_VALIDATION_REJECTED ? NOAH_PROFILE_WORKING_VALIDATION_REJECTED : NOAH_PROFILE_WORKING_PUBLICATION_FAILED, error == NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE ? NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE : error);
    return true;
}

// One step of Save. Commands go to the candidate transaction's mailbox as a
// host's would; its own scan (the HOST slot) processes them.
static __attribute__((noinline)) bool save_step(noah_profile_owner_t *owner) {
    noah_profile_owner_save_t            *save        = &owner->save;
    noah_profile_candidate_transaction_t *transaction = &owner->host_transaction;
    noah_profile_candidate_v1_command_t   command;

    memset(&command, 0, sizeof(command));
    command.transaction_id = save->transaction_id;
    if (save->phase != NOAH_PROFILE_OWNER_SAVE_CHECKSUM && transaction->mailbox.pending) {
        return false;
    }
    if (save->phase >= NOAH_PROFILE_OWNER_SAVE_REUSE && save->phase <= NOAH_PROFILE_OWNER_SAVE_WAIT && (transaction->status.transaction_id != save->transaction_id || transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_REJECTED || transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_AUTHORITY_FAILED)) {
        return fail_save(owner);
    }
    switch (save->phase) {
        case NOAH_PROFILE_OWNER_SAVE_CHECKSUM: {
            uint32_t count = save->length - save->offset;

            if (!owner->working.active) {
                finish_save(owner, NOAH_PROFILE_WORKING_ABORTED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
                return true;
            }
            if (count > OWNER_SAVE_CHECKSUM_BYTES) {
                count = OWNER_SAVE_CHECKSUM_BYTES;
            }
            save->crc32  = noah_profile_crc32_update(save->crc32, owner->working.bytes + save->offset, count);
            save->digest = noah_profile_fnv1a_update(save->digest, owner->working.bytes + save->offset, count);
            save->offset += count;
            if (save->offset == save->length) {
                const noah_profile_validator_v1_profile_t *saved = &owner->working.saved.profile;

                save->crc32  = noah_profile_crc32_finish(save->crc32);
                save->offset = 0u;
                // Edits undone, or a value set back: possibly the saved
                // profile's own bytes, which a save would write again.
                save->phase = save->length == saved->byte_length && save->crc32 == saved->crc32 && save->digest == saved->digest && save->domain_mask == saved->domain_mask ? NOAH_PROFILE_OWNER_SAVE_SAME : NOAH_PROFILE_OWNER_SAVE_BEGIN;
            }
            return true;
        }
        case NOAH_PROFILE_OWNER_SAVE_SAME: {
            uint8_t  saved[NOAH_PROFILE_WORKING_LOAD_PIECE];
            uint32_t end = save->length - save->offset > OWNER_SAVE_CHECKSUM_BYTES ? save->offset + OWNER_SAVE_CHECKSUM_BYTES : save->length;

            if (!owner->working.active) {
                finish_save(owner, NOAH_PROFILE_WORKING_ABORTED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
                return true;
            }
            // Byte for byte, in pieces the saved profile's reader takes.
            while (save->offset < end) {
                uint32_t piece = end - save->offset < sizeof(saved) ? end - save->offset : sizeof(saved);

                if (!noah_effective_profile_snapshot_read(&owner->working.saved, save->offset, saved, piece) || memcmp(saved, owner->working.bytes + save->offset, piece) != 0) {
                    save->offset = 0u;
                    save->phase  = NOAH_PROFILE_OWNER_SAVE_BEGIN;
                    return true;
                }
                save->offset += piece;
            }
            if (save->offset < save->length) {
                return true;
            }
            if (noah_profile_working_copy_settle_saved(&owner->working) != NOAH_PROFILE_WORKING_QUEUED) {
                finish_save(owner, NOAH_PROFILE_WORKING_PUBLICATION_FAILED, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
                return true;
            }
            save->phase = NOAH_PROFILE_OWNER_SAVE_SETTLE;
            return true;
        }
        case NOAH_PROFILE_OWNER_SAVE_SETTLE:
            if (noah_profile_working_copy_busy(&owner->working)) {
                return false;
            }
            // Saved: the keyboard runs its saved profile, no generation was
            // written, and the revision did not move.
            finish_save(owner, owner->working.active ? NOAH_PROFILE_WORKING_PUBLICATION_FAILED : NOAH_PROFILE_WORKING_OK, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
            return true;
        case NOAH_PROFILE_OWNER_SAVE_BEGIN:
            command.operation     = NOAH_PROFILE_CANDIDATE_V1_OPERATION_BEGIN;
            command.payload.begin = (noah_profile_candidate_v1_metadata_t){
                .schema_major         = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                .schema_minor         = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                .requested_domains    = save->domain_mask,
                .payload_length       = save->length,
                .crc32                = save->crc32,
                .digest               = save->digest,
                .action_abi_digest    = owner->compiled.metadata.action_abi_digest,
                .store_format_version = NOAH_PROFILE_STORE_FORMAT_VERSION,
            };
            if (!noah_profile_candidate_transaction_submit(transaction, &command)) {
                return false;
            }
            save->phase = NOAH_PROFILE_OWNER_SAVE_REUSE;
            return true;
        case NOAH_PROFILE_OWNER_SAVE_REUSE:
            if (!transaction->has_candidate) {
                return fail_save(owner);
            }
            if (save->offset < save->length) {
                const noah_effective_profile_identity_t *identity = &owner->working.saved.identity;
                uint32_t                                  count    = save->length - save->offset;

                if (count > OWNER_SAVE_REUSE_BYTES) {
                    count = OWNER_SAVE_REUSE_BYTES;
                }
                command.operation             = NOAH_PROFILE_CANDIDATE_V1_OPERATION_REUSE;
                command.payload.reuse.source  = (noah_profile_candidate_v1_source_t){
                     .generation = identity->generation,
                     .digest     = owner->working.revision,
                     .crc32      = owner->working.session,
                     .kind       = NOAH_EFFECTIVE_PROFILE_KIND_WORKING_COPY - 1u,
                     .origin     = identity->origin,
                };
                command.payload.reuse.offset        = save->offset;
                command.payload.reuse.source_offset = save->offset;
                command.payload.reuse.length        = (uint16_t)count;
                if (!noah_profile_candidate_transaction_submit(transaction, &command)) {
                    return false;
                }
                save->offset += count;
                return true;
            }
            if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_COMPLETE) {
                return transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_RECEIVING ? fail_save(owner) : false;
            }
            command.operation = NOAH_PROFILE_CANDIDATE_V1_OPERATION_VALIDATE;
            if (!noah_profile_candidate_transaction_submit(transaction, &command)) {
                return false;
            }
            save->phase = NOAH_PROFILE_OWNER_SAVE_VALIDATE;
            return true;
        case NOAH_PROFILE_OWNER_SAVE_VALIDATE:
            if (transaction->status.state == NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATING) {
                return false;
            }
            if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_VALIDATED) {
                return fail_save(owner);
            }
            command.operation = NOAH_PROFILE_CANDIDATE_V1_OPERATION_COMMIT;
            if (!noah_profile_candidate_transaction_submit(transaction, &command)) {
                return false;
            }
            save->phase = NOAH_PROFILE_OWNER_SAVE_WAIT;
            return true;
        case NOAH_PROFILE_OWNER_SAVE_WAIT:
            if (transaction->status.state != NOAH_PROFILE_CANDIDATE_V1_STATE_IDLE || transaction->has_candidate) {
                return false;
            }
            if (transaction->last_committed_transaction_id != save->transaction_id) {
                return fail_save(owner);
            }
            finish_save(owner, NOAH_PROFILE_WORKING_OK, NOAH_PROFILE_CANDIDATE_V1_ERROR_NONE);
            return true;
        default:
            finish_save(owner, NOAH_PROFILE_WORKING_INVALID_STATE, NOAH_PROFILE_CANDIDATE_V1_ERROR_INVALID_STATE);
            return true;
    }
}

noah_profile_working_result_t noah_profile_owner_set_edit_mode(noah_profile_owner_t *owner, uint8_t mode) {
    if (!owner_ready(owner)) {
        return NOAH_PROFILE_WORKING_INVALID_STATE;
    }
    if (!noah_profile_edit_mode_valid(mode)) {
        return NOAH_PROFILE_WORKING_INVALID_ARGUMENT;
    }
    // Leaving a mode with unsaved edits is the host's question to ask first
    // (Save or Revert); the keyboard does not decide it.
    if (owner->working.active || owner->auto_save.pending || owner->auto_save.edit_transaction) {
        return NOAH_PROFILE_WORKING_NOT_CLEAN;
    }
    if (owner->save.phase != NOAH_PROFILE_OWNER_SAVE_IDLE || noah_profile_working_copy_busy(&owner->working)) {
        return NOAH_PROFILE_WORKING_BUSY;
    }
    return noah_profile_working_sync_set_mode(&owner->working_sync, mode) ? NOAH_PROFILE_WORKING_OK : NOAH_PROFILE_WORKING_READ_ERROR;
}

bool noah_profile_owner_working_status(const noah_profile_owner_t *owner, noah_profile_owner_working_status_t *status) {
    noah_effective_profile_status_t provider;
    const noah_effective_profile_identity_t *saved;

    if (!owner || !status) {
        return false;
    }
    memset(status, 0, sizeof(*status));
    if (!owner->working_ready || !noah_profile_working_copy_status(&owner->working, &status->local) || noah_effective_profile_provider_status(&owner->provider, &provider) != NOAH_EFFECTIVE_PROFILE_OK) {
        return false;
    }
    saved                             = owner->working.active ? &owner->working.saved.identity : &provider.active;
    status->supported                 = true;
    status->peer                      = owner->working_sync.peer;
    status->mode                      = owner->working_sync.mode;
    status->save                      = owner->save;
    status->saved_generation          = saved->generation;
    status->saved_origin              = saved->origin;
    status->saved_compiled            = saved->generation == 0u;
    status->safe_boundary_reason_mask = provider.safe_boundary_reason_mask;
    status->peer_required             = owner->config.peer_required;
    status->peer_converged            = !halves_apart(owner);
    status->mode_conflict             = owner->working_sync.mode_conflict;
    status->adjustment_pending        = owner->auto_save.pending != 0u;
    status->edits_blocked             = profile_held(owner) || halves_apart(owner) || noah_profile_working_copy_busy(&owner->working) || owner->auto_save.pending || owner->auto_save.edit_transaction;
    status->storage_paused            = owner->config.storage_paused && owner->config.storage_paused(owner->config.storage_paused_context);
    status->peer_storage_paused       = owner->reconciler.busy_streak != 0u && owner->reconciler.last_busy_reason == NOAH_PROFILE_SPLIT_V1_BUSY_STORAGE_PAUSED;
    status->peer_refused              = noah_profile_working_sync_refused(&owner->working_sync);
    status->mode_store_failed         = owner->working_sync.mode_store_failed;
    return true;
}

bool noah_profile_owner_working_read(const noah_profile_owner_t *owner, uint32_t offset, uint8_t *bytes, uint32_t length) {
    return owner && owner->working_ready && noah_profile_working_copy_read(&owner->working, offset, bytes, length);
}

bool noah_profile_owner_working_receive(noah_profile_owner_t *owner, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return owner && owner->working_ready && noah_profile_working_sync_receive(&owner->working_sync, request, request_length, response, response_length);
}

#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(noah_profile_owner_t) <= NOAH_PROFILE_OWNER_STATE_BUDGET_32BIT, "live-profile owner state exceeded its 32-bit engineering regression policy");
#endif

// Settings are ordered by their scalar offsets, for a canonical compound edit.
static const uint8_t adjustment_settings[] = {
    NOAH_SETTING_DEFAULT_DPI, NOAH_SETTING_SNIPING_DPI,
    NOAH_SETTING_RGB_MODE, NOAH_SETTING_RGB_COLOR,
};

bool noah_profile_owner_adjust(noah_profile_owner_t *owner, uint8_t setting, uint32_t value) {
    if (!owner_ready(owner) || !owner->master || !noah_profile_setting_v1_valid(setting, value, NOAH_SETTINGS_LAYERS)) {
        return false;
    }
    for (uint8_t i = 0u; i < sizeof(adjustment_settings); i++) {
        if (adjustment_settings[i] == setting) {
            owner->auto_save.values[i] = value;
            owner->auto_save.pending |= 1u << i;
            owner->auto_save.quiet_since = owner->now_ms;
            owner->auto_save.attempted = false;
            return true;
        }
    }
    return false; // toggles are never edits
}

bool noah_profile_owner_pending_adjustment(const noah_profile_owner_t *owner, uint8_t setting, uint32_t *value) {
    if (!owner || !value) return false;
    for (uint8_t i = 0u; i < sizeof(adjustment_settings); i++) {
        if (adjustment_settings[i] == setting && (owner->auto_save.pending & (1u << i))) {
            *value = owner->auto_save.values[i];
            return true;
        }
    }
    return false;
}

static uint16_t automatic_transaction(noah_profile_owner_t *owner) {
    // Same candidate namespace as a host; avoid the previous finished IDs so
    // retries cannot mistake a keyboard operation for their own last result.
    do {
        owner->auto_save.sequence++;
    } while (!owner->auto_save.sequence || owner->auto_save.sequence == owner->working.last_transaction_id || owner->auto_save.sequence == owner->save.last_transaction_id || owner->auto_save.sequence == owner->host_transaction.status.transaction_id);
    return owner->auto_save.sequence;
}

// The scalar bytes of a settings domain this owner would write: the authored
// defaults, with the adjusted scalars replaced.
static uint8_t inherited_settings_byte(const noah_profile_owner_t *owner, uint8_t mask, uint16_t offset) {
    if (offset >= NOAH_SETTINGS_SCALARS_OFFSET && offset < NOAH_SETTINGS_LAYER_RECORDS_OFFSET) {
        uint16_t scalar = (uint16_t)((offset - NOAH_SETTINGS_SCALARS_OFFSET) / 4u);
        for (uint8_t i = 0u; i < sizeof(adjustment_settings); i++) {
            if ((mask & (1u << i)) && adjustment_settings[i] == scalar) {
                return (uint8_t)(owner->auto_save.values[i] >> (8u * ((offset - NOAH_SETTINGS_SCALARS_OFFSET) % 4u)));
            }
        }
    }
    return owner->config.settings_defaults_byte(offset);
}

// A profile without a settings domain inherits the authored defaults, which
// leaves nowhere to replace four bytes in. Create the domain from those
// defaults with the adjusted scalars in it; every other value is unchanged.
static __attribute__((noinline)) noah_profile_working_result_t adjustment_inherit(noah_profile_owner_t *owner, uint16_t transaction, uint8_t mask, uint16_t length) {
    noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .insert = length, .flags = NOAH_PROFILE_WORKING_SPLICE_CREATE};
    uint8_t                       bytes[32];
    uint32_t                      crc = NOAH_PROFILE_CRC32_INITIAL;
    noah_profile_working_result_t result = noah_profile_working_copy_splice(&owner->working, transaction, 0u, &splice);

    // First pass: the stream's checksum. Second: the stream itself.
    for (uint8_t pass = 0u; pass < 2u && result == NOAH_PROFILE_WORKING_OK; pass++) {
        for (uint16_t offset = 0u; offset < length && result == NOAH_PROFILE_WORKING_OK;) {
            uint8_t count = length - offset < sizeof(bytes) ? (uint8_t)(length - offset) : (uint8_t)sizeof(bytes);
            for (uint8_t i = 0u; i < count; i++) bytes[i] = inherited_settings_byte(owner, mask, (uint16_t)(offset + i));
            if (pass == 0u) crc = noah_profile_crc32_update(crc, bytes, count);
            else result = noah_profile_working_copy_data(&owner->working, transaction, offset, bytes, count);
            offset = (uint16_t)(offset + count);
        }
    }
    return result == NOAH_PROFILE_WORKING_OK ? noah_profile_working_copy_commit(&owner->working, transaction, noah_profile_crc32_finish(crc)) : result;
}

static __attribute__((noinline)) bool adjustment_step(noah_profile_owner_t *owner) {
    noah_profile_owner_auto_save_t *state = &owner->auto_save;
    uint8_t bytes[16];
    uint8_t count = 0u;
    uint8_t mask = state->pending;
    uint16_t inherited = 0u;
    noah_profile_working_status_t working;
    noah_profile_working_result_t result;

    for (uint8_t i = 0u; i < sizeof(adjustment_settings); i++) {
        if (mask & (1u << i)) count++;
    }
    // The settings domain is optional; a profile without one inherits the
    // compiled defaults and is given its own before it is adjusted.
    if (noah_profile_working_copy_status(&owner->working, &working) && !(working.domain_mask & NOAH_PROFILE_DOMAIN_MASK_SETTINGS) && owner->config.settings_defaults_length && owner->config.settings_defaults_byte) {
        inherited = owner->config.settings_defaults_length();
    }
    uint16_t transaction = automatic_transaction(owner);
    result = noah_profile_working_copy_begin(&owner->working, transaction, owner->working.session, owner->working.revision, inherited ? 1u : count, inherited ? inherited : count * 4u, 0u, false);
    if (result != NOAH_PROFILE_WORKING_OK) return false;
    if (inherited) {
        result = adjustment_inherit(owner, transaction, mask, inherited);
    } else {
        count = 0u;
        for (uint8_t i = 0u; i < sizeof(adjustment_settings); i++) {
            if (!(mask & (1u << i))) continue;
            noah_profile_working_splice_t splice = {.domain_id = NOAH_PROFILE_DOMAIN_V1_SETTINGS, .offset = NOAH_SETTINGS_SCALARS_OFFSET + adjustment_settings[i] * 4u, .remove = 4u, .insert = 4u};
            result = noah_profile_working_copy_splice(&owner->working, transaction, count, &splice);
            if (result != NOAH_PROFILE_WORKING_OK) break;
            for (uint8_t j = 0u; j < 4u; j++) bytes[count * 4u + j] = (uint8_t)(state->values[i] >> (8u * j));
            count++;
        }
        if (result == NOAH_PROFILE_WORKING_OK) result = noah_profile_working_copy_data(&owner->working, transaction, 0u, bytes, count * 4u);
        if (result == NOAH_PROFILE_WORKING_OK) result = noah_profile_working_copy_commit(&owner->working, transaction, noah_profile_crc32_finish(noah_profile_crc32_update(NOAH_PROFILE_CRC32_INITIAL, bytes, count * 4u)));
    }
    if (result == NOAH_PROFILE_WORKING_QUEUED) {
        if (!owner->working.active) state->apply_adjustments_only = true;
        state->edit_transaction = transaction;
        state->pending &= (uint8_t)~mask;
    } else {
        (void)noah_profile_working_copy_abort(&owner->working, transaction);
        // Refused (the keyboard cannot store them here): expose the ordinary
        // edit result, consume this batch, and do not retry each scan.
        state->pending &= (uint8_t)~mask;
    }
    return true;
}

static __attribute__((noinline)) bool automatic_step(noah_profile_owner_t *owner) {
    noah_profile_owner_auto_save_t *state = &owner->auto_save;
    uint8_t mode = owner->working_sync.mode.mode;

    if (state->edit_transaction && !noah_profile_working_copy_busy(&owner->working)) state->edit_transaction = 0u;
    // Admission takes provider and peer-authority snapshots. Nothing needs
    // them until an adjustment is pending or an automatic save is due.
    // Adjustments publish in every edit mode, including onto a saved profile.
    if (!state->pending) {
        if (!owner->working.active || state->attempted || owner->working_sync.mode_conflict || (mode != NOAH_PROFILE_EDIT_MODE_LIVE && !(mode == NOAH_PROFILE_EDIT_MODE_APPLY && state->apply_adjustments_only))) return false;
        if (!elapsed_at_least(owner->now_ms, state->quiet_since, NOAH_PROFILE_OWNER_AUTO_SAVE_QUIET_MS)) return false;
    }
    if (profile_held(owner) || halves_apart(owner) || noah_profile_working_copy_busy(&owner->working)) return false;
    if (state->pending) return adjustment_step(owner);
    if (!owner->config.keyboard_quiet || !owner->config.keyboard_quiet(owner->config.keyboard_quiet_context)) return false;
    noah_profile_working_result_t result = noah_profile_owner_save(owner, automatic_transaction(owner), owner->working.session, owner->working.revision);
    if (result == NOAH_PROFILE_WORKING_QUEUED) {
        state->attempted = true; // one attempt per edited revision, no wear loop
        owner->save.automatic = true;
        return true;
    }
    return false;
}

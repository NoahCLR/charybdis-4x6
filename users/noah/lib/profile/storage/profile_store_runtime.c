// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ─────────────────────────────────────────────────────────────────────────

#include "profile_store_runtime.h"

#include <stddef.h>

#include "../../compat/qmk_profile_eeprom.h"
#include "../schema/profile_compiled_defaults_v1.h"

#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#    include "../../compat/qmk_physical_half.h"
#    include "../../compat/qmk_profile_split_transport.h"
#    include "../../compat/qmk_via_logical_profile.h"
#    include "../../compat/qmk_via_split_sync.h"
#    include "../runtime/profile_owner.h"
#endif

static noah_profile_store_runtime_state_t runtime_state;
static noah_profile_store_result_t        discovery_result;

#if defined(VIA_ENABLE) && !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static noah_profile_store_t runtime_store;
#endif

#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
static noah_profile_owner_t runtime_owner;
static bool                 runtime_owner_initialized;
static bool                 runtime_transport_registered;
static bool                 runtime_integration_error;

static bool runtime_logical_via_ready(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_staged(transaction_id, generation, digest);
}

static bool runtime_logical_via_accept(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_profile_accept(transaction_id, generation, digest);
}

static bool runtime_logical_via_abort(void *context, uint16_t transaction_id, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_profile_abort(transaction_id, generation, digest);
}

static bool runtime_logical_via_converged(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_converged(generation, digest);
}

static bool runtime_logical_via_boot_recover(void *context, uint32_t generation, uint32_t digest) {
    (void)context;
    return noah_qmk_via_logical_boot_recover(generation, digest);
}

static void runtime_logical_via_boot_release(void *context) {
    (void)context;
    noah_qmk_via_logical_boot_release();
}

static const noah_profile_logical_via_ops_t runtime_logical_via_ops = {
    .ready        = runtime_logical_via_ready,
    .accept       = runtime_logical_via_accept,
    .abort        = runtime_logical_via_abort,
    .converged    = runtime_logical_via_converged,
    .boot_recover = runtime_logical_via_boot_recover,
    .boot_release = runtime_logical_via_boot_release,
    .context      = NULL,
};
#endif

void noah_profile_store_runtime_init(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    uint8_t origin;

    runtime_owner_initialized    = false;
    runtime_transport_registered = false;
    runtime_integration_error    = false;
    discovery_result             = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
#    if defined(VIA_ENABLE) && defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)
    if (!noah_qmk_physical_half_origin(&origin) || !noah_profile_owner_init(&runtime_owner, &(noah_profile_owner_config_t){
                                                                                                .store_io                = noah_qmk_profile_eeprom_io(),
                                                                                                .split_exchange          = noah_qmk_profile_split_transport_exchange,
                                                                                                .split_transport_context = NULL,
                                                                                                .origin_half             = origin,
                                                                                                .peer_required           = true,
                                                                                                .logical_via             = &runtime_logical_via_ops,
                                                                                            })) {
        runtime_integration_error = true;
        runtime_state             = NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR;
        return;
    }
    runtime_owner_initialized = true;
    runtime_state             = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
#    else
    (void)origin;
    runtime_integration_error = true;
    runtime_state             = NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR;
#    endif
#elif defined(VIA_ENABLE)
    noah_profile_compiled_v1_t compiled;

    if (noah_profile_compiled_v1_open(&compiled, NULL) != NOAH_PROFILE_COMPILED_V1_OK) {
        discovery_result = NOAH_PROFILE_STORE_INVALID_PAYLOAD;
        runtime_state    = NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR;
        return;
    }
    noah_profile_store_init(&runtime_store, noah_qmk_profile_eeprom_read_only_io(),
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = compiled.metadata.digest,
                                .action_abi_digest       = compiled.metadata.action_abi_digest,
                            });
    discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    runtime_state    = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
#else
    discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    runtime_state    = NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK;
#endif
}

bool noah_profile_store_runtime_matrix_scan_step(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    bool worked;

    if (!runtime_owner_initialized || runtime_integration_error) {
        return false;
    }
    worked = noah_profile_owner_scan(&runtime_owner, is_keyboard_master(), timer_read32());
    if (!runtime_transport_registered) {
        noah_profile_split_reconciler_t *reconciler = noah_profile_owner_split_reconciler(&runtime_owner);

        if (reconciler) {
            runtime_transport_registered = noah_qmk_profile_split_transport_init(reconciler);
            if (!runtime_transport_registered) {
                runtime_integration_error = true;
                runtime_state             = NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR;
                return true;
            }
        }
    }
    discovery_result = noah_profile_owner_discovery_result(&runtime_owner);
    switch (noah_profile_owner_state(&runtime_owner)) {
        case NOAH_PROFILE_OWNER_READY_COMPILED:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK;
            break;
        case NOAH_PROFILE_OWNER_READY_VALIDATED:
        case NOAH_PROFILE_OWNER_ACTIVATING_COMMITTED:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND;
            break;
        case NOAH_PROFILE_OWNER_GENERATION_CONFLICT:
        case NOAH_PROFILE_OWNER_CONCURRENT_COMMIT:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT;
            break;
        case NOAH_PROFILE_OWNER_COMPILED_ERROR:
        case NOAH_PROFILE_OWNER_STORAGE_ERROR:
        case NOAH_PROFILE_OWNER_DURABILITY_UNKNOWN:
        case NOAH_PROFILE_OWNER_POSTCOMMIT_AUTHORITY_LOST:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR;
            break;
        case NOAH_PROFILE_OWNER_INTEGRATION_ERROR:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR;
            break;
        default:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
            break;
    }
    return worked;
#elif defined(VIA_ENABLE)
    if (runtime_state != NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING) {
        return false;
    }

    discovery_result = noah_profile_store_boot_select(&runtime_store, &runtime_store.committed);
    switch (discovery_result) {
        case NOAH_PROFILE_STORE_OK:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND;
            break;
        case NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK;
            break;
        case NOAH_PROFILE_STORE_GENERATION_CONFLICT:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_GENERATION_CONFLICT;
            break;
        default:
            runtime_state = NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR;
            break;
    }
    return true;
#else
    return false;
#endif
}

void noah_profile_store_runtime_matrix_scan(void) {
    (void)noah_profile_store_runtime_matrix_scan_step();
}

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void) {
    return runtime_state;
}

noah_profile_store_result_t noah_profile_store_runtime_discovery_result(void) {
    return discovery_result;
}

const noah_profile_store_record_t *noah_profile_store_runtime_committed(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error ? noah_profile_owner_committed(&runtime_owner) : NULL;
#elif defined(VIA_ENABLE)
    return runtime_state == NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND ? &runtime_store.committed : NULL;
#else
    return NULL;
#endif
}

bool noah_profile_store_runtime_read_committed(uint16_t offset, uint8_t *target, uint16_t length) {
#if defined(VIA_ENABLE)
    const noah_profile_store_record_t *record = noah_profile_store_runtime_committed();
    const noah_profile_store_t        *store;
    uint16_t                           payload_start;

#    if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    store = noah_profile_owner_store(&runtime_owner);
#    else
    store = &runtime_store;
#    endif

    if (!record || !store || !target || length == 0u || !store->io.read) {
        return false;
    }
    // Bounds first: offset and length arrive from the host, so a request that
    // runs past the committed payload is refused rather than clamped.
    if ((uint32_t)offset + (uint32_t)length > (uint32_t)record->payload_length) {
        return false;
    }
    if (record->slot == NOAH_PROFILE_SLOT_A) {
        payload_start = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_A_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    } else if (record->slot == NOAH_PROFILE_SLOT_B) {
        payload_start = (uint16_t)(NOAH_PROFILE_STORAGE_SLOT_B_START_ADDR + NOAH_PROFILE_STORAGE_SLOT_HEADER_SIZE);
    } else {
        return false;
    }
    return store->io.read(store->io.context, (uint16_t)(payload_start + offset), target, length);
#else
    (void)offset;
    (void)target;
    (void)length;
    return false;
#endif
}

// Opened lazily on first host request rather than at boot: computing the
// compiled digest walks the authored tables, and nothing on the scan path
// needs it. One open is enough, since the compiled bytes cannot change at
// runtime.
static bool                       compiled_opened;
static bool                       compiled_valid;
static noah_profile_compiled_v1_t compiled_profile;
static noah_profile_reader_t      compiled_reader;

static bool ensure_compiled_open(void) {
    if (!compiled_opened) {
        compiled_opened = true;
        compiled_valid  = noah_profile_compiled_v1_open(&compiled_profile, NULL) == NOAH_PROFILE_COMPILED_V1_OK;
        if (compiled_valid) {
            compiled_reader = noah_profile_compiled_v1_reader(&compiled_profile);
        }
    }
    return compiled_valid;
}

bool noah_profile_store_runtime_compiled_metadata(noah_profile_compiled_v1_metadata_t *metadata) {
    if (!metadata || !ensure_compiled_open()) {
        return false;
    }
    *metadata = compiled_profile.metadata;
    return true;
}

bool noah_profile_store_runtime_read_compiled(uint16_t offset, uint8_t *target, uint16_t length) {
    if (!target || length == 0u || !ensure_compiled_open() || !compiled_reader.read) {
        return false;
    }
    // Bounds first: offset and length arrive from the host.
    if ((uint32_t)offset + (uint32_t)length > (uint32_t)compiled_profile.metadata.byte_length) {
        return false;
    }
    return compiled_reader.read(compiled_reader.context, offset, target, length);
}

bool noah_profile_store_runtime_owner_status(noah_profile_owner_status_t *status) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error && noah_profile_owner_status(&runtime_owner, status);
#else
    (void)status;
    return false;
#endif
}

bool noah_profile_store_runtime_candidate_status(noah_profile_candidate_v1_status_t *status) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    noah_profile_owner_status_t owner_status;

    if (!status || !noah_profile_store_runtime_owner_status(&owner_status)) {
        return false;
    }
    *status = owner_status.candidate;
    return true;
#else
    (void)status;
    return false;
#endif
}

bool noah_profile_store_runtime_candidate_receive(uint8_t *frame, size_t length) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error && noah_profile_owner_receive(&runtime_owner, frame, length);
#else
    (void)frame;
    (void)length;
    return false;
#endif
}

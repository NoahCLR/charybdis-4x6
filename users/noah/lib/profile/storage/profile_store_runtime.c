// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ─────────────────────────────────────────────────────────────────────────

#include "profile_store_runtime.h"

#include <stddef.h>
#include <string.h>

#include "../../compat/qmk_profile_flash.h"
#include "../schema/profile_compiled_defaults_v1.h"
#include "profile_paused_clock.h"

#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#    include "../../compat/qmk_physical_half.h"
#    include "../../compat/qmk_profile_split_transport.h"
#    include "../../compat/qmk_profile_working.h"
#    include "../runtime/profile_owner.h"
#    ifdef NOAH_PORTABLE_PROFILE_ENABLE
#        include "../schema/profile_settings_defaults.h"
#    endif
#endif

static noah_profile_store_runtime_state_t runtime_state;
static noah_profile_store_result_t        discovery_result;
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
// The owner's clock stops while its storage step is not allowed to run.
static noah_profile_paused_clock_t runtime_clock;
static volatile bool               runtime_storage_paused;

// Out of line, as the registration below: both run inside the durable-I/O
// step, whose frame every reviewed storage path carries.
__attribute__((noinline)) void noah_profile_store_runtime_note_storage_paused(bool paused) {
    runtime_storage_paused = paused;
    noah_profile_paused_clock_note(&runtime_clock, paused, timer_read32());
}

static bool keyboard_quiet(void *context) {
    (void)context;
    return noah_qmk_profile_flash_keyboard_quiet();
}

static bool storage_paused(void *context) {
    (void)context;
    return runtime_storage_paused;
}
#else
// A store without an owner keeps no timers.
void noah_profile_store_runtime_note_storage_paused(bool paused) {
    (void)paused;
}
#endif

#if !defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static noah_profile_store_t runtime_store;
#endif

#ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
static noah_profile_owner_t runtime_owner;
// The working copy (D-F32): one slot payload of RAM per half, the largest
// static claim in the firmware (docs/architecture/memory-budgets.md).
static uint8_t              runtime_working_bytes[NOAH_PROFILE_PAYLOAD_MAX] __attribute__((aligned(4)));
static bool                 runtime_owner_initialized;
static bool                 runtime_working_transport_registered;
static bool                 runtime_transport_registered;
static bool                 runtime_integration_error;
#endif

void noah_profile_store_runtime_init(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    uint8_t origin;

    runtime_owner_initialized    = false;
    runtime_transport_registered = false;
    runtime_working_transport_registered = false;
    runtime_integration_error    = false;
    discovery_result             = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
#    if defined(SPLIT_KEYBOARD) && defined(SPLIT_TRANSACTION_IDS_USER)
    if (!noah_qmk_physical_half_origin(&origin) || !noah_profile_owner_init(&runtime_owner, &(noah_profile_owner_config_t){
                                                                                                .store_io                = noah_qmk_profile_flash_io(),
                                                                                                .split_exchange          = noah_qmk_profile_split_transport_exchange,
                                                                                                .split_transport_context = NULL,
                                                                                                .origin_half             = origin,
                                                                                                .peer_required           = true,
                                                                                                .working_bytes           = runtime_working_bytes,
                                                                                                .working_capacity        = sizeof(runtime_working_bytes),
                                                                                                .working_session         = noah_qmk_profile_working_session(),
                                                                                                .working_exchange        = noah_qmk_profile_working_transport_exchange,
                                                                                                .edit_mode_storage       = noah_qmk_edit_mode_storage(),
                                                                                                .storage_paused          = storage_paused,
                                                                                                .keyboard_quiet          = keyboard_quiet,
#        ifdef NOAH_PORTABLE_PROFILE_ENABLE
                                                                                                .settings_defaults_length = noah_profile_settings_defaults_length,
                                                                                                .settings_defaults_byte   = noah_profile_settings_defaults_byte,
#        endif
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
#else
    noah_profile_compiled_v1_t compiled;

    if (noah_profile_compiled_v1_open(&compiled, NULL) != NOAH_PROFILE_COMPILED_V1_OK) {
        discovery_result = NOAH_PROFILE_STORE_INVALID_PAYLOAD;
        runtime_state    = NOAH_PROFILE_STORE_RUNTIME_STORAGE_ERROR;
        return;
    }
    noah_profile_store_init(&runtime_store, noah_qmk_profile_flash_read_only_io(),
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = compiled.metadata.digest,
                                .action_abi_digest       = compiled.metadata.action_abi_digest,
                            });
    discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    runtime_state    = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
#endif
}

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static bool working_receive(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length) {
    return noah_profile_owner_working_receive(context, request, request_length, response, response_length);
}
#endif

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
// Once, as soon as the owner's reconciler exists: both split RPCs. False after
// an integration error.
static __attribute__((noinline)) bool register_transports(void) {
    if (!runtime_transport_registered) {
        noah_profile_split_reconciler_t *reconciler = noah_profile_owner_split_reconciler(&runtime_owner);

        if (reconciler) {
            runtime_transport_registered = noah_qmk_profile_split_transport_init(reconciler);
            if (!runtime_transport_registered) {
                runtime_integration_error = true;
                runtime_state             = NOAH_PROFILE_STORE_RUNTIME_INTEGRATION_ERROR;
                return false;
            }
        }
    }
    if (!runtime_working_transport_registered) {
        runtime_working_transport_registered = noah_qmk_profile_working_transport_init(working_receive, &runtime_owner);
    }
    return true;
}
#endif

bool noah_profile_store_runtime_storage_busy(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error && noah_profile_owner_storage_busy(&runtime_owner);
#else
    // Boot discovery without an owner reads a slot once per grant.
    return false;
#endif
}

void noah_profile_store_runtime_scan_again(bool again) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    if (runtime_owner_initialized && !runtime_integration_error) {
        noah_profile_owner_scan_again(&runtime_owner, again);
    }
#else
    (void)again;
#endif
}

bool noah_profile_store_runtime_matrix_scan_step(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    bool worked;

    if (!runtime_owner_initialized || runtime_integration_error) {
        return false;
    }
    worked = noah_profile_owner_scan(&runtime_owner, is_keyboard_master(), noah_profile_paused_clock_now(&runtime_clock, timer_read32()));
    if ((!runtime_transport_registered || !runtime_working_transport_registered) && !register_transports()) {
        return true;
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
#else
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
#else
    return runtime_state == NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND ? &runtime_store.committed : NULL;
#endif
}

bool noah_profile_store_runtime_read_committed(uint32_t offset, uint8_t *target, uint32_t length) {
    const noah_profile_store_record_t *record = noah_profile_store_runtime_committed();
    const noah_profile_store_t        *store;
    noah_profile_storage_address_t  payload_start;

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
    if (offset > record->payload_length || length > record->payload_length - offset || !noah_profile_store_slot_payload_start(record->slot, &payload_start)) {
        return false;
    }
    return store->io.read(store->io.context, (payload_start + offset), target, length);
}

// Opened lazily on first use rather than in the scan path: computing the
// compiled digest walks the authored tables. The boot keyboard-name read and
// the first host request use it. One open is enough, since the compiled bytes
// cannot change at runtime.
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

bool noah_profile_store_runtime_read_compiled(uint32_t offset, uint8_t *target, uint32_t length) {
    if (!target || length == 0u || !ensure_compiled_open() || !compiled_reader.read) {
        return false;
    }
    // Bounds first: offset and length arrive from the host.
    if (offset > compiled_profile.metadata.byte_length || length > compiled_profile.metadata.byte_length - offset) {
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

bool noah_profile_store_runtime_peer_transfer(noah_profile_candidate_v1_peer_status_t *peer) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error && noah_profile_owner_peer_transfer(&runtime_owner, peer);
#else
    (void)peer;
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

typedef struct {
    const noah_profile_store_t    *store;
    noah_profile_storage_address_t start;
} boot_payload_t;

static bool boot_payload_read(void *context, size_t offset, uint8_t *target, size_t length) {
    const boot_payload_t *payload = context;

    return length == 0u || (length <= NOAH_PROFILE_STORAGE_REGION_SIZE && noah_profile_store_read_span(payload->store, (noah_profile_storage_address_t)(payload->start + offset), target, (uint32_t)length));
}

bool noah_profile_store_runtime_read_keyboard_name(noah_profile_store_t *store, noah_profile_keyboard_name_t *name) {
    noah_profile_store_record_t record;
    noah_profile_domain_range_t settings;
    boot_payload_t              payload = {.store = store};

    if (!name) {
        return false;
    }
    memset(name, 0, sizeof(*name));
    if (!store || noah_profile_store_boot_select(store, &record) != NOAH_PROFILE_STORE_OK || !noah_profile_store_slot_payload_start(record.slot, &payload.start)) {
        return false;
    }
    noah_profile_reader_t reader = {.read = boot_payload_read, .context = &payload, .length = record.payload_length};

    return noah_profile_blob_v1_find_domain(&reader, 0u, record.payload_length, NOAH_PROFILE_DOMAIN_V1_SETTINGS, &settings) && noah_profile_settings_v1_keyboard_name(&reader, settings.offset, settings.length, name);
}

bool noah_profile_store_runtime_boot_keyboard_name(noah_profile_keyboard_name_t *name) {
#if defined(NOAH_PORTABLE_PROFILE_ENABLE)
    // Borrow the store the runtime has not started yet: no RAM of its own and
    // nothing large on the boot stack.
#    if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    noah_profile_store_t *store = runtime_owner_initialized ? NULL : &runtime_owner.store;
#    else
    noah_profile_store_t *store = runtime_state == NOAH_PROFILE_STORE_RUNTIME_UNINITIALIZED ? &runtime_store : NULL;
#    endif
    bool found;

    if (name) {
        memset(name, 0, sizeof(*name));
    }
    if (!name || !store || !ensure_compiled_open()) {
        return false;
    }
    noah_profile_store_init(store, noah_qmk_profile_flash_read_only_io(),
                            (noah_profile_store_compatibility_t){
                                .schema_major            = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
                                .schema_minor            = NOAH_PROFILE_STORE_SCHEMA_MINOR,
                                .compiled_default_digest = compiled_profile.metadata.digest,
                                .action_abi_digest       = compiled_profile.metadata.action_abi_digest,
                            });
    found = noah_profile_store_runtime_read_keyboard_name(store, name);
    memset(store, 0, sizeof(*store));
    return found;
#else
    if (name) {
        memset(name, 0, sizeof(*name));
    }
    return false;
#endif
}

#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
static noah_profile_owner_t *editor(void) {
    return runtime_owner_initialized && !runtime_integration_error ? &runtime_owner : NULL;
}

bool noah_profile_store_runtime_pending_adjustment(uint8_t setting, uint32_t *value) {
    return editor() && noah_profile_owner_pending_adjustment(editor(), setting, value);
}

bool noah_profile_store_runtime_adjust(uint8_t setting, uint32_t value) {
    return editor() && noah_profile_owner_adjust(editor(), setting, value);
}

noah_profile_working_result_t noah_profile_store_runtime_edit_begin(uint16_t transaction_id, uint32_t session, uint32_t revision, uint8_t splice_count, uint32_t insert_total, uint8_t flags) {
    return editor() ? noah_profile_owner_edit_begin(editor(), transaction_id, session, revision, splice_count, insert_total, flags) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_edit_splice(uint16_t transaction_id, uint8_t index, const noah_profile_working_splice_t *splice) {
    return editor() ? noah_profile_owner_edit_splice(editor(), transaction_id, index, splice) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_edit_data(uint16_t transaction_id, uint32_t offset, const uint8_t *bytes, uint32_t count) {
    return editor() ? noah_profile_owner_edit_data(editor(), transaction_id, offset, bytes, count) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_edit_commit(uint16_t transaction_id, uint32_t crc32) {
    return editor() ? noah_profile_owner_edit_commit(editor(), transaction_id, crc32) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_edit_abort(uint16_t transaction_id) {
    return editor() ? noah_profile_owner_edit_abort(editor(), transaction_id) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_revert(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    return editor() ? noah_profile_owner_revert(editor(), transaction_id, session, revision) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_save(uint16_t transaction_id, uint32_t session, uint32_t revision) {
    return editor() ? noah_profile_owner_save(editor(), transaction_id, session, revision) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

noah_profile_working_result_t noah_profile_store_runtime_set_edit_mode(uint8_t mode) {
    return editor() ? noah_profile_owner_set_edit_mode(editor(), mode) : NOAH_PROFILE_WORKING_INVALID_STATE;
}

bool noah_profile_store_runtime_working_status(noah_profile_owner_working_status_t *status) {
    return editor() && noah_profile_owner_working_status(editor(), status);
}

bool noah_profile_store_runtime_read_working(uint32_t offset, uint8_t *target, uint32_t length) {
    return editor() && noah_profile_owner_working_read(editor(), offset, target, length);
}
#endif

bool noah_profile_store_runtime_usb_restart_safe(void) {
#if defined(NOAH_LIVE_PROFILE_OWNER_ENABLE)
    return runtime_owner_initialized && !runtime_integration_error && noah_profile_owner_usb_restart_safe(&runtime_owner);
#else
    return false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────
// Live-Profile Store Boot Owner
// ─────────────────────────────────────────────────────────────────────────

#include "profile_store_runtime.h"

#include <stddef.h>

#include "../../compat/qmk_profile_eeprom.h"

static noah_profile_store_runtime_state_t runtime_state;
static noah_profile_store_result_t        discovery_result;

#ifdef VIA_ENABLE
static noah_profile_store_t runtime_store;
static const noah_profile_store_compatibility_t runtime_compatibility = {
    .schema_major      = NOAH_PROFILE_STORE_SCHEMA_MAJOR,
    .schema_minor      = NOAH_PROFILE_STORE_SCHEMA_MINOR,
    .action_abi_digest = 0u,
};
#endif

void noah_profile_store_runtime_init(void) {
#ifdef VIA_ENABLE
    noah_profile_store_init(&runtime_store, noah_qmk_profile_eeprom_read_only_io(), runtime_compatibility);
    discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    runtime_state    = NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING;
#else
    discovery_result = NOAH_PROFILE_STORE_NO_COMMITTED_PROFILE;
    runtime_state    = NOAH_PROFILE_STORE_RUNTIME_COMPILED_FALLBACK;
#endif
}

void noah_profile_store_runtime_matrix_scan(void) {
#ifdef VIA_ENABLE
    if (runtime_state != NOAH_PROFILE_STORE_RUNTIME_DISCOVERY_PENDING) {
        return;
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
#endif
}

noah_profile_store_runtime_state_t noah_profile_store_runtime_state(void) {
    return runtime_state;
}

noah_profile_store_result_t noah_profile_store_runtime_discovery_result(void) {
    return discovery_result;
}

const noah_profile_store_record_t *noah_profile_store_runtime_committed(void) {
#ifdef VIA_ENABLE
    return runtime_state == NOAH_PROFILE_STORE_RUNTIME_COMMITTED_FOUND ? &runtime_store.committed : NULL;
#else
    return NULL;
#endif
}

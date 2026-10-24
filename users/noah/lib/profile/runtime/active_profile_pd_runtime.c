#include "active_profile_pd_runtime.h"
#include <string.h>

// The validated records themselves: the structure is the record byte for byte
// (profile_pd_v1.c asserts it), so publication copies them in unchanged.
static noah_pd_config_t records[NOAH_PROFILE_PD_V1_SLOT_COUNT];
static bool             ready;

bool noah_active_profile_pd_ready(void) { return ready; }

// Every slot disabled with an empty name: what an omitted record means.
static void clear_records(void) {
    memset(records, 0, sizeof(records));
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) records[slot].id = slot;
}

void noah_active_profile_pd_load_compiled_defaults(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]) {
    ready = false;
    if (!defaults) return;
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        noah_profile_pd_v1_encode_record(&defaults[slot], (uint8_t *)&records[slot]);
        if (noah_profile_pd_v1_validate_record((const uint8_t *)&records[slot], sizeof(records[slot]), slot, NULL) != NOAH_PROFILE_PD_V1_OK) return;
    }
    ready = true;
}

const noah_pd_config_t *noah_active_profile_pd_record(uint8_t slot) {
    return ready && slot < NOAH_PROFILE_PD_V1_SLOT_COUNT ? &records[slot] : NULL;
}

const noah_pd_config_t *noah_active_profile_pd_for_mask(uint32_t mode) {
    if (mode == 0u || (mode & (mode - 1u)) != 0u) return NULL;
    uint8_t slot = 0;
    while (!(mode & 1u)) {
        mode >>= 1u;
        slot++;
    }
    const noah_pd_config_t *record = noah_active_profile_pd_record(slot);
    return record && record->kind != NOAH_PD_KIND_DISABLED ? record : NULL;
}

// Out of line, so the reviewed stack paths can name it whatever the
// invalidator around it does.
static __attribute__((noinline)) bool publish_pd_cache(const noah_active_profile_snapshot_t *view) {
    noah_profile_pd_v1_iterator_t iterator = {0};
    noah_profile_domain_range_t payload;
    if (!(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD) || !noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_PD, &payload)) return false;
    size_t base = view->base_offset + payload.offset;
    clear_records();
    do {
        noah_profile_pd_v1_iteration_t result = noah_profile_pd_v1_iterator_step(&iterator, &view->reader, base, payload.length, NULL);
        if (result == NOAH_PROFILE_PD_V1_REJECTED) return false;
        if (result == NOAH_PROFILE_PD_V1_RECORD) memcpy(&records[iterator.bytes[offsetof(noah_pd_config_t, id)]], iterator.bytes, sizeof(records[0]));
    } while (!noah_profile_pd_v1_iterator_complete(&iterator));
    return true;
}

void noah_active_profile_pd_invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view) {
    (void)context; (void)publication; (void)previous;
    // An edit to other domains (D-F32) keeps the cached records.
    if (ready && view && active.kind == NOAH_ACTIVE_PROFILE_KIND_WORKING_COPY && !(view->changed_domain_mask & NOAH_PROFILE_DOMAIN_MASK_PD)) return;
    ready = false;
    ready = view && publish_pd_cache(view);
}

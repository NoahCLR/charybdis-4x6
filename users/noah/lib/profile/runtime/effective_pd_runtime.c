#include "effective_pd_runtime.h"
#ifdef NOAH_PD_PROFILE_ENABLE
#include <string.h>


static uint8_t records[NOAH_PROFILE_PD_V1_SLOT_COUNT][NOAH_PROFILE_PD_V1_RECORD_SIZE];
static bool    ready;

bool noah_effective_pd_ready(void) { return ready; }

// Every slot disabled with an empty name: what an omitted record means.
static void clear_records(void) {
    memset(records, 0, sizeof(records));
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) records[slot][0] = slot;
}

void noah_effective_pd_load_compiled_defaults(const noah_pd_config_t defaults[NOAH_PROFILE_PD_V1_SLOT_COUNT]) {
    ready = false;
    if (!defaults) return;
    for (uint8_t slot = 0; slot < NOAH_PROFILE_PD_V1_SLOT_COUNT; slot++) {
        noah_profile_pd_v1_encode_record(&defaults[slot], records[slot]);
        if (noah_profile_pd_v1_validate_record(records[slot], NOAH_PROFILE_PD_V1_RECORD_SIZE, slot, NULL) != NOAH_PROFILE_PD_V1_OK) return;
    }
    ready = true;
}

const uint8_t *noah_effective_pd_record(uint8_t slot) {
    return ready && slot < NOAH_PROFILE_PD_V1_SLOT_COUNT ? records[slot] : NULL;
}

const uint8_t *noah_effective_pd_for_mask(uint32_t mode) {
    if (mode == 0u || (mode & (mode - 1u)) != 0u) return NULL;
    uint8_t slot = 0;
    while (!(mode & 1u)) {
        mode >>= 1u;
        slot++;
    }
    const uint8_t *record = noah_effective_pd_record(slot);
    return record && record[1] ? record : NULL;
}

static bool publish_pd_cache(const noah_effective_profile_snapshot_t *view) {
    noah_profile_pd_v1_iterator_t iterator = {0};
    noah_profile_domain_range_t payload;
    if (!(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD) || !noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_PD, &payload)) return false;
    size_t base = view->base_offset + payload.offset;
    clear_records();
    do {
        noah_profile_pd_v1_iteration_t result = noah_profile_pd_v1_iterator_step(&iterator, &view->reader, base, payload.length, NULL);
        if (result == NOAH_PROFILE_PD_V1_REJECTED) return false;
        if (result == NOAH_PROFILE_PD_V1_RECORD) memcpy(records[iterator.bytes[0]], iterator.bytes, NOAH_PROFILE_PD_V1_RECORD_SIZE);
    } while (!noah_profile_pd_v1_iterator_complete(&iterator));
    return true;
}

void noah_effective_pd_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    (void)context; (void)publication; (void)previous; (void)active;
    ready = false;
    ready = view && publish_pd_cache(view);
}
#endif

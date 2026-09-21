#include "effective_pd_runtime.h"
#ifdef NOAH_PD_PROFILE_ENABLE
static uint8_t records[NOAH_PROFILE_PD_V1_SIZE];
static bool ready;

bool noah_effective_pd_ready(void) { return ready; }

const uint8_t *noah_effective_pd_record(uint8_t slot) {
    return ready && slot < 8 ? records + 8 + (size_t)slot * 96 : NULL;
}

const uint8_t *noah_effective_pd_for_mask(uint8_t mode) {
    for (uint8_t slot = 0; slot < 8; slot++) {
        if (mode == (uint8_t)(1u << slot)) {
            const uint8_t *record = noah_effective_pd_record(slot);
            return record && record[1] ? record : NULL;
        }
    }
    return NULL;
}

void noah_effective_pd_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    (void)context; (void)publication; (void)previous; (void)active;
    ready = false;
    if (!view || !(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD) || view->profile.pd.length != sizeof(records)) return;
    for (size_t offset = 0; offset < sizeof(records); offset += 20) {
        size_t count = sizeof(records) - offset;
        if (count > 20) count = 20;
        if (!noah_profile_reader_read(&view->reader, view->base_offset + view->profile.pd.offset + offset, records + offset, count)) return;
    }
    ready = noah_profile_pd_v1_validate(records, sizeof(records), NULL) == NOAH_PROFILE_PD_V1_OK;
}
#endif

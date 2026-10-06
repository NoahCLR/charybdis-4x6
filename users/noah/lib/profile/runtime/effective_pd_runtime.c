#include "effective_pd_runtime.h"
#ifdef NOAH_PD_PROFILE_ENABLE
#include <string.h>

enum { PD_CACHE_READ_MAX = 20u };

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

static bool read_pd_chunks(const noah_effective_profile_snapshot_t *view, size_t offset, uint8_t *target, size_t length) {
    for (size_t done = 0; done < length; done += PD_CACHE_READ_MAX) {
        size_t count = length - done;
        if (count > PD_CACHE_READ_MAX) count = PD_CACHE_READ_MAX;
        if (!noah_profile_reader_read(&view->reader, offset + done, target + done, count)) return false;
    }
    return true;
}

// Reads at most 20 bytes per call. Each record's first read names its slot;
// the rest of the record is read straight into that slot's row.
static bool publish_pd_cache(const noah_effective_profile_snapshot_t *view) {
    uint8_t header[NOAH_PROFILE_PD_V1_HEADER_SIZE];
    uint8_t first[PD_CACHE_READ_MAX];
    uint8_t count, minimum = 0;
    size_t  base = view->base_offset + view->profile.pd.offset;

    if (!(view->profile.domain_mask & NOAH_PROFILE_VALIDATOR_V1_DOMAIN_PD) || view->profile.pd.length < sizeof(header) || view->profile.pd.length > NOAH_PROFILE_PD_V1_MAX_SIZE) return false;
    if (!noah_profile_reader_read(&view->reader, base, header, sizeof(header))) return false;
    if (noah_profile_pd_v1_validate_header(header, view->profile.pd.length, &count, NULL) != NOAH_PROFILE_PD_V1_OK) return false;
    clear_records();
    for (uint8_t index = 0; index < count; index++) {
        size_t offset = base + NOAH_PROFILE_PD_V1_HEADER_SIZE + (size_t)index * NOAH_PROFILE_PD_V1_RECORD_SIZE;
        if (!noah_profile_reader_read(&view->reader, offset, first, sizeof(first))) return false;
        uint8_t slot = first[0];
        if (slot >= NOAH_PROFILE_PD_V1_SLOT_COUNT || slot < minimum) return false;
        memcpy(records[slot], first, sizeof(first));
        if (!read_pd_chunks(view, offset + sizeof(first), records[slot] + sizeof(first), NOAH_PROFILE_PD_V1_RECORD_SIZE - sizeof(first))) return false;
        if (noah_profile_pd_v1_validate_entry(records[slot], NOAH_PROFILE_PD_V1_RECORD_SIZE, minimum, NULL) != NOAH_PROFILE_PD_V1_OK) return false;
        minimum = (uint8_t)(slot + 1u);
    }
    return true;
}

void noah_effective_pd_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view) {
    (void)context; (void)publication; (void)previous; (void)active;
    ready = false;
    ready = view && publish_pd_cache(view);
}
#endif

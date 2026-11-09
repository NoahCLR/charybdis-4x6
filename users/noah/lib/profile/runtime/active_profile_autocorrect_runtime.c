#include "active_profile_autocorrect_runtime.h"

#include "noah_keymap_ids.h"

static noah_profile_autocorrect_v1_list_t active_list;

void noah_active_profile_autocorrect_invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view) {
    (void)context;
    (void)publication;
    (void)previous;
    noah_profile_domain_range_t range;
    // A working-copy edit to another domain can still move this one, so the
    // range is found again on every publication.
    if (view && active.kind != NOAH_ACTIVE_PROFILE_KIND_COMPILED_DEFAULTS && (view->profile.domain_mask & NOAH_PROFILE_DOMAIN_MASK_AUTOCORRECT)) {
        // A validated profile always has the range and a list that opens; if
        // either fails, nothing is corrected rather than something else.
        if (!noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_AUTOCORRECT, &range) || !noah_profile_autocorrect_v1_open(&active_list, &view->reader, view->base_offset + range.offset, range.length)) {
            active_list = (noah_profile_autocorrect_v1_list_t){0};
        }
        return;
    }
    // The compiled reader re-encodes the profile on every read, so the
    // authored list is read where it lies.
    noah_profile_reader_t authored = noah_profile_reader_from_memory(authored_autocorrect_list, authored_autocorrect_list_size);
    noah_profile_autocorrect_v1_open(&active_list, &authored, 0u, authored_autocorrect_list_size);
}

const noah_profile_autocorrect_v1_list_t *noah_active_profile_autocorrect_list(void) {
    return &active_list;
}

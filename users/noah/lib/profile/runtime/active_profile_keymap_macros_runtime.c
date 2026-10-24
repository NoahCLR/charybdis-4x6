#include "active_profile_keymap_macros_runtime.h"

// No profile-sized cache, no snapshot on a key path. The store/provider keeps
// this generation immutable until a safe publication boundary.
static noah_profile_reader_t active_reader;
static size_t keymap_base, macros_base;
static uint32_t keymap_length, macros_length, epoch;
static bool keymap_live, macros_live;

void noah_active_profile_keymap_macros_invalidate(void *context, uint32_t publication, noah_active_profile_identity_t previous, noah_active_profile_identity_t active, const noah_active_profile_snapshot_t *view) {
    (void)context;
    (void)publication;
    (void)previous;
    keymap_length = macros_length = 0u;
    keymap_live = macros_live = false;
    epoch++;
    if (!view || active.kind == NOAH_ACTIVE_PROFILE_KIND_COMPILED_DEFAULTS) return;
    active_reader = view->reader;
    // A domain is live only once its range is found. A validated profile
    // always has it; if the lookup still fails, keys and macros keep the
    // authored data rather than reading an empty range.
    noah_profile_domain_range_t range;
    if ((view->profile.domain_mask & NOAH_PROFILE_DOMAIN_MASK_KEYMAP) && noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_KEYMAP, &range)) {
        keymap_base = view->base_offset + range.offset;
        keymap_length = range.length;
        keymap_live = true;
    }
    if ((view->profile.domain_mask & NOAH_PROFILE_DOMAIN_MASK_MACROS) && noah_profile_blob_v1_find_domain(&view->reader, view->base_offset, view->profile.byte_length, NOAH_PROFILE_DOMAIN_V1_MACROS, &range)) {
        macros_base = view->base_offset + range.offset;
        macros_length = range.length;
        macros_live = true;
    }
}
bool noah_active_profile_keymap_is_live(void) { return keymap_live; }
bool noah_active_profile_keymap_read(uint32_t offset, uint8_t bytes[2]) {
    return offset <= keymap_length && 2u <= keymap_length - offset && noah_profile_reader_read(&active_reader, keymap_base + offset, bytes, 2u);
}
uint32_t noah_active_profile_keymap_macros_publication(void) { return epoch; }
bool noah_active_profile_macro_bank_is_live(void) { return macros_live; }
uint16_t noah_active_profile_macro_bank_length(void) { return (uint16_t)macros_length; }
// The range was found inside the reader when it was published, so a read
// inside it calls the reader's own callback: one frame less on the key press
// that starts a macro.
static inline __attribute__((always_inline)) bool bank_read(uint32_t offset, uint8_t *bytes, uint16_t length) {
    return macros_live && length != 0u && offset <= macros_length && length <= macros_length - offset && active_reader.read(active_reader.context, macros_base + offset, bytes, length);
}
__attribute__((noinline)) bool noah_active_profile_macro_bank_read(uint16_t offset, uint8_t *bytes, uint16_t length) {
    return bank_read(offset, bytes, length);
}
bool noah_active_profile_macro_read_byte(uint16_t offset, uint8_t *byte, void *context) {
    const uint16_t *start = context;
    return byte && start && bank_read((uint32_t)*start + offset, byte, 1u);
}

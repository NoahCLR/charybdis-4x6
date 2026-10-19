#pragma once
#include "effective_profile_provider.h"

// Immutable active-domain readers. Missing domains use authored defaults;
// installed domains fail closed on a read error. Publication is scan-owned.
void noah_effective_keymap_macros_invalidate(void *context, uint32_t publication, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
bool noah_effective_keymap_is_live(void);
bool noah_effective_keymap_read(uint32_t offset, uint8_t bytes[2]);
uint32_t noah_effective_keymap_macros_publication(void);
bool noah_effective_macro_bank_is_live(void);
uint16_t noah_effective_macro_bank_length(void);
bool noah_effective_macro_bank_read(uint16_t offset, uint8_t *bytes, uint16_t length);
// A macro decoder's byte reader: context points at the macro's start in the
// bank (uint16_t), offset is from there.
bool noah_effective_macro_read_byte(uint16_t offset, uint8_t *byte, void *context);

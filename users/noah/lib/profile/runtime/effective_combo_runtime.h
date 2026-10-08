#pragma once

#include QMK_KEYBOARD_H
#include "effective_profile_provider.h"
#ifdef COMBO_ENABLE

// QMK mutates combo state in place and retains its input pointer. The native
// table must therefore live as long as the owner, independent of EEPROM reads.
typedef struct {
    const noah_effective_profile_snapshot_t *compiled_defaults;
    combo_t  rows[32];
    uint16_t inputs[32][5];
    uint16_t terms[32]; // resolved: a row that follows the default holds it
    uint32_t follows_default;
    uint16_t default_term;
    uint16_t hold_term;
    uint8_t  flags[32];
    uint8_t  count;
    bool     live;
    bool     valid;
} noah_effective_combo_runtime_t;

void noah_effective_combo_runtime_init(noah_effective_combo_runtime_t *runtime);
bool noah_effective_combo_runtime_install(noah_effective_combo_runtime_t *runtime);
void noah_effective_combo_runtime_uninstall(noah_effective_combo_runtime_t *runtime);
// Runs only at the strict idle publication boundary. Reads the header and copies
// at most 32 fixed 28-byte rows once; ordinary key processing performs no
// profile-reader I/O.
void     noah_effective_combo_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
uint16_t noah_effective_combo_count(void);
combo_t *noah_effective_combo_get(uint16_t index);
bool     noah_effective_combo_valid(void);
uint16_t noah_effective_combo_term(uint16_t index);
// The window a combo without its own follows, and whether a combo does.
// A compiled COMBO follows COMBO_TERM; a COMBO_WINDOW row has its own.
uint16_t noah_effective_combo_default_term(void);
bool     noah_effective_combo_follows_default(uint16_t index);
uint16_t noah_effective_combo_hold_term(void);
uint8_t  noah_effective_combo_flags(uint16_t index);

#endif

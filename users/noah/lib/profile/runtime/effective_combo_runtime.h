#pragma once

#include QMK_KEYBOARD_H
#include "effective_profile_provider.h"
#ifdef COMBO_ENABLE

// QMK mutates combo state in place and retains its input pointer. The native
// table must therefore live as long as the owner, independent of EEPROM reads.
typedef struct {
    combo_t rows[32];
    uint16_t inputs[32][5];
    uint16_t terms[32];
    uint16_t hold_term;
    uint8_t flags[32];
    uint8_t count;
    bool live;
    bool valid;
} noah_effective_combo_runtime_t;

void noah_effective_combo_runtime_init(noah_effective_combo_runtime_t *runtime);
bool noah_effective_combo_runtime_install(noah_effective_combo_runtime_t *runtime);
void noah_effective_combo_runtime_uninstall(noah_effective_combo_runtime_t *runtime);
// Runs only at the strict idle publication boundary. Copies at most 32 fixed
// 28-byte rows once; ordinary key processing performs no profile-reader I/O.
void noah_effective_combo_runtime_invalidate(void *context, uint32_t publication_count, noah_effective_profile_identity_t previous, noah_effective_profile_identity_t active, const noah_effective_profile_snapshot_t *view);
uint16_t noah_effective_combo_count(void);
combo_t *noah_effective_combo_get(uint16_t index);
bool noah_effective_combo_valid(void);
uint16_t noah_effective_combo_term(uint16_t index);
uint16_t noah_effective_combo_hold_term(void);
uint8_t noah_effective_combo_flags(uint16_t index);

#endif

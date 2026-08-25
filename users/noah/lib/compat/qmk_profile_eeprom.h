// ───────────────────────────────────────────────────────────────────────────
// QMK Live-Profile EEPROM Adapter
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include "../profile/storage/profile_store.h"

// Stage 02 boot discovery deliberately receives no write callback. Candidate
// persistence receives the write-capable form only from its scan-context
// owner; exposing this adapter does not make the VIA callback writable.
noah_profile_store_io_t noah_qmk_profile_eeprom_read_only_io(void);
noah_profile_store_io_t noah_qmk_profile_eeprom_io(void);

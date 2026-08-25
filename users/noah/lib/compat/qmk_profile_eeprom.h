// ───────────────────────────────────────────────────────────────────────────
// QMK Live-Profile EEPROM Adapter
// ──────────────────────────────────────────────────────────────────────────
#pragma once

#include "../profile/storage/profile_store.h"

// Stage 02 boot discovery deliberately receives no write callback. Candidate
// persistence will add its scan-context writer only with the mutation protocol.
noah_profile_store_io_t noah_qmk_profile_eeprom_read_only_io(void);

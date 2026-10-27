// ───────────────────────────────────────────────────────────────────────────
// QMK Working Copy Boundary (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// What the working copy needs from QMK and the RP2040: a session that differs
// between boots and the edit mode in QMK's EEPROM user datablock. Its sync's
// split RPC is qmk_profile_split_transport.c's.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../profile/runtime/profile_edit_mode.h"

enum {
    // Two records, so a write cut short leaves the other: magic, layout,
    // mode, three zero bytes, counter, CRC-32 of the first twelve bytes.
    NOAH_QMK_EDIT_MODE_RECORD_SIZE = 16u,
    NOAH_QMK_EDIT_MODE_RECORDS     = 2u,
    NOAH_QMK_EDIT_MODE_MAGIC       = 0x4D45u, // "EM"
    NOAH_QMK_EDIT_MODE_LAYOUT      = 1u,
};

// A value that differs between boots: the ring oscillator's random bit on the
// RP2040, mixed with the boot time.
uint32_t noah_qmk_profile_working_session(void);

void noah_qmk_edit_mode_encode(const noah_profile_edit_mode_record_t *record, uint8_t bytes[NOAH_QMK_EDIT_MODE_RECORD_SIZE]);
bool noah_qmk_edit_mode_decode(const uint8_t bytes[NOAH_QMK_EDIT_MODE_RECORD_SIZE], noah_profile_edit_mode_record_t *record);
// The newer valid record of a datablock; false when neither is valid.
bool noah_qmk_edit_mode_choose(const uint8_t datablock[NOAH_QMK_EDIT_MODE_RECORD_SIZE * NOAH_QMK_EDIT_MODE_RECORDS], noah_profile_edit_mode_record_t *record, uint8_t *slot);
// The edit mode's storage: reads and writes QMK's user datablock, a write
// going to the record that is not the newer one and read back.
noah_profile_edit_mode_storage_t noah_qmk_edit_mode_storage(void);

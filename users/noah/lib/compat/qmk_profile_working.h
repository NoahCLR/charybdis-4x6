// ───────────────────────────────────────────────────────────────────────────
// QMK Working Copy Boundary (D-F32)
// ───────────────────────────────────────────────────────────────────────────
//
// What the working copy needs from QMK and the RP2040: a session that differs
// between boots, the edit mode in QMK's EEPROM user datablock, and the split
// RPC the working copy's sync rides on.
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

// The working copy sync's split RPC. `receive` answers on the other half.
typedef bool (*noah_qmk_profile_working_receive_fn)(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
bool noah_qmk_profile_working_transport_init(noah_qmk_profile_working_receive_fn receive, void *context);
bool noah_qmk_profile_working_transport_exchange(void *context, const uint8_t *request, uint8_t request_length, uint8_t *response, uint8_t response_length);
#ifdef NOAH_PROFILE_SPLIT_TRANSPORT_TEST
void noah_qmk_profile_working_transport_reset_for_test(void);
#endif

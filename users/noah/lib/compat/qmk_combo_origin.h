// ────────────────────────────────────────────────────────────────────────────
// QMK Combo Origin Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// QMK emits normal combo outputs as COMBO_EVENT records at key position
// (0,0). This module shadows the live physical combo press stream so userspace
// can recover the representative owner key and full locality footprint before
// runtime consumers see the event.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

#include "../key/runtime/origin_registry.h"

void              noah_qmk_combo_origin_init(void);
void              noah_qmk_combo_origin_reset(void);
void              noah_qmk_combo_origin_observe_physical_key_event(uint16_t keycode, keyrecord_t *record);
void              noah_qmk_combo_origin_normalize_record(uint16_t keycode, keyrecord_t *record);
void              noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap);
bool              noah_qmk_combo_origin_event_owner_keypos(const keyrecord_t *record, keypos_t *out);
bool              noah_qmk_combo_origin_event_bitmap(const keyrecord_t *record, uint8_t *out_bitmap);
split_side_mask_t noah_qmk_combo_origin_event_side_mask(const keyrecord_t *record);

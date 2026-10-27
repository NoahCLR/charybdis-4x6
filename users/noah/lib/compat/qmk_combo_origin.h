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

#include "../key/runtime/slot/origin_registry.h"

typedef struct {
    uint8_t  pending_count;
    uint8_t  active_count;
    uint8_t  pending_high_water;
    uint16_t suppressed_retirement_count;
    uint16_t deadline_expiry_count;
    uint16_t cache_full_refusal_count;
    uint16_t unmatched_delayed_output_count;
} noah_qmk_combo_origin_debug_snapshot_t;

void              noah_qmk_combo_origin_init(void);
void              noah_qmk_combo_origin_reset(void);
void              noah_qmk_combo_origin_scan(void);
void              noah_qmk_combo_origin_observe_physical_key_event(uint16_t keycode, keyrecord_t *record);
void              noah_qmk_combo_origin_normalize_record(uint16_t keycode, keyrecord_t *record);
void              noah_qmk_combo_origin_pressed_combo_bitmap(uint8_t *out_bitmap);
bool              noah_qmk_combo_origin_pressed_combo_matches(uint16_t keycode, keypos_t owner_key_pos, uint16_t since, uint16_t term_ms);
// Whether any combo output is held: without one the partition below is empty.
bool              noah_qmk_combo_origin_any_active(void);
// Active combo footprints, split by owner: a combo owning the preview or PD
// state goes underneath them, any other above. A combo whose owner key is in
// quiet_owner_bitmap (may be NULL) contributes to neither.
void              noah_qmk_combo_origin_active_bitmaps_partitioned(keypos_t preview_owner_key_pos, keypos_t pd_owner_key_pos, const uint8_t *quiet_owner_bitmap, uint8_t *out_underlay_bitmap, uint8_t *out_overlay_bitmap);
bool              noah_qmk_combo_origin_event_owner_keypos(const keyrecord_t *record, keypos_t *out);
bool              noah_qmk_combo_origin_event_bitmap(const keyrecord_t *record, uint8_t *out_bitmap);
split_side_mask_t noah_qmk_combo_origin_event_side_mask(const keyrecord_t *record);
void              noah_qmk_combo_origin_debug_snapshot(noah_qmk_combo_origin_debug_snapshot_t *out);

#ifdef COMBO_ORIGIN_TEST_INSTRUMENTATION
// Pending-output slots the reconcile pass has looked at since the last reset.
void     noah_qmk_combo_origin_test_pending_slot_visits_reset(void);
uint16_t noah_qmk_combo_origin_test_pending_slot_visits(void);
#endif

// Read-only generated-output layer context, before tapping/admission queues.
uint8_t noah_qmk_combo_origin_record_source_layer(uint16_t keycode, const keyrecord_t *record);
// Attach the completed chord's output permission to a generated record
// without consuming its pending origin. Release uses
// the matching press's snapshot, even when that press is still queued.
void noah_qmk_combo_origin_capture_record(uint16_t keycode, keyrecord_t *record);
// Where the runtime holds the dispatched output whose release, of this
// keycode, dispatch pairs next: its chord's owner key. Record admission
// judges a captured release there (D-F27 "Releases that wait").
bool noah_qmk_combo_origin_release_owner(uint16_t keycode, keypos_t *out);
// Whether QMK has delivered the release of an output held at owner_key_pos
// that has not been dispatched yet: it is queued, so that output reaches no
// later hold tier.
bool noah_qmk_combo_origin_release_delivered_at(keypos_t owner_key_pos);

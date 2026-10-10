// ────────────────────────────────────────────────────────────────────────────
// Behaviour and Combo Participation
// ────────────────────────────────────────────────────────────────────────────
//
// The one place that decides whether a press uses its keycode's behaviour
// row and whether it may join a combo (docs/architecture/participation-
// policy.md, D-F14). Every applicable scope must allow it: the master, the
// press's source layer, the definition, and the placement (source layer, key
// position). Callers pass the press context and act on the answer; none
// rebuilds the policy. It reads the effective settings cache and the
// effective behaviour and combo views, never storage.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include QMK_KEYBOARD_H // IWYU pragma: keep

// A physical press of keycode at key_pos, its resolved keycode from
// source_layer: does it use the keycode's behaviour row? False means it takes
// the normal action, as a key without a row always does.
bool noah_participation_behavior(uint16_t keycode, uint8_t source_layer, keypos_t key_pos);
// A custom key a combo emits has no placement: the master and the row's
// enable decide, with the combo's origin layer as its layer context.
bool noah_participation_behavior_generated(uint16_t keycode, uint8_t origin_layer);
// A physical press's own combo permission: its source layer's combo switch and
// its placement's exclusion bit. Decided at the press and kept for its release.
bool noah_participation_combo_press(uint8_t source_layer, keypos_t key_pos);
// A combo definition's: enabled, and allowed on the press's source layer.
bool noah_participation_combo_row(uint16_t combo_index, uint8_t source_layer);

// The decisions each physical key was last pressed with, kept until it is
// pressed again. These supply initial record capture; queued records read
// their own immutable context. A position never pressed allows both.
void noah_participation_press_store(keypos_t key_pos, bool behavior, bool combo);
bool noah_participation_press_behavior(keypos_t key_pos);
bool noah_participation_press_combo(keypos_t key_pos);

// Opaque QMK record context carries the captured source and decisions through
// every record copy. Releases copy their physical press's context.
void noah_participation_record_capture(keyrecord_t *record, uint8_t source_layer);
// A physical record leaving the queues, with its keycode and the layer QMK
// resolves that keycode from now. A press is settled once, the first time it
// is delivered: re-decided if it waited while the layers changed, so it
// resolves from another layer than at its press, and kept until its release,
// which takes that decision. Combo permission stays the physical press's.
void noah_participation_record_deliver(keyrecord_t *record, uint16_t keycode, uint8_t source_layer);
// A delivered press held back again (record admission): it settles anew when
// it is replayed.
void noah_participation_record_defer(keyrecord_t *record);
bool noah_participation_record_behavior(const keyrecord_t *record);
bool noah_participation_record_combo(const keyrecord_t *record);
uint8_t noah_participation_record_source(const keyrecord_t *record);
bool noah_participation_record_generated_captured(const keyrecord_t *record);
void noah_participation_record_generated(keyrecord_t *record, uint16_t keycode, uint8_t origin_layer);

bool noah_participation_record_generated_normalized(const keyrecord_t *record);
void noah_participation_record_generated_normalize(keyrecord_t *record, uint16_t keycode, uint8_t origin_layer);

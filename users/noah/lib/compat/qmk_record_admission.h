#pragma once

#include QMK_KEYBOARD_H

// Holds key records back while a runtime-owned tap/hold key (an authored LT,
// MT or OSM row) is down and undecided, as QMK's tapping engine holds keys
// behind its tapping key. Once that key is decided (released: tap; held past
// its tap-hold term: hold) the records replay in order through QMK's
// process_record, keeping their physical timestamps, so "/" then "," types
// "/," and a key pressed under a held layer resolves on that layer.
//
// Admission sits at the fork's process_record_admit_user() hook: after combos
// and native tapping, before every QMK feature. A release whose press was not
// held back (including the deciding key's own release) passes straight
// through, as it does in QMK.
//
// At capacity, replay exactly the oldest held record before appending the new
// one. That record may run before the tap/hold decision; later records still
// wait in FIFO order. No record is dropped or allowed to bypass its own press,
// and replay keeps the original timestamp, event type, keycode and tap data.
bool noah_record_admission_admit(keyrecord_t *record);

// Replays held records once no tap/hold key is undecided. Runs each matrix
// scan after the key runtime scan, so a hold reached this scan is applied
// before the records it held back.
void noah_record_admission_task(void);

void    noah_record_admission_reset(void);
uint8_t noah_record_admission_held_count(void);

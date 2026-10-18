#pragma once

#include QMK_KEYBOARD_H
#include "../macro/macro_input_guard.h"

// Holds key records back while a tap/hold key whose hold changes other keys
// (any LT, MT or OSM key, a layer or modifier hold: D-F27) is down and
// undecided, as QMK's tapping engine holds keys
// behind its tapping key. Once that key is decided (released: tap; held past
// its tap-hold term: hold) the records replay in order through QMK's
// process_record, keeping their physical timestamps, so "/" then "," types
// "/," and a key pressed under a held layer resolves on that layer.
//
// Admission sits at the fork's process_record_admit_user() hook: after combos
// and native tapping, before every QMK feature. A release whose press was not
// held back passes straight through, as it does in QMK, except a modifier's or
// a layer key's: QMK keeps those while its tapping key is undecided, so the
// tap still gets them, and so does admission (D-F27). The deciding key's own
// release always passes.
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

// Whether a held-back press of the physical key at pos happened within term
// ms of since; a tap series waiting for its next press counts it as pending,
// as it does a press in QMK's combo buffer (D-F01).
bool    noah_record_admission_press_pending(keypos_t pos, uint16_t since, uint16_t term);
// Whether a held-back release of the physical key at pos happened within
// term ms of since; the key runtime's hold gate counts it as queued.
bool    noah_record_admission_release_pending(keypos_t pos, uint16_t since, uint16_t term);

void    noah_record_admission_reset(void);
uint8_t noah_record_admission_held_count(void);

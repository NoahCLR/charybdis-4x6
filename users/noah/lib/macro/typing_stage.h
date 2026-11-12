#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

// The typing stage sits between live key ownership and the keyboard report:
// it sees every key the person's typing puts in a report, and may hold keys
// back and let them out later, in order. Autocorrect is the stage (D-F69,
// lib/autocorrect/). The callers below define weak defaults that do nothing,
// so a build or host test without the stage links unchanged.

// False while the stage has nothing to see or hold; the report then goes out
// as live ownership made it.
bool noah_typing_stage_active(void);

// Called for every outgoing report while the stage is active. `keys` arrives
// as the live usages, bit per usage, and leaves as the usages the host is to
// see from the person's typing. `macro_keys` and `macro_mods` are what a
// playing macro holds; the stage leaves them out of what it reads, and the
// caller adds them back.
void noah_typing_stage_keys(uint8_t keys[32], const uint8_t macro_keys[32], uint8_t macro_mods);

// True, with the modifiers the host is to see from the person's typing, while
// the stage holds keys back; false leaves QMK's own modifiers.
bool noah_typing_stage_mods(uint8_t *mods);

// True when the stage took a macro key pressed while it holds keys back; the
// macro then starts when its turn comes (macro/profile_macro_provider.c).
bool noah_typing_stage_defer_macro(uint8_t slot);

// A mouse button went down (action/owned_keycode.c).
void noah_typing_stage_note_click(void);

// The Leader key acted (action/action_kind_dispatch.c): what is typed next is
// held, never shown, until it matches a typed form that fires after the
// leader or is dropped (D-F71). It takes its place among the keys the stage holds back.
void noah_typing_stage_leader(void);

// The key whose press or action is about to put keys in a report; a position
// outside the matrix is an action no key names. A key QMK registers is noted
// as the hook hands its press on (key/runtime/process.c), a tap where it is
// dispatched (action/action_kind_dispatch.c) and a hold where it is registered
// (key/ownership/held_action.c). The stage keeps it with each key it reads
// going down, so lighting can show which key ended a word (D-F70). The note is
// the position stored as it is, not a call or a sum: it sits on the deepest
// reviewed stack paths, where anything more costs each of them a frame slot.
// Each file that notes defines a weak one, for a build or host test without
// the stage.
extern keypos_t noah_typing_stage_noted_key;

// A usage's owners in the ledger changed, and `owners` hold it now, a playing
// macro's leases among them (action/owned_keycode.c). While a macro holds a
// usage the report shows nothing of the person pressing or releasing that
// same usage, so the stage reads those changes here.
void noah_typing_stage_note_owners(uint8_t usage, uint16_t owners);

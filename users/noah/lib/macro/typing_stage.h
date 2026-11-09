#pragma once

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

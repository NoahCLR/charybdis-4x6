// ───────────────────────────────────────────────────────────────────────────
// QMK Profile Slot Flash Adapter Hooks
// ───────────────────────────────────────────────────────────────────────────
//
// What the scan scheduler, housekeeping and the Profile Wire channel call,
// without the store's layout (qmk_profile_flash.h).
// ───────────────────────────────────────────────────────────────────────────
#pragma once

#include <stdbool.h>
#include <stdint.h>

// keyboard_pre_init, before QMK's keyboard init: an EEPROM reset forgets the
// saved profiles (D-F29). Runs before anything reads a slot.
void noah_qmk_profile_flash_pre_init(void);

// Called at the start of every durable-I/O grant. With the keyboard quiet it
// may erase one sector ahead of a save's writes.
void noah_qmk_profile_flash_grant_begin(void);

// Whether the profile store's step may run in this grant: false while a save
// waits for erased flash or the grant has used its flash time (D-F29).
bool noah_qmk_profile_flash_may_write(void);

// Whether this grant has erased or programmed anything yet: the scheduler
// repeats the store's local steps only until it has (D-F29).
bool noah_qmk_profile_flash_grant_flashed(void);

// The flash driver's free-running microsecond clock (the scheduler's time
// budget for repeated steps).
uint32_t noah_qmk_profile_flash_now_us(void);

// Handles Profile Wire custom get 0x0D (flash pauses); false for any other
// request.
bool noah_qmk_profile_flash_wire_get(uint8_t *data, uint8_t length);

// Housekeeping on both halves: registers the split pull's handler, and on
// the USB half pulls the other half's figures once a second.
void noah_qmk_profile_flash_housekeeping(void);

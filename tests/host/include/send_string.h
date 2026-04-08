#pragma once

#include <stdint.h>

#define SS_QMK_PREFIX 1
#define SS_TAP_CODE 1
#define SS_DOWN_CODE 2
#define SS_UP_CODE 3
#define SS_DELAY_CODE 4

void send_char_with_delay(char ascii_code, uint8_t interval);

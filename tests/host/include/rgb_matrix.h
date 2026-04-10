#pragma once

#include "qmk_stub.h"

#ifndef NO_LED
#    define NO_LED 255u
#endif

typedef struct {
    uint8_t matrix_co[MATRIX_ROWS][MATRIX_COLS];
} led_config_t;

extern led_config_t g_led_config;

void    rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);
uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i);

#pragma once

#include "qmk_stub.h"
#ifndef RGB_MATRIX_SOLID_COLOR
#    define RGB_MATRIX_SOLID_COLOR 1
#endif

#ifndef NO_LED
#    define NO_LED 255u
#endif

typedef struct {
    uint8_t matrix_co[MATRIX_ROWS][MATRIX_COLS];
    uint8_t flags[256]; // host native-effect fixtures use the real flag tests
} led_config_t;

extern led_config_t g_led_config;

bool rgb_matrix_get_suspend_state(void);
uint8_t rgb_matrix_is_enabled(void);
uint8_t rgb_matrix_get_mode(void);
void    rgb_matrix_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);
int     rgb_matrix_led_index(int index);
uint8_t rgb_matrix_map_row_column_to_led(uint8_t row, uint8_t column, uint8_t *led_i);

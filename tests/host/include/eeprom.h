#pragma once

#include <stddef.h>

#ifndef TOTAL_EEPROM_BYTE_COUNT
#    define TOTAL_EEPROM_BYTE_COUNT 4096u
#endif

void eeprom_read_block(void *target, const void *source, size_t length);

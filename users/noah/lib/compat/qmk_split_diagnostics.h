#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef SPLIT_TRANSACTION_DIAGNOSTICS
bool noah_split_diagnostics_command(uint8_t *data, uint8_t length);
#else
static inline bool noah_split_diagnostics_command(uint8_t *data, uint8_t length) {
    (void)data;
    (void)length;
    return false;
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>

// Read-only Profile Wire value 0x06. Page zero describes the compiled combo
// table and current matching policy; one complete row follows per page.
bool noah_qmk_combo_readback_get(uint8_t *report, uint8_t length);

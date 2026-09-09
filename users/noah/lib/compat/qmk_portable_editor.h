#pragma once
#include <stdint.h>

#ifdef NOAH_PORTABLE_PROFILE_ENABLE
// Optional cold metadata under GET 0x08. Zero length means unsupported page.
uint8_t noah_qmk_portable_editor_page(uint8_t page, uint8_t *payload);
void noah_qmk_portable_apply_lighting(uint32_t mode, uint32_t color);
#endif

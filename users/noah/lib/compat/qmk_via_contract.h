// ────────────────────────────────────────────────────────────────────────────
// QMK VIA Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────
//
// Centralizes this userspace's dependence on VIA/QMK internals used for
// authored default macro seeding and VIA command classification.
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include QMK_KEYBOARD_H // IWYU pragma: keep

#include <stdbool.h>
#include <stdint.h>

enum {
    NOAH_QMK_VIA_COMMAND_EFFECT_NONE           = 0,
    NOAH_QMK_VIA_COMMAND_EFFECT_INVALIDATE_RGB = 1u << 0,
    NOAH_QMK_VIA_COMMAND_EFFECT_RESEED_MACROS  = 1u << 1,
};

#ifdef VIA_ENABLE
uint16_t noah_qmk_via_macro_seed_capacity(void);
void     noah_qmk_via_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data);
bool     noah_qmk_via_should_seed_defaults_post_init(void);
uint8_t  noah_qmk_via_command_effects(uint8_t command_id);
#else
static inline uint16_t noah_qmk_via_macro_seed_capacity(void) {
    return 0;
}

static inline void noah_qmk_via_macro_set_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    (void)offset;
    (void)size;
    (void)data;
}

static inline bool noah_qmk_via_should_seed_defaults_post_init(void) {
    return false;
}

static inline uint8_t noah_qmk_via_command_effects(uint8_t command_id) {
    (void)command_id;
    return NOAH_QMK_VIA_COMMAND_EFFECT_NONE;
}
#endif

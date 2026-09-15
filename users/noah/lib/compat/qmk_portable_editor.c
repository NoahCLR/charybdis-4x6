#include QMK_KEYBOARD_H
#include "qmk_portable_editor.h"
#ifdef NOAH_PORTABLE_PROFILE_ENABLE
#    include <string.h>
#    include "keycode_config.h"
#    include "rgb_matrix.h"

// Reuse QMK's actual enabled-effect inventory and compiler bitfield layout.
// These const tables are firmware metadata, never an authored profile cache.
static const char effect_names[][64] = {
#    define RGB_MATRIX_EFFECT(name, ...) #name,
#    include "rgb_matrix_effects.inc"
#    undef RGB_MATRIX_EFFECT
#    ifdef COMMUNITY_MODULES_ENABLE
#        define RGB_MATRIX_EFFECT(name, ...) "COMMUNITY_" #name,
#        include "rgb_matrix_community_modules.inc"
#        undef RGB_MATRIX_EFFECT
#    endif
#    if defined(RGB_MATRIX_CUSTOM_KB) || defined(RGB_MATRIX_CUSTOM_USER)
#        define RGB_MATRIX_EFFECT(name, ...) "CUSTOM_" #name,
#        ifdef RGB_MATRIX_CUSTOM_KB
#            include "rgb_matrix_kb.inc"
#        endif
#        ifdef RGB_MATRIX_CUSTOM_USER
#            include "rgb_matrix_user.inc"
#        endif
#        undef RGB_MATRIX_EFFECT
#    endif
};
static const keymap_config_t option_masks[] = {
    {.swap_control_capslock = true}, {.capslock_to_control = true}, {.swap_lalt_lgui = true}, {.swap_ralt_rgui = true}, {.no_gui = true}, {.swap_grave_esc = true}, {.swap_backslash_backspace = true}, {.nkro = true}, {.swap_lctl_lgui = true}, {.swap_rctl_rgui = true}, {.oneshot_enable = true}, {.swap_escape_capslock = true}, {.autocorrect_enable = true},
};
enum { OPTIONS_COUNT = 13, EFFECT_COUNT = sizeof(effect_names) / sizeof(effect_names[0]), EDITOR_BYTES = OPTIONS_COUNT * 2 + sizeof(effect_names) };
_Static_assert(EFFECT_COUNT == RGB_MATRIX_EFFECT_MAX - 1, "editor effect inventory must match QMK");
_Static_assert(EDITOR_BYTES <= 253 * 25, "editor metadata must fit GET pages 3 through 255");

static uint16_t supported_options(void) {
    uint16_t result = 0;
#    ifdef MAGIC_ENABLE
    result |= 0x0b7f; // remapping and GUI suppression, semantic option IDs
#    endif
#    ifdef NKRO_ENABLE
    result |= 1u << 7;
#    endif
#    ifndef NO_ACTION_ONESHOT
    result |= 1u << 10;
#    endif
#    ifdef AUTOCORRECT_ENABLE
    result |= 1u << 12;
#    endif
    return result;
}

uint8_t noah_qmk_portable_editor_page(uint8_t page, uint8_t *payload) {
    if (!payload || !page) return 0;
    if (page == 1) {
        payload[0] = 1;
        payload[1] = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        return 2;
    }
    if (page == 2) {
        uint16_t options   = supported_options();
        uint8_t  led_flags = 0;
        for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++)
            led_flags |= g_led_config.flags[i];
        const uint8_t metadata[] = {1, 25, EDITOR_BYTES & 255, EDITOR_BYTES >> 8, EFFECT_COUNT, OPTIONS_COUNT, options & 255, options >> 8, led_flags};
        memcpy(payload, metadata, sizeof(metadata));
        return sizeof(metadata);
    }
    uint16_t offset = (page - 3u) * 25u;
    if (offset >= EDITOR_BYTES) return 0;
    uint8_t count = EDITOR_BYTES - offset < 25 ? EDITOR_BYTES - offset : 25;
    for (uint8_t i = 0; i < count; i++, offset++) {
        payload[i] = offset < OPTIONS_COUNT * 2 ? option_masks[offset / 2].raw >> ((offset % 2) * 8) : ((const uint8_t *)effect_names)[offset - OPTIONS_COUNT * 2];
    }
    return count;
}

void noah_qmk_portable_apply_lighting(uint32_t mode, uint32_t color) {
    // QMK ignores mode and HSV setters while disabled. This synchronous safe-
    // boundary update never runs the LED renderer between enabling and restoring
    // the requested state, and persists the final on/off choice.
    rgb_matrix_enable_noeeprom();
    rgb_matrix_mode((mode >> 8) & 255);
    rgb_matrix_set_speed((mode >> 16) & 255);
    rgb_matrix_set_flags(mode >> 24);
    rgb_matrix_sethsv(color & 255, (color >> 8) & 255, (color >> 16) & 255);
    if (mode & 255)
        rgb_matrix_enable();
    else
        rgb_matrix_disable();
}
#endif

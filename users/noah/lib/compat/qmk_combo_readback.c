#include QMK_KEYBOARD_H

#include "qmk_combo_readback.h"

#ifdef VIA_ENABLE
#    include <string.h>
#    include "qmk_effective_combos.h"
#    include "../profile/protocol/profile_wire_v1.h"
#    include "../profile/storage/profile_checksum.h"
#    include "../profile/storage/profile_storage_layout.h"

_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO == 4u, "Combo readout v1 has four input slots");
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_COMBOS == 32u, "Combo readout v1 has a 32-row bound");

#    ifdef COMBO_ENABLE
bool is_combo_enabled(void);
#        ifndef COMBO_ONLY_FROM_LAYER
uint8_t combo_ref_from_layer(uint8_t layer);
#        endif
#        ifdef COMBO_TERM_PER_COMBO
uint16_t get_combo_term(uint16_t index, combo_t *combo);
#        endif
#        ifdef COMBO_MUST_HOLD_PER_COMBO
bool get_combo_must_hold(uint16_t index, combo_t *combo);
#        endif
#        ifdef COMBO_MUST_TAP_PER_COMBO
bool get_combo_must_tap(uint16_t index, combo_t *combo);
#        endif
#        ifdef COMBO_MUST_PRESS_IN_ORDER_PER_COMBO
bool get_combo_must_press_in_order(uint16_t index, combo_t *combo);
#        endif
#        ifndef COMBO_HOLD_TERM
#            define COMBO_HOLD_TERM TAPPING_TERM
#        endif
#    endif

#    ifdef COMBO_ENABLE
static void write_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
}
#    endif

static bool combo_row(uint8_t index, uint8_t out[25]) {
    memset(out, 0, 25u);
#    ifdef COMBO_ENABLE
    if (index >= noah_qmk_combo_count()) return false;
    combo_t *combo = noah_qmk_combo_get(index);
    if (!combo->keys) return false;
    out[0] = index;
    write_u16(&out[2], combo->keycode);
#        ifdef COMBO_TERM_PER_COMBO
    write_u16(&out[4], get_combo_term(index, combo));
#        else
    write_u16(&out[4], COMBO_TERM);
#        endif
    write_u16(&out[6], COMBO_HOLD_TERM);
#        if !defined(COMBO_NO_TIMER)
#            ifdef COMBO_MUST_HOLD_PER_COMBO
    if (get_combo_must_hold(index, combo)) out[8] |= 1u;
#            elif defined(COMBO_MUST_HOLD_MODS)
    uint16_t key = combo->keycode;
    if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) out[8] |= 1u;
#            endif
#        endif
#        ifdef COMBO_MUST_TAP_PER_COMBO
    if (get_combo_must_tap(index, combo)) out[8] |= 2u;
#        endif
#        ifdef COMBO_MUST_PRESS_IN_ORDER_PER_COMBO
    if (get_combo_must_press_in_order(index, combo)) out[8] |= 4u;
#        elif defined(COMBO_MUST_PRESS_IN_ORDER)
    out[8] |= 4u;
#        endif
    for (uint8_t member = 0u; member <= NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO; member++) {
        uint16_t key = pgm_read_word(&combo->keys[member]);
        if (key == COMBO_END) return member >= 2u;
        if (member == NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO) return false;
        for (uint8_t previous = 0u; previous < member; previous++) {
            if (pgm_read_word(&combo->keys[previous]) == key) return false;
        }
        write_u16(&out[9u + 2u * member], key);
        out[1]++;
    }
#    else
    (void)index;
#    endif
    return false;
}

static bool combo_metadata(uint8_t out[25]) {
    uint8_t row[25];
    memset(out, 0, 25u);
    if (LAYER_COUNT < 1u || LAYER_COUNT > 8u) return false;
    out[0] = 1u;
    out[2] = NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO;
    out[3] = LAYER_COUNT;
#    ifdef COMBO_ENABLE
#        ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
    if (!noah_effective_combo_valid()) return false;
#        endif
    if (noah_qmk_combo_count() > NOAH_PROFILE_WIRE_V1_MAX_COMBOS) return false;
    out[1] = noah_qmk_combo_count();
    out[4] = is_combo_enabled() ? 1u : 0u;
#        ifdef COMBO_NO_TIMER
    out[5] |= 1u;
#        endif
#        ifdef COMBO_STRICT_TIMER
    out[5] |= 2u;
#        endif
#        ifdef COMBO_SHOULD_TRIGGER
    out[5] |= 4u;
#        endif
#        ifdef COMBO_PROCESS_KEY_RELEASE
    out[5] |= 8u;
#        endif
#        ifdef COMBO_PROCESS_KEY_REPRESS
    out[5] |= 16u;
#        endif
#        ifdef COMBO_ONLY_FROM_LAYER
    out[5] |= 32u;
#        endif
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
#        ifdef COMBO_ONLY_FROM_LAYER
        uint8_t reference = COMBO_ONLY_FROM_LAYER;
#        else
        uint8_t reference = combo_ref_from_layer(layer);
#        endif
        if (reference >= LAYER_COUNT) return false;
        out[6u + layer] = reference;
    }
#    endif
    uint32_t digest = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, out, 14u);
    for (uint8_t index = 0u; index < out[1]; index++) {
        if (!combo_row(index, row)) return false;
        digest = noah_profile_fnv1a_update(digest, row, sizeof(row));
    }
    for (uint8_t byte = 0u; byte < 4u; byte++) out[14u + byte] = (uint8_t)(digest >> (8u * byte));
    return true;
}

bool noah_qmk_combo_readback_get(uint8_t *report, uint8_t length) {
    if (!report || length != 32u || report[0] != 0x08u || report[1] != 0u || report[2] != NOAH_PROFILE_WIRE_V1_VALUE_COMBOS) return false;
    bool valid = report[3] != 0u;
    for (uint8_t byte = 5u; byte < 32u; byte++) valid = valid && report[byte] == 0u;
    memset(&report[5], 0, 27u);
    if (!valid) {report[5] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED; return true;}
    if (!combo_metadata(&report[7])) {
        memset(&report[7], 0, 25u);
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    if (report[4] > report[8]) {
        memset(&report[7], 0, 25u);
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE;
        return true;
    }
    if (report[4] > 0u && !combo_row((uint8_t)(report[4] - 1u), &report[7])) {
        memset(&report[7], 0, 25u);
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    report[6] = 25u;
    return true;
}

#else
bool noah_qmk_combo_readback_get(uint8_t *report, uint8_t length) {
    (void)report;
    (void)length;
    return false;
}
#endif

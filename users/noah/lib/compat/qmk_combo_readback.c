#include QMK_KEYBOARD_H

#include "qmk_combo_readback.h"

#ifdef VIA_ENABLE
#    include <string.h>
#    include "qmk_effective_combos.h"
#    include "../profile/protocol/profile_wire_v1.h"
#    include "../profile/storage/profile_checksum.h"
#    include "../profile/storage/profile_storage_layout.h"

// Combo readout v3 (D-F14): page 0 is metadata, page 1 the reference layer
// of every layer, then two pages per row, which hold up to sixteen inputs.
enum {
    READOUT_VERSION       = 3u,
    READOUT_PAGE          = 25u,
    READOUT_PAGES_PER_ROW = 2u,
    READOUT_FIRST_ROW     = 2u,
    READOUT_ROW_INPUTS_A  = 7u,
    READOUT_MAX_INPUTS    = 16u,
    READOUT_MAX_LAYERS    = 16u,
};
_Static_assert(NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO <= READOUT_MAX_INPUTS, "Combo readout v3 rows hold at most sixteen inputs");
_Static_assert(READOUT_FIRST_ROW + READOUT_PAGES_PER_ROW * NOAH_PROFILE_WIRE_V1_MAX_COMBOS <= UINT16_MAX, "Combo readout pages fit a wide page number");

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
// The window a combo without its own follows. Only the live owner stores
// windows per combo; without it a per-combo hook is the user's own, so its
// rows are reported as explicit.
#        ifdef NOAH_LIVE_PROFILE_OWNER_ENABLE
#            define NOAH_COMBO_READBACK_DEFAULT_TERM noah_effective_combo_default_term()
#            define NOAH_COMBO_READBACK_FOLLOWS_DEFAULT(index) noah_effective_combo_follows_default(index)
#        elif defined(COMBO_TERM_PER_COMBO)
#            define NOAH_COMBO_READBACK_DEFAULT_TERM COMBO_TERM
#            define NOAH_COMBO_READBACK_FOLLOWS_DEFAULT(index) false
#        else
#            define NOAH_COMBO_READBACK_DEFAULT_TERM COMBO_TERM
#            define NOAH_COMBO_READBACK_FOLLOWS_DEFAULT(index) true
#        endif
#    endif

#    ifdef COMBO_ENABLE
static void write_u16(uint8_t *out, uint16_t value) {
    out[0] = (uint8_t)value;
    out[1] = (uint8_t)(value >> 8u);
}
#    endif

static void write_u32(uint8_t *out, uint32_t value) {
    for (uint8_t byte = 0u; byte < 4u; byte++)
        out[byte] = (uint8_t)(value >> (8u * byte));
}

// One row as its two pages, 50 bytes: page A is the row index, input count,
// native output, window, flags, allowed-layer mask and inputs 0..6; page B
// inputs 7..15. Flags: bit 0 must hold, 1 must tap, 2 press in order, 3
// follows the default window, 4 disabled.
static bool combo_row(uint8_t index, uint8_t out[2u * READOUT_PAGE]) {
    memset(out, 0, 2u * READOUT_PAGE);
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
    if (NOAH_COMBO_READBACK_FOLLOWS_DEFAULT(index)) out[6] |= 8u;
#        if !defined(COMBO_NO_TIMER)
#            ifdef COMBO_MUST_HOLD_PER_COMBO
    if (get_combo_must_hold(index, combo)) out[6] |= 1u;
#            elif defined(COMBO_MUST_HOLD_MODS)
    uint16_t key = combo->keycode;
    if ((key >= 0xe0u && key <= 0xe7u) || (key >= 0x100u && key <= 0x1fffu && (key & 0xffu) == 0u) || (key >= 0x5220u && key <= 0x523fu)) out[6] |= 1u;
#            endif
#        endif
#        ifdef COMBO_MUST_TAP_PER_COMBO
    if (get_combo_must_tap(index, combo)) out[6] |= 2u;
#        endif
#        ifdef COMBO_MUST_PRESS_IN_ORDER_PER_COMBO
    if (get_combo_must_press_in_order(index, combo)) out[6] |= 4u;
#        elif defined(COMBO_MUST_PRESS_IN_ORDER)
    out[6] |= 4u;
#        endif
    if (!noah_qmk_combo_enabled(index)) out[6] |= 16u;
    write_u32(&out[7], noah_qmk_combo_allowed_layers(index));
    for (uint8_t member = 0u; member <= NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO; member++) {
        uint16_t key = pgm_read_word(&combo->keys[member]);
        if (key == COMBO_END) return member >= 2u;
        if (member == NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO) return false;
        for (uint8_t previous = 0u; previous < member; previous++) {
            if (pgm_read_word(&combo->keys[previous]) == key) return false;
        }
        // Inputs 0..6 fill page A from byte 11; 7..15 page B from byte 0.
        write_u16(member < READOUT_ROW_INPUTS_A ? &out[11u + 2u * member] : &out[READOUT_PAGE + 2u * (member - READOUT_ROW_INPUTS_A)], key);
        out[1]++;
    }
#    else
    (void)index;
#    endif
    return false;
}

static bool reference_page(uint8_t out[READOUT_PAGE]) {
    memset(out, 0, READOUT_PAGE);
#    ifdef COMBO_ENABLE
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++) {
#        ifdef COMBO_ONLY_FROM_LAYER
        uint8_t reference = COMBO_ONLY_FROM_LAYER;
#        else
        uint8_t reference = combo_ref_from_layer(layer);
#        endif
        if (reference >= LAYER_COUNT) return false;
        out[layer] = reference;
    }
#    else
    for (uint8_t layer = 0u; layer < LAYER_COUNT; layer++)
        out[layer] = layer;
#    endif
    return true;
}

// Page 0. Its digest covers bytes 0..5 and 10..24 of this page, page 1, then
// every row's two pages in order.
static bool combo_metadata(uint8_t out[READOUT_PAGE]) {
    uint8_t row[2u * READOUT_PAGE];
    memset(out, 0, READOUT_PAGE);
    if ((unsigned)LAYER_COUNT < 1u || (unsigned)LAYER_COUNT > (unsigned)READOUT_MAX_LAYERS) return false;
    out[0]  = READOUT_VERSION;
    out[2]  = NOAH_PROFILE_WIRE_V1_MAX_KEYS_PER_COMBO;
    out[3]  = LAYER_COUNT;
    out[14] = READOUT_PAGES_PER_ROW;
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
#        if defined(COMBO_SHOULD_TRIGGER) && !defined(NOAH_COMBO_PARTICIPATION_HOOK)
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
    write_u16(&out[10], NOAH_COMBO_READBACK_DEFAULT_TERM);
    write_u16(&out[12], COMBO_HOLD_TERM);
#    endif
    uint32_t digest = noah_profile_fnv1a_update(NOAH_PROFILE_FNV1A_INITIAL, out, 6u);
    digest          = noah_profile_fnv1a_update(digest, &out[10], READOUT_PAGE - 10u);
    if (!reference_page(row)) return false;
    digest = noah_profile_fnv1a_update(digest, row, READOUT_PAGE);
    for (uint8_t index = 0u; index < out[1]; index++) {
        if (!combo_row(index, row)) return false;
        digest = noah_profile_fnv1a_update(digest, row, sizeof(row));
    }
    write_u32(&out[6], digest);
    return true;
}

// GET 0x06 takes a wide page (feature bit 19): 128 rows need 258 pages.
bool noah_qmk_combo_readback_get(uint8_t *report, uint8_t length) {
    if (!report || length != 32u || report[0] != 0x08u || report[1] != 0u || report[2] != NOAH_PROFILE_WIRE_V1_VALUE_COMBOS) return false;
    uint16_t page  = noah_profile_wire_v1_wide_page(report);
    bool     valid = report[3] != 0u;
    for (uint8_t byte = NOAH_PROFILE_WIRE_V1_WIDE_REQUEST_FIXED; byte < 32u; byte++)
        valid = valid && report[byte] == 0u;
    memset(&report[5], 0, 27u);
    if (!valid) {
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_MALFORMED;
        return true;
    }
    uint8_t row[2u * READOUT_PAGE];
    if (!combo_metadata(&report[7])) {
        memset(&report[7], 0, 25u);
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
        return true;
    }
    if (page >= READOUT_FIRST_ROW + READOUT_PAGES_PER_ROW * (uint16_t)report[8]) {
        memset(&report[7], 0, 25u);
        report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNKNOWN_PAGE;
        return true;
    }
    if (page == 1u) {
        if (!reference_page(&report[7])) {
            memset(&report[7], 0, 25u);
            report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
            return true;
        }
    } else if (page >= READOUT_FIRST_ROW) {
        uint16_t index = (uint16_t)((page - READOUT_FIRST_ROW) / READOUT_PAGES_PER_ROW);
        if (!combo_row((uint8_t)index, row)) {
            memset(&report[7], 0, 25u);
            report[5] = NOAH_PROFILE_WIRE_V1_STATUS_UNAVAILABLE;
            return true;
        }
        memcpy(&report[7], &row[((page - READOUT_FIRST_ROW) % READOUT_PAGES_PER_ROW) * READOUT_PAGE], READOUT_PAGE);
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

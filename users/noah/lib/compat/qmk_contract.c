// ────────────────────────────────────────────────────────────────────────────
// QMK Contract Compatibility
// ────────────────────────────────────────────────────────────────────────────

#include "qmk_via_playback_contract.h"

#ifdef VIA_ENABLE
#    include "dynamic_keymap.h"
#    include "../macro/via_macro_provider.h"

uint8_t noah_qmk_via_macro_count(void) {
    return dynamic_keymap_macro_get_count();
}

uint16_t noah_qmk_via_macro_buffer_size(void) {
    return dynamic_keymap_macro_get_buffer_size();
}

void noah_qmk_via_macro_get_buffer(uint16_t offset, uint16_t size, uint8_t *data) {
    dynamic_keymap_macro_get_buffer(offset, size, data);
}
#endif

bool noah_qmk_contract_try_play_via_macro(uint16_t action) {
#ifdef VIA_ENABLE
    return via_macro_provider_try_play(action);
#else
    (void)action;
#endif

    return false;
}

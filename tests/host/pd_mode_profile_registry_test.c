#include <assert.h>
#include <stddef.h>

#include "noah_keymap_ids.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

int main(void) {
    // Thirty-two slots: slot n holds at 0x7e80 + n and toggles at 0x7ea0 + n,
    // filling both reserved blocks; slot 31's flag is the mask's top bit.
    assert(PD_MODE_COUNT == 32);
    assert(PD_SLOT_0 == 0x7e80 && PD_SLOT_7 == 0x7e87 && PD_SLOT_8 == 0x7e88 && PD_SLOT_31 == 0x7e9f);
    assert(PD_SLOT_0_LOCK == 0x7ea0 && PD_SLOT_7_LOCK == 0x7ea7 && PD_SLOT_8_LOCK == 0x7ea8 && PD_SLOT_31_LOCK == 0x7ebf);
    assert((pd_mode_mask_t)PD_MODE_SLOT_31 == UINT32_C(0x80000000));
    assert((pd_mode_mask_t)PD_MODE_SLOT_16 == UINT32_C(0x00010000));
    assert(pd_mode_id_from_mask(UINT32_C(0x80000000)) == 31);
    assert(pd_mode_id_from_mask(UINT32_C(0x80000001)) == PD_MODE_ID_NONE);
    assert(pd_mode_id_from_mask(0) == PD_MODE_ID_NONE);
    assert(pd_mode_id_from_mask(UINT32_C(0x00000003)) == PD_MODE_ID_NONE);
    assert(pd_mode_id_from_mask(UINT32_C(0xffffffff)) == PD_MODE_ID_NONE);
    assert(pd_mode_mask_from_id(31) == UINT32_C(0x80000000));
    assert(pd_mode_mask_from_id(32) == 0);

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        const pd_mode_def_t *mode = &pd_modes[i];
        assert(mode->mode_flag == ((pd_mode_mask_t)1u << i));
        assert(pd_mode_id_from_mask(mode->mode_flag) == i);
        assert(mode->keycode == (uint16_t)(0x7e80 + i));
        assert(mode->lock_action == (uint16_t)(0x7ea0 + i));
        assert(mode->handler == NULL);
        assert(mode->key_handler == NULL);
        assert(mode->reset == NULL);
        assert(mode->dpi == 0);
        assert(mode->traits == PD_MODE_TRAIT_NONE);
        assert(mode->lifecycle == NULL);
    }
    return 0;
}

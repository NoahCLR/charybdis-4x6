#include <assert.h>
#include <stddef.h>

#include "noah_keymap_ids.h"
#include "users/noah/lib/pointing/defs/pd_modes.h"

int main(void) {
    const uint16_t keys[PD_MODE_COUNT] = {
        PD_SLOT_0, PD_SLOT_1, PD_SLOT_2, PD_SLOT_3,
        PD_SLOT_4, PD_SLOT_5, PD_SLOT_6, PD_SLOT_7,
    };
    const uint16_t locks[PD_MODE_COUNT] = {
        PD_SLOT_0_LOCK, PD_SLOT_1_LOCK, PD_SLOT_2_LOCK, PD_SLOT_3_LOCK,
        PD_SLOT_4_LOCK, PD_SLOT_5_LOCK, PD_SLOT_6_LOCK, PD_SLOT_7_LOCK,
    };

    for (uint8_t i = 0; i < PD_MODE_COUNT; i++) {
        const pd_mode_def_t *mode = &pd_modes[i];
        assert(mode->mode_flag == (pd_mode_mask_t)(1u << i));
        assert(mode->keycode == keys[i]);
        assert(mode->lock_action == locks[i]);
        assert(mode->handler == NULL);
        assert(mode->key_handler == NULL);
        assert(mode->reset == NULL);
        assert(mode->dpi == 0);
        assert(mode->traits == PD_MODE_TRAIT_NONE);
        assert(mode->lifecycle == NULL);
    }
    return 0;
}

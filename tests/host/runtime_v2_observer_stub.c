#include "users/noah/lib/runtime_v2/runtime_v2.h"

void runtime_v2_layer_lock_set(uint8_t layer, bool active) {
    (void)layer;
    (void)active;
}

void runtime_v2_pd_mode_lock_set(pd_mode_mask_t mode, bool active) {
    (void)mode;
    (void)active;
}

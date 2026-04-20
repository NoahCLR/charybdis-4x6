#include <stdbool.h>
#include "users/noah/lib/key/runtime/core/runtime.h"

void runtime_v2_layer_lock_set(uint8_t layer, bool locked) {
    (void)layer;
    (void)locked;
}

void runtime_v2_pd_mode_lock_set(pd_mode_mask_t mode, bool locked) {
    (void)mode;
    (void)locked;
}

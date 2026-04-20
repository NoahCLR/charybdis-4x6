#include <stdbool.h>
#include "users/noah/lib/key/runtime/core/runtime.h"

void key_runtime_core_layer_lock_set(uint8_t layer, bool locked) {
    (void)layer;
    (void)locked;
}

void key_runtime_core_pd_mode_lock_set(pd_mode_mask_t mode, bool locked) {
    (void)mode;
    (void)locked;
}

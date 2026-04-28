#include <stdbool.h>
#include "users/noah/lib/key/runtime/core/runtime.h"

void key_runtime_core_layer_lock_set(uint8_t layer, bool locked) {
    (void)layer;
    (void)locked;
}

void key_runtime_core_observe_pd_mode_lock_state(pd_mode_mask_t mode, bool locked) {
    (void)mode;
    (void)locked;
}

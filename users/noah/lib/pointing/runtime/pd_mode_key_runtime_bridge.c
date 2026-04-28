#include "pd_mode_key_runtime_bridge.h"

#include "../../key/runtime/core/runtime.h"

void pd_mode_key_runtime_bridge_observe_lock_state(pd_mode_mask_t mode, bool locked) {
    key_runtime_core_pd_mode_lock_set(mode, locked);
}

#include "pointing_mode_key_runtime_bridge.h"

#include "../../key/runtime/reducer/ownership_state.h"

void pointing_mode_key_runtime_bridge_observe_local_lock_state(pointing_mode_mask_t mode, bool locked) {
    key_runtime_core_observe_pointing_mode_lock_state(mode, locked);
}
